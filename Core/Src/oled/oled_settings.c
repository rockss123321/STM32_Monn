#include "oled/oled_settings.h"
#include "ssd1306_driver/ssd1306_fonts.h"
#include "ssd1306_driver/ssd1306.h"
#include "lwip/netif.h"
#include "lwip/dhcp.h"
#include "lwip/ip_addr.h"
#include "main.h"
#include <string.h>
#include <stdio.h>
#include "buttons/buttons_process.h"
#include "settings_storage.h"
#include "stm32f2xx_hal.h"

extern RTC_HandleTypeDef hrtc;

extern ip4_addr_t new_ip, new_mask, new_gw;
extern uint8_t new_dhcp_enabled;
extern uint8_t apply_network_settings;


#define SSD1306_ROTATED_WIDTH  (SSD1306_HEIGHT)
#define SSD1306_ROTATED_HEIGHT (SSD1306_WIDTH)

// Глобальные сетевые настройки
uint8_t last_ip[4]   = {192,168,1,178};
uint8_t last_mask[4] = {255,255,255,0};
uint8_t last_gw[4]   = {192,168,1,1};

// Пункты меню
static const char *menu_items[] = {
    "IP",
    "Mask",
    "Gateway",
    "DHCP",
    "Reset (reboot)",
    "Reboot to defaults",
    "Display rotation"
};
#define MENU_ITEMS_COUNT (sizeof(menu_items)/sizeof(menu_items[0]))

static int selected_index = 0;
static const SSD1306_Font_t *menu_font = &Font_7x10;
static const SSD1306_Font_t *edit_font = &Font_11x18;
static const int vpad = 3;

// --- Редактирование IP/Mask/GW ---
static uint8_t edit_parts[4] = {192,168,1,178};
static int edit_digit = 0;
static bool editing_active = false;
static char edit_title[16] = "Set IP";

// --- DHCP флаг ---
static bool dhcp_on = true;

// --- Rotation state (0 = 0°, 1 = 180°) ---
static uint8_t rotation_180 = 0;

// Время последней активности
static uint32_t last_activity_time = 0;

// Для обработки удержания кнопок
static uint32_t button_press_time = 0;
static bool button_held = false;

// Состояние подтверждения
static bool confirm_active = false;
static int confirm_selection = 0; // 0 = Yes, 1 = No

// Внешний сетевой интерфейс
extern struct netif gnetif;

// Объявления static функций
static void update_activity_time(void);
static void change_edit_value(int delta);
static void OLED_Draw_Edit(void);
static void OLED_Draw_Confirm(void);
static void DHCP_Apply(void);
static bool OLED_Confirm(const char *msg);
static void OLED_Draw_Rotation(void);
static void OLED_Draw_DHCP(void);

// Инициализация меню
void OLED_Settings_Init(void)
{
    selected_index = 0;
    editing_active = false;
    confirm_active = false;
    settings_active = true;
    last_activity_time = HAL_GetTick();
    OLED_Settings_Draw();
}

// --- Отображение меню подтверждения ---
static void OLED_Draw_Confirm(void)
{
    ssd1306_Fill(Black);
    const int SW = SSD1306_ROTATED_WIDTH;

    // Заголовок
    ssd1306_SetCursor(0, 2);
    ssd1306_WriteString("Confirm", *menu_font, White);

    ssd1306_SetCursor(0, 12);
    ssd1306_WriteString("settings", *menu_font, White);

    // IP адрес в две строки
    ssd1306_SetCursor(0, 22);
    ssd1306_WriteString("IP:", *menu_font, White);

    // Первая строка IP: 192.168.
    char ip_str1[16];
    snprintf(ip_str1, sizeof(ip_str1), "%d.%d.", last_ip[0], last_ip[1]);
    ssd1306_SetCursor(10, 32);
    ssd1306_WriteString(ip_str1, *menu_font, White);

    // Вторая строка: 1.178
    char ip_str2[16];
    snprintf(ip_str2, sizeof(ip_str2), "%d.%d", last_ip[2], last_ip[3]);
    ssd1306_SetCursor(10, 42);
    ssd1306_WriteString(ip_str2, *menu_font, White);

    // Опции Yes/No
    if(confirm_selection == 0) {
        // Yes выделено (слева)
        ssd1306_FillRect(0, 55, 25, menu_font->height + 2, White);
        ssd1306_SetCursor(2, 57);
        ssd1306_WriteString("Yes", *menu_font, Black);

        ssd1306_SetCursor(30, 57);
        ssd1306_WriteString("No", *menu_font, White);
    } else {
        // No выделено (справа)
        ssd1306_SetCursor(0, 57);
        ssd1306_WriteString("Yes", *menu_font, White);

        ssd1306_FillRect(30, 55, 25, menu_font->height + 2, White);
        ssd1306_SetCursor(32, 57);
        ssd1306_WriteString("No", *menu_font, Black);
    }

    ssd1306_UpdateScreen();
}

// --- Применение сетевых настроек в LwIP ---
void Apply_Network_Settings(void)
{
    ip_addr_t ip_addr, netmask, gw;

    IP4_ADDR(&ip_addr, last_ip[0], last_ip[1], last_ip[2], last_ip[3]);
    IP4_ADDR(&netmask, last_mask[0], last_mask[1], last_mask[2], last_mask[3]);
    IP4_ADDR(&gw, last_gw[0], last_gw[1], last_gw[2], last_gw[3]);

    netif_set_addr(&gnetif, &ip_addr, &netmask, &gw);

    printf("Network settings applied:\n");
    printf("IP: %d.%d.%d.%d\n", last_ip[0], last_ip[1], last_ip[2], last_ip[3]);
    printf("Mask: %d.%d.%d.%d\n", last_mask[0], last_mask[1], last_mask[2], last_mask[3]);
    printf("GW: %d.%d.%d.%d\n", last_gw[0], last_gw[1], last_gw[2], last_gw[3]);

    /* --- сохраняем в backup-регистры --- */
    IP4_ADDR(&new_ip, last_ip[0], last_ip[1], last_ip[2], last_ip[3]);
    IP4_ADDR(&new_mask, last_mask[0], last_mask[1], last_mask[2], last_mask[3]);
    IP4_ADDR(&new_gw, last_gw[0], last_gw[1], last_gw[2], last_gw[3]);
    new_dhcp_enabled = dhcp_on ? 1 : 0;
    apply_network_settings = 1;  // чтобы main тоже увидел изменение

    /* Сохраняем в backup (используем текущие SNMP community из main) */
    extern char snmp_read[32], snmp_write[32], snmp_trap[32];
    Settings_Save_To_Backup(new_ip, new_mask, new_gw, new_dhcp_enabled,
                            snmp_read, snmp_write, snmp_trap);
}


// --- Применение DHCP ---
static void DHCP_Apply()
{
    if (dhcp_on)
    {
        dhcp_start(&gnetif);
        printf("DHCP enabled\n");
    }
    else
    {
        dhcp_stop(&gnetif);
        Apply_Network_Settings();
        printf("DHCP disabled, static IP applied\n");
    }

    /* --- сохраняем флаг DHCP в backup --- */
    IP4_ADDR(&new_ip, last_ip[0], last_ip[1], last_ip[2], last_ip[3]);
    IP4_ADDR(&new_mask, last_mask[0], last_mask[1], last_mask[2], last_mask[3]);
    IP4_ADDR(&new_gw, last_gw[0], last_gw[1], last_gw[2], last_gw[3]);
    new_dhcp_enabled = dhcp_on ? 1 : 0;
    apply_network_settings = 1;

    extern char snmp_read[32], snmp_write[32], snmp_trap[32];
    Settings_Save_To_Backup(new_ip, new_mask, new_gw, new_dhcp_enabled,
                            snmp_read, snmp_write, snmp_trap);
}


// --- Подтверждение действий ---
static bool OLED_Confirm(const char *msg)
{
    ssd1306_Fill(Black);
    ssd1306_SetCursor(0, 0);
    ssd1306_WriteString((char*)msg, *menu_font, White);
    ssd1306_SetCursor(0, 16);
    ssd1306_WriteString("Ср=ОК", *menu_font, White);
    ssd1306_SetCursor(0, 26);
    ssd1306_WriteString("Л/П=Отмена", *menu_font, White);
    ssd1306_UpdateScreen();

    // Ждём нажатия
    while(1)
    {
        update_activity_time();

        if(HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_2) == GPIO_PIN_RESET)
            return true;
        if(HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_1) == GPIO_PIN_RESET)
            return false;
        if(HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_3) == GPIO_PIN_RESET)
            return false;

        // Добавляем небольшую задержку для уменьшения нагрузки на CPU
        HAL_Delay(50);
    }
}

// Рисуем меню
void OLED_Settings_Draw(void)
{
    if(confirm_active) {
        OLED_Draw_Confirm();
        return;
    }

    ssd1306_Fill(Black);
    const int SW = SSD1306_ROTATED_WIDTH;

    // Заголовок
    const char title[] = "Settings";
    int title_x = (SW / 2) - ((int)strlen(title) * menu_font->width / 2);
    if(title_x < 0) title_x = 0;
    ssd1306_SetCursor(title_x, 2);
    ssd1306_WriteString((char*)title, *menu_font, White);

    // Список пунктов
    int y = 16;
    for(int i = 0; i < MENU_ITEMS_COUNT; i++)
    {
        if(i == selected_index)
        {
            ssd1306_FillRect(0, y - 1, SW, menu_font->height + 2, White);
            ssd1306_SetCursor(2, y);
            ssd1306_WriteString((char*)menu_items[i], *menu_font, Black);
        }
        else
        {
            ssd1306_SetCursor(2, y);
            ssd1306_WriteString((char*)menu_items[i], *menu_font, White);
        }
        y += menu_font->height + vpad;
    }

    ssd1306_UpdateScreen();
}

// --- Отображение IP/Mask/GW при редактировании ---
static void OLED_Draw_Edit()
{
    ssd1306_Fill(Black);
    const int SW = SSD1306_ROTATED_WIDTH;
    const int SH = SSD1306_ROTATED_HEIGHT;

    // Заголовок по центру
    int title_x = (SW / 2) - ((int)strlen(edit_title) * menu_font->width / 2);
    if(title_x < 0) title_x = 0;
    ssd1306_SetCursor(title_x, 2);
    ssd1306_WriteString(edit_title, *menu_font, White);

    // Отображение IP адреса в столбик по центру
    int start_y = 15;
    int part_spacing = edit_font->height + 2;

    for(int i = 0; i < 4; i++)
    {
        char part_str[4];
        snprintf(part_str, sizeof(part_str), "%d", edit_parts[i]);

        // Центрирование по горизонтали
        int part_width = strlen(part_str) * edit_font->width;
        int part_x = (SW / 2) - (part_width / 2);
        if(part_x < 0) part_x = 0;

        // Подсветка текущей части
        if(i == edit_digit)
        {
            ssd1306_FillRect(part_x - 2, start_y - 1, part_width + 4, edit_font->height + 2, White);
            ssd1306_SetCursor(part_x, start_y);
            ssd1306_WriteString(part_str, *edit_font, Black);
        }
        else
        {
            ssd1306_SetCursor(part_x, start_y);
            ssd1306_WriteString(part_str, *edit_font, White);
        }

        start_y += part_spacing;
    }

    // Инструкция внизу
    ssd1306_SetCursor(0, SH - 30);
    ssd1306_WriteString("Up/Dn:", *menu_font, White);
    ssd1306_SetCursor(0, SH - 20);
    ssd1306_WriteString("Change", *menu_font, White);
    ssd1306_SetCursor(0, SH - 10);
    ssd1306_WriteString("Mid: Next", *menu_font, White);

    ssd1306_UpdateScreen();
}

// Обновление времени активности
static void update_activity_time(void)
{
    last_activity_time = HAL_GetTick();
}

// Изменение значения с учетом удержания
static void change_edit_value(int delta)
{
    uint32_t now = HAL_GetTick();
    static uint32_t last_change_time = 0;
    static uint32_t change_delay = 300;

    if(!button_held)
    {
        edit_parts[edit_digit] += delta;
        if(edit_parts[edit_digit] > 255) edit_parts[edit_digit] = 255;
        if(edit_parts[edit_digit] < 0) edit_parts[edit_digit] = 0;
        OLED_Draw_Edit();
        return;
    }

    if(now - last_change_time > change_delay)
    {
        edit_parts[edit_digit] += delta;
        if(edit_parts[edit_digit] > 255) edit_parts[edit_digit] = 255;
        if(edit_parts[edit_digit] < 0) edit_parts[edit_digit] = 0;

        if(change_delay > 50) change_delay -= 10;

        last_change_time = now;
        OLED_Draw_Edit();
    }
}

// --- Навигация меню ---
void OLED_Settings_MoveUp(void)
{
    if(!settings_active) return;

    update_activity_time();

    if(confirm_active) {
        confirm_selection = 0; // Yes (слева)
        OLED_Draw_Confirm();
        return;
    }

    if(editing_active)
    {
        change_edit_value(1);
        return;
    }

    if(selected_index > 0)
        selected_index--;
    else
        selected_index = MENU_ITEMS_COUNT - 1;

    OLED_Settings_Draw();
}

void OLED_Settings_MoveDown(void)
{
    if(!settings_active) return;

    update_activity_time();

    if(confirm_active) {
        confirm_selection = 1; // No (справа)
        OLED_Draw_Confirm();
        return;
    }

    if(editing_active)
    {
        change_edit_value(-1);
        return;
    }

    if(selected_index < MENU_ITEMS_COUNT - 1)
        selected_index++;
    else
        selected_index = 0;

    OLED_Settings_Draw();
}
// --- Выбор пункта меню ---
void OLED_Settings_Select(void)
{
    if(!settings_active) return;

    update_activity_time();

    if(confirm_active) {
        if(confirm_selection == 0) {
            // Yes - применяем настройки
            Apply_Network_Settings();
        }
        confirm_active = false;
        OLED_Settings_Draw();
        return;
    }

    if(editing_active)
    {
        edit_digit++;
        if(edit_digit > 3)
        {
            editing_active = false;

            // Сохраняем изменения и показываем подтверждение
            switch(selected_index)
            {
                case 0:
                    memcpy(last_ip, edit_parts, 4);
                    break;
                case 1:
                    memcpy(last_mask, edit_parts, 4);
                    break;
                case 2:
                    memcpy(last_gw, edit_parts, 4);
                    break;
            }

            confirm_active = true;
            confirm_selection = 0;
            OLED_Draw_Confirm();
        }
        else
            OLED_Draw_Edit();
        return;
    }

    switch(selected_index)
    {
        case 0: // IP
            editing_active = true;
            edit_digit = 0;
            strcpy(edit_title, "Set IP");
            memcpy(edit_parts, last_ip, 4);
            OLED_Draw_Edit();  // ПЕРЕХОДИМ В РЕЖИМ РЕДАКТИРОВАНИЯ
            break;

        case 1: // Mask
            editing_active = true;
            edit_digit = 0;
            strcpy(edit_title, "Set Mask");
            memcpy(edit_parts, last_mask, 4);
            OLED_Draw_Edit();  // ПЕРЕХОДИМ В РЕЖИМ РЕДАКТИРОВАНИЯ
            break;

        case 2: // Gateway
            editing_active = true;
            edit_digit = 0;
            strcpy(edit_title, "Set GW");
            memcpy(edit_parts, last_gw, 4);
            OLED_Draw_Edit();  // ПЕРЕХОДИМ В РЕЖИМ РЕДАКТИРОВАНИЯ
            break;

        case 3: // DHCP
            OLED_Draw_DHCP();
            // Wait for selection
            while (1) {
                // left/right to toggle, middle to select
                if (HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_1) == GPIO_PIN_RESET || HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_3) == GPIO_PIN_RESET) {
                    dhcp_on = !dhcp_on;
                    OLED_Draw_DHCP();
                    HAL_Delay(200);
                }
                if (HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_2) == GPIO_PIN_RESET) {
                    DHCP_Apply();
                    break;
                }
                HAL_Delay(50);
            }
            OLED_Settings_Draw();
            break;

        case 4: // Reset (reboot)
            if(OLED_Confirm("Reset: программная перезагрузка")) {
                NVIC_SystemReset();
            } else {
                OLED_Settings_Draw();
            }
            break;

        case 5: // Reboot to defaults (Factory)
            if(OLED_Confirm("Reboot: заводские настройки и перезапуск"))
            {
                // IP: 192.168.0.254, Mask: 255.255.255.0, GW: 192.168.0.1, DHCP=0
                last_ip[0] = 192; last_ip[1] = 168; last_ip[2] = 0; last_ip[3] = 254;
                last_mask[0] = 255; last_mask[1] = 255; last_mask[2] = 255; last_mask[3] = 0;
                last_gw[0] = 192; last_gw[1] = 168; last_gw[2] = 0; last_gw[3] = 1;
                dhcp_on = false;
                // Сброс времени и даты в нули
                RTC_TimeTypeDef t = {0};
                RTC_DateTypeDef d = {0};
                // Время 00:00:00
                t.Hours = 0; t.Minutes = 0; t.Seconds = 0;
                t.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
                t.StoreOperation = RTC_STOREOPERATION_RESET;
                HAL_RTC_SetTime(&hrtc, &t, RTC_FORMAT_BIN);
                // Дата: 01-01-00 (минимально валидная в HAL; "нули" по году)
                d.Year = 0; d.Month = RTC_MONTH_JANUARY; d.Date = 1; d.WeekDay = RTC_WEEKDAY_MONDAY;
                HAL_RTC_SetDate(&hrtc, &d, RTC_FORMAT_BIN);

                Apply_Network_Settings();

                // Немедленный перезапуск
                NVIC_SystemReset();
            }
            OLED_Settings_Draw();
            break;

        case 6: // Display rotation
            OLED_Draw_Rotation();
            // Wait for selection
            while (1) {
                if (HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_1) == GPIO_PIN_RESET) {
                    rotation_180 = 0;
                    OLED_Draw_Rotation();
                    HAL_Delay(200);
                } else if (HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_3) == GPIO_PIN_RESET) {
                    rotation_180 = 1;
                    OLED_Draw_Rotation();
                    HAL_Delay(200);
                }
                if (HAL_GPIO_ReadPin(GPIOD, GPIO_PIN_2) == GPIO_PIN_RESET) {
                    ssd1306_SetRotation180(rotation_180);
                    OLED_Settings_Draw();
                    break;
                }
                HAL_Delay(50);
            }
            break;
    }
}

// Выход из меню настроек
void OLED_Settings_Exit(void)
{
    settings_active = false;
    editing_active = false;
    confirm_active = false;
}

// Проверка таймаута бездействия
void OLED_Settings_TimeoutCheck(void)
{
    if (!settings_active) return;

    uint32_t now = HAL_GetTick();

    // Таймаут работает ТОЛЬКО если мы не в режиме редактирования и не в подтверждении
    if (!editing_active && !confirm_active && (now - last_activity_time) >= 10000)
    {
        OLED_Settings_Exit();
    }
}

void OLED_Settings_ButtonHeld(bool held)
{
    button_held = held;
    if(!held) button_press_time = 0;
}

void OLED_UpdateDisplay(void)
{
    static uint32_t last_display_update = 0;
    uint32_t now = HAL_GetTick();

    if (now - last_display_update < 500) {
        return;
    }
    last_display_update = now;

    if (settings_active) {
        if (confirm_active) {
            OLED_Draw_Confirm();
        }
        else if (editing_active) {
            OLED_Draw_Edit();
        }
        else {
            OLED_Settings_Draw();
        }
    }
    else {
        OLED_ShowCurrentPage();
    }
}

// --- Рисование экрана выбора поворота ---
static void OLED_Draw_Rotation(void)
{
    ssd1306_Fill(Black);
    const int SW = SSD1306_ROTATED_WIDTH;
    const int SH = SSD1306_ROTATED_HEIGHT;

    const char *title = "Rotation";
    int title_x = (SW / 2) - ((int)strlen(title) * menu_font->width / 2);
    if (title_x < 0) title_x = 0;
    ssd1306_SetCursor(title_x, 2);
    ssd1306_WriteString((char*)title, *menu_font, White);

    // Options: 0° and 180°
    const char *opt0 = "0°";
    const char *opt180 = "180°";
    int y0 = 24;
    int y1 = y0 + menu_font->height + vpad;

    // Highlight current selection
    if (rotation_180 == 0) {
        ssd1306_FillRect(0, y0 - 1, SW, menu_font->height + 2, White);
        ssd1306_SetCursor(2, y0);
        ssd1306_WriteString((char*)opt0, *menu_font, Black);
        ssd1306_SetCursor(2, y1);
        ssd1306_WriteString((char*)opt180, *menu_font, White);
    } else {
        ssd1306_SetCursor(2, y0);
        ssd1306_WriteString((char*)opt0, *menu_font, White);
        ssd1306_FillRect(0, y1 - 1, SW, menu_font->height + 2, White);
        ssd1306_SetCursor(2, y1);
        ssd1306_WriteString((char*)opt180, *menu_font, Black);
    }

    // Instruction
    ssd1306_SetCursor(0, SH - 20);
    ssd1306_WriteString("Л/П: выбрать", *menu_font, White);
    ssd1306_SetCursor(0, SH - 10);
    ssd1306_WriteString("Ср: применить", *menu_font, White);

    ssd1306_UpdateScreen();
}

// --- Экран выбора DHCP ---
static void OLED_Draw_DHCP(void)
{
    ssd1306_Fill(Black);
    const int SW = SSD1306_ROTATED_WIDTH;
    const int SH = SSD1306_ROTATED_HEIGHT;

    const char *title = "DHCP";
    int title_x = (SW / 2) - ((int)strlen(title) * menu_font->width / 2);
    if (title_x < 0) title_x = 0;
    ssd1306_SetCursor(title_x, 2);
    ssd1306_WriteString((char*)title, *menu_font, White);

    const char *opt_on = "Включен";
    const char *opt_off = "Отключен";
    int y0 = 24;
    int y1 = y0 + menu_font->height + vpad;

    if (dhcp_on) {
        ssd1306_FillRect(0, y0 - 1, SW, menu_font->height + 2, White);
        ssd1306_SetCursor(2, y0);
        ssd1306_WriteString((char*)opt_on, *menu_font, Black);
        ssd1306_SetCursor(2, y1);
        ssd1306_WriteString((char*)opt_off, *menu_font, White);
    } else {
        ssd1306_SetCursor(2, y0);
        ssd1306_WriteString((char*)opt_on, *menu_font, White);
        ssd1306_FillRect(0, y1 - 1, SW, menu_font->height + 2, White);
        ssd1306_SetCursor(2, y1);
        ssd1306_WriteString((char*)opt_off, *menu_font, Black);
    }

    ssd1306_SetCursor(0, SH - 20);
    ssd1306_WriteString("Л/П: переключить", *menu_font, White);
    ssd1306_SetCursor(0, SH - 10);
    ssd1306_WriteString("Ср: применить", *menu_font, White);

    ssd1306_UpdateScreen();
}
