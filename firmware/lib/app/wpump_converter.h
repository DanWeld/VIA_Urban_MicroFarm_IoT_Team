#pragma once
#include <stdint.h>

// Convert a requested volume in millilitres to a pump-on duration in
// milliseconds, including a hardware-calibrated compensation offset.
// Returns 0 if the converted time would be zero or negative after compensation.
uint32_t wpump_converter_convert_mL_to_ms(uint32_t number_in_mL);