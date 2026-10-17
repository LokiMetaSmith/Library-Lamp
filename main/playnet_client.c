#include "playnet_client.h"
#include "reticulum.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "mbedtls/sha256.h"
#include "mbedtls/base64.h"
#include "sodium.h"
#include "cJSON.h"
#include <string.h>
#include <time.h>
#include "esp_wifi.h"

static const char *TAG = "PLAYNET_CLIENT";

void playnet_client_init(void) {
    ESP_LOGI(TAG, "Playnet client initialized.");
}

static void get_current_time_str(char *buf, size_t max_len) {
    time_t now;
    struct tm timeinfo;
    time(&now);
    gmtime_r(&now, &timeinfo);
    // RFC 9421 might need created timestamp, let's just use unix time for now
    snprintf(buf, max_len, "%lld", (long long)now);
}

// Generates the SHA-256 Content-Digest (RFC 8941 Dictionary Format)
static void generate_content_digest(const char *content, char *digest_out, size_t digest_out_len) {
    unsigned char hash[32];
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0); // 0 for SHA-256
    mbedtls_sha256_update(&ctx, (const unsigned char *)content, strlen(content));
    mbedtls_sha256_finish(&ctx, hash);
    mbedtls_sha256_free(&ctx);

    unsigned char base64_hash[64];
    size_t olen = 0;
    mbedtls_base64_encode(base64_hash, sizeof(base64_hash), &olen, hash, 32);

    // RFC 8941 dict format for Digest header (or Content-Digest)
    snprintf(digest_out, digest_out_len, "sha-256=:%s:", base64_hash);
}

// RFC 9421 signature generation
static esp_err_t generate_rfc9421_signature(const char *method, const char *target_uri, const char *content_digest, char *sig_input_out, size_t sig_input_len, char *sig_out, size_t sig_out_len) {
    // 1. Create the Signature-Input string
    char created_str[32];
    get_current_time_str(created_str, sizeof(created_str));

    // We assume keyid is the Ed25519 public key encoded in hex or base64. Let's use hex.
    char pubkey_hex[ED25519_PUBLIC_KEY_BYTES * 2 + 1];
    for (int i = 0; i < ED25519_PUBLIC_KEY_BYTES; i++) {
        sprintf(pubkey_hex + (i * 2), "%02x", g_rns_local_identity.public_key[i]);
    }

    snprintf(sig_input_out, sig_input_len, "sig1=(\"@method\" \"@target-uri\" \"content-digest\");created=%s;keyid=\"%s\"", created_str, pubkey_hex);

    // 2. Create the signature base string
    char sig_base[512];
    snprintf(sig_base, sizeof(sig_base),
             "\"@method\": %s\n"
             "\"@target-uri\": %s\n"
             "\"content-digest\": %s\n"
             "\"@signature-params\": (\"@method\" \"@target-uri\" \"content-digest\");created=%s;keyid=\"%s\"",
             method, target_uri, content_digest, created_str, pubkey_hex);

    // 3. Sign the base string
    unsigned char signature[crypto_sign_BYTES];
    unsigned long long sig_len = 0;

    // Use libsodium
    crypto_sign_ed25519_detached(signature, &sig_len, (const unsigned char*)sig_base, strlen(sig_base), g_rns_local_identity.private_key);

    // 4. Base64 encode the signature
    unsigned char base64_sig[128];
    size_t olen = 0;
    mbedtls_base64_encode(base64_sig, sizeof(base64_sig), &olen, signature, sig_len);

    snprintf(sig_out, sig_out_len, "sig1=:%s:", base64_sig);
    return ESP_OK;
}

// Send HTTP POST over Wi-Fi
static esp_err_t send_wifi_http_post(const char *url, const char *payload, const char *content_digest, const char *sig_input, const char *signature, char **response_out) {
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 10000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        return ESP_FAIL;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Content-Digest", content_digest);
    esp_http_client_set_header(client, "Signature-Input", sig_input);
    esp_http_client_set_header(client, "Signature", signature);

    esp_http_client_set_post_field(client, payload, strlen(payload));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP POST Status = %d", status_code);

        int content_length = esp_http_client_get_content_length(client);
        if (content_length > 0) {
            *response_out = malloc(content_length + 1);
            if (*response_out) {
                int read_len = 0;
                while (read_len < content_length) {
                    int ret = esp_http_client_read(client, *response_out + read_len, content_length - read_len);
                    if (ret <= 0) break;
                    read_len += ret;
                }
                (*response_out)[read_len] = '\0';
            }
        } else {
            // Chunked response handling
            int buf_size = 512;
            int total_read = 0;
            char *buf = malloc(buf_size);
            if (buf) {
                while (1) {
                    if (total_read + 256 > buf_size) {
                        buf_size *= 2;
                        char *new_buf = realloc(buf, buf_size);
                        if (!new_buf) {
                            free(buf);
                            buf = NULL;
                            break;
                        }
                        buf = new_buf;
                    }
                    int ret = esp_http_client_read(client, buf + total_read, buf_size - total_read - 1);
                    if (ret < 0) {
                        break;
                    } else if (ret == 0) {
                        if (esp_http_client_is_complete_data_received(client)) break;
                        continue;
                    }
                    total_read += ret;
                }
                if (buf) {
                    buf[total_read] = '\0';
                    *response_out = buf;
                }
            }
        }
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return err;
}

// LoRa dispatch via Reticulum Link/LXMF
extern void lora_wan_transmit_raw(const uint8_t *data, size_t len);
static esp_err_t send_lora_rpc(const char *payload) {
    ESP_LOGI(TAG, "Sending Playnet RPC via LoRa/Reticulum: %s", payload);

    // Create a Reticulum packet targeting a mock gateway hash
    uint8_t gateway_hash[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};

    rns_packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    memcpy(pkt.dest_hash, gateway_hash, 16);
    pkt.type = RNS_PKT_TYPE_DATA;
    pkt.hops = 0;
    pkt.flags = RNS_PKT_FLAG_HEADER; // Minimal headers for raw dispatch
    pkt.payload = (uint8_t*)payload;
    pkt.payload_len = strlen(payload);

    uint8_t encoded_buf[255]; // SX1262 MTU size
    size_t encoded_len = 0;

    if (rns_encode_packet(&pkt, encoded_buf, &encoded_len)) {
        if (encoded_len > 255) {
            ESP_LOGE(TAG, "Payload too large for LoRa MTU (255 bytes).");
            return ESP_FAIL;
        }

        ESP_LOGI(TAG, "Transmitting Playnet request via LoRa...");
        lora_wan_transmit_raw(encoded_buf, encoded_len);
        return ESP_OK;
    }

    ESP_LOGE(TAG, "Failed to encode RNS packet for Playnet LoRa transport");
    return ESP_FAIL;
}

esp_err_t playnet_mcp_request(const char *method, const char *params_json, char **response_json_out) {
    // 1. Build JSON-RPC payload
    size_t payload_len = 128 + strlen(method) + (params_json ? strlen(params_json) : 2);
    char *payload = malloc(payload_len);
    if (!payload) return ESP_FAIL;
    snprintf(payload, payload_len, "{\"jsonrpc\": \"2.0\", \"method\": \"%s\", \"params\": %s, \"id\": 1}", method, params_json ? params_json : "{}");

    // 2. Generate Content-Digest
    char content_digest[128];
    generate_content_digest(payload, content_digest, sizeof(content_digest));

    // 3. Generate RFC 9421 Signatures
    char sig_input[256];
    char signature[256];
    // RFC 9421 requires derived components like @method to be lowercase in the signature base string.
    generate_rfc9421_signature("post", "/api/mcp", content_digest, sig_input, sizeof(sig_input), signature, sizeof(signature));

    // 4. Decide Transport
    // Check if we have Wi-Fi STA connection
    wifi_ap_record_t ap_info;
    esp_err_t wifi_status = esp_wifi_sta_get_ap_info(&ap_info);

    esp_err_t ret = ESP_FAIL;
    if (wifi_status == ESP_OK) {
        ESP_LOGI(TAG, "Wi-Fi STA is connected. Sending direct HTTP POST.");
        ret = send_wifi_http_post("https://playnet.earth/api/mcp", payload, content_digest, sig_input, signature, response_json_out);
    } else {
        ESP_LOGI(TAG, "Wi-Fi STA not connected. Falling back to LoRa/Reticulum gateway.");
        ret = send_lora_rpc(payload);
    }

    free(payload);
    return ret;
}

esp_err_t playnet_push_post(const char *post_json) {
    // We can call playnet_mcp_request with method="declare-means" or "declare-need" or similar depending on the exact Playnet api requirement
    // For now we'll just mock it as "post-commitment"
    char *response = NULL;
    esp_err_t err = playnet_mcp_request("declare-means", post_json, &response);
    if (response) {
        ESP_LOGI(TAG, "Response from playnet: %s", response);
        free(response);
    }
    return err;
}

// No longer needed as we inline the cJSON parsing in playnet_sync_task.c
