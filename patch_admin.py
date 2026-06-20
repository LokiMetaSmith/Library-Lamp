import re

with open('main/main.c', 'r') as f:
    content = f.read()

admin_endpoint = """
static esp_err_t admin_set_public_uploads_handler(httpd_req_t *req) {
    if (!bb_is_admin_request(req)) {
        httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Forbidden");
        return ESP_FAIL;
    }

    char content[100];
    int ret = httpd_req_recv(req, content, sizeof(content) - 1);
    if (ret <= 0) {
        return ESP_FAIL;
    }
    content[ret] = '\0';

    cJSON *json = cJSON_Parse(content);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *enabled_item = cJSON_GetObjectItem(json, "enabled");
    if (!cJSON_IsBool(enabled_item)) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    allow_public_uploads = cJSON_IsTrue(enabled_item);
    cJSON_Delete(json);

    nvs_handle_t my_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err == ESP_OK) {
        nvs_set_u8(my_handle, NVS_KEY_PUBLIC_UPLOADS, allow_public_uploads ? 1 : 0);
        nvs_commit(my_handle);
        nvs_close(my_handle);
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\\"success\\": true}", 17);
    return ESP_OK;
}
"""

# Insert before start_webserver
content = re.sub(r'httpd_handle_t start_webserver\(void\)', admin_endpoint + r'\nhttpd_handle_t start_webserver(void)', content)

# Register endpoint
reg_endpoint = """
        httpd_uri_t public_uploads_uri = { "/admin/set_public_uploads", HTTP_POST, admin_set_public_uploads_handler, NULL };
        httpd_register_uri_handler(server, &public_uploads_uri);
"""
content = re.sub(r'(httpd_register_uri_handler\(server, &download_uri\);)', r'\1\n' + reg_endpoint, content)

with open('main/main.c', 'w') as f:
    f.write(content)
