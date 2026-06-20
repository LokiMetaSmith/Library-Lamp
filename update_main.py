import re

with open('main/main.c', 'r') as f:
    content = f.read()

# 1. Add global variable allow_public_uploads
if 'bool allow_public_uploads = false;' not in content:
    content = re.sub(r'(bool g_usb_mounted = false;)', r'bool allow_public_uploads = false;\n\1', content)

# 2. Add nvs read logic to load_wifi_credentials or similar. Let's just add it to app_main where nvs_flash is initialized.
if 'NVS_KEY_PUBLIC_UPLOADS' not in content:
    content = re.sub(r'#define NVS_KEY_PASSWORD "wifi_pass"', r'#define NVS_KEY_PASSWORD "wifi_pass"\n#define NVS_KEY_PUBLIC_UPLOADS "pub_upload"', content)

with open('main/main.c', 'w') as f:
    f.write(content)
