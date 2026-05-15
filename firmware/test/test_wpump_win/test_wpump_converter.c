#include "unity.h"
#include <stdint.h>

/* Embedded converter logic for native testing */
#define MILLI_SECOND_PER_SECOND 1000UL
#define FACTOR 39UL

typedef struct
{
    uint32_t max_ml;
    int16_t compensation_ms;
} wpump_compensation_step_t;

static const wpump_compensation_step_t wpump_compensation_table[] = {
    {25U, 500},
    {50U, 300},
    {75U, 100},
    {100U, -100},
    {125U, -300},
    {150U, -600},
    {175U, -800},
    {200U, -1000},
    {225U, -1250},
    {250U, -1350},
    {275U, -1600},
    {300U, -1800},
    {330U, -2000},
    {350U, -2200},
    {375U, -2500},
    {400U, -2700},
    {430U, -2800},
    {460U, -3000},
    {UINT32_MAX, -3700}
};

static int16_t get_compensation(uint32_t ml)
{
    for (uint8_t i = 0; i < (uint8_t)(sizeof(wpump_compensation_table) / sizeof(wpump_compensation_table[0])); ++i)
    {
        if (ml <= wpump_compensation_table[i].max_ml)
        {
            return wpump_compensation_table[i].compensation_ms;
        }
    }
    return wpump_compensation_table[(sizeof(wpump_compensation_table) / sizeof(wpump_compensation_table[0])) - 1U].compensation_ms;
}

uint32_t wpump_converter_convert_mL_to_ms(uint32_t ml)
{
    uint32_t base_time;
    int32_t corrected_time;
    base_time = (ml * MILLI_SECOND_PER_SECOND) / FACTOR;
    corrected_time = (int32_t)base_time + (int32_t)get_compensation(ml);
    if (corrected_time < 0)
    {
        corrected_time = 0;
    }
    return (uint32_t)corrected_time;
}

static void assert_conversion(uint32_t ml, uint32_t expected_ms)
{
    TEST_ASSERT_EQUAL_UINT32(expected_ms, wpump_converter_convert_mL_to_ms(ml));
}

void setUp(void)
{
}

void tearDown(void)
{
}

void test_wpump_converter_uses_expected_calibration_table(void)
{
    assert_conversion(0U, 500U);
    assert_conversion(25U, 1141U);
    assert_conversion(26U, 966U);
    assert_conversion(50U, 1582U);
    assert_conversion(75U, 2023U);
    assert_conversion(100U, 2464U);
    assert_conversion(150U, 3246U);
    assert_conversion(250U, 5060U);
    assert_conversion(460U, 8794U);
    assert_conversion(461U, 8120U);
    assert_conversion(500U, 9120U);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_wpump_converter_uses_expected_calibration_table);
    return UNITY_END();
}