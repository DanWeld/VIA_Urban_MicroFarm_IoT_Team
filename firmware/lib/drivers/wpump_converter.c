#include "wpump_converter.h"
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#define LIMIT 500
#define FACTOR 60U


uint16_t convert_mL_to_ms(uint16_t number_in_mL){
        number_in_mL / FACTOR;
       return (number_in_mL*1000U) / FACTOR;
}