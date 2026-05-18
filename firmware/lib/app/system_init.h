#pragma once // #pragma once ensures this header is only included once per compilation unit, preventing redefinition errors.

// Initialises all hardware peripherals needed by the application.
// Must be called once at the start of main(), before interrupts are enabled.
//
// Initialises in this order:
//   1. Display  – starts Timer1 for 7-segment multiplexing
//   2. Light    – configures ADC channel PK7
//   3. Soil     – configures ADC channel PK0
//   4. Pump     – sets PC7 as output, pump off by default
//   5. WiFi     – initialises UART2 for ESP8266 AT commands

void system_init(void);