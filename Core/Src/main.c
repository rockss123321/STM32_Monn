/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "lwip.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "lwip/apps/httpd.h"

#include "lwip/apps/snmp.h"
#include "lwip/apps/snmp_scalar.h"
#include "lwip/apps/snmp_mib2.h"
#include "lwip/err.h"
#include "lwip/udp.h"
#include "lwip/ip_addr.h"

#include "signal_processor.h"
#include "ssd1306_driver/ssd1306.h"
#include <string.h>
#include "ssd1306_driver/ssd1306_fonts.h"
#include "oled/oled_netinfo.h"
#include "oled/oled_abpage.h"
#include "buttons/buttons_process.h"
#include "oled/oled_display.h"
#include "oled/oled_settings.h"
#include "credentials.h"
#include "buttons/buttons.h"
#include "settings_storage.h"
#include "auth.h"
#include <ctype.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#include "snmp/custom_mib.h"
#include "lwip/apps/snmp_mib2.h"

/* Список MIB */
const struct snmp_mib* mib_array[] = {
  &mib2,        /* стандартный MIB-II */
  &custom_mib   /* ваш кастомный */
};

const u8_t snmp_num_mibs = LWIP_ARRAYSIZE(mib_array);

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

extern float voltage1;
extern float voltage2;
extern float current;
extern float s_power;

extern float selected_voltage;

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

#define ADC_BUFFER_SIZE   (3 * ADC_SAMPLES)
uint32_t adc_buffer[ADC_BUFFER_SIZE];
volatile uint8_t dma_ready = 0;

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    dma_ready = 1;
}
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

I2C_HandleTypeDef hi2c1;

RTC_HandleTypeDef hrtc;

TIM_HandleTypeDef htim3;

/* USER CODE BEGIN PV */
extern struct netif gnetif;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_I2C1_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM3_Init(void);
static void MX_RTC_Init(void);
/* USER CODE BEGIN PFP */

void ApplySNMPSettings(void);
void httpd_ssi_init_custom(void);



void ApplySNMPSettings(void) {
    // Пока пусто — позже добавишь код для применения SNMP community
}

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
volatile uint8_t g_is_authenticated = 0; // глобальный флаг авторизации (устаревший, оставлен для совместимости)
volatile uint32_t g_auth_deadline_ms = 0; // срок действия авторизации (общий, не используется при IP-режиме)

#ifndef AUTH_TTL_MS
#define AUTH_TTL_MS (15U * 60U * 1000U) // 15 минут
#endif

// Глобальная настраиваемая длительность сессии (можно изменить в рантайме)
uint32_t g_auth_ttl_ms = AUTH_TTL_MS;


ip4_addr_t new_ip, new_mask, new_gw;
uint8_t new_dhcp_enabled = 0;
uint8_t apply_network_settings = 0;  // флаг применения


// Прототипы CGI-функций
const char * NET_CGI_Handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[]);
const char * LOGIN_CGI_Handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[]);

// Таблица CGI
const tCGI NET_CGI = {"/set_network.cgi", NET_CGI_Handler};
const tCGI LOGIN_CGI = {"/login.cgi", LOGIN_CGI_Handler};
tCGI CGI_TAB[6];

const char* NET_CGI_Handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    // Reset DHCP flag; it's set only when parameter is present
    new_dhcp_enabled = 0;
    for (int i=0; i<iNumParams; i++) {
        // Сетевые
        if (strcmp(pcParam[i], "ip") == 0 && pcValue[i][0] != '\0') new_ip.addr = ipaddr_addr(pcValue[i]);
        else if (strcmp(pcParam[i], "mask") == 0 && pcValue[i][0] != '\0') new_mask.addr = ipaddr_addr(pcValue[i]);
        else if (strcmp(pcParam[i], "gateway") == 0 && pcValue[i][0] != '\0') new_gw.addr = ipaddr_addr(pcValue[i]);
        else if (strcmp(pcParam[i], "dhcp") == 0) new_dhcp_enabled = 1;
    }

    apply_network_settings = 1;

    return "/settings.html";  // редирект обратно
}

#include "lwip/ip_addr.h"
#include <stdlib.h>
#include <string.h>



uint8_t new_year=0, new_month=0, new_day=0;
uint8_t apply_date_settings = 0;

const char* DATE_CGI_Handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    for(int i=0; i<iNumParams; i++)
    {
        if(strcmp(pcParam[i],"date")==0 && pcValue[i][0]!='\0')
        {
            int y,m,d;
            if(sscanf(pcValue[i], "%d-%d-%d", &y,&m,&d)==3)
            {
                new_year  = (uint8_t)(y % 100);
                new_month = (uint8_t)m;
                new_day   = (uint8_t)d;
                apply_date_settings = 1;
            }
        }
    }
    // --- сразу сохраняем в backup (если RTC уже инициализирован) ---
}

const tCGI DATE_CGI = {"/set_date.cgi", DATE_CGI_Handler};

uint8_t new_hours = 0, new_minutes = 0, new_seconds = 0;
uint8_t apply_time_settings = 0;

const char* TIME_CGI_Handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    apply_time_settings = 0;

    for(int i = 0; i < iNumParams; i++)
    {
        if(strcmp(pcParam[i], "time") == 0 && strlen(pcValue[i]) >= 5)
        {
            // Простой парсинг без sscanf
            char *colon = strchr(pcValue[i], ':');
            if(colon != NULL)
            {
                // Берем первые 2 символа как часы
                char hour_str[3] = {0};
                strncpy(hour_str, pcValue[i], 2);
                hour_str[2] = '\0';

                // Берем 2 символа после двоеточия как минуты
                char min_str[3] = {0};
                strncpy(min_str, colon + 1, 2);
                min_str[2] = '\0';

                int h = atoi(hour_str);
                int mi = atoi(min_str);

                if(h >= 0 && h < 24 && mi >= 0 && mi < 60)
                {
                    new_hours = (uint8_t)h;
                    new_minutes = (uint8_t)mi;
                    new_seconds = 0;
                    apply_time_settings = 1;
                }
            }
        }
    }
    return "/settings.html";
}

const tCGI TIME_CGI = {"/set_time.cgi", TIME_CGI_Handler};

extern const char *snmp_community[];
extern const char *snmp_community_write[];
extern const char *snmp_trap_community[];

// --- SNMP ---
char snmp_read[32] = "public";
char snmp_write[32] = "private";
char snmp_trap[32] = "public";
uint8_t apply_snmp_settings = 0;

// CGI-хэндлер для SNMP
const char* SNMP_CGI_Handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    for(int i=0; i<iNumParams; i++)
    {
        if(strcmp(pcParam[i],"snmp-read")==0 && pcValue[i][0]!='\0')
        {
            strncpy(snmp_read, pcValue[i], sizeof(snmp_read)-1);
            snmp_read[sizeof(snmp_read)-1] = 0;
        }
        else if(strcmp(pcParam[i],"snmp-write")==0 && pcValue[i][0]!='\0')
        {
            strncpy(snmp_write, pcValue[i], sizeof(snmp_write)-1);
            snmp_write[sizeof(snmp_write)-1] = 0;
        }
        else if(strcmp(pcParam[i],"snmp-trap")==0 && pcValue[i][0]!='\0')
        {
            strncpy(snmp_trap, pcValue[i], sizeof(snmp_trap)-1);
            snmp_trap[sizeof(snmp_trap)-1] = 0;
        }
    }
    apply_snmp_settings = 1; // ставим флаг применения в main()
    return "/settings.html";  // редирект обратно
}

// Таблица CGI для SNMP
const tCGI SNMP_CGI = {"/set_snmp.cgi", SNMP_CGI_Handler};

#include "stm32f2xx_hal.h"
#include <string.h>
#include <stdbool.h>

#define FLASH_SNMP_ADDR  0x080E0000  // выбери свободный сектор

void Save_SNMP_Settings_To_Flash(const char* read, const char* write, const char* trap)
{
    HAL_FLASH_Unlock();

    FLASH_EraseInitTypeDef eraseInit;
    uint32_t pageError = 0;

    eraseInit.TypeErase = FLASH_TYPEERASE_SECTORS;
    eraseInit.Sector = FLASH_SECTOR_11;   // зависит от твоего чипа!
    eraseInit.NbSectors = 1;
    eraseInit.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    HAL_FLASHEx_Erase(&eraseInit, &pageError);

    uint32_t address = FLASH_SNMP_ADDR;

    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, *((uint32_t*)read));
    address += 4;
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, *((uint32_t*)write));
    address += 4;
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, *((uint32_t*)trap));

    HAL_FLASH_Lock();
}


// Попытка OTA: область для приёма прошивки. ВНИМАНИЕ:
// Для STM32F207VCTx (256KB) приложение в текущей сборке занимает сектор 5 (0x08020000..0x0803FFFF),
// поэтому место для OTA, скорее всего, недоступно. Код ниже выполняет безопасные проверки
// и откажется от обновления, если область занята (не пустая), чтобы не "убить" прошивку.
#define FLASH_UPDATE_ADDR   0x08020000U    // предполагаемый слот OTA (Sector 5, 128KB)
#define FLASH_UPDATE_END    0x08040000U
#define RAM_BUFFER_SIZE     (8*1024)       // небольшой буфер для выравнивания

typedef struct {
    uint8_t buffer[RAM_BUFFER_SIZE];
    uint32_t buffer_len;
    uint32_t total_len;
    bool active;
    bool error;
    uint32_t write_addr;
    uint8_t word_buf[4];
    uint8_t word_buf_len;
    uint32_t crc;
    bool erased;
} FW_Update_Context;

FW_Update_Context fw_ctx;
static bool fw_request_active = false;   // текущий POST = fw_update?
// --- Helpers for Flash OTA ---
static uint32_t Flash_GetSector(uint32_t Address)
{
    if (Address < 0x08004000U) return FLASH_SECTOR_0;
    if (Address < 0x08008000U) return FLASH_SECTOR_1;
    if (Address < 0x0800C000U) return FLASH_SECTOR_2;
    if (Address < 0x08010000U) return FLASH_SECTOR_3;
    if (Address < 0x08020000U) return FLASH_SECTOR_4;
    return FLASH_SECTOR_5; // up to 0x0803FFFF for 256KB devices
}

static bool Flash_IsBlank(uint32_t addr, uint32_t bytes_to_check)
{
    for (uint32_t off = 0; off < bytes_to_check; off += 4) {
        uint32_t v = *(volatile uint32_t *)(addr + off);
        if (v != 0xFFFFFFFFU) return false;
    }
    return true;
}

static void FW_ResetContext(void)
{
    fw_ctx.active = false;
    fw_ctx.error = false;
    fw_ctx.buffer_len = 0;
    fw_ctx.total_len = 0;
    fw_ctx.write_addr = FLASH_UPDATE_ADDR;
    fw_ctx.word_buf_len = 0;
    fw_ctx.crc = 0xFFFFFFFFU;
    fw_ctx.erased = false;
}

static inline void FW_CrcUpdate(const uint8_t *data, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        fw_ctx.crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            fw_ctx.crc = (fw_ctx.crc >> 1) ^ (0xEDB88320U & (~(fw_ctx.crc & 1U) + 1U));
        }
    }
}

static HAL_StatusTypeDef FW_EnsureErasedForAddress(uint32_t address)
{
    if (fw_ctx.erased) return HAL_OK;
    // Проверка: область должна быть пустой, иначе это часть прошивки — отменяем OTA
    if (!Flash_IsBlank(FLASH_UPDATE_ADDR, 1024U)) { // проверим первые 1KB
        fw_ctx.error = true;
        return HAL_ERROR;
    }

    FLASH_EraseInitTypeDef erase;
    uint32_t pageError = 0;
    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    erase.Sector = Flash_GetSector(address);
    erase.NbSectors = 1; // стираем только сектор слота OTA
    HAL_StatusTypeDef st = HAL_FLASHEx_Erase(&erase, &pageError);
    if (st == HAL_OK) fw_ctx.erased = true;
    else fw_ctx.error = true;
    return st;
}

static HAL_StatusTypeDef FW_FlashWriteStream(const uint8_t *data, uint32_t len)
{
    // Обеспечиваем стирание перед первой записью
    if (!fw_ctx.erased) {
        HAL_StatusTypeDef est = FW_EnsureErasedForAddress(fw_ctx.write_addr);
        if (est != HAL_OK) return est;
    }

    // Обновляем CRC по потоку
    FW_CrcUpdate(data, len);

    uint32_t idx = 0;
    // Дополним незавершённое слово, если было
    if (fw_ctx.word_buf_len > 0) {
        while (fw_ctx.word_buf_len < 4 && idx < len) {
            fw_ctx.word_buf[fw_ctx.word_buf_len++] = data[idx++];
        }
        if (fw_ctx.word_buf_len == 4) {
            uint32_t word;
            memcpy(&word, fw_ctx.word_buf, 4);
            if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, fw_ctx.write_addr, word) != HAL_OK) return HAL_ERROR;
            fw_ctx.write_addr += 4;
            fw_ctx.word_buf_len = 0;
        }
    }

    // Пишем целыми словами напрямую из входного буфера
    while ((idx + 4) <= len) {
        uint32_t word;
        memcpy(&word, &data[idx], 4);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, fw_ctx.write_addr, word) != HAL_OK) return HAL_ERROR;
        fw_ctx.write_addr += 4;
        idx += 4;
        if (fw_ctx.write_addr >= FLASH_UPDATE_END) { fw_ctx.error = true; return HAL_ERROR; }
    }

    // Остаток < 4 байт сохраняем в буфер до завершения
    while (idx < len) {
        fw_ctx.word_buf[fw_ctx.word_buf_len++] = data[idx++];
    }
    return HAL_OK;
}

// CRC32 функция (можно заменить на HAL/STM32 встроенную)
uint32_t crc32(uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFF;
    for(uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for(uint8_t j = 0; j < 8; j++)
            crc = (crc >> 1) ^ (0xEDB88320 & (~(crc & 1) + 1));
    }
    return ~crc;
}

// --- CGI ---
const char* FW_Update_CGI_Handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    return "/update.html";
}

const tCGI FW_UPDATE_CGI = {"/fw_update.cgi", FW_Update_CGI_Handler};

// GET-логин через CGI-параметры user/pass
const char* LOGIN_CGI_Handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    char user[32] = {0};
    char pass[32] = {0};
    for (int i = 0; i < iNumParams; i++) {
        if (strcmp(pcParam[i], "user") == 0 && pcValue[i] && pcValue[i][0]) {
            strncpy(user, pcValue[i], sizeof(user) - 1);
        } else if (strcmp(pcParam[i], "pass") == 0 && pcValue[i] && pcValue[i][0]) {
            strncpy(pass, pcValue[i], sizeof(pass) - 1);
        }
    }
    url_decode(user, user);
    url_decode(pass, pass);
    extern volatile uint8_t g_is_authenticated;
    extern volatile uint32_t g_auth_deadline_ms;
    extern uint32_t g_auth_ttl_ms;
    g_is_authenticated = (user[0] && pass[0] && Creds_CheckLogin(user, pass)) ? 1 : 0;
    if (g_is_authenticated) {
        uint32_t now = HAL_GetTick();
        g_auth_deadline_ms = now + g_auth_ttl_ms; /* legacy */
        /* создаём cookie-сессию; заголовок Set-Cookie вернётся при следующем fs_open */
        (void)Auth_CreateSessionForCurrentRequest(now, g_auth_ttl_ms);
        return "/index.html";
    }
    return "/login_failed.html";
}

void url_decode(char *dst, const char *src)
{
    char a, b;
    while (*src)
    {
        if ((*src == '%') &&
            ((a = src[1]) && (b = src[2])) &&
            (isxdigit(a) && isxdigit(b)))
        {
            a = (a >= 'a') ? a - 'a' + 10 : (a >= 'A') ? a - 'A' + 10 : a - '0';
            b = (b >= 'a') ? b - 'a' + 10 : (b >= 'A') ? b - 'A' + 10 : b - '0';
            *dst++ = (char)(16 * a + b);
            src += 3;
        }
        else if (*src == '+')
        {
            *dst++ = ' ';
            src++;
        }
        else
        {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}


// --- POST обработчики ---
err_t httpd_post_begin(void *connection,
                       const char *uri,
                       const char *http_request,
                       u16_t content_len,
                       int total_data_len,
                       char *post_data,
                       u16_t post_data_len,
                       u8_t *connection_status)
{
    // Сброс признаков по умолчанию
    fw_request_active = false;

    if(strcmp(uri, "/fw_update.cgi") == 0) {
        fw_request_active = true;
        FW_ResetContext();
        // Если слот OTA потенциально пересекается с текущей прошивкой (не пустой) — не начинаем запись
        if (!Flash_IsBlank(FLASH_UPDATE_ADDR, 1024U)) {
            fw_ctx.error = true;
            fw_ctx.active = false;
            *connection_status = 0; // не буферизуем лишние данные
            return ERR_OK;
        }
        fw_ctx.active = true;
        *connection_status = 1; // продолжаем принимать
        HAL_FLASH_Unlock();
    }
    return ERR_OK;
}



err_t httpd_post_receive_data(void *connection, struct pbuf *p)
{
    if (p == NULL) return ERR_OK;

    if (fw_ctx.active) {
        struct pbuf *q = p;
        while(q) {
            if (FW_FlashWriteStream((const uint8_t*)q->payload, q->len) != HAL_OK) {
                fw_ctx.error = true;
                fw_ctx.active = false;
                break;
            }
            fw_ctx.total_len += q->len;
            q = q->next;
        }
    }
    return ERR_OK;
}

void httpd_post_finished(void *connection, char *response_uri, u16_t response_uri_len)
{
    // Если это был fw_update — завершим запись и отдадим соответствующую страницу
    if (fw_request_active) {
        // Завершаем запись: дописываем неполное слово, если нужно
        if (fw_ctx.active && !fw_ctx.error) {
            if (fw_ctx.word_buf_len > 0) {
                while (fw_ctx.word_buf_len < 4) fw_ctx.word_buf[fw_ctx.word_buf_len++] = 0xFF;
                uint32_t word;
                memcpy(&word, fw_ctx.word_buf, 4);
                if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, fw_ctx.write_addr, word) != HAL_OK) {
                    fw_ctx.error = true;
                } else {
                    fw_ctx.write_addr += 4;
                }
            }
        }
        HAL_FLASH_Lock();
        fw_ctx.active = false;
        if (response_uri && response_uri_len) {
            if (fw_ctx.error || fw_ctx.total_len == 0) {
                strncpy(response_uri, "/update.html", response_uri_len);
            } else {
                strncpy(response_uri, "/update_complete.html", response_uri_len);
            }
        }
        return;
    }
}


/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_I2C1_Init();
  MX_LWIP_Init();
  MX_ADC1_Init();
  MX_TIM3_Init();
  MX_RTC_Init();
  /* USER CODE BEGIN 2 */
  // Инициализация логина/пароля (admin/admin по умолчанию)
  Creds_Init();
  Settings_Init();

  ip4_addr_t bk_ip, bk_mask, bk_gw;
  uint8_t bk_dhcp;
  char bk_snmp_read[32], bk_snmp_write[32], bk_snmp_trap[32];

  Settings_Load_From_Backup(&bk_ip, &bk_mask, &bk_gw, &bk_dhcp,
                            bk_snmp_read, sizeof(bk_snmp_read),
                            bk_snmp_write, sizeof(bk_snmp_write),
                            bk_snmp_trap, sizeof(bk_snmp_trap));

  // Apply saved network settings on boot if present
  if (bk_dhcp || bk_ip.addr != 0) {
      netif_set_down(&gnetif);
      if (bk_dhcp) {
          dhcp_start(&gnetif);
      } else {
          netif_set_addr(&gnetif, &bk_ip, &bk_mask, &bk_gw);
      }
      netif_set_up(&gnetif);
  }

  /* Инициализируем SNMP community из backup */
  if (bk_snmp_read[0])  strncpy(snmp_read,  bk_snmp_read,  sizeof(snmp_read)-1);
  if (bk_snmp_write[0]) strncpy(snmp_write, bk_snmp_write, sizeof(snmp_write)-1);
  if (bk_snmp_trap[0])  strncpy(snmp_trap,  bk_snmp_trap,  sizeof(snmp_trap)-1);
  /* Применяем к SNMP-агенту на старте */
  snmp_community[0] = snmp_read;
  snmp_community_write[0] = snmp_write;
  snmp_set_community_trap(snmp_trap);


  httpd_init();

  httpd_ssi_init_custom();

  Auth_Init();

  // Регистрация CGI

  CGI_TAB[0] = NET_CGI;
  CGI_TAB[1] = DATE_CGI;
  CGI_TAB[2] = TIME_CGI;
  CGI_TAB[3] = SNMP_CGI;
  CGI_TAB[4] = FW_UPDATE_CGI;
  CGI_TAB[5] = LOGIN_CGI;
  http_set_cgi_handlers(CGI_TAB, 6); // количество зарегистрированных CGI
  snmp_init();

  snmp_set_mibs(mib_array, snmp_num_mibs);


  ssd1306_Init();

  ssd1306_UpdateScreen();

  ssd1306_Fill(Black);

  Buttons_Init();


  HAL_ADC_Start_DMA(&hadc1, adc_buffer, ADC_BUFFER_SIZE);

  HAL_TIM_Base_Start(&htim3);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  MX_LWIP_Process();

      if (dma_ready) {
          SignalProcessor_Update(adc_buffer, ADC_SAMPLES);
          dma_ready = 0;
      }
	    Buttons_Process();

	    OLED_Settings_TimeoutCheck();

	    OLED_UpdateDisplay();

	    if (apply_network_settings) {
	        apply_network_settings = 0;

	        netif_set_down(&gnetif);
	        dhcp_stop(&gnetif);

	        if (new_dhcp_enabled) {
	            dhcp_start(&gnetif);
	        } else {
	            netif_set_addr(&gnetif, &new_ip, &new_mask, &new_gw);
	        }
	        netif_set_up(&gnetif);

	        Settings_Save_To_Backup(new_ip, new_mask, new_gw, new_dhcp_enabled,
	                                snmp_read, snmp_write, snmp_trap);
	    }


	    	// Дата
	    if(apply_date_settings)
	    {
	        apply_date_settings = 0;

	        RTC_DateTypeDef sDate = {0};
	        sDate.Year  = new_year;
	        sDate.Month = new_month;
	        sDate.Date  = new_day;
	        sDate.WeekDay = RTC_WEEKDAY_TUESDAY;

	        if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN) == HAL_OK)
	        {
	            RTC_TimeTypeDef t;
	            HAL_RTC_GetTime(&hrtc, &t, RTC_FORMAT_BIN);
	        }
	    }


	    	// Обработка ВРЕМЕНИ
	    if(apply_time_settings)
	    {
	        apply_time_settings = 0;

	        RTC_TimeTypeDef sTime = {0};
	        sTime.Hours   = new_hours;
	        sTime.Minutes = new_minutes;
	        sTime.Seconds = new_seconds;
	        sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
	        sTime.StoreOperation = RTC_STOREOPERATION_RESET;

        if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) == HAL_OK)
	        {
	            RTC_DateTypeDef d;
	            HAL_RTC_GetDate(&hrtc, &d, RTC_FORMAT_BIN);
	        }
	    }


	        // Проверка применения SNMP
	    	if (apply_snmp_settings) {
	    	    apply_snmp_settings = 0;
            snmp_community[0] = snmp_read;
            snmp_community_write[0] = snmp_write;
            snmp_set_community_trap(snmp_trap);

            // Preserve current network settings: reload them from backup and rewrite with new SNMP
            ip4_addr_t saved_ip, saved_mask, saved_gw;
            uint8_t saved_dhcp;
            Settings_Load_From_Backup(&saved_ip, &saved_mask, &saved_gw, &saved_dhcp,
                                      NULL, 0, NULL, 0, NULL, 0);
            Settings_Save_To_Backup(saved_ip, saved_mask, saved_gw, saved_dhcp,
                                    snmp_read, snmp_write, snmp_trap);
	    	}




    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 20;
  RCC_OscInitStruct.PLL.PLLN = 192;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_RTC;
  PeriphClkInitStruct.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = ENABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
  hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T3_TRGO;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 3;
  hadc1.Init.DMAContinuousRequests = ENABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_5;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_56CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_6;
  sConfig.Rank = 2;
  sConfig.SamplingTime = ADC_SAMPLETIME_56CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_10;
  sConfig.Rank = 3;
  sConfig.SamplingTime = ADC_SAMPLETIME_56CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess(); // 🔹 доступ к backup-домену

    __HAL_RCC_RTC_ENABLE();     // 🔹 включаем тактирование RTC, если ещё не включено

    hrtc.Instance = RTC;
    hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
    hrtc.Init.AsynchPrediv = 127;
    hrtc.Init.SynchPrediv = 255;
    hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
    hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
    hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;

    if (HAL_RTC_Init(&hrtc) != HAL_OK)
    {
        Error_Handler();
    }

    /* Проверяем, был ли RTC уже инициализирован */
    if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0) != 0x32F2)
    {
        // --- Первый запуск ---
        sTime.Hours = 0;
        sTime.Minutes = 0;
        sTime.Seconds = 0;
        sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
        sTime.StoreOperation = RTC_STOREOPERATION_RESET;
        HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);

        sDate.WeekDay = RTC_WEEKDAY_MONDAY;
        sDate.Month = RTC_MONTH_JANUARY;
        sDate.Date = 1;
        sDate.Year = 25;
        HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

        HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR0, 0x32F2); // 💾 флаг инициализации
    }
    else
    {
        // --- RTC уже настроен, ничего не трогаем ---
    }
}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 59;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 99;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA2_Stream0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pins : PD1 PD2 PD3 */
  GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI1_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI1_IRQn);

  HAL_NVIC_SetPriority(EXTI2_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI2_IRQn);

  HAL_NVIC_SetPriority(EXTI3_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI3_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
