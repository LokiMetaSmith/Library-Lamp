#include "bulletin_api.h"
#include "bulletin_board.h"
#include "cJSON.h"
#include <string.h>
#include "esp_timer.h"

extern unsigned long nowSecs(void); // From bulletin_board.c

static void sanitize(const char* in, char* out, int maxLen) {
    int outIdx = 0;
    for (int i = 0; in[i] && outIdx < maxLen - 1; i++) {
        if (in[i] != '<' && in[i] != '>') {
            out[outIdx++] = in[i];
        }
    }
    out[outIdx] = '\0';
}

static esp_err_t board_info_handler(httpd_req_t *req) {
    cJSON *doc = cJSON_CreateObject();
    cJSON_AddStringToObject(doc, "name", id_name);
    cJSON_AddStringToObject(doc, "icon", id_icon);
    cJSON_AddStringToObject(doc, "tagline", id_tagline);
    cJSON_AddStringToObject(doc, "rules", id_rules);
    cJSON_AddStringToObject(doc, "footer", id_footer);
    
    char uptime_str[32];
    snprintf(uptime_str, sizeof(uptime_str), "Uptime: %lu m", (unsigned long)(esp_timer_get_time() / 60000000ULL));
    cJSON_AddStringToObject(doc, "uptime", uptime_str);

    char *json_str = cJSON_PrintUnformatted(doc);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));
    free(json_str);
    cJSON_Delete(doc);
    return ESP_OK;
}

static esp_err_t board_messages_handler(httpd_req_t *req) {
    cJSON *doc = cJSON_CreateArray();
    unsigned long now = nowSecs();
    for (int i = 0; i < msgCount; i++) {
        if (msgs[i].expires < now) continue;
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(obj, "id", msgs[i].id);
        cJSON_AddStringToObject(obj, "author", msgs[i].author);
        cJSON_AddStringToObject(obj, "type", msgs[i].type);
        cJSON_AddStringToObject(obj, "text", msgs[i].text);
        cJSON_AddNumberToObject(obj, "expires", msgs[i].expires);
        cJSON_AddItemToArray(doc, obj);
    }
    char *json_str = cJSON_PrintUnformatted(doc);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));
    free(json_str);
    cJSON_Delete(doc);
    return ESP_OK;
}

static esp_err_t board_post_handler(httpd_req_t *req) {
    char buf[1024];
    int ret, remaining = req->content_len;
    if (remaining >= sizeof(buf)) {
        httpd_resp_send_408(req);
        return ESP_FAIL;
    }

    ret = httpd_req_recv(req, buf, remaining);
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = '\0';

    cJSON *json = cJSON_Parse(buf);
    if (!json) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    cJSON *j_author = cJSON_GetObjectItem(json, "author");
    cJSON *j_type = cJSON_GetObjectItem(json, "type");
    cJSON *j_text = cJSON_GetObjectItem(json, "text");
    cJSON *j_expiry = cJSON_GetObjectItem(json, "expiry");

    char author[25] = "neighbor";
    if (j_author && j_author->valuestring) sanitize(j_author->valuestring, author, 25);
    
    char type[16] = "Notice";
    if (j_type && j_type->valuestring) {
        if (strcmp(j_type->valuestring, "Offer") == 0 || strcmp(j_type->valuestring, "Need") == 0 || strcmp(j_type->valuestring, "Event") == 0) {
            strncpy(type, j_type->valuestring, 15);
        }
    }

    char text[301] = "";
    if (j_text && j_text->valuestring) sanitize(j_text->valuestring, text, 301);

    int expiry = 72;
    if (j_expiry && cJSON_IsNumber(j_expiry)) expiry = j_expiry->valueint;

    cJSON_Delete(json);

    if (strlen(text) == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "empty message");
        return ESP_FAIL;
    }

    bb_add_message(author, type, text, expiry);
    httpd_resp_send(req, "ok", 2);
    return ESP_OK;
}

static esp_err_t admin_auth_handler(httpd_req_t *req) {
    char buf[256];
    int ret, remaining = req->content_len;
    if (remaining >= sizeof(buf)) {
        httpd_resp_send_408(req);
        return ESP_FAIL;
    }

    ret = httpd_req_recv(req, buf, remaining);
    if (ret <= 0) return ESP_FAIL;
    buf[ret] = '\0';

    cJSON *json = cJSON_Parse(buf);
    if (!json) { httpd_resp_send_500(req); return ESP_FAIL; }

    cJSON *j_key = cJSON_GetObjectItem(json, "key");
    if (j_key && j_key->valuestring && bb_check_key(j_key->valuestring)) {
        char token[33];
        bb_generate_token(token);
        httpd_resp_send(req, token, strlen(token));
    } else {
        httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "forbidden");
    }
    cJSON_Delete(json);
    return ESP_OK;
}

bool bb_is_admin_request(httpd_req_t *req) {
    if (g_hardware_key_authenticated) return true;
    char buf[128];
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        char token[33];
        if (httpd_query_key_value(buf, "token", token, sizeof(token)) == ESP_OK) {
            return bb_is_token_valid(token);
        }
    }
    return false;
}

static esp_err_t admin_auth_status_handler(httpd_req_t *req) {
    if (bb_is_admin_request(req)) {
        httpd_resp_send(req, "{\"authenticated\": true}", 23);
        return ESP_OK;
    }
    httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "forbidden");
    return ESP_FAIL;
}

static esp_err_t admin_identity_get_handler(httpd_req_t *req) {
    if (!bb_is_admin_request(req)) { httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "forbidden"); return ESP_FAIL; }
    return board_info_handler(req);
}

static esp_err_t admin_identity_set_handler(httpd_req_t *req) {
    if (!bb_is_admin_request(req)) { httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "forbidden"); return ESP_FAIL; }
    
    char buf[512];
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        char temp[101];
        if (httpd_query_key_value(buf, "name", temp, sizeof(temp)) == ESP_OK) sanitize(temp, id_name, 49);
        if (httpd_query_key_value(buf, "icon", temp, sizeof(temp)) == ESP_OK) sanitize(temp, id_icon, 9);
        if (httpd_query_key_value(buf, "tagline", temp, sizeof(temp)) == ESP_OK) sanitize(temp, id_tagline, 101);
        if (httpd_query_key_value(buf, "rules", temp, sizeof(temp)) == ESP_OK) sanitize(temp, id_rules, 101);
        if (httpd_query_key_value(buf, "footer", temp, sizeof(temp)) == ESP_OK) sanitize(temp, id_footer, 101);
        bb_save_identity();
        httpd_resp_send(req, "identity saved", 14);
        return ESP_OK;
    }
    httpd_resp_send_500(req);
    return ESP_FAIL;
}

static esp_err_t admin_setkey_handler(httpd_req_t *req) {
    if (!bb_is_admin_request(req)) { httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "forbidden"); return ESP_FAIL; }
    
    char buf[128];
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        char temp[65];
        if (httpd_query_key_value(buf, "newkey", temp, sizeof(temp)) == ESP_OK) {
            strncpy(admin_key, temp, 64);
            bb_save_admin_key();
            httpd_resp_send(req, "key updated", 11);
            return ESP_OK;
        }
    }
    httpd_resp_send_500(req);
    return ESP_FAIL;
}

static esp_err_t admin_clear_handler(httpd_req_t *req) {
    if (!bb_is_admin_request(req)) { httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "forbidden"); return ESP_FAIL; }
    bb_clear_messages();
    httpd_resp_send(req, "cleared", 7);
    return ESP_OK;
}

static esp_err_t admin_delete_post_handler(httpd_req_t *req) {
    if (!bb_is_admin_request(req)) { httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "forbidden"); return ESP_FAIL; }
    
    char buf[128];
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        char temp[16];
        if (httpd_query_key_value(buf, "id", temp, sizeof(temp)) == ESP_OK) {
            uint16_t targetId = atoi(temp);
            bb_delete_message(targetId);
            httpd_resp_send(req, "deleted", 7);
            return ESP_OK;
        }
    }
    httpd_resp_send_500(req);
    return ESP_FAIL;
}

void register_bulletin_api_handlers(httpd_handle_t server) {
    httpd_uri_t info_uri = { .uri = "/board/info", .method = HTTP_GET, .handler = board_info_handler, .user_ctx = NULL };
    httpd_register_uri_handler(server, &info_uri);

    httpd_uri_t msgs_uri = { .uri = "/board/messages", .method = HTTP_GET, .handler = board_messages_handler, .user_ctx = NULL };
    httpd_register_uri_handler(server, &msgs_uri);

    httpd_uri_t post_uri = { .uri = "/board/post", .method = HTTP_POST, .handler = board_post_handler, .user_ctx = NULL };
    httpd_register_uri_handler(server, &post_uri);

    httpd_uri_t admin_auth_uri = { .uri = "/board/admin/auth", .method = HTTP_POST, .handler = admin_auth_handler, .user_ctx = NULL };
    httpd_register_uri_handler(server, &admin_auth_uri);

    httpd_uri_t admin_auth_status_uri = { .uri = "/board/admin/auth/status", .method = HTTP_GET, .handler = admin_auth_status_handler, .user_ctx = NULL };
    httpd_register_uri_handler(server, &admin_auth_status_uri);

    httpd_uri_t admin_id_get_uri = { .uri = "/board/admin/identity/get", .method = HTTP_GET, .handler = admin_identity_get_handler, .user_ctx = NULL };
    httpd_register_uri_handler(server, &admin_id_get_uri);

    httpd_uri_t admin_id_set_uri = { .uri = "/board/admin/identity/set", .method = HTTP_GET, .handler = admin_identity_set_handler, .user_ctx = NULL };
    httpd_register_uri_handler(server, &admin_id_set_uri);

    httpd_uri_t admin_setkey_uri = { .uri = "/board/admin/setkey", .method = HTTP_GET, .handler = admin_setkey_handler, .user_ctx = NULL };
    httpd_register_uri_handler(server, &admin_setkey_uri);

    httpd_uri_t admin_clear_uri = { .uri = "/board/admin/clear", .method = HTTP_GET, .handler = admin_clear_handler, .user_ctx = NULL };
    httpd_register_uri_handler(server, &admin_clear_uri);

    httpd_uri_t admin_del_uri = { .uri = "/board/admin/delete/post", .method = HTTP_GET, .handler = admin_delete_post_handler, .user_ctx = NULL };
    httpd_register_uri_handler(server, &admin_del_uri);
}
