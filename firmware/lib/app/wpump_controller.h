#pragma once
#include <stdint.h>
#include <stdbool.h>

// Maximum dispense volume accepted. Requests above this are silently ignored
// to protect the hardware from runaway pump operation.
#define WPUMP_MAX_ML 500U

// Start the pump and schedule an automatic stop after the time needed to
// deliver the requested volume (calculated by wpump_converter).
// Does nothing if ml is 0, exceeds WPUMP_MAX_ML, or the converter returns 0.
void wpump_controller_dispense(uint32_t ml);

// Returns true if the pump is currently running (i.e. a dispense is in progress).
bool wpump_controller_get_status();