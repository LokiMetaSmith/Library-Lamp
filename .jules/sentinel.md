## 2025-02-14 - cJSON Null Pointer Dereference in Handlers
**Vulnerability:** Null Pointer Dereference (DoS) and Path Traversal
**Learning:** ESP-IDF HTTP handlers using `cJSON` must explicitly check that `cJSON_GetObjectItem(json, "key")` does not return NULL before attempting to access its properties like `->valuestring`. Failing to do so causes a hard crash when malformed payloads are received. Additionally, VFS operations in ESP-IDF require manual path traversal checks (e.g. `strstr(filename, "..")`).
**Prevention:** Always check both the item and its specific value field for NULL, and manually validate inputs used in file system paths.

## 2026-06-19 - Fix HTTP Server Stack Overflow DoS

**Learning:** When using FreeRTOS, tasks are given a fixed stack size. Functions handling HTTP requests (such as `board_post_handler`) that declare large stack buffers (like `char buf[1024];`) can quickly exhaust the task stack, leading to a `LoadProhibited` Guru Meditation Error (Crash). This acts as a Denial of Service (DoS) vulnerability.

**Action:** Increased the HTTP server task stack size (`config.stack_size = 8192;`) to provide a safer baseline for endpoints performing heavily recursive or stack-intensive operations like `cJSON` parsing and File I/O. Furthermore, migrated large string buffers in `bulletin_api.c` (e.g., `1024`, `512`, `256` bytes) and `bulletin_board.c` from the stack to the heap using `malloc()` and carefully implemented `free()` calls across all exit and error paths to prevent memory leaks.
## 2026-06-20 - [Path Traversal in static file handler]
**Vulnerability:** Unauthenticated path traversal in static_file_handler allowing arbitrary file read.
**Learning:** The ESP-IDF VFS does not automatically sanitize directory traversal sequences like '..', requiring manual explicit checks.
**Prevention:** Always sanitize or reject URIs containing '..' before appending them to base paths in custom VFS HTTP handlers.

## 2025-02-14 - Prevent query string truncation DoS
**Vulnerability:** Fixed-size buffers for query string extraction (Truncation Risk / DoS)
**Learning:** ESP-IDF's `httpd_req_get_url_query_str` relies on the buffer size passed as an argument. If a fixed-size buffer is used (e.g., `char buf[512]`) and the client sends a query string longer than this buffer, the function returns `ESP_ERR_HTTPD_RESULT_TRUNC`. If this error isn't explicitly handled, or if the buffer truncates maliciously constructed input, it can bypass security checks, truncate data unexpectedly, or silently fail.
**Prevention:** Always dynamically size the buffer for `httpd_req_get_url_query_str` using `size_t query_len = httpd_req_get_url_query_len(req);` and allocate memory as `malloc(query_len + 1)`. Ensure `free()` is called on all code paths.
## 2024-05-18 - [Buffer Over-read]
**Vulnerability:** Found a buffer over-read (CWE-125) in BLE GATT write handler where `strncpy` was used on `param->write.value`, which is not guaranteed to be null-terminated.
**Learning:** `strncpy(dest, src, max_len)` reads from `src` until a null-terminator or `max_len`. If `src` comes from an untrusted network packet (like BLE) without null-termination, it will read past the end of the packet until it finds a null byte, leading to memory disclosure.
**Prevention:** When dealing with length-specified external buffers (like `param->write.len`), use `memcpy` constrained by both the destination buffer size and the incoming length, and then manually null-terminate.
