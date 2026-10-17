#include "sample.h"

int current;
int temp;

#define CURRENT_AVG_LEN      10
#define CURRENT_ADC_BASE_MV  1349

int current_mv_buf[CURRENT_AVG_LEN];
uint8_t current_mv_pos;
uint8_t current_mv_count;

/* 1 ms：采样入队，满 10 个后覆盖最旧数据 */
int Current_SamplePush(void)
{
    uint32_t Adc_value;
    int adc_mv;

    Adc_value = Get_Adc1Channe9Value();
    if (Adc_value == 0xFFFFFFFFU)
    {
        return -2;
    }

    adc_mv = (int)(3.3 * Adc_value / 4096 * 1000);
	if(adc_mv - CURRENT_ADC_BASE_MV > 40 || CURRENT_ADC_BASE_MV - adc_mv > 40) 
	{
		return -1;
	}
    current_mv_buf[current_mv_pos] = adc_mv;
    current_mv_pos++;
    if (current_mv_pos >= CURRENT_AVG_LEN)
    {
        current_mv_pos = 0;
    }
    if (current_mv_count < CURRENT_AVG_LEN)
    {
        current_mv_count++;
    }
	return 0;
}

/* 10 ms：窗口平均毫伏，再换算 current */
void Current_CalcFromAverage(void)
{
    int sum = 0;
    int adc;
    uint8_t i;

    if (current_mv_count == 0U)
    {
        return;
    }

    for (i = 0; i < current_mv_count; i++)
    {
        sum += current_mv_buf[i];
    }
    adc = (sum + (int)current_mv_count / 2) / (int)current_mv_count;
    if (adc > (CURRENT_ADC_BASE_MV + 2))
    {
        current = (adc - CURRENT_ADC_BASE_MV) * 50;
    }
    else if (adc < (CURRENT_ADC_BASE_MV - 2))
    {
        current = (CURRENT_ADC_BASE_MV - adc) * 50;
    }
    else
    {
        current = 0;
    }
}

void Temp_SamplePush(void)
{
    uint32_t Adc_value;
    float adc;
    float Rp = 10000.0f;
    float T2 = 273.15f + 25.0f;
    float Bx = 3950.0f;
    float Ka = 273.15f;
    float Rt;

    Adc_value = Get_Adc1Channe0Value();
    if (Adc_value == 0xFFFFFFFFU)
    {
        return;
    }

    adc = (float)(3.3 * Adc_value / 4096);
    if (adc <= 0.0f || adc >= 3.3f)
    {
        return;
    }

    Rt = Rp * adc / (3.3f - adc);
    temp = (int)(1.0f / (1.0f / T2 + (logf(Rt / Rp)) / Bx) - Ka + 0.5f);
}
