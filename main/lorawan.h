#ifndef LORAWAN_H
#define LORAWAN_H

#ifdef __cplusplus
extern "C" {
#endif

void lora_wan_init(void);
void lora_wan_broadcast(const char *message);

#ifdef __cplusplus
}
#endif

#endif // LORAWAN_H
