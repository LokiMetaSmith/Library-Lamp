import re

with open('main/main.c', 'r', encoding='utf-8', errors='ignore') as f:
    content = f.read()

upload_code = """
static esp_err_t upload_handler(httpd_req_t *req) {
    if (!allow_public_uploads && !bb_is_admin_request(req)) {
        httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Public uploads are disabled.");
        return ESP_FAIL;
    }

    // Since multipart/form-data can be complex to parse perfectly without a library,
    // and ebook files can be large, we'll implement a basic parser that works for our specific JS client.
    // The client will send title, author, and file via FormData.

    // A more robust approach for large files is receiving the whole body, but ESP32 memory is limited.
    // We expect the client to send 'title', 'author', and 'file' using a simple custom JSON + Binary approach
    // or we parse the multipart headers manually.

    // Actually, to make things easy and robust, the frontend can send a JSON metadata packet first,
    // or send the file directly in a POST, with title/author in URL query parameters!
    // That avoids memory-intensive multipart parsing on the ESP32.

    char buf[256];
    char title[100] = "Unknown Title";
    char author[100] = "Unknown Author";
    char filename[100] = "upload.epub";

    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        char param[100];
        if (httpd_query_key_value(buf, "title", param, sizeof(param)) == ESP_OK) urldecode(title, param);
        if (httpd_query_key_value(buf, "author", param, sizeof(param)) == ESP_OK) urldecode(author, param);
        if (httpd_query_key_value(buf, "filename", param, sizeof(param)) == ESP_OK) urldecode(filename, param);
    }

    // Check for directory traversal
    if (strstr(filename, "..") || strchr(filename, '/')) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid filename.");
        return ESP_FAIL;
    }

    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/%s", MOUNT_POINT_SD, filename);

    FILE *f = fopen(filepath, "w");
    if (!f) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file on SD card.");
        return ESP_FAIL;
    }

    // Receive the file chunk by chunk
    char *recv_buf = malloc(4096);
    if (!recv_buf) {
        fclose(f);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed.");
        return ESP_FAIL;
    }

    int received = 0;
    int remaining = req->content_len;

    while (remaining > 0) {
        int ret = httpd_req_recv(req, recv_buf, MIN(remaining, 4096));
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            fclose(f);
            free(recv_buf);
            unlink(filepath); // delete incomplete file
            return ESP_FAIL;
        }
        fwrite(recv_buf, 1, ret, f);
        remaining -= ret;
    }

    fclose(f);
    free(recv_buf);

    // Update catalog.json
    char catalog_path[256];
    snprintf(catalog_path, sizeof(catalog_path), "%s/catalog.json", MOUNT_POINT_SD);
    FILE *cat_f = fopen(catalog_path, "r");
    cJSON *cat_json = NULL;

    if (cat_f) {
        fseek(cat_f, 0, SEEK_END);
        long fsize = ftell(cat_f);
        fseek(cat_f, 0, SEEK_SET);
        if (fsize > 0) {
            char *json_data = malloc(fsize + 1);
            if (json_data) {
                fread(json_data, 1, fsize, cat_f);
                json_data[fsize] = 0;
                cat_json = cJSON_Parse(json_data);
                free(json_data);
            }
        }
        fclose(cat_f);
    }

    if (!cat_json) cat_json = cJSON_CreateArray();

    cJSON *new_book = cJSON_CreateObject();
    cJSON_AddStringToObject(new_book, "name", filename);
    cJSON_AddStringToObject(new_book, "title", title);
    cJSON_AddStringToObject(new_book, "author", author);
    cJSON_AddItemToArray(cat_json, new_book);

    cat_f = fopen(catalog_path, "w");
    if (cat_f) {
        char *new_json_str = cJSON_PrintUnformatted(cat_json);
        if (new_json_str) {
            fwrite(new_json_str, 1, strlen(new_json_str), cat_f);
            free(new_json_str);
        }
        fclose(cat_f);
    }
    cJSON_Delete(cat_json);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\\"success\\":true}", 16);
    return ESP_OK;
}
"""

content = re.sub(r'(static esp_err_t download_handler\(httpd_req_t \*req\) \{)', upload_code + r'\n\1', content)

reg_upload = """
        httpd_uri_t upload_uri = { "/upload", HTTP_POST, upload_handler, NULL };
        httpd_register_uri_handler(server, &upload_uri);
"""
content = re.sub(r'(httpd_register_uri_handler\(server, &download_uri\);)', r'\1\n' + reg_upload, content)

with open('main/main.c', 'w', encoding='utf-8') as f:
    f.write(content)
