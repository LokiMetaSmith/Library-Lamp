import re

with open('main/main.c', 'r', encoding='utf-8') as f:
    content = f.read()

# 1. Remove the raw null byte
content = content.replace("content[ret] = 0;", "content[ret] = '\\0';")

# 2. Fix f_getfree directly by replacing it with esp_vfs_fat_info
storage_replacement = """
    if (g_sd_card_initialized) {
        uint64_t out_total_bytes, out_free_bytes;
        if (esp_vfs_fat_info(MOUNT_POINT_SD, &out_total_bytes, &out_free_bytes) == ESP_OK) {
            uint32_t total_mb = out_total_bytes / (1024 * 1024);
            uint32_t free_mb = out_free_bytes / (1024 * 1024);
            uint32_t used_mb = total_mb > free_mb ? total_mb - free_mb : 0;

            cJSON_AddNumberToObject(root, "sd_total_mb", total_mb);
            cJSON_AddNumberToObject(root, "sd_used_mb", used_mb);
        }
    }
"""
content = re.sub(r'    if \(g_sd_card_initialized\) \{\n        FATFS \*fs;.*?    \}', storage_replacement, content, flags=re.DOTALL)

# 3. Increase buffer size for upload query parameters from 256 to 512
content = content.replace("char buf[256];", "char buf[512];")

with open('main/main.c', 'w', encoding='utf-8') as f:
    f.write(content)

with open('main/web_assets/index.html', 'r', encoding='utf-8') as f:
    index_content = f.read()

# We don't have login in index.html, but the user is an admin if they have the token from admin.html?
# Wait, admin.html uses a JS variable `SESSION_TOKEN` for token, not localStorage.
# So there's no way for index.html to know if we are admin.
# BUT the prompt says the admin key is a hardware USB key!
# "The device uses a standard USB Mass Storage device containing a hidden key file (.library_admin.key) as a physical security fob to provide passwordless admin authentication and protect administrative API endpoints."
# AND "bb_is_admin_request(req)" checks both the query string ?token= AND if the hardware key is authenticated.
