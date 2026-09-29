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
        // Trigger the Link streaming process for the requested file
        const char* filename = path + 10;

        // Prevent path traversal
        if (strstr(filename, "..") || strchr(filename, '/')) {
            int written = snprintf(out_buf + offset, max_len - offset,
                "# 403 Forbidden\n\n"
                "Invalid file path.\n\n"
            );
            if (written > 0 && written < max_len - offset) offset += written;
            return offset;
        }

        static char download_path[256];
        snprintf(download_path, sizeof(download_path), "%s/%s", MOUNT_POINT_SD, filename);

        int written = snprintf(out_buf + offset, max_len - offset,
            "# Downloading Book\n\n"
            "File: %s\n\n"
            "> Streaming initiated. Check your link downloads.", filename
        );
        if (written > 0 && written < max_len - offset) offset += written;

        // Let the RNS Link state machine know it should start chunking this file
        extern void rns_link_start_file_stream_global(const char* filepath);
        rns_link_start_file_stream_global(download_path);

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
