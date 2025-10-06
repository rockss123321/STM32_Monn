#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdint.h>
#include "main.h"

typedef enum {
    BTN_A,  // Теперь правая кнопка (GPIO_PIN_3)
    BTN_B,  // Средняя кнопка (GPIO_PIN_2)
    BTN_C,  // Теперь левая кнопка (GPIO_PIN_1)
    BTN_COUNT
} ButtonId_t;

typedef enum {
    BUTTON_RELEASED = 0,
    BUTTON_PRESSED = 1
} ButtonState_t;

typedef enum {
    BTN_EVENT_SHORT,
    BTN_EVENT_LONG,
    BTN_EVENT_LONG30,
    BTN_EVENT_LONG60
} ButtonEvent_t;

typedef void (*btn_cb_t)(ButtonId_t id, ButtonEvent_t event);

// Прототипы функций
void Buttons_Init(void);
void Buttons_RegisterCallback(ButtonId_t id, btn_cb_t callback);
ButtonState_t Buttons_GetState(ButtonId_t id);
void Buttons_SetDebounceMs(uint32_t ms);
void Buttons_SetLongPressMs(uint32_t ms);
void Buttons_Task(void);
void Buttons_EXTI_Handle(uint16_t GPIO_Pin);

#endif // BUTTONS_H
