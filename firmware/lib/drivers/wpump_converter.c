#include "wpump_converter.h"
#include <stdint.h>

#define MILLI_SECOND_PER_SECOND 1000UL
#define FACTOR 39UL

static int16_t get_compensation(uint32_t ml)
{
    if (ml <= 25)  return 500;
    if (ml <= 50)  return 300;
    if (ml <= 75)  return 100;
    if (ml <= 100) return -100;
    if (ml <= 125) return -300;
    if (ml <= 150) return -600;
    if (ml <= 175)  return -800;
    if (ml <= 200)  return -1000;
    if (ml <= 225)  return -1250;
    if (ml <= 250) return -1350;
    if (ml <= 275) return -1600;
    if (ml <= 300) return -1800;
    if (ml <= 330)  return -2000;
    if (ml <= 350) return -2200;
    if (ml <= 375) return -2500;
    if (ml <= 400) return -2700;
    if (ml <= 430)  return -2800;
    if (ml <= 460) return -3000;
    

    return -3700;
}

uint32_t wpump_converter_convert_mL_to_ms(uint32_t ml)
{
    uint32_t base_time;
    int32_t corrected_time;

    base_time = (ml * MILLI_SECOND_PER_SECOND) / FACTOR;

    corrected_time = (int32_t)base_time + get_compensation(ml);

    if (corrected_time < 0)
    {
        corrected_time = 0;
    }

    return (uint32_t)corrected_time;
}