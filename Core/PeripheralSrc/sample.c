#include "sample.h"

int16_t Current;
uint8_t temp;

#define CURRENT_AVG_LEN        100
#define CURRENT_ADC_BASE_MV    1361
#define CURRENT_MV_FRAC        10
#define CURRENT_DEADBAND_X10   5
#define CURRENT_PER_MV         50
#define ADC_FULL_SCALE         4096
#define ADC_VREF_MV            3300

uint16_t current_raw_buf[CURRENT_AVG_LEN];
uint8_t current_raw_pos;
uint8_t current_raw_count;

/* 1 ms：原始码入窗。平均留到 100 ms */
void Current_SamplePush(void)
{
    uint16_t adc_value;

    adc_value = Get_Adc1Channe9Value();
    if (adc_value == 0xFFFF)
    {
        return;
    }

    current_raw_buf[current_raw_pos] = adc_value;
    current_raw_pos++;
    if (current_raw_pos >= CURRENT_AVG_LEN)
    {
        current_raw_pos = 0;
    }
    if (current_raw_count < CURRENT_AVG_LEN)
    {
        current_raw_count++;
    }
}

uint8_t Current_WindowCount(void)
{
    return current_raw_count;
}

void Current_WindowClear(void)
{
    current_raw_pos = 0U;
    current_raw_count = 0U;
}

/*
 * 这一窗原始码换成 0.1 mV，四舍五入：
 * avg_x10 = (raw_sum × 3300 × 10 + 分母 / 2) / (count × 4096)
 */
static uint16_t Current_AverageMvX10(void)
{
    uint32_t raw_sum = 0;
    uint8_t i;
    uint32_t count = current_raw_count;
    uint64_t denominator;
    uint64_t mv_x10_per_lsb;
    uint64_t numerator;

    for (i = 0U; i < count; i++)
    {
        raw_sum += current_raw_buf[i];
    }

    /* 分母：点数 × 4096 */
    denominator = (uint64_t)count * ADC_FULL_SCALE;
    /* 一个原始码对应的 0.1 mV：3300 × 10 */
    mv_x10_per_lsb = (uint64_t)ADC_VREF_MV * CURRENT_MV_FRAC;
    /* 分子：总和 × 比例，再加上分母的一半 */
    numerator = (uint64_t)raw_sum * mv_x10_per_lsb + denominator / 2U;
    return (uint16_t)(numerator / denominator);
}

/*
 * 100 ms：偏差也用 0.1 mV。
 * delta_x10 = 1361 * 10 - avg_x10
 * |delta_x10| <= 5（±0.5 mV）时电流为 0。
 * 超出后 Current = delta_x10 * (50 / 10)，1 mV 仍是 50。
 */
void Current_CalcFromAverage(void)
{
    uint16_t avg_x10;
    int32_t base_x10;
    int32_t delta_x10;
    int32_t counts_per_0p1_mv;
    int32_t current;

    if (current_raw_count == 0U)
    {
        return;
    }

    avg_x10 = Current_AverageMvX10();
    /* 零点 1361 mV 换成 0.1 mV */
    base_x10 = (int32_t)CURRENT_ADC_BASE_MV * CURRENT_MV_FRAC;
    /* 高于零点为负，低于零点为正 */
    delta_x10 = base_x10 - (int32_t)avg_x10;
    /* 1 个 0.1 mV 对应 5 个电流计数 */
    counts_per_0p1_mv = CURRENT_PER_MV / CURRENT_MV_FRAC;
    if (delta_x10 > CURRENT_DEADBAND_X10 || delta_x10 < -CURRENT_DEADBAND_X10)
    {
        current = delta_x10 * counts_per_0p1_mv;
    }
    else
    {
        current = 0;
    }

    if (current > 32767)
    {
        current = 32767;
    }
    else if (current < -32768)
    {
        current = -32768;
    }
    Current = (int16_t)current;
}

void Temp_SamplePush(void)
{
    uint16_t Adc_value;
    float adc;
    float Rp = 10000.0f;
    float T2 = 273.15f + 25.0f;
    float Bx = 3950.0f;
    float Ka = 273.15f;
    float Rt;

    Adc_value = Get_Adc1Channe0Value();
    if (Adc_value == 0xFFFF)
    {
        return;
    }

    adc = (float)(3.3 * Adc_value / 4096);
    if (adc <= 0.0f || adc >= 3.3f)
    {
        return;
    }

    Rt = Rp * adc / (3.3f - adc);
    temp = (uint8_t)(1.0f / (1.0f / T2 + (logf(Rt / Rp)) / Bx) - Ka + 0.5f);
}

