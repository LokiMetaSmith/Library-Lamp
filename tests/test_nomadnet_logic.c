#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>

#define TAG "NOMADNET_TEST"
#define ESP_LOGI(tag, fmt, ...) printf("INFO [%s] " fmt "\n", tag, ##__VA_ARGS__)

typedef enum {
    IF_NONE = 0,
    IF_LORA = 1,
    IF_WIFI = 2,
    IF_ANY = 3
} rns_interface_t;

bool g_sd_card_initialized = true;
char g_last_streamed_file[256] = "";
rns_interface_t g_last_streamed_ifac = IF_NONE;

void rns_link_start_file_stream_global(const char* filepath, rns_interface_t ifac) {
    strncpy(g_last_streamed_file, filepath, sizeof(g_last_streamed_file) - 1);
    g_last_streamed_file[sizeof(g_last_streamed_file) - 1] = '\0';
    g_last_streamed_ifac = ifac;
}

#define MOUNT_POINT_SD "/sdcard"

size_t lxmf_nomadnet_generate_response(const char* path, char* out_buf, size_t max_len, rns_interface_t ifac) {
    if (!out_buf || max_len == 0) return 0;

    out_buf[0] = '\0';
    size_t offset = 0;

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
            written = snprintf(out_buf + offset, max_len - offset, "`[ Library is empty ]`\n");
            if (written > 0 && written < max_len - offset) offset += written;
        }

    } else if (strncmp(path, "/download/", 10) == 0) {
        const char* filename = path + 10;

        // Prevent path traversal and unauthorized access to sensitive files
        if (strstr(filename, "..") || strchr(filename, '/') || strchr(filename, '\\') ||
            strcasestr(filename, "adminkey.json") || strcasestr(filename, ".key")) {
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

        rns_link_start_file_stream_global(download_path, ifac);

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

void test_home_page() {
    char buf[512];
    g_sd_card_initialized = true;
    size_t len = lxmf_nomadnet_generate_response("/", buf, sizeof(buf), IF_LORA);
    assert(len > 0);
    assert(strstr(buf, "# Library Lamp") != NULL);
    assert(strstr(buf, "## Available Books:") != NULL);

    g_sd_card_initialized = false;
    len = lxmf_nomadnet_generate_response("/", buf, sizeof(buf), IF_LORA);
    assert(len > 0);
    assert(strstr(buf, "Warning: SD Card not mounted") != NULL);
    printf("test_home_page passed.\n");
}

void test_download_valid() {
    char buf[512];
    g_last_streamed_file[0] = '\0';
    size_t len = lxmf_nomadnet_generate_response("/download/sample.epub", buf, sizeof(buf), IF_WIFI);
    assert(len > 0);
    assert(strstr(buf, "# Downloading Book") != NULL);
    assert(strstr(buf, "sample.epub") != NULL);
    assert(strcmp(g_last_streamed_file, "/sdcard/sample.epub") == 0);
    assert(g_last_streamed_ifac == IF_WIFI);
    printf("test_download_valid passed.\n");
}

void test_download_security_hardening() {
    char buf[512];

    // Path traversal with ..
    g_last_streamed_file[0] = '\0';
    size_t len = lxmf_nomadnet_generate_response("/download/../../etc/passwd", buf, sizeof(buf), IF_LORA);
    assert(len > 0);
    assert(strstr(buf, "# 403 Forbidden") != NULL);
    assert(strlen(g_last_streamed_file) == 0);

    // Path traversal with slash /
    g_last_streamed_file[0] = '\0';
    len = lxmf_nomadnet_generate_response("/download/subdir/book.epub", buf, sizeof(buf), IF_LORA);
    assert(len > 0);
    assert(strstr(buf, "# 403 Forbidden") != NULL);
    assert(strlen(g_last_streamed_file) == 0);

    // Path traversal with backslash
    g_last_streamed_file[0] = '\0';
    len = lxmf_nomadnet_generate_response("/download/..\\secret.txt", buf, sizeof(buf), IF_LORA);
    assert(len > 0);
    assert(strstr(buf, "# 403 Forbidden") != NULL);
    assert(strlen(g_last_streamed_file) == 0);

    // Sensitive files: adminkey.json
    g_last_streamed_file[0] = '\0';
    len = lxmf_nomadnet_generate_response("/download/AdminKey.json", buf, sizeof(buf), IF_LORA);
    assert(len > 0);
    assert(strstr(buf, "# 403 Forbidden") != NULL);
    assert(strlen(g_last_streamed_file) == 0);

    // Sensitive files: .key
    g_last_streamed_file[0] = '\0';
    len = lxmf_nomadnet_generate_response("/download/.library_admin.key", buf, sizeof(buf), IF_LORA);
    assert(len > 0);
    assert(strstr(buf, "# 403 Forbidden") != NULL);
    assert(strlen(g_last_streamed_file) == 0);

    printf("test_download_security_hardening passed.\n");
}

void test_404_routing() {
    char buf[512];
    size_t len = lxmf_nomadnet_generate_response("/nonexistent_page", buf, sizeof(buf), IF_LORA);
    assert(len > 0);
    assert(strstr(buf, "# 404 Not Found") != NULL);
    assert(strstr(buf, "[Return to Home](/)" ) != NULL);
    printf("test_404_routing passed.\n");
}

int main() {
    test_home_page();
    test_download_valid();
    test_download_security_hardening();
    test_404_routing();
    printf("All NomadNet unit tests passed!\n");
    return 0;
}
