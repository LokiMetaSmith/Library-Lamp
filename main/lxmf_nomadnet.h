#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Handler to generate a NomadNet response based on the requested page path.
// The caller must provide a buffer and its maximum size.
// Returns the actual length of the response written to out_buf, or 0 on error.
size_t lxmf_nomadnet_generate_response(const char* path, char* out_buf, size_t max_len);

#ifdef __cplusplus
}
#endif
