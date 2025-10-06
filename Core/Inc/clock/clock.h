#ifndef CLOCK_H
#define CLOCK_H

#include "stm32f2xx_hal.h"

extern RTC_HandleTypeDef hrtc; // используем существующий

void Clock_GetTime(uint8_t *hh, uint8_t *mm, uint8_t *ss);
void Clock_GetDate(uint8_t *dd, uint8_t *mmn, uint16_t *yy);

#endif
