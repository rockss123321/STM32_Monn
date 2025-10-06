#include "buttons/buttons.h"

// Меняем пины местами: BTN_A и BTN_C меняются
#define BTN_A_PIN      GPIO_PIN_3  // Было GPIO_PIN_1
#define BTN_B_PIN      GPIO_PIN_2  // Оставляем как есть (средняя)
#define BTN_C_PIN      GPIO_PIN_1  // Было GPIO_PIN_3
#define BTN_GPIO_PORT  GPIOD

static uint32_t debounce_ms   = 50;
static uint32_t longpress_ms  = 1000;

#define LONG30_MS 30000
#define LONG60_MS 60000

typedef struct {
    uint32_t last_tick;      // антидребезг
    uint32_t press_start;    // начало удержания
    ButtonState_t state;     // текущее состояние
    btn_cb_t callback;
    uint8_t long30_reported; // чтобы 1 раз вызвать
    uint8_t long60_reported;
} ButtonInfo_t;

static volatile ButtonInfo_t buttons[BTN_COUNT];

static int pin_to_index(uint16_t gpio_pin)
{
    switch (gpio_pin) {
    case GPIO_PIN_3: return BTN_A;  // Изменено
    case GPIO_PIN_2: return BTN_B;
    case GPIO_PIN_1: return BTN_C;  // Изменено
    default: return -1;
    }
}

ButtonState_t Buttons_GetState(ButtonId_t id)
{
    uint16_t pin;
    switch (id) {
    case BTN_A: pin = BTN_A_PIN; break;  // Теперь это правая кнопка
    case BTN_B: pin = BTN_B_PIN; break;  // Средняя
    case BTN_C: pin = BTN_C_PIN; break;  // Теперь это левая кнопка
    default: return BUTTON_RELEASED;
    }
    return (HAL_GPIO_ReadPin(BTN_GPIO_PORT, pin) == GPIO_PIN_SET) ?
            BUTTON_PRESSED : BUTTON_RELEASED;
}

void Buttons_SetDebounceMs(uint32_t ms) { debounce_ms = ms; }
void Buttons_SetLongPressMs(uint32_t ms) { longpress_ms = ms; }

void Buttons_Init(void)
{
    for (int i = 0; i < BTN_COUNT; ++i) {
        buttons[i].last_tick = 0;
        buttons[i].press_start = 0;
        buttons[i].state = BUTTON_RELEASED;
        buttons[i].callback = NULL;
        buttons[i].long30_reported = 0;
        buttons[i].long60_reported = 0;
    }
}

void Buttons_RegisterCallback(ButtonId_t id, btn_cb_t callback)
{
    if (id < BTN_COUNT) {
        buttons[id].callback = callback;
    }
}

void Buttons_EXTI_Handle(uint16_t GPIO_Pin)
{
    int idx = pin_to_index(GPIO_Pin);
    if (idx < 0) return;

    uint32_t now = HAL_GetTick();

    if ((now - buttons[idx].last_tick) < debounce_ms) return;
    buttons[idx].last_tick = now;

    ButtonState_t cur = Buttons_GetState((ButtonId_t)idx);

    if (cur != buttons[idx].state) {
        buttons[idx].state = cur;

        if (cur == BUTTON_PRESSED) {
            buttons[idx].press_start = now;
            buttons[idx].long30_reported = 0;
            buttons[idx].long60_reported = 0;
        } else {
            /* Отпускание */
            uint32_t dur = now - buttons[idx].press_start;
            if (buttons[idx].callback) {
                if (dur >= longpress_ms && dur < LONG30_MS) {
                    buttons[idx].callback((ButtonId_t)idx, BTN_EVENT_LONG);
                } else if (dur < longpress_ms) {
                    buttons[idx].callback((ButtonId_t)idx, BTN_EVENT_SHORT);
                }
                /* Если удержали >30 или >60 — событие уже сработало в Task */
            }
        }
    }
}

void Buttons_Task(void)
{
    uint32_t now = HAL_GetTick();

    for (int i = 0; i < BTN_COUNT; ++i) {
        if (buttons[i].state == BUTTON_PRESSED && buttons[i].callback) {
            uint32_t dur = now - buttons[i].press_start;

            if (!buttons[i].long30_reported && dur >= LONG30_MS) {
                buttons[i].long30_reported = 1;
                buttons[i].callback((ButtonId_t)i, BTN_EVENT_LONG30);
            }
            if (!buttons[i].long60_reported && dur >= LONG60_MS) {
                buttons[i].long60_reported = 1;
                buttons[i].callback((ButtonId_t)i, BTN_EVENT_LONG60);
            }
        }
    }
}

/* вызывать из stm32f2xx_it.c */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    Buttons_EXTI_Handle(GPIO_Pin);
}
