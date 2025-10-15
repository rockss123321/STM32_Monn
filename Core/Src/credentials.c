#include "credentials.h"
#include <string.h>
#include "main.h"   // там объявлен extern RTC_HandleTypeDef hrtc;


extern RTC_HandleTypeDef hrtc;   // <-- добавить


/* Store only password, 8 chars: two BKP registers starting at DR17 */
#define BKP_PASS_DR   RTC_BKP_DR17

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

static void backup_write_pass(const char *pass) {
    uint32_t word = 0;
    /* password: 8 chars max packed into 2 DRs */
    for (int i = 0; i < 2; ++i) {
        word = 0;
        for (int b = 0; b < 4; ++b) {
            int idx = i * 4 + b;
            uint8_t ch = pass && pass[idx] ? (uint8_t)pass[idx] : 0;
            word |= ((uint32_t)ch) << (8 * b);
        }
        HAL_RTCEx_BKUPWrite(&hrtc, (uint32_t)(BKP_PASS_DR + i), word);
    }
}

static void backup_read_pass(char *pass) {
    for (int i = 0; i < 2; ++i) {
        uint32_t w = HAL_RTCEx_BKUPRead(&hrtc, (uint32_t)(BKP_PASS_DR + i));
        for (int b = 0; b < 4; ++b) pass[i*4 + b] = (char)((w >> (8*b)) & 0xFF);
    }
    pass[8] = 0;
}

// публичные функции
void Creds_Init(void) {
    char p[9] = {0};
    backup_read_pass(p);
    if (p[0] == 0) {
        creds.username[0] = 0; // username unused
        strncpy(creds.password, "admin", MAX_CRED_LEN-1);
        backup_write_pass(creds.password);
    } else {
        strncpy(creds.password, p, MAX_CRED_LEN-1);
        creds.password[MAX_CRED_LEN-1] = 0;
        creds.username[0] = 0;
    }
}

bool Creds_CheckLogin(const char *user, const char *pass) {
    (void)user; // username disabled
    return (strncmp(pass, creds.password, MAX_CRED_LEN) == 0);
}

void Creds_Update(const char *user, const char *pass) {
    (void)user; // ignore username, only password used
    strncpy(creds.password, pass, MAX_CRED_LEN-1);
    creds.password[MAX_CRED_LEN-1] = 0;
    backup_write_pass(creds.password);
}

const credentials_t* Creds_Get(void) {
    return &creds;
}
