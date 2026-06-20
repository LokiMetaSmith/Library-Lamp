import re

with open('main/main.c', 'r') as f:
    content = f.read()

# Instead of NVS_READONLY, we should use NVS_READWRITE per AGENTS.md, but load_wifi_credentials uses NVS_READONLY and works...
# Let's change load_wifi_credentials to use NVS_READWRITE and also read NVS_KEY_PUBLIC_UPLOADS.
new_load = """
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGI(TAG, "NVS namespace '%s' not found. First boot?", NVS_NAMESPACE);
        } else {
            ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
        }
        return err;
    }

    uint8_t u_val = 0;
    if (nvs_get_u8(my_handle, NVS_KEY_PUBLIC_UPLOADS, &u_val) == ESP_OK) {
        allow_public_uploads = (u_val == 1);
    }
"""

content = re.sub(
    r'err = nvs_open\(NVS_NAMESPACE, NVS_READONLY, &my_handle\);\n    if \(err != ESP_OK\) \{\n        if \(err == ESP_ERR_NVS_NOT_FOUND\) \{\n            ESP_LOGI\(TAG, "NVS namespace \'%s\' not found. First boot\?", NVS_NAMESPACE\);\n        \} else \{\n            ESP_LOGE\(TAG, "Error \(%s\) opening NVS handle!", esp_err_to_name\(err\)\);\n        \}\n        return err;\n    \}',
    new_load,
    content,
    flags=re.DOTALL
)

with open('main/main.c', 'w') as f:
    f.write(content)
