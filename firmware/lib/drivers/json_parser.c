#include <stdint.h>
#include <ArduinoJson.h>

typedef struct {
    char actuator;
    uint16_t amount_ml;
}Response;


void process_json(const char *payload){

    StaticJsonDocument<200> doc;

    DeserializationError error =
        deserializeJson(doc, payload);

    if(error)
    {
        printf("JSON failed\n");
        return;
    }
}