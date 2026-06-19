## 2025-02-14 - cJSON Null Pointer Dereference in Handlers
**Vulnerability:** Null Pointer Dereference (DoS) and Path Traversal
**Learning:** ESP-IDF HTTP handlers using `cJSON` must explicitly check that `cJSON_GetObjectItem(json, "key")` does not return NULL before attempting to access its properties like `->valuestring`. Failing to do so causes a hard crash when malformed payloads are received. Additionally, VFS operations in ESP-IDF require manual path traversal checks (e.g. `strstr(filename, "..")`).
**Prevention:** Always check both the item and its specific value field for NULL, and manually validate inputs used in file system paths.
