## 2024-05-24 - File Deletion Endpoint

**Vulnerability:** A missing file deletion endpoint preventing secure administrative management of files via the frontend UI.
**Learning:** ESP-IDF VFS does not automatically protect against directory traversal (`..` or `/`) or sensitive file paths (`.key`, `adminkey.json`). These checks must be manually implemented in every file-handling endpoint before performing destructive actions like `remove()`. Destructive endpoints are safer implemented via POST and JSON bodies to avoid accidental triggering via query param logging.
**Prevention:** Always combine `bb_is_admin_request()` checks with stringent path traversal input sanitization and restrict destructive methods to `POST` in ESP-IDF API routes.

## 2024-06-25 - Prevent CSRF by using POST for destructive actions
**Vulnerability:** The `admin_delete_post_handler` and `admin_clear_handler` endpoints in `main/bulletin_api.c` previously used `HTTP_GET` for destructive actions (deleting and clearing data) and took parameters via the query string.
**Learning:** Using `GET` for state-modifying actions violates REST principles and introduces security risks, such as exposing sensitive parameters in logs and enabling Cross-Site Request Forgery (CSRF). CSRF allows attackers to execute unintended actions if an authenticated user visits a malicious link. Additionally, when using `req->content_len` use `size_t` rather than `int` to avoid integer overflow which could cause heap buffer overflows.
**Prevention:** Always use `HTTP_POST`, `HTTP_PUT`, or `HTTP_DELETE` for operations that alter application state. Pass sensitive parameters or data in the request body (e.g., as a JSON payload) rather than the query string. And ensure safe types when working with request bodies lengths.
## 2025-02-12 - Integer Overflow in HTTP Handlers
**Vulnerability:** `req->content_len` was being assigned to an `int` instead of a `size_t` in `audio_api.c`, `bulletin_api.c`, and `main.c`.
**Learning:** Using an `int` to store `req->content_len` can lead to integer overflow and heap buffer overflow vulnerabilities when allocating dynamic memory for payloads in ESP-IDF.
**Prevention:** Always store `req->content_len` in a `size_t` variable when handling ESP-IDF HTTP requests.
