#ifndef BULLETIN_API_H
#define BULLETIN_API_H

#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

// URL Decode function from main.c
void urldecode(char *dst, const char *src);

// Check if request has an admin token or if the hardware key is present
bool bb_is_admin_request(httpd_req_t *req);

// Register the handlers with the given web server instance
void register_bulletin_api_handlers(httpd_handle_t server);

#ifdef __cplusplus
}
#endif

#endif // BULLETIN_API_H
