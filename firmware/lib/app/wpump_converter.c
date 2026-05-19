#include "wpump_converter.h"
#include <stdint.h>

// Measured flow rate: ~39 ml per second at nominal voltage.
// base_time = (ml * 1000) / 39  gives milliseconds for the requested volume.
#define MILLI_SECOND_PER_SECOND 1000UL
#define FACTOR 39UL


// The pump takes a moment to reach full flow, and the relay has a small
// cut-off delay. The compensation table corrects for these non-linearities:
// small volumes need extra time (pump not yet at speed), large volumes need
// less (momentum carries flow after switch-off). Values were determined by
// physical calibration on the target hardware.
typedef struct
{
    uint32_t max_ml;// upper bound of this compensation band (inclusive)
    int16_t compensation_ms;// signed correction added to the base time
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
    {UINT32_MAX, -3700}// catch-all for any value above 460 ml
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
