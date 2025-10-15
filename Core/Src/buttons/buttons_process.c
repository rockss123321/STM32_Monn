#include "buttons/buttons_process.h"
#include "main.h"
#include "oled/oled_abpage.h"
#include "oled/oled_netinfo.h"
#include "oled/oled_settings.h"
#include "lwip/netif.h"
#include <stdint.h>

// Внешний сетевой интерфейс (определён в lwIP)
extern struct netif gnetif;

bool settings_active = false;



// Состояние кнопки 2
static uint32_t btn2_press_time = 0;
static uint8_t btn2_held = 0;


// Добавим глобальную переменную для текущей страницы
int current_page = 0; // 0 = AB page, 1 = NETINFO page

void OLED_ShowCurrentPage(void) {
    ssd1306_Fill(Black);

    switch (current_page) {
        case 0:
            OLED_DrawABPage();
            break;
        case 1:
            OLED_DrawNetInfo(&gnetif, 0, 0);
            break;
        default:
            current_page = 0;
            OLED_DrawABPage();
            break;
    }

    ssd1306_UpdateScreen();
}

void Buttons_Process(void) {
    static uint32_t settings_last_activity = 0;
    static uint32_t btn1_last_action = 0;
    static uint32_t btn3_last_action = 0;
    static uint8_t btn1_held = 0;
    static uint8_t btn3_held = 0;

    uint32_t now = HAL_GetTick();

    // ---- Если меню Settings активно ----
    if (settings_active) {
        int any_pressed = 0;

        // Обработка кнопки 3 (ПРАВАЯ кнопка - ВВЕРХ)
        if (HAL_GPIO_ReadPin(BTN_PORT, BTN3_PIN) == GPIO_PIN_RESET) {
            if (!btn3_held) {
                // Первое нажатие
                btn3_held = 1;
                btn3_last_action = now;
                OLED_Settings_MoveUp();
                any_pressed = 1;
            } else {
                // Удержание - быстрое изменение с ускорением
                uint32_t hold_time = now - btn3_last_action;
                uint32_t delay_ms;

                if (hold_time > 5000) {
                    delay_ms = 20;
                } else if (hold_time > 3000) {
                    delay_ms = 50;
                } else if (hold_time > 1000) {
                    delay_ms = 100;
                } else {
                    delay_ms = 300;
                }

                if (now - btn3_last_action > delay_ms) {
                    OLED_Settings_MoveUp();
                    btn3_last_action = now;
                    any_pressed = 1;
                }
            }
        } else {
            btn3_held = 0;
        }

        // Обработка кнопки 1 (ЛЕВАЯ кнопка - ВНИЗ)
        if (HAL_GPIO_ReadPin(BTN_PORT, BTN1_PIN) == GPIO_PIN_RESET) {
            if (!btn1_held) {
                // Первое нажатие
                btn1_held = 1;
                btn1_last_action = now;
                OLED_Settings_MoveDown();
                any_pressed = 1;
            } else {
                // Удержание - быстрое изменение с ускорением
                uint32_t hold_time = now - btn1_last_action;
                uint32_t delay_ms;

                if (hold_time > 5000) {
                    delay_ms = 20;
                } else if (hold_time > 3000) {
                    delay_ms = 50;
                } else if (hold_time > 1000) {
                    delay_ms = 100;
                } else {
                    delay_ms = 300;
                }

                if (now - btn1_last_action > delay_ms) {
                    OLED_Settings_MoveDown();
                    btn1_last_action = now;
                    any_pressed = 1;
                }
            }
        } else {
            btn1_held = 0;
        }

        // Обработка кнопки 2 (Выбор / Долгое нажатие = Назад)
        if (HAL_GPIO_ReadPin(BTN_PORT, BTN2_PIN) == GPIO_PIN_RESET) {
            // фиксируем момент первого нажатия (debounce)
            if (btn2_press_time == 0) {
                btn2_press_time = now;
            }
            HAL_Delay(20);
            if (HAL_GPIO_ReadPin(BTN_PORT, BTN2_PIN) == GPIO_PIN_RESET) {
                uint32_t held_ms = now - btn2_press_time;
                if (held_ms >= 1000) {
                    // долгое нажатие → назад
                    OLED_Settings_Back();
                    // ждём отпускания
                    while (HAL_GPIO_ReadPin(BTN_PORT, BTN2_PIN) == GPIO_PIN_RESET) HAL_Delay(10);
                    btn2_press_time = 0;
                    any_pressed = 1;
                }
            }
        } else {
            if (btn2_press_time != 0) {
                // короткое нажатие → Select
                uint32_t press_ms = HAL_GetTick() - btn2_press_time;
                if (press_ms >= 50 && press_ms < 1000) {
                    OLED_Settings_Select();
                    any_pressed = 1;
                }
                btn2_press_time = 0;
            }
        }

        // Обновляем таймер последней активности
        if (any_pressed) settings_last_activity = now;

        // Если прошло 10 секунд без действий → выход из настроек
        if ((now - settings_last_activity) >= 10000) {
            settings_active = 0;
            OLED_DrawABPage();
        }

        return;
    }

    // ---- Режим обычного переключения страниц ----
    if (HAL_GPIO_ReadPin(BTN_PORT, BTN3_PIN) == GPIO_PIN_RESET) {  // Правая кнопка = предыдущая страница
        HAL_Delay(200);
        if (HAL_GPIO_ReadPin(BTN_PORT, BTN3_PIN) == GPIO_PIN_RESET) {
            current_page--;
            if (current_page < 0) current_page = 1;
            OLED_ShowCurrentPage();
            while (HAL_GPIO_ReadPin(BTN_PORT, BTN3_PIN) == GPIO_PIN_RESET);
        }
    }

    if (HAL_GPIO_ReadPin(BTN_PORT, BTN1_PIN) == GPIO_PIN_RESET) {  // Левая кнопка = следующая страница
        HAL_Delay(200);
        if (HAL_GPIO_ReadPin(BTN_PORT, BTN1_PIN) == GPIO_PIN_RESET) {
            current_page++;
            if (current_page > 1) current_page = 0;
            OLED_ShowCurrentPage();
            while (HAL_GPIO_ReadPin(BTN_PORT, BTN1_PIN) == GPIO_PIN_RESET);
        }
    }

    // ---- Кнопка 2 (Settings / RESET) ----
    if (HAL_GPIO_ReadPin(BTN_PORT, BTN2_PIN) == GPIO_PIN_RESET) {
        if (!btn2_held) {
            btn2_held = 1;
            btn2_press_time = HAL_GetTick();
        } else {
            uint32_t held_time = (HAL_GetTick() - btn2_press_time) / 1000;

            if (held_time >= 60) {
                // HARD RESET через IWDG
                IWDG->KR = 0x5555;
                IWDG->PR = 0;
                IWDG->RLR = 10;
                IWDG->KR = 0xAAAA;
                IWDG->KR = 0xCCCC;
            } else if (held_time >= 30) {
                // SOFT RESET
                NVIC_SystemReset();
            }
        }
    } else {
        if (btn2_held) {
            uint32_t press_time = (HAL_GetTick() - btn2_press_time);
            if (press_time >= 50 && press_time < 1000) {
                // Короткое нажатие → открыть/закрыть Settings
                if (!settings_active) {
                    settings_active = 1;
                    OLED_Settings_Init();
                    settings_last_activity = HAL_GetTick();
                } else {
                    settings_active = 0;
                    OLED_DrawABPage();
                }
            }
            // Сбросим время нажатия, чтобы избежать двойной обработки при входе в Settings
            btn2_press_time = 0;
        }
        btn2_held = 0;
    }
}
