#ifndef CREDENTIALS_H
#define CREDENTIALS_H

#include "stm32f2xx_hal.h"
#include <stdbool.h>

#define MAX_CRED_LEN 9 /* up to 8 visible chars + NUL */

typedef struct {
    char password[MAX_CRED_LEN];
    /* crc is not persisted in BKP anymore */
    uint32_t crc;
} credentials_t;

void     Creds_Init(void);
bool     Creds_CheckPassword(const char *pass);
void     Creds_UpdatePassword(const char *pass);
const credentials_t* Creds_Get(void);

#endif
