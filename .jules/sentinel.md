## 2026-06-28 - Prevent DoS from unverified cJSON object items
**Vulnerability:** HTTP endpoints parse JSON requests using `cJSON_Parse` and directly access properties like `->valueint` on pointers returned by `cJSON_GetObjectItem` without checking their type (e.g., using `cJSON_IsNumber`).
**Learning:** In ESP-IDF, if `cJSON_GetObjectItem` returns a cJSON object of a different type, accessing fields like `->valuestring` or `->valueint` can lead to invalid memory access or Denial of Service (DoS) crashes if an attacker sends an unexpected JSON structure.
**Prevention:** Always verify the type of the returned item using functions like `cJSON_IsString()`, `cJSON_IsNumber()`, or `cJSON_IsBool()` before accessing its internal data.
