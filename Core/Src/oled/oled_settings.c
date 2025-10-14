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
    "Reboot",
    "Reset",
    "Set rot."
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

// --- Подменю (DHCP / Rotation) ---
typedef enum { SUBMENU_NONE = 0, SUBMENU_DHCP, SUBMENU_ROTATION } SubmenuType;
static SubmenuType submenu_type = SUBMENU_NONE;
static bool submenu_active = false;
static int submenu_index = 0; // 0/1
static void OLED_Draw_Submenu(void);
static void Sync_From_Netif(void);
static void OLED_Draw_YesNo(void);

// --- Тип подтверждения действий ---
typedef enum {
    CONFIRM_NONE = 0,
    CONFIRM_APPLY_IP,
    CONFIRM_APPLY_MASK,
    CONFIRM_APPLY_GW,
    CONFIRM_DHCP_ENABLE,
    CONFIRM_DHCP_DISABLE,
    CONFIRM_RESET_MCU,
    CONFIRM_FACTORY_RESET
} ConfirmType;
static ConfirmType confirm_type = CONFIRM_NONE;
// Pending values to apply after confirmation

// Инициализация меню
void OLED_Settings_Init(void)
{
    selected_index = 0;
    editing_active = false;
    confirm_active = false;
    settings_active = true;
    last_activity_time = HAL_GetTick();
    submenu_active = false;
    submenu_type = SUBMENU_NONE;
    // Подтягиваем актуальные IP/Mask/GW из сетевого интерфейса
    Sync_From_Netif();
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

    // Подзаголовок и содержимое в зависимости от типа подтверждения
    char line[16];
    switch (confirm_type) {
        case CONFIRM_APPLY_IP: {
            ssd1306_SetCursor(0, 14);
            ssd1306_WriteString("Apply IP", *menu_font, White);

            // Показ значения IP в две строки (192.168. / x.y)
            snprintf(line, sizeof(line), "%d.%d.", last_ip[0], last_ip[1]);
            ssd1306_SetCursor(0, 28);
            ssd1306_WriteString(line, *menu_font, White);
            snprintf(line, sizeof(line), "%d.%d", last_ip[2], last_ip[3]);
            ssd1306_SetCursor(0, 40);
            ssd1306_WriteString(line, *menu_font, White);
            break;
        }
        case CONFIRM_APPLY_MASK: {
            ssd1306_SetCursor(0, 14);
            ssd1306_WriteString("Apply msk", *menu_font, White);

            snprintf(line, sizeof(line), "%d.%d.", last_mask[0], last_mask[1]);
            ssd1306_SetCursor(0, 28);
            ssd1306_WriteString(line, *menu_font, White);
            snprintf(line, sizeof(line), "%d.%d", last_mask[2], last_mask[3]);
            ssd1306_SetCursor(0, 40);
            ssd1306_WriteString(line, *menu_font, White);
            break;
        }
        case CONFIRM_APPLY_GW: {
            ssd1306_SetCursor(0, 14);
            ssd1306_WriteString("Apply GW", *menu_font, White);

            snprintf(line, sizeof(line), "%d.%d.", last_gw[0], last_gw[1]);
            ssd1306_SetCursor(0, 28);
            ssd1306_WriteString(line, *menu_font, White);
            snprintf(line, sizeof(line), "%d.%d", last_gw[2], last_gw[3]);
            ssd1306_SetCursor(0, 40);
            ssd1306_WriteString(line, *menu_font, White);
            break;
        }
        case CONFIRM_DHCP_ENABLE: {
            ssd1306_SetCursor(0, 14);
            ssd1306_WriteString("DHCP on", *menu_font, White);
            break;
        }
        case CONFIRM_DHCP_DISABLE: {
            ssd1306_SetCursor(0, 14);
            ssd1306_WriteString("DHCP off", *menu_font, White);
            break;
        }
        case CONFIRM_RESET_MCU: {
            ssd1306_SetCursor(0, 14);
            ssd1306_WriteString("MCU reset", *menu_font, White);
            ssd1306_SetCursor(0, 28);
            ssd1306_WriteString("Just reboot", *menu_font, White);
            break;
        }
        case CONFIRM_FACTORY_RESET: {
            ssd1306_SetCursor(0, 14);
            ssd1306_WriteString("Reset to", *menu_font, White);
            ssd1306_SetCursor(0, 28);
            ssd1306_WriteString("factory settings", *menu_font, White);
            break;
        }
        default:
            break;
    }

    OLED_Draw_YesNo();

    ssd1306_UpdateScreen();
}

// --- Рисование подменю (2 варианта) ---
static void OLED_Draw_Submenu(void)
{
    ssd1306_Fill(Black);
    const int SW = SSD1306_ROTATED_WIDTH;

    const char *title = (submenu_type == SUBMENU_DHCP) ? "DHCP" : "Rotation";
    int title_x = (SW / 2) - ((int)strlen(title) * menu_font->width / 2);
    if (title_x < 0) title_x = 0;
    ssd1306_SetCursor(title_x, 2);
    ssd1306_WriteString((char*)title, *menu_font, White);

    const char *opt0 = (submenu_type == SUBMENU_DHCP) ? "Enable" : "0 deg";
    const char *opt1 = (submenu_type == SUBMENU_DHCP) ? "Disable" : "180 deg";

    int y = 20;
    for (int i = 0; i < 2; i++) {
        const char *label = (i == 0) ? opt0 : opt1;
        if (i == submenu_index) {
            ssd1306_FillRect(0, y - 1, SW, menu_font->height + 2, White);
            ssd1306_SetCursor(2, y);
            ssd1306_WriteString((char*)label, *menu_font, Black);
        } else {
            ssd1306_SetCursor(2, y);
            ssd1306_WriteString((char*)label, *menu_font, White);
        }
        y += menu_font->height + vpad;
    }

    // Подписи управления: боковые = Yes/No, средняя — Select
    ssd1306_SetCursor(0, 52);
    ssd1306_WriteString("Left/Right: Yes/No", *menu_font, White);
    ssd1306_SetCursor(0, 52 + menu_font->height);
    ssd1306_WriteString("Mid - Select", *menu_font, White);

    ssd1306_UpdateScreen();
}

// Синхронизация last_ip/mask/gw из текущего состояния netif
static void Sync_From_Netif(void)
{
    const ip4_addr_t *a_ip = netif_ip4_addr(&gnetif);
    const ip4_addr_t *a_mask = netif_ip4_netmask(&gnetif);
    const ip4_addr_t *a_gw = netif_ip4_gw(&gnetif);

    if (a_ip && a_ip->addr != 0) {
        last_ip[0] = ip4_addr1_16(a_ip);
        last_ip[1] = ip4_addr2_16(a_ip);
        last_ip[2] = ip4_addr3_16(a_ip);
        last_ip[3] = ip4_addr4_16(a_ip);
    }
    if (a_mask && a_mask->addr != 0) {
        last_mask[0] = ip4_addr1_16(a_mask);
        last_mask[1] = ip4_addr2_16(a_mask);
        last_mask[2] = ip4_addr3_16(a_mask);
        last_mask[3] = ip4_addr4_16(a_mask);
    }
    if (a_gw && a_gw->addr != 0) {
        last_gw[0] = ip4_addr1_16(a_gw);
        last_gw[1] = ip4_addr2_16(a_gw);
        last_gw[2] = ip4_addr3_16(a_gw);
        last_gw[3] = ip4_addr4_16(a_gw);
    }
}

// Общий рендер для кнопок Да/Нет внизу
static void OLED_Draw_YesNo(void)
{
    const uint8_t y = 53;
    const uint8_t h = (uint8_t)(menu_font->height);
    // Убрали рамку, чтобы не перекрывать текст
    if (confirm_selection == 0) {
        // Yes выделено
        ssd1306_FillRect(0, y, 30, h, White);
        ssd1306_SetCursor(2, y);
        ssd1306_WriteString("Yes", *menu_font, Black);

        ssd1306_SetCursor(34, y);
        ssd1306_WriteString("No", *menu_font, White);
    } else {
        ssd1306_SetCursor(2, y);
        ssd1306_WriteString("Yes", *menu_font, White);

        ssd1306_FillRect(32, y, 26, h, White);
        ssd1306_SetCursor(34, y);
        ssd1306_WriteString("No", *menu_font, Black);
    }
}

void OLED_Settings_Back(void)
{
    if (!settings_active) return;
    if (confirm_active) {
        confirm_active = false;
        confirm_type = CONFIRM_NONE;
        OLED_Settings_Draw();
        return;
    }
    if (submenu_active) {
        submenu_active = false;
        submenu_type = SUBMENU_NONE;
        OLED_Settings_Draw();
        return;
    }
    if (editing_active) {
        editing_active = false;
        OLED_Settings_Draw();
        return;
    }
    // Если мы в корневом меню — выходим на главную страницу
    OLED_Settings_Exit();
    OLED_ShowCurrentPage();
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
    ssd1306_WriteString("Middle=OK", *menu_font, White);
    ssd1306_SetCursor(0, 26);
    ssd1306_WriteString("Left/Right=Cancel", *menu_font, White);
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
    if (submenu_active) {
        OLED_Draw_Submenu();
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
    ssd1306_WriteString("Mid- Next", *menu_font, White);

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

    if (submenu_active) {
        submenu_index = 0; // всегда 0/1
        OLED_Draw_Submenu();
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

    if (submenu_active) {
        submenu_index = 1; // всегда 0/1
        OLED_Draw_Submenu();
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
            // Yes - применяем/выполняем действие в зависимости от типа
            switch (confirm_type) {
                case CONFIRM_APPLY_IP:
                case CONFIRM_APPLY_MASK:
                case CONFIRM_APPLY_GW:
                    Apply_Network_Settings();
                    break;
                case CONFIRM_DHCP_ENABLE:
                case CONFIRM_DHCP_DISABLE:
                    DHCP_Apply();
                    break;
                case CONFIRM_RESET_MCU:
                    NVIC_SystemReset();
                    break;
                case CONFIRM_FACTORY_RESET: {
                    // Устанавливаем заводские сетевые настройки
                    last_ip[0] = 192; last_ip[1] = 168; last_ip[2] = 0; last_ip[3] = 254;
                    last_mask[0] = 255; last_mask[1] = 255; last_mask[2] = 255; last_mask[3] = 0;
                    last_gw[0] = 192; last_gw[1] = 168; last_gw[2] = 0; last_gw[3] = 1;
                    dhcp_on = false;
                    Apply_Network_Settings();

                    // Обнуляем RTC
                    RTC_TimeTypeDef t = {0};
                    t.Hours = 0; t.Minutes = 0; t.Seconds = 0;
                    t.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
                    t.StoreOperation = RTC_STOREOPERATION_RESET;
                    HAL_RTC_SetTime(&hrtc, &t, RTC_FORMAT_BIN);
                    RTC_DateTypeDef d = {0};
                    d.Year = 0; d.Month = RTC_MONTH_JANUARY; d.Date = 1; d.WeekDay = RTC_WEEKDAY_MONDAY;
                    HAL_RTC_SetDate(&hrtc, &d, RTC_FORMAT_BIN);

                    // Чистим backup-регистры
                    Settings_Clear_Backup();

                    // Полная перезагрузка
                    NVIC_SystemReset();
                    break;
                }
                default:
                    break;
            }
        }
        confirm_active = false;
        confirm_type = CONFIRM_NONE;
        submenu_active = false;
        submenu_type = SUBMENU_NONE;
        OLED_Settings_Draw();
        return;
    }

    if (submenu_active) {
        if (submenu_type == SUBMENU_DHCP) {
            dhcp_on = (submenu_index == 0);
            // Применяем без подтверждения
            DHCP_Apply();
            submenu_active = false;
            submenu_type = SUBMENU_NONE;
            OLED_Settings_Draw();
        } else if (submenu_type == SUBMENU_ROTATION) {
            uint8_t rot180 = (submenu_index == 1) ? 1 : 0;
            ssd1306_SetRotation180(rot180);
            Settings_Save_Rotation(rot180);
            submenu_active = false;
            submenu_type = SUBMENU_NONE;
            OLED_Settings_Draw();
        }
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
                    confirm_type = CONFIRM_APPLY_IP;
                    break;
                case 1:
                    memcpy(last_mask, edit_parts, 4);
                    confirm_type = CONFIRM_APPLY_MASK;
                    break;
                case 2:
                    memcpy(last_gw, edit_parts, 4);
                    confirm_type = CONFIRM_APPLY_GW;
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
            Sync_From_Netif();
            memcpy(edit_parts, last_ip, 4);
            OLED_Draw_Edit();  // ПЕРЕХОДИМ В РЕЖИМ РЕДАКТИРОВАНИЯ
            break;

        case 1: // Mask
            editing_active = true;
            edit_digit = 0;
            strcpy(edit_title, "Set Mask");
            Sync_From_Netif();
            memcpy(edit_parts, last_mask, 4);
            OLED_Draw_Edit();  // ПЕРЕХОДИМ В РЕЖИМ РЕДАКТИРОВАНИЯ
            break;

        case 2: // Gateway
            editing_active = true;
            edit_digit = 0;
            strcpy(edit_title, "Set GW");
            Sync_From_Netif();
            memcpy(edit_parts, last_gw, 4);
            OLED_Draw_Edit();  // ПЕРЕХОДИМ В РЕЖИМ РЕДАКТИРОВАНИЯ
            break;

        case 3: // DHCP
            submenu_active = true;
            submenu_type = SUBMENU_DHCP;
            submenu_index = dhcp_on ? 0 : 1;
            OLED_Draw_Submenu();
            break;

        case 4: // Reboot -> Factory Reset (с подтверждением в общем диалоге)
            confirm_active = true;
            confirm_selection = 0;
            confirm_type = CONFIRM_FACTORY_RESET;
            OLED_Draw_Confirm();
            break;

        case 5: // Reset -> Программная перезагрузка MCU (с подтверждением)
            confirm_active = true;
            confirm_selection = 0;
            confirm_type = CONFIRM_RESET_MCU;
            OLED_Draw_Confirm();
            break;

        case 6: // Set rotation
            submenu_active = true;
            submenu_type = SUBMENU_ROTATION;
            submenu_index = ssd1306_GetRotation180() ? 1 : 0;
            OLED_Draw_Submenu();
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
        else if (submenu_active) {
            OLED_Draw_Submenu();
        }
        else {
            OLED_Settings_Draw();
        }
    }
    else {
        OLED_ShowCurrentPage();
    }
}
