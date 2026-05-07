#include "wpump_converter.h"
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#define MILLI_SECOND_PER_SECOND 1000U
#define FACTOR 60U



uint16_t wpump_converter_convert_mL_to_ms(uint16_t number_in_mL){
        
       return (number_in_mL* MILLI_SECOND_PER_SECOND) / FACTOR;
}