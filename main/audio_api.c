#include "audio_api.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <cJSON.h>
#include <string.h>
#include "bulletin_api.h"

static const char *TAG = "AUDIO_API";

#define MAX_QUEUE_SIZE 50
#define MAX_FILENAME_LEN 128

typedef struct {
    char filename[MAX_FILENAME_LEN];
} audio_track_t;

static audio_track_t audio_queue[MAX_QUEUE_SIZE];
static int queue_count = 0;

static char current_track[MAX_FILENAME_LEN] = "";
static bool is_playing = false;
static int64_t track_start_time_us = 0; // The timestamp when play started or resumed
static float track_position_sec = 0.0;   // The position when paused

static void send_json_response(httpd_req_t *req, cJSON *root) {
    char *json_str = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));
    free(json_str);
    cJSON_Delete(root);
}

static esp_err_t audio_state_get_handler(httpd_req_t *req) {
    cJSON *root = cJSON_CreateObject();

    // Add current track info
    cJSON *current = cJSON_CreateObject();
    if (strlen(current_track) > 0) {
        cJSON_AddStringToObject(current, "filename", current_track);
        cJSON_AddBoolToObject(current, "playing", is_playing);

        float current_pos = track_position_sec;
        if (is_playing) {
            int64_t now = esp_timer_get_time();
            current_pos += (float)(now - track_start_time_us) / 1000000.0f;
        }
        cJSON_AddNumberToObject(current, "position", current_pos);
    } else {
        cJSON_AddNullToObject(current, "filename");
        cJSON_AddBoolToObject(current, "playing", false);
        cJSON_AddNumberToObject(current, "position", 0.0);
    }
    cJSON_AddItemToObject(root, "current", current);

    // Add queue info
    cJSON *queue = cJSON_CreateArray();
    for (int i = 0; i < queue_count; i++) {
        cJSON_AddItemToArray(queue, cJSON_CreateString(audio_queue[i].filename));
    }
    cJSON_AddItemToObject(root, "queue", queue);

    send_json_response(req, root);
    return ESP_OK;
}

static esp_err_t audio_queue_post_handler(httpd_req_t *req) {
    char buf[256];
    int ret, remaining = req->content_len;

    if (remaining >= sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Body too large");
        return ESP_FAIL;
    }

    ret = httpd_req_recv(req, buf, remaining);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    cJSON *json = cJSON_Parse(buf);
    if (!json) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    cJSON *filename_item = cJSON_GetObjectItem(json, "filename");
    if (!filename_item || !cJSON_IsString(filename_item)) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing filename");
        return ESP_FAIL;
    }

    const char* filename = filename_item->valuestring;

    if (queue_count < MAX_QUEUE_SIZE) {
        strncpy(audio_queue[queue_count].filename, filename, MAX_FILENAME_LEN - 1);
        audio_queue[queue_count].filename[MAX_FILENAME_LEN - 1] = '\0';
        queue_count++;

        // If nothing is playing, automatically start the added track
        if (strlen(current_track) == 0) {
            strncpy(current_track, audio_queue[0].filename, MAX_FILENAME_LEN - 1);
            current_track[MAX_FILENAME_LEN - 1] = '\0';

            // Remove from queue
            for (int i = 0; i < queue_count - 1; i++) {
                strncpy(audio_queue[i].filename, audio_queue[i+1].filename, MAX_FILENAME_LEN);
            }
            queue_count--;

            is_playing = true;
            track_position_sec = 0.0;
            track_start_time_us = esp_timer_get_time();
        }
    } else {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Queue is full");
        return ESP_FAIL;
    }

    cJSON_Delete(json);
    httpd_resp_send(req, "{\"success\":true}", 16);
    return ESP_OK;
}

static esp_err_t audio_queue_delete_handler(httpd_req_t *req) {
    if (!bb_is_admin_request(req)) {
        httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Forbidden");
        return ESP_FAIL;
    }

    char buf[256];
    int ret, remaining = req->content_len;

    if (remaining >= sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Body too large");
        return ESP_FAIL;
    }

    ret = httpd_req_recv(req, buf, remaining);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    cJSON *json = cJSON_Parse(buf);
    if (!json) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    cJSON *index_item = cJSON_GetObjectItem(json, "index");
    if (!index_item || !cJSON_IsNumber(index_item)) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing index");
        return ESP_FAIL;
    }

    int index = index_item->valueint;
    if (index >= 0 && index < queue_count) {
        for (int i = index; i < queue_count - 1; i++) {
            strncpy(audio_queue[i].filename, audio_queue[i+1].filename, MAX_FILENAME_LEN);
        }
        queue_count--;
    }

    cJSON_Delete(json);
    httpd_resp_send(req, "{\"success\":true}", 16);
    return ESP_OK;
}

static esp_err_t audio_state_post_handler(httpd_req_t *req) {
    char buf[512];
    int ret, remaining = req->content_len;

    if (remaining >= sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Body too large");
        return ESP_FAIL;
    }

    ret = httpd_req_recv(req, buf, remaining);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    cJSON *json = cJSON_Parse(buf);
    if (!json) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    cJSON *action_item = cJSON_GetObjectItem(json, "action");
    if (!action_item || !cJSON_IsString(action_item)) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing action");
        return ESP_FAIL;
    }

    const char *action = action_item->valuestring;

    if (strcmp(action, "play") == 0) {
        if (!is_playing && strlen(current_track) > 0) {
            is_playing = true;
            track_start_time_us = esp_timer_get_time();
        }
    } else if (strcmp(action, "pause") == 0) {
        if (is_playing) {
            is_playing = false;
            int64_t now = esp_timer_get_time();
            track_position_sec += (float)(now - track_start_time_us) / 1000000.0f;
        }
    } else if (strcmp(action, "seek") == 0) {
        cJSON *pos_item = cJSON_GetObjectItem(json, "position");
        if (pos_item && cJSON_IsNumber(pos_item)) {
            track_position_sec = pos_item->valuedouble;
            if (is_playing) {
                track_start_time_us = esp_timer_get_time();
            }
        }
    } else if (strcmp(action, "next") == 0) {
        if (queue_count > 0) {
            strncpy(current_track, audio_queue[0].filename, MAX_FILENAME_LEN - 1);
            current_track[MAX_FILENAME_LEN - 1] = '\0';

            for (int i = 0; i < queue_count - 1; i++) {
                strncpy(audio_queue[i].filename, audio_queue[i+1].filename, MAX_FILENAME_LEN);
            }
            queue_count--;

            is_playing = true;
            track_position_sec = 0.0;
            track_start_time_us = esp_timer_get_time();
        } else {
            current_track[0] = '\0';
            is_playing = false;
            track_position_sec = 0.0;
        }
    }

    cJSON_Delete(json);
    httpd_resp_send(req, "{\"success\":true}", 16);
    return ESP_OK;
}

void register_audio_api_handlers(httpd_handle_t server) {
    httpd_uri_t get_state = { "/audio/state", HTTP_GET, audio_state_get_handler, NULL };
    httpd_register_uri_handler(server, &get_state);

    httpd_uri_t post_queue = { "/audio/queue", HTTP_POST, audio_queue_post_handler, NULL };
    httpd_register_uri_handler(server, &post_queue);

    httpd_uri_t delete_queue = { "/audio/queue", HTTP_DELETE, audio_queue_delete_handler, NULL };
    httpd_register_uri_handler(server, &delete_queue);

    httpd_uri_t post_state = { "/audio/state", HTTP_POST, audio_state_post_handler, NULL };
    httpd_register_uri_handler(server, &post_state);
}
