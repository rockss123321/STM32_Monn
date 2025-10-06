#ifndef SIGNAL_PROCESSOR_H
#define SIGNAL_PROCESSOR_H

#include <stdint.h>
#include <stddef.h>

#define ADC_RESOLUTION     4096.0f   // 12-bit ADC
#define VREF               3.3f
#define VREF_HALF          (VREF / 2.0f)

// Количество отсчётов для RMS (4 периода по 50 Гц при 10 кГц = 800)
#define ADC_SAMPLES        800

// Глобальные переменные с результатами
extern float voltage1;
extern float voltage2;
extern float current;
extern float selected_voltage;

// Функция для обновления значений
void SignalProcessor_Update(uint32_t *adc_buf, size_t samples);

#endif // SIGNAL_PROCESSOR_H
