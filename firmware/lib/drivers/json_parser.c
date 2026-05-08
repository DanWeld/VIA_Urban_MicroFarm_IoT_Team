#include <stdint.h>
#include <cJSON.h>
#include <string.h>
#include "json_parser.h"
Response process_json(const char *payload){

     Response res = {"", 0};

    cJSON *json = cJSON_Parse(payload);
    if (!json)
    {
        return res;
    }

    cJSON *actuator = cJSON_GetObjectItem(json, "actuator");
    cJSON *amount   = cJSON_GetObjectItem(json, "amount_ml");

    if (cJSON_IsString(actuator) && actuator->valuestring != NULL)
    {
        strncpy(res.actuator,
                actuator->valuestring,
                sizeof(res.actuator) - 1);
    }


    if (cJSON_IsNumber(amount))
    {
        res.amount_ml = (uint16_t)amount->valueint;
    }

    cJSON_Delete(json);

    return res;
}