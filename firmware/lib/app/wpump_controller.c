#include <stdint.h>
#include <stdbool.h>
#include "timer.h"
#include "wpump.h"
#include "wpump_converter.h"
#include "wpump_controller.h"

// Tracks whether a dispense cycle is currently active. Updated by
// wpump_controller_dispense() and the timer callback.
static bool pump_running = false;

// Invoked by the software timer when the calculated dispense time elapses.
// Stops the pump and cleans up the one-shot timer.
static void wpump_callback(uint8_t id) 
{
    wpump_stop(); // force stop water pump 
    pump_running = false;
    timer_delete(id);
}

void wpump_controller_dispense(uint32_t ml)
{
    if (ml > LIMIT)
    {
        return; // the number is too big
    }

    // Convert the requested volume to a pump-on duration.
    // The converter applies a hardware-calibrated compensation offset.
    uint32_t time_ms = wpump_converter_convert_mL_to_ms(ml);
    if (time_ms == 0 )
    {
        return; // compensation offset reduced time to zero — nothing to do
    }

    pump_running = true;
    wpump_start();
    
     // Create a one-shot software timer; the callback stops the pump when it fires.
    timer_create_sw(wpump_callback,time_ms);
}

bool wpump_controller_get_status()
{
    return pump_running;
}