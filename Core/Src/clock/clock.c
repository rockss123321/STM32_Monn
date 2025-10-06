#include "clock/clock.h"

void Clock_GetTime(uint8_t *hh, uint8_t *mm, uint8_t *ss)
{
    RTC_TimeTypeDef sTime;
    RTC_DateTypeDef sDate;

    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN); // обязательно после GetTime

    if(hh) *hh = sTime.Hours;
    if(mm) *mm = sTime.Minutes;
    if(ss) *ss = sTime.Seconds;
}

void Clock_GetDate(uint8_t *dd, uint8_t *mmn, uint16_t *yy)
{
    RTC_DateTypeDef sDate;

    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

    if(dd)  *dd  = sDate.Date;
    if(mmn) *mmn = sDate.Month;
    if(yy)  *yy  = 2000 + sDate.Year; // STM32 хранит год от 2000
}
