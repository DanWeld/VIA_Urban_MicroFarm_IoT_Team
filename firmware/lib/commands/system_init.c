#include "system_init.h"
#include "adc.h"
#include "led.h"
#include "display.h"
#include "wifi.h"
#include "adc.h"
#include "light.h"
#include "soil.h"
#include "wpump.h"
void system_init(){
    display_init();
    light_init();
    soil_init(ADC_PK0);
    wpump_configure();
    wifi_init();
}