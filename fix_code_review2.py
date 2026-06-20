import re

with open('main/main.c', 'r', encoding='utf-8') as f:
    content = f.read()

# Make sure we add isAdmin to /status API
status_admin = """
    cJSON_AddBoolToObject(root, "lora_initialized", lora_initialized);
    cJSON_AddBoolToObject(root, "allow_public_uploads", allow_public_uploads);
    cJSON_AddBoolToObject(root, "is_admin", bb_is_admin_request(req));
"""
content = re.sub(r'    cJSON_AddBoolToObject\(root, "lora_initialized", lora_initialized\);\n    cJSON_AddBoolToObject\(root, "allow_public_uploads", allow_public_uploads\);', status_admin, content)

with open('main/main.c', 'w', encoding='utf-8') as f:
    f.write(content)

with open('main/web_assets/index.html', 'r', encoding='utf-8') as f:
    index_content = f.read()

# Change v-if to allow public uploads OR is_admin
index_content = index_content.replace('v-if="allowPublicUploads"', 'v-if="allowPublicUploads || isAdmin"')

with open('main/web_assets/index.html', 'w', encoding='utf-8') as f:
    f.write(index_content)

with open('main/web_assets/script.js', 'r', encoding='utf-8') as f:
    js_content = f.read()

if 'isAdmin: false,' not in js_content:
    js_content = js_content.replace('allowPublicUploads: false,', 'allowPublicUploads: false,\n            isAdmin: false,')

if 'this.isAdmin = data.is_admin;' not in js_content:
    js_content = js_content.replace('if (data.allow_public_uploads !== undefined) this.allowPublicUploads = data.allow_public_uploads;', 'if (data.allow_public_uploads !== undefined) this.allowPublicUploads = data.allow_public_uploads;\n                if (data.is_admin !== undefined) this.isAdmin = data.is_admin;')

with open('main/web_assets/script.js', 'w', encoding='utf-8') as f:
    f.write(js_content)
