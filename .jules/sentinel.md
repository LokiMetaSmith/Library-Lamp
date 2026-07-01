## 2026-06-28 - Prevent DoS from unverified cJSON object items
**Vulnerability:** HTTP endpoints parse JSON requests using `cJSON_Parse` and directly access properties like `->valueint` on pointers returned by `cJSON_GetObjectItem` without checking their type (e.g., using `cJSON_IsNumber`).
**Learning:** In ESP-IDF, if `cJSON_GetObjectItem` returns a cJSON object of a different type, accessing fields like `->valuestring` or `->valueint` can lead to invalid memory access or Denial of Service (DoS) crashes if an attacker sends an unexpected JSON structure.
**Prevention:** Always verify the type of the returned item using functions like `cJSON_IsString()`, `cJSON_IsNumber()`, or `cJSON_IsBool()` before accessing its internal data.

## 2024-05-18 - Fix Path Traversal and FAT32 Case-Insensitive Bypass
**Vulnerability:** HTTP handlers were vulnerable to FAT32 case-insensitive bypasses (e.g., `AdminKey.json` instead of `adminkey.json`) and lacked backslash `\` path traversal checks in addition to `..` and `/` protections.
**Learning:** The ESP32 VFS backed by FAT32 is case-insensitive. Simple `strstr()` checks for sensitive filenames or extensions are insufficient and act as a bypass. When checking for sensitive files, query parameters appended to URLs could also bypass exact matches of file extensions or names, meaning substring checks like `strcasestr` are safer than extracting and matching exact names if the URI contains a query string.
**Prevention:** Always use `strcasestr` for sensitive filenames and extensions to catch case-insensitive bypasses. Explicitly check for both `/` and `\` using `strchr` when validating path inputs unless the path explicitly requires subdirectories.
