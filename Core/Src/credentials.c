#include "credentials.h"
#include <string.h>
#include "main.h"   // там объявлен extern RTC_HandleTypeDef hrtc;


extern RTC_HandleTypeDef hrtc;   // <-- добавить


#define BKP_USER_DR   RTC_BKP_DR10
#define BKP_PASS_DR   RTC_BKP_DR14

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

static void backup_write_userpass(const char *user, const char *pass) {
    uint32_t word = 0;
    /* username: 8 chars max packed into 2 DRs */
    for (int i = 0; i < 2; ++i) {
        word = 0;
        for (int b = 0; b < 4; ++b) {
            int idx = i * 4 + b;
            uint8_t ch = user && user[idx] ? (uint8_t)user[idx] : 0;
            word |= ((uint32_t)ch) << (8 * b);
        }
        HAL_RTCEx_BKUPWrite(&hrtc, (uint32_t)(BKP_USER_DR + i), word);
    }
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

static void backup_read_userpass(char *user, char *pass) {
    for (int i = 0; i < 2; ++i) {
        uint32_t w = HAL_RTCEx_BKUPRead(&hrtc, (uint32_t)(BKP_USER_DR + i));
        for (int b = 0; b < 4; ++b) user[i*4 + b] = (char)((w >> (8*b)) & 0xFF);
    }
    for (int i = 0; i < 2; ++i) {
        uint32_t w = HAL_RTCEx_BKUPRead(&hrtc, (uint32_t)(BKP_PASS_DR + i));
        for (int b = 0; b < 4; ++b) pass[i*4 + b] = (char)((w >> (8*b)) & 0xFF);
    }
    user[8] = 0; pass[8] = 0;
}

// публичные функции
void Creds_Init(void) {
    char u[9] = {0}, p[9] = {0};
    backup_read_userpass(u, p);
    if (u[0] == 0 || p[0] == 0) {
        strncpy(creds.username, "admin", MAX_CRED_LEN-1);
        strncpy(creds.password, "admin", MAX_CRED_LEN-1);
        backup_write_userpass(creds.username, creds.password);
    } else {
        strncpy(creds.username, u, MAX_CRED_LEN-1);
        creds.username[MAX_CRED_LEN-1] = 0;
        strncpy(creds.password, p, MAX_CRED_LEN-1);
        creds.password[MAX_CRED_LEN-1] = 0;
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
    backup_write_userpass(creds.username, creds.password);
}

const credentials_t* Creds_Get(void) {
    return &creds;
}
