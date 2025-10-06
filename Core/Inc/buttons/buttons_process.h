#ifndef BUTTONS_PROCESS_H
#define BUTTONS_PROCESS_H

#ifndef BOOL_TYPE_DEFINED
#define BOOL_TYPE_DEFINED

typedef enum { false = 0, true = 1 } bool;

#endif

typedef unsigned char uint8_t;


void Buttons_Process(void);

// Только объявление
extern bool settings_active;


// Функции проверки короткого/длинного нажатия
bool Btn_ShortPress(uint8_t btn);
bool Btn_LongPress(uint8_t btn);

// Статус страницы Settings
bool SettingsActive(void);

// Добавьте эти объявления в buttons_process.h
extern int current_page; // Добавьте extern для глобальной переменной
void OLED_ShowCurrentPage(void); // Объявление новой функции

#endif // BUTTONS_PROCESS_H
