#include <stdint.h>
#include "timer.h"
#include "wpump.h"
#include "wpump_converter.h"
#include <stdbool.h>

#define LIMIT 1000U

 static bool pump_running = false;
static void wpump_callback(uint8_t id) {
    wpump_stop(); // force stop water pump 
        pump_running = false;
        timer_delete(id);
    }

void wpump_controller_dispense(uint32_t ml){
    
    
    uint32_t time_ms = wpump_converter_convert_mL_to_ms(ml);
    
    
    if (ml>LIMIT || time_ms == 0 )
    {
        return; // the number is too big 
    }
    pump_running = true;
     wpump_start();
     timer_create_sw(wpump_callback,time_ms);
}
bool wpump_controller_get_status(){
    return pump_running;
}