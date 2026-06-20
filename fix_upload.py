import re

with open('main/main.c', 'r', encoding='utf-8', errors='ignore') as f:
    content = f.read()

# Fix unlink
content = content.replace("unlink(filepath);", "remove(filepath);")
# Fix unused variable
content = content.replace("int received = 0;\n", "")

with open('main/main.c', 'w', encoding='utf-8') as f:
    f.write(content)
