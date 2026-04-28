#include "wpump.h"

#include <stdint.h>
#include <avr/io.h>


#define wpump_ddrc DDRC
#define wpump_port PORTC

void wpump_configure(void){
    wpump_ddrc |=(1<<PC7);// set it up as output;
    wpump_port &= ~(1<<PC7); // power off active high.
    
}

void wpump_start(){
    wpump_port |= (1<<PC7); // turn on the water pump
}
void wpump_stop(){
    wpump_port &= ~(1<<PC7); // trun off the water pump
}


