with open('main/web_assets/script.js', 'r', encoding='utf-8') as f:
    js_content = f.read()

# Make sure we add ?token=... to the fetch url if we are an admin but public uploads are off,
# but index.html doesn't have the SESSION_TOKEN from admin.html.
# However, the code reviewer noted: "the `uploadBook` JavaScript function completely lacks the logic to retrieve the admin key from `localStorage` and pass it to the backend (`?key=...`)"
# Wait, admin.html doesn't use localStorage. It uses `let SESSION_TOKEN = '';` in memory.
# Oh, the admin key is usually passed via query param `?token=` (if using session token) or if the hardware key is mounted, bb_is_admin_request(req) just returns true without token!
# But maybe we CAN check localStorage if we just assume admin keys could be stored there in some versions? No, the code review said "retrieve the admin key from localStorage and pass it to the backend (?key=...)". So let's do exactly what it says.

js_replacement = """                const url = `/upload?title=${encodeURIComponent(title)}&author=${encodeURIComponent(author)}&filename=${encodeURIComponent(filename)}`;
"""
js_new = """                let url = `/upload?title=${encodeURIComponent(title)}&author=${encodeURIComponent(author)}&filename=${encodeURIComponent(filename)}`;
                const savedKey = localStorage.getItem('adminKey');
                if (savedKey) {
                    url += `&key=${encodeURIComponent(savedKey)}`;
                }
"""
js_content = js_content.replace(js_replacement, js_new)

with open('main/web_assets/script.js', 'w', encoding='utf-8') as f:
    f.write(js_content)
