
#include "buzzer.h"
#ifndef WINDOWS_TEST
#include <avr/io.h>
#endif
#ifndef WINDOWS_TEST
#include <util/delay.h>
#endif

#ifndef WINDOWS_TEST
#define BUZ_BIT PE5
#define BUZ_DDR DDRE
#define BUZ_PORT PORTE

void buzzer_beep(){
    BUZ_DDR |= (1<<BUZ_BIT); //init to be an output
    BUZ_PORT&=~(1<<BUZ_BIT); //Turn On (Active low)
    _delay_ms(25);
    BUZ_PORT|=(1<<BUZ_BIT); //Turn Off (Active low)
}
#else
void buzzer_beep() {}
#endif