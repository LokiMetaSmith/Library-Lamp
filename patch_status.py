import re

with open('main/main.c', 'r') as f:
    content = f.read()

# Add allow_public_uploads to status_handler
if 'cJSON_AddBoolToObject(root, "allow_public_uploads", allow_public_uploads);' not in content:
    content = re.sub(
        r'(cJSON_AddBoolToObject\(root, "lora_initialized", lora_initialized\);)',
        r'\1\n    cJSON_AddBoolToObject(root, "allow_public_uploads", allow_public_uploads);',
        content
    )

with open('main/main.c', 'w') as f:
    f.write(content)
