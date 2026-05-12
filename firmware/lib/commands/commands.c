#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "commands.h"
#include "wpump_controller.h"
static void handle_backend_command(const char *payload) {
    if (strstr(payload, "\"actuator\":\"water_pump\"") != NULL) {
        const char *amount_ptr = strstr(payload, "\"amount_ml\":");
        uint16_t amount_ml = 0;

        if (amount_ptr != NULL) {
            amount_ptr += strlen("\"amount_ml\":");
            amount_ml = (uint16_t)atoi(amount_ptr);
        }

        printf("MQTT command received: water_pump %u ml\n", amount_ml);
        wpump_controller_dispense(amount_ml);
        printf("Water pump command completed\n");
    }
}