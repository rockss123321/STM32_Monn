#include "credentials.h"
#include <string.h>
#include "main.h"   // там объявлен extern RTC_HandleTypeDef hrtc;


extern RTC_HandleTypeDef hrtc;   // <-- добавить


#define BKP_BASE      RTC_BKP_DR0  // первый backup-регистр
#define BKP_WORDS     (sizeof(credentials_t)/4)  // сколько слов займём

static credentials_t creds;

// CRC32 для контроля
static uint32_t crc32_calc(const void *data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    const uint8_t *p = data;
    while (len--) {
        crc ^= *p++;
        for (int i = 0; i < 8; i++) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
        }
    }
    return ~crc;
}

static void backup_write(const credentials_t *c) {
    const uint32_t *src = (const uint32_t*)c;
    for (uint32_t i = 0; i < BKP_WORDS; i++) {
        HAL_RTCEx_BKUPWrite(&hrtc, BKP_BASE + i, src[i]);
    }
}

static void backup_read(credentials_t *c) {
    uint32_t *dst = (uint32_t*)c;
    for (uint32_t i = 0; i < BKP_WORDS; i++) {
        dst[i] = HAL_RTCEx_BKUPRead(&hrtc, BKP_BASE + i);
    }
}

// публичные функции
void Creds_Init(void) {
    backup_read(&creds);

    uint32_t crc = crc32_calc(&creds, sizeof(credentials_t) - sizeof(uint32_t));
    if (crc != creds.crc) {
        // дефолтные значения
        strcpy(creds.username, "admin");
        strcpy(creds.password, "admin");
        creds.crc = crc32_calc(&creds, sizeof(credentials_t) - sizeof(uint32_t));
        backup_write(&creds);
    }
}

bool Creds_CheckLogin(const char *user, const char *pass) {
    return (strncmp(user, creds.username, MAX_CRED_LEN) == 0 &&
            strncmp(pass, creds.password, MAX_CRED_LEN) == 0);
}

void Creds_Update(const char *user, const char *pass) {
    strncpy(creds.username, user, MAX_CRED_LEN-1);
    creds.username[MAX_CRED_LEN-1] = 0;
    strncpy(creds.password, pass, MAX_CRED_LEN-1);
    creds.password[MAX_CRED_LEN-1] = 0;
    creds.crc = crc32_calc(&creds, sizeof(credentials_t) - sizeof(uint32_t));
    backup_write(&creds);
}

const credentials_t* Creds_Get(void) {
    return &creds;
}
