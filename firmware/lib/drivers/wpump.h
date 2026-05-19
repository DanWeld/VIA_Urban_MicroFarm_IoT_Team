#pragma once
#include <stdint.h>

// Initialise PC7 as output and ensure the pump starts in the off state.
// Must be called once during system_init() before the pump is used.
void wpump_configure(void);

// Drive PC7 high to switch the pump relay on.
void wpump_start();

// Drive PC7 low to switch the pump relay off.
void wpump_stop();

