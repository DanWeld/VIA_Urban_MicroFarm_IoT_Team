#include "wpump.h"
#include "button.h"
#include "timer.h"

#include <stdint.h>
#include <avr/io.h>

// Map logical names to the actual AVR registers so the pin assignment
// is visible in one place and easy to change if the hardware is revised.
#define wpump_ddrc DDRC
#define wpump_port PORTC


void wpump_configure(void)
{
    wpump_ddrc |=(1<<PC7);// PC7 as output;
    wpump_port &= ~(1<<PC7); // relay off (active high).
}

void wpump_start()
{
    wpump_port |= (1<<PC7); //turn on the water pump
}

void wpump_stop()
{
    wpump_port &= ~(1<<PC7); //turn off the water pump
}


