import re

with open('main/main.c', 'r', encoding='utf-8', errors='ignore') as f:
    content = f.read()

# Fix duplicate static
content = content.replace("static \nstatic esp_err_t admin_set_public_uploads_handler(httpd_req_t *req) {", "static esp_err_t admin_set_public_uploads_handler(httpd_req_t *req) {")

# Fix null character preservation in literal
content = content.replace("content[ret] = '\\0';", "content[ret] = 0;")

with open('main/main.c', 'w', encoding='utf-8') as f:
    f.write(content)
