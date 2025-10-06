#ifndef CREDENTIALS_H
#define CREDENTIALS_H

#include "stm32f2xx_hal.h"
#include <stdbool.h>

#define MAX_CRED_LEN 16

typedef struct {
    char username[MAX_CRED_LEN];
    char password[MAX_CRED_LEN];
    uint32_t crc;
} credentials_t;

void     Creds_Init(void);
bool     Creds_CheckLogin(const char *user, const char *pass);
void     Creds_Update(const char *user, const char *pass);
const credentials_t* Creds_Get(void);

#endif
