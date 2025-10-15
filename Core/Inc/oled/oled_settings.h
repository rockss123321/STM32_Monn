#ifndef OLED_SETTINGS_H
#define OLED_SETTINGS_H

#include <stdint.h>
#include "buttons/buttons_process.h"
// Инициализация страницы Settings
void OLED_Settings_Init(void);

// Обновление экрана
void OLED_Settings_Draw(void);

// Навигация по меню
void OLED_Settings_MoveUp(void);
void OLED_Settings_MoveDown(void);

// Выполнить действие по выбранному пункту
void OLED_Settings_Select(void);
// Таймаут выхода из меню
void OLED_Settings_TimeoutCheck(void);

void OLED_Settings_ButtonHeld(bool held);

void OLED_UpdateDisplay(void);

// Возврат назад (из подтверждения/подменю/редактирования к меню; из меню — выход)
void OLED_Settings_Back(void);

#endif // OLED_SETTINGS_H
