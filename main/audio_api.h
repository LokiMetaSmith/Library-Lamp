#ifndef AUDIO_API_H
#define AUDIO_API_H

#include <esp_http_server.h>

void register_audio_api_handlers(httpd_handle_t server);

#endif // AUDIO_API_H
