#include "signal_processor.h"
#include <math.h>

// Глобальные переменные
float voltage1 = 0.0f;
float voltage2 = 0.0f;
float current  = 0.0f;
float selected_voltage = 0.0f;

void SignalProcessor_Update(uint32_t *adc_buf, size_t samples)
{
    double sum_v1 = 0.0;
    double sum_v2 = 0.0;
    double sum_i  = 0.0;

    for (size_t n = 0; n < samples; n++) {
        // Буфер имеет вид [ch1, ch2, ch3, ch1, ch2, ch3, ...]
        uint32_t raw1 = adc_buf[n * 3 + 0];
        uint32_t raw2 = adc_buf[n * 3 + 1];
        uint32_t raw3 = adc_buf[n * 3 + 2];

        // Переводим в напряжение на пине (0…3.3 В)
        float v1 = (raw1 * VREF / ADC_RESOLUTION) - VREF_HALF;
        float v2 = (raw2 * VREF / ADC_RESOLUTION) - VREF_HALF;
        float i  = (raw3 * VREF / ADC_RESOLUTION) - VREF_HALF;

        // Суммируем квадраты
        sum_v1 += (double)(v1 * v1);
        sum_v2 += (double)(v2 * v2);
        sum_i  += (double)(i  * i);
    }

    // RMS
    voltage1 = (sqrtf(sum_v1 / samples) - 0.82)*780;
    if (voltage1 < 20.0f) {
        voltage1 = 0.0f;
    }
    voltage2 = (sqrtf(sum_v2 / samples) - 0.82)*780;
    if (voltage2 < 20.0f) {
        voltage2 = 0.0f;
    }
	current  = (sqrtf(sum_i  / samples) - 0.82)*25;
    if (current < 0.205f) {
        current = 0.0f;
    }

    // Выбор активного напряжения
    if (voltage1 > 50.0f) {
        selected_voltage = voltage1;
    } else {
        selected_voltage = voltage2;
    }
}
