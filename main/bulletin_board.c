#include "bulletin_board.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_random.h"
#include "esp_timer.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include "cJSON.h"
#include "lorawan.h"

static const char *TAG = "BULLETIN";

#define STORAGE_FILE  "/spiffs/msgs.json"
#define IDENTITY_FILE "/spiffs/identity.json"
#define ADMIN_KEY_FILE "/spiffs/adminkey.json"

char id_name[49]    = "Community Hub";
char id_icon[9]     = "🌱";
char id_tagline[101] = "Take what you need • Share what you can";
char id_rules[101]   = "Be local • Be kind • No spam";
char id_footer[101]  = "Powered locally — no internet required";
char admin_key[65]   = "change_me";
char session_token[33] = "";
unsigned long token_issued_at = 0;
bool g_hardware_key_authenticated = false;

int msgCount = 0;
uint16_t nextMsgId = 1;
BulletinMessage msgs[MAX_MSGS];
bool msgsDirty = false;

void bb_load_identity(void) {
    FILE *f = fopen(IDENTITY_FILE, "r");
    if (!f) return;
    
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *string = malloc(fsize + 1);
    fread(string, 1, fsize, f);
    fclose(f);
    string[fsize] = 0;

    cJSON *json = cJSON_Parse(string);
    if (json) {
        cJSON *item = cJSON_GetObjectItem(json, "name");
        if (item && cJSON_IsString(item) && item->valuestring) strncpy(id_name, item->valuestring, sizeof(id_name)-1);
        item = cJSON_GetObjectItem(json, "icon");
        if (item && cJSON_IsString(item) && item->valuestring) strncpy(id_icon, item->valuestring, sizeof(id_icon)-1);
        item = cJSON_GetObjectItem(json, "tagline");
        if (item && cJSON_IsString(item) && item->valuestring) strncpy(id_tagline, item->valuestring, sizeof(id_tagline)-1);
        item = cJSON_GetObjectItem(json, "rules");
        if (item && cJSON_IsString(item) && item->valuestring) strncpy(id_rules, item->valuestring, sizeof(id_rules)-1);
        item = cJSON_GetObjectItem(json, "footer");
        if (item && cJSON_IsString(item) && item->valuestring) strncpy(id_footer, item->valuestring, sizeof(id_footer)-1);
        cJSON_Delete(json);
    }
    free(string);
}

void bb_save_identity(void) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "name", id_name);
    cJSON_AddStringToObject(root, "icon", id_icon);
    cJSON_AddStringToObject(root, "tagline", id_tagline);
    cJSON_AddStringToObject(root, "rules", id_rules);
    cJSON_AddStringToObject(root, "footer", id_footer);
    
    char *json_str = cJSON_PrintUnformatted(root);
    FILE *f = fopen(IDENTITY_FILE, "w");
    if (f) {
        fwrite(json_str, 1, strlen(json_str), f);
        fclose(f);
    }
    free(json_str);
    cJSON_Delete(root);
}

void bb_load_admin_key(void) {
    FILE *f = fopen(ADMIN_KEY_FILE, "r");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *string = malloc(fsize + 1);
    fread(string, 1, fsize, f);
    fclose(f);
    string[fsize] = 0;
    
    cJSON *json = cJSON_Parse(string);
    if (json) {
        cJSON *item = cJSON_GetObjectItem(json, "key");
        if (item && cJSON_IsString(item) && item->valuestring) strncpy(admin_key, item->valuestring, sizeof(admin_key)-1);
        cJSON_Delete(json);
    }
    free(string);
}

void bb_save_admin_key(void) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "key", admin_key);
    char *json_str = cJSON_PrintUnformatted(root);
    FILE *f = fopen(ADMIN_KEY_FILE, "w");
    if (f) {
        fwrite(json_str, 1, strlen(json_str), f);
        fclose(f);
    }
    free(json_str);
    cJSON_Delete(root);
}

unsigned long nowSecs(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec;
}

unsigned long get_millis() {
    return (unsigned long) (esp_timer_get_time() / 1000ULL);
}

bool bb_check_key(const char* submitted_key) {
    if (!submitted_key) return false;
    return strcmp(submitted_key, admin_key) == 0;
}

void bb_generate_token(char* out_token) {
    uint32_t r1 = esp_random();
    uint32_t r2 = esp_random();
    uint32_t r3 = esp_random();
    uint32_t r4 = esp_random();
    snprintf(out_token, 33, "%08lx%08lx%08lx%08lx", (unsigned long)r1, (unsigned long)r2, (unsigned long)r3, (unsigned long)r4);
    strncpy(session_token, out_token, 33);
    token_issued_at = get_millis();
}

bool bb_is_token_valid(const char* token) {
    if (!token || strlen(session_token) == 0) return false;
    if (get_millis() - token_issued_at > 1800000UL) return false;
    return strcmp(token, session_token) == 0;
}

void bb_load_messages(void) {
    FILE *f = fopen(STORAGE_FILE, "r");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize == 0) { fclose(f); return; }
    
    char *string = malloc(fsize + 1);
    fread(string, 1, fsize, f);
    fclose(f);
    string[fsize] = 0;
    
    cJSON *json = cJSON_Parse(string);
    if (json && cJSON_IsArray(json)) {
        msgCount = 0;
        int count = cJSON_GetArraySize(json);
        for (int i = 0; i < count && msgCount < MAX_MSGS; i++) {
            cJSON *item = cJSON_GetArrayItem(json, i);
            if (!item) continue;
            
            cJSON *cid = cJSON_GetObjectItem(item, "id");
            msgs[msgCount].id = (cid && cJSON_IsNumber(cid)) ? cid->valueint : nextMsgId;
            
            cJSON *cauthor = cJSON_GetObjectItem(item, "author");
            if (cauthor && cJSON_IsString(cauthor) && cauthor->valuestring) strncpy(msgs[msgCount].author, cauthor->valuestring, sizeof(msgs[0].author)-1);
            
            cJSON *ctype = cJSON_GetObjectItem(item, "type");
            if (ctype && cJSON_IsString(ctype) && ctype->valuestring) strncpy(msgs[msgCount].type, ctype->valuestring, sizeof(msgs[0].type)-1);
            
            cJSON *ctext = cJSON_GetObjectItem(item, "text");
            if (ctext && cJSON_IsString(ctext) && ctext->valuestring) strncpy(msgs[msgCount].text, ctext->valuestring, sizeof(msgs[0].text)-1);
            
            cJSON *cexp = cJSON_GetObjectItem(item, "expires");
            msgs[msgCount].expires = (cexp && cJSON_IsNumber(cexp)) ? cexp->valuedouble : 0;
            
            if (msgs[msgCount].id >= nextMsgId) nextMsgId = msgs[msgCount].id + 1;
            msgCount++;
        }
    }
    if(json) cJSON_Delete(json);
    free(string);
}

void bb_save_messages(void) {
    cJSON *root = cJSON_CreateArray();
    for (int i = 0; i < msgCount; i++) {
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(obj, "id", msgs[i].id);
        cJSON_AddStringToObject(obj, "author", msgs[i].author);
        cJSON_AddStringToObject(obj, "type", msgs[i].type);
        cJSON_AddStringToObject(obj, "text", msgs[i].text);
        cJSON_AddNumberToObject(obj, "expires", msgs[i].expires);
        cJSON_AddItemToArray(root, obj);
    }
    char *json_str = cJSON_PrintUnformatted(root);
    FILE *f = fopen(STORAGE_FILE, "w");
    if (f) {
        fwrite(json_str, 1, strlen(json_str), f);
        fclose(f);
    }
    free(json_str);
    cJSON_Delete(root);
    msgsDirty = false;
}

void bb_add_message(const char* author, const char* type, const char* text, int expiry_hours) {
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
        if (evict < 0) return;
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
    
    char *lora_msg = (char *)malloc(512);
    if (lora_msg) {
        snprintf(lora_msg, 512, "MSG|%s|%s|%s", author, type, text);
        lora_wan_broadcast(lora_msg);
        free(lora_msg);
    }
}

void bb_delete_message(uint16_t targetId) {
    for (int i = 0; i < msgCount; i++) {
        if (msgs[i].id == targetId) {
            memmove(&msgs[i], &msgs[i + 1], (msgCount - 1 - i) * sizeof(BulletinMessage));
            msgCount--;
            msgsDirty = true;
            bb_save_messages();
            return;
        }
    }
}

void bb_clear_messages(void) {
    msgCount = 0;
    msgsDirty = true;
    bb_save_messages();
}

void bb_initialize(void) {
    bb_load_identity();
    bb_load_admin_key();
    bb_load_messages();
    ESP_LOGI(TAG, "Bulletin board initialized. %d messages loaded.", msgCount);
}
