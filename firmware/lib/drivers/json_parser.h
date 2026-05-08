#pragma once
#include <stdint.h>


typedef struct {
    char actuator[16];
    uint16_t amount_ml;
}Response;
Response process_json(const char *payload);