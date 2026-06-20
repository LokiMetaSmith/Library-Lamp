import re

with open('main/web_assets/admin.html', 'r', encoding='utf-8') as f:
    admin_content = f.read()

# Make admin.html actually save the key to localStorage!
admin_content = admin_content.replace(
"""  .then(token => {
    SESSION_TOKEN = token;""",
"""  .then(token => {
    SESSION_TOKEN = token;
    localStorage.setItem('adminKey', val);"""
)

with open('main/web_assets/admin.html', 'w', encoding='utf-8') as f:
    f.write(admin_content)
