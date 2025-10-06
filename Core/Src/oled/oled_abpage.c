#include "oled/oled_abpage.h"
#include <stdio.h>
#include <math.h>
#include "ssd1306_driver/ssd1306_fonts.h"
#include "signal_processor.h"

#ifndef ABPAGE_VOLTAGE_THRESHOLD
#define ABPAGE_VOLTAGE_THRESHOLD 20.0f
#endif

#define SSD1306_ROTATED_WIDTH  (SSD1306_HEIGHT)
#define SSD1306_ROTATED_HEIGHT (SSD1306_WIDTH)

// Внешние переменные (определяются в signal_processor.c)
extern float voltage1;
extern float voltage2;
extern float current;

/* --- Вспомогательные функции --- */

/* Рисует крест поверх круга */
static void draw_cross(int cx, int cy, int r) {
    int d = (int)(r * 0.65f);
    ssd1306_Line(cx - d, cy - d, cx + d, cy + d, White);
    ssd1306_Line(cx - d, cy + d, cx + d, cy - d, White);
}

/* Центрированный символ */
static void draw_centered_char(int cx, int cy, char ch, const SSD1306_Font_t *font) {
    char s[2] = { ch, '\0' };
    int fw = font->width;
    int fh = font->height;
    int x = cx - fw / 2;
    int y = cy - fh / 2;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    ssd1306_SetCursor((uint8_t)x, (uint8_t)y);
    ssd1306_WriteString(s, *font, White);
}

/* Кружок с буквой. Если нет входа — крест. Если активный — двойной круг */
static void draw_input_circle(int cx, int cy, char label, uint8_t hasInput, uint8_t isActive) {
    const int r = 12;
    ssd1306_DrawCircle((uint8_t)cx, (uint8_t)cy, (uint8_t)r, White);

    // буква всегда рисуется
    draw_centered_char(cx, cy, label, &Font_11x18);

    if (!hasInput) {
        draw_cross(cx, cy, r);
    } else if (isActive) {
        ssd1306_DrawCircle((uint8_t)cx, (uint8_t)cy, (uint8_t)(r + 3), White);
    }
}

/* --- Главная функция --- */
void OLED_DrawABPage(void) {
    ssd1306_Fill(Black);

    const int SW = SSD1306_ROTATED_WIDTH;   // 64
    const int SH = SSD1306_ROTATED_HEIGHT;  // 128

    // Заголовок сверху - БОЛЬШИМИ БУКВАМИ ДЛЯ ТЕСТА
    const char title[] = "BONCH-ATS";
    const SSD1306_Font_t *title_font = &Font_7x10;
    int title_x = (SW / 2) - ((int)strlen(title) * title_font->width / 2);
    if (title_x < 0) title_x = 0;
    ssd1306_SetCursor(title_x, 2);
    ssd1306_WriteString((char*)title, *title_font, White);
    int title_bottom = 2 + title_font->height;

    // Нижний блок (U/I/P)
    const SSD1306_Font_t *fval = &Font_7x10;
    const int fh = fval->height;   // ~10
    const int vpad = 4;
    const int box_x = 2;
    const int lines = 3;
    const int box_h = (fh * lines) + (vpad * (lines + 1));
    const int box_w = SW - 4;
    const int box_y = SH - box_h - 4;
    int data_top = box_y;

    // Параметры входов
    uint8_t hasA = (voltage1 > ABPAGE_VOLTAGE_THRESHOLD);
    uint8_t hasB = (voltage2 > ABPAGE_VOLTAGE_THRESHOLD);
    uint8_t activeA = 0, activeB = 0;
    if (hasA && hasB) activeA = 1;     // A приоритет
    else if (hasA) activeA = 1;
    else if (hasB) activeB = 1;

    // Кружки A и B — по центру между заголовком и блоком данных
    // ЯВНО СДВИГАЕМ ВНИЗ НА 10 ПИКСЕЛЕЙ ДЛЯ ТЕСТА
    int center_y = title_bottom + 10 + ((data_top - title_bottom - 10) / 2);

    const int ax = SW / 4;
    const int bx = (SW * 3) / 4;

    draw_input_circle(ax, center_y, 'A', hasA, activeA);
    draw_input_circle(bx, center_y, 'B', hasB, activeB);

    // Рисуем нижний блок
    ssd1306_DrawRectangle(box_x, box_y, box_x + box_w, box_y + box_h, White);

    // Значения
    float u_work = 0.0f;
    if (activeA) u_work = voltage1;
    else if (activeB) u_work = voltage2;

    // Округление мощности до целых - ТЕПЕРЬ ТОЧНО БЕЗ ДРОБНОЙ ЧАСТИ
    int power_int = (int)(current * u_work);
    if (current * u_work - power_int >= 0.5) power_int++; // Правильное округление

    char buf[32];
    int text_x = box_x + 4;
    int y = box_y + vpad;

    // ИЗМЕНИЛ ФОРМАТ ДЛЯ ТЕСТА - БОЛЬШИЕ БУКВЫ
    snprintf(buf, sizeof(buf), "U=%.1fV", u_work);
    ssd1306_SetCursor(text_x, y);
    ssd1306_WriteString(buf, *fval, White);

    y += fh + vpad;
    snprintf(buf, sizeof(buf), "I=%.1f A", current);
    ssd1306_SetCursor(text_x, y);
    ssd1306_WriteString(buf, *fval, White);

    y += fh + vpad;
    // ЯВНОЕ ИЗМЕНЕНИЕ - ЦЕЛОЕ ЧИСЛО БЕЗ ТОЧКИ
    snprintf(buf, sizeof(buf), "P=%d W", power_int);
    ssd1306_SetCursor(text_x, y);
    ssd1306_WriteString(buf, *fval, White);

    ssd1306_UpdateScreen();
}
