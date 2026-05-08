#include <stdint.h>
#include <cJSON.h>

typedef struct {
    char actuator;
    uint16_t amount_ml;
}Response;


void process_json(const char *payload){

    cJSON *json = cJSON_Parse(payload);

    if (!json)
    {
        printf("JSON error\n");
        return;
    }
    char actuator_w   = cJSON_GetObjectItem(json, "actuator")->valueint;
    int ml = cJSON_GetObjectItem(json, "amount_ml")->valueint;

    cJSON_Delete(json);

};