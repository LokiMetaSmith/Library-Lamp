#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>

// --- Mocks and Stubs ---
#define MAX_MSGS 200

// Mock struct matching the original header
typedef struct {
    uint16_t id;
    char author[25];
    char type[16];
    char text[301];
    unsigned long expires;
} BulletinMessage;

int msgCount = 0;
uint16_t nextMsgId = 1;
bool msgsDirty = false;
BulletinMessage msgs[MAX_MSGS];

#define TAG "BULLETIN_TEST"
#define ESP_LOGI(tag, fmt, ...) printf("INFO [%s] " fmt "\n", tag, ##__VA_ARGS__)

unsigned long mocked_time = 1000;

unsigned long nowSecs(void) {
    return mocked_time;
}

void bb_save_messages(void) {
    // Stub
}

void lora_wan_broadcast(const char* msg) {
    // Stub for tracking? Not strictly necessary for this unit test if we verify state.
}

// Function under test (copied implementation or linked in a real scenario, here we copy to easily test isolated logic)
void bb_add_message_local(const char* author, const char* type, const char* text, int expiry_hours) {
    if (msgCount >= MAX_MSGS) {
        unsigned long now = nowSecs();
        int evict = -1;
        unsigned long oldest = 0xFFFFFFFF;
        for (int i = 0; i < msgCount; i++) {
            if (msgs[i].expires <= now && msgs[i].expires < oldest) {
                oldest = msgs[i].expires;
                evict = i;
            }
        }
        // Fallback eviction if none expired: evict oldest by time anyway
        if (evict < 0) {
            oldest = 0xFFFFFFFF;
            for (int i = 0; i < msgCount; i++) {
                if (msgs[i].expires < oldest) {
                    oldest = msgs[i].expires;
                    evict = i;
                }
            }
        }

        if (evict < 0) evict = 0; // Absolute fallback

        memmove(&msgs[evict], &msgs[evict + 1], (msgCount - 1 - evict) * sizeof(BulletinMessage));
        msgCount--;
    }

    msgs[msgCount].id = nextMsgId++;
    strncpy(msgs[msgCount].author, author, sizeof(msgs[0].author)-1);
    strncpy(msgs[msgCount].type, type, sizeof(msgs[0].type)-1);
    strncpy(msgs[msgCount].text, text, sizeof(msgs[0].text)-1);
    msgs[msgCount].expires = nowSecs() + (expiry_hours * 3600);
    msgCount++;
    msgsDirty = true;
    bb_save_messages();
}

void bb_parse_lora_msg(const char* payload) {
    if (!payload || strncmp(payload, "MSG|", 4) != 0) return;

    char buffer[512];
    strncpy(buffer, payload, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    char *saveptr;
    char* token = strtok_r(buffer, "|", &saveptr);
    if (!token) return; // MSG

    char* author = strtok_r(NULL, "|", &saveptr);
    char* type = strtok_r(NULL, "|", &saveptr);
    char* text = strtok_r(NULL, "", &saveptr); // Rest of string is the text

    if (author && type && text) {
        ESP_LOGI(TAG, "Parsed LoRa MSG: author='%s', type='%s', text='%s'", author, type, text);
        // Add locally, don't broadcast to prevent loop
        bb_add_message_local(author, type, text, 24);
    }
}


// --- Tests ---

void test_parse_lora_msg() {
    msgCount = 0; // reset
    const char* valid_payload = "MSG|TestAuthor|Notice|Hello World!";
    bb_parse_lora_msg(valid_payload);

    assert(msgCount == 1);
    assert(strcmp(msgs[0].author, "TestAuthor") == 0);
    assert(strcmp(msgs[0].type, "Notice") == 0);
    assert(strcmp(msgs[0].text, "Hello World!") == 0);
    printf("test_parse_lora_msg passed.\n");
}

void test_parse_lora_msg_invalid() {
    msgCount = 0;

    // Invalid prefix
    bb_parse_lora_msg("BAD|Data|Here");
    assert(msgCount == 0);

    // Missing parts
    bb_parse_lora_msg("MSG|OnlyAuthor");
    assert(msgCount == 0);

    // Empty payload
    bb_parse_lora_msg("");
    assert(msgCount == 0);

    printf("test_parse_lora_msg_invalid passed.\n");
}

void test_eviction_logic() {
    msgCount = 0;
    mocked_time = 1000;

    // Fill up to max
    for (int i = 0; i < MAX_MSGS; i++) {
        char text[32];
        snprintf(text, sizeof(text), "Message %d", i);
        // Vary expiry slightly
        bb_add_message_local("Author", "Notice", text, 1);
    }

    assert(msgCount == MAX_MSGS);

    // Now advance time so first message expires
    mocked_time = 1000 + (2 * 3600); // 2 hours later

    // Add one more, should trigger eviction
    bb_add_message_local("Author", "Notice", "Overflow Message", 1);

    assert(msgCount == MAX_MSGS);
    // Because they all expired, it should evict the oldest one, which was index 0
    // "Message 1" should now be at index 0
    assert(strcmp(msgs[0].text, "Message 1") == 0);
    // The new one should be at the end
    assert(strcmp(msgs[MAX_MSGS - 1].text, "Overflow Message") == 0);

    printf("test_eviction_logic passed.\n");
}


int main() {
    test_parse_lora_msg();
    test_parse_lora_msg_invalid();
    test_eviction_logic();
    printf("All bulletin board unit tests passed!\n");
    return 0;
}
