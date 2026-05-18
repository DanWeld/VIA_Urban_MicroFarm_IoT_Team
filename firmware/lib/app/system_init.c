#include "system_init.h"
#include "display.h"
#include "light.h"
#include "soil.h"
#include "wpump.h"
#include "wifi.h"
#include "adc.h"

void system_init(void)
{
    display_init();       // start Timer1 for 7-segment display multiplexing
    light_init();         // configure ADC channel PK7 for the light sensor
    soil_init(ADC_PK0);   // configure ADC channel PK0 for the soil moisture sensor
    wpump_configure();    // set PC7 as output, ensure pump starts off
    wifi_init();          // initialise UART2 for ESP8266 AT command communication
}