## 2024-05-24 - File Deletion Endpoint

**Vulnerability:** A missing file deletion endpoint preventing secure administrative management of files via the frontend UI.
**Learning:** ESP-IDF VFS does not automatically protect against directory traversal (`..` or `/`) or sensitive file paths (`.key`, `adminkey.json`). These checks must be manually implemented in every file-handling endpoint before performing destructive actions like `remove()`. Destructive endpoints are safer implemented via POST and JSON bodies to avoid accidental triggering via query param logging.
**Prevention:** Always combine `bb_is_admin_request()` checks with stringent path traversal input sanitization and restrict destructive methods to `POST` in ESP-IDF API routes.
