#ifndef BULLETIN_BOARD_H
#define BULLETIN_BOARD_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_MSGS 200

typedef struct {
    uint16_t id;
    char author[25];
    char type[16];
    char text[301];
    unsigned long expires;
} BulletinMessage;

extern char id_name[49];
extern char id_icon[9];
extern char id_tagline[101];
extern char id_rules[101];
extern char id_footer[101];
extern char admin_key[65];
extern char session_token[33];
extern unsigned long token_issued_at;

extern int msgCount;
extern BulletinMessage msgs[MAX_MSGS];

void bb_initialize(void);

void bb_load_identity(void);
void bb_save_identity(void);
void bb_load_admin_key(void);
void bb_save_admin_key(void);
bool bb_check_key(const char* submitted_key);
void bb_generate_token(char* out_token);
bool bb_is_token_valid(const char* token);

void bb_load_messages(void);
void bb_save_messages(void);
void bb_add_message(const char* author, const char* type, const char* text, int expiry_hours);
void bb_delete_message(uint16_t targetId);
void bb_clear_messages(void);

#ifdef __cplusplus
}
#endif

#endif // BULLETIN_BOARD_H
