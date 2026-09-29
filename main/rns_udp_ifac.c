#include "rns_udp_ifac.h"
#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/igmp.h"
#include "reticulum.h"
#include "lxmf.h"
#include "rns_link.h"

static const char *TAG = "RNS_UDP";
#define RNS_UDP_PORT 4242
#define RNS_MULTICAST_IP "224.0.0.114"

static int g_udp_sock = -1;

// We need to forward packet processing to rns_link and lxmf
extern bool rns_link_process_packet(const rns_packet_t* pkt);
extern bool lxmf_handle_packet(const rns_packet_t* pkt);

static void udp_receive_task(void *pvParameters) {
    uint8_t rx_buffer[2048]; // Generous buffer for UDP

    while (1) {
        if (g_udp_sock < 0) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        struct sockaddr_storage source_addr;
        socklen_t socklen = sizeof(source_addr);
        int len = recvfrom(g_udp_sock, rx_buffer, sizeof(rx_buffer), 0, (struct sockaddr *)&source_addr, &socklen);

        if (len < 0) {
            ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        // We received a packet, attempt to decode it
        rns_packet_t rns_pkt = {0};
        if (rns_decode_packet(rx_buffer, len, &rns_pkt)) {
            // Tag interface
            rns_pkt.recv_interface = IF_WIFI;

            if (rns_pkt.type == RNS_PKT_TYPE_ANNOUNCE) {
                rns_verify_announce_packet(&rns_pkt);
            } else {
                if (!rns_link_process_packet(&rns_pkt)) {
                    lxmf_handle_packet(&rns_pkt);
                }
            }
        }
    }
}

void rns_udp_ifac_init(void) {
    ESP_LOGI(TAG, "Initializing RNS UDP Interface");

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(RNS_UDP_PORT);

    g_udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (g_udp_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        return;
    }

    int opt = 1;
    setsockopt(g_udp_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    int err = bind(g_udp_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        close(g_udp_sock);
        g_udp_sock = -1;
        return;
    }

    // Join multicast group
    struct ip_mreq imreq = {0};
    imreq.imr_multiaddr.s_addr = inet_addr(RNS_MULTICAST_IP);
    imreq.imr_interface.s_addr = INADDR_ANY;

    err = setsockopt(g_udp_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &imreq, sizeof(struct ip_mreq));
    if (err < 0) {
        ESP_LOGE(TAG, "Failed to set IP_ADD_MEMBERSHIP: errno %d", errno);
    } else {
        ESP_LOGI(TAG, "Successfully joined multicast group %s", RNS_MULTICAST_IP);
    }

    // Create RX task
    xTaskCreate(udp_receive_task, "udp_receive_task", 4096, NULL, 5, NULL);
}

void rns_udp_transmit_raw(const uint8_t *data, size_t len) {
    if (g_udp_sock < 0 || !data || len == 0) return;

    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(RNS_UDP_PORT);
    dest_addr.sin_addr.s_addr = inet_addr(RNS_MULTICAST_IP); // Send to multicast group

    int err = sendto(g_udp_sock, data, len, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
    }
}
