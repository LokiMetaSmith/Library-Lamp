## 2025-02-14 - cJSON Null Pointer Dereference in Handlers
**Vulnerability:** Null Pointer Dereference (DoS) and Path Traversal
**Learning:** ESP-IDF HTTP handlers using `cJSON` must explicitly check that `cJSON_GetObjectItem(json, "key")` does not return NULL before attempting to access its properties like `->valuestring`. Failing to do so causes a hard crash when malformed payloads are received. Additionally, VFS operations in ESP-IDF require manual path traversal checks (e.g. `strstr(filename, "..")`).
**Prevention:** Always check both the item and its specific value field for NULL, and manually validate inputs used in file system paths.

## 2026-06-19 - Fix HTTP Server Stack Overflow DoS

**Learning:** When using FreeRTOS, tasks are given a fixed stack size. Functions handling HTTP requests (such as `board_post_handler`) that declare large stack buffers (like `char buf[1024];`) can quickly exhaust the task stack, leading to a `LoadProhibited` Guru Meditation Error (Crash). This acts as a Denial of Service (DoS) vulnerability.

**Action:** Increased the HTTP server task stack size (`config.stack_size = 8192;`) to provide a safer baseline for endpoints performing heavily recursive or stack-intensive operations like `cJSON` parsing and File I/O. Furthermore, migrated large string buffers in `bulletin_api.c` (e.g., `1024`, `512`, `256` bytes) and `bulletin_board.c` from the stack to the heap using `malloc()` and carefully implemented `free()` calls across all exit and error paths to prevent memory leaks.
