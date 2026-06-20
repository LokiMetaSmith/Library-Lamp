with open('main/main.c', 'rb') as f:
    content = f.read()

# Replace binary null byte with the ascii string '\0'
content = content.replace(b"content[ret] = '\x00';", b"content[ret] = '\\0';")

with open('main/main.c', 'wb') as f:
    f.write(content)
