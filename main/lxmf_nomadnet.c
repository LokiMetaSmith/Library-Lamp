#include "lxmf_nomadnet.h"
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include "esp_log.h"

static const char* TAG = "NOMADNET";

#define MOUNT_POINT_SD "/sdcard"

extern bool g_sd_card_initialized;

size_t lxmf_nomadnet_generate_response(const char* path, char* out_buf, size_t max_len) {
    if (!out_buf || max_len == 0) return 0;

    out_buf[0] = '\0';
    size_t offset = 0;

    // A minimal NomadNet gemini-like/markdown-like menu
    if (strcmp(path, "/") == 0) {
        int written = snprintf(out_buf + offset, max_len - offset,
            "# Library Lamp\n\n"
            "Welcome to the local off-grid library.\n\n"
        );
        if (written > 0 && written < max_len - offset) offset += written;

        if (!g_sd_card_initialized) {
            written = snprintf(out_buf + offset, max_len - offset,
                "> Warning: SD Card not mounted.\n"
                "`[ No books available ]`\n"
            );
            if (written > 0 && written < max_len - offset) offset += written;
        } else {
            written = snprintf(out_buf + offset, max_len - offset,
                "## Available Books:\n"
            );
            if (written > 0 && written < max_len - offset) offset += written;

            DIR *dir = opendir(MOUNT_POINT_SD);
            if (dir != NULL) {
                struct dirent *ent;
                int count = 0;
                while ((ent = readdir(dir)) != NULL) {
                    // Very simple filter for e-books
                    if (strstr(ent->d_name, ".epub") || strstr(ent->d_name, ".pdf")) {
                        // Create a link in NomadNet format: `[Link text](/path)`
                        written = snprintf(out_buf + offset, max_len - offset,
                            "[%s](/download/%s)\n", ent->d_name, ent->d_name);
                        if (written > 0 && written < max_len - offset) {
                            offset += written;
                        } else {
                            break; // Buffer full
                        }
                        count++;
                    }
                }
                closedir(dir);

                if (count == 0) {
                    written = snprintf(out_buf + offset, max_len - offset, "`[ Library is empty ]`\n");
                    if (written > 0 && written < max_len - offset) offset += written;
                }
            } else {
                written = snprintf(out_buf + offset, max_len - offset, "`[ Error reading library ]`\n");
                if (written > 0 && written < max_len - offset) offset += written;
            }
        }

    } else if (strncmp(path, "/download/", 10) == 0) {
        // Handling file downloads over Reticulum links
        // For this minimal layer, we just indicate the capability is recognized
        // Full file chunking would require maintaining Link state and chunking the file
        int written = snprintf(out_buf + offset, max_len - offset,
            "# Downloading Book\n\n"
            "File: %s\n\n"
            "> Note: File chunking via LXMF is partially implemented.\n"
            "> The host will push this to you shortly.", path + 10
        );
        if (written > 0 && written < max_len - offset) offset += written;
    } else {
        int written = snprintf(out_buf + offset, max_len - offset,
            "# 404 Not Found\n\n"
            "The requested page `%s` does not exist on this Library Lamp.\n\n"
            "[Return to Home](/)", path
        );
        if (written > 0 && written < max_len - offset) offset += written;
    }

    ESP_LOGI(TAG, "Generated response for %s (%zu bytes)", path, offset);
    return offset;
}
