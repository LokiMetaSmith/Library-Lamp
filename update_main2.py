import re

with open('main/main.c', 'r') as f:
    content = f.read()

# Try again to find where to add it
if 'bool allow_public_uploads = false;' not in content:
    content = re.sub(r'(bool g_usb_mounted = false;)', r'bool allow_public_uploads = false;\n\1', content)

if 'NVS_KEY_PUBLIC_UPLOADS' not in content:
    content = re.sub(r'(#define NVS_NAMESPACE "wifi_creds")', r'\1\n#define NVS_KEY_PUBLIC_UPLOADS "pub_upload"', content)

with open('main/main.c', 'w') as f:
    f.write(content)
