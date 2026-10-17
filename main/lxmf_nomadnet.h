#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "reticulum.h"

// Handler to generate a NomadNet response based on the requested page path.
// The caller must provide a buffer and its maximum size.
// The ifac specifies which interface the request originated from, ensuring streaming occurs on the correct link.
// Returns the actual length of the response written to out_buf, or 0 on error.
size_t lxmf_nomadnet_generate_response(const char* path, char* out_buf, size_t max_len, rns_interface_t ifac);

#ifdef __cplusplus
}
#endif
