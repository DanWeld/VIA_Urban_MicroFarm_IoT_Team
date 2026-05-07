#include "unity.h"
#include "wpump.h"
#include <stdint.h>
#undef wpump_ddrc
#undef wpump_port
#define wpump_ddrc fake_ddrc
#define wpump_port fake_portc

//  Handler and tests for soil moisture requests 

// Only start pump if soil moisture is outside 200-600
void wpump_handle_soil_request(int soil_adc_value) {
    if (soil_adc_value < 200 || soil_adc_value > 600) {
        wpump_start();
    } else {
        wpump_stop();
    }
}

void test_wpump_handle_soil_request_starts_pump(void) {
    fake_portc = 0;
    wpump_handle_soil_request(100); // Too dry, should start
    TEST_ASSERT_TRUE((fake_portc & (1 << 7)) != 0);

    fake_portc = 0;
    wpump_handle_soil_request(700); // Too wet, should start
    TEST_ASSERT_TRUE((fake_portc & (1 << 7)) != 0);
}

void test_wpump_handle_soil_request_stops_pump(void) {
    fake_portc = 0xFF;
    wpump_handle_soil_request(500); // In range, should stop
    TEST_ASSERT_TRUE((fake_portc & (1 << 7)) == 0);

    fake_portc = 0xFF;
    wpump_handle_soil_request(600); // Upper bound, should stop
    TEST_ASSERT_TRUE((fake_portc & (1 << 7)) == 0);

    fake_portc = 0xFF;
    wpump_handle_soil_request(200); // Lower bound, should stop
    TEST_ASSERT_TRUE((fake_portc & (1 << 7)) == 0);
}


// --- Test Suite: Water Pump Driver ---

// Mocks for hardware registers
uint8_t fake_ddrc = 0;
uint8_t fake_portc = 0;



void setUp(void) {
    fake_ddrc = 0;
    fake_portc = 0xFF; // Assume all high initially
}

void tearDown(void) {}


// --- Basic Driver Tests ---
void test_wpump_configure_sets_ddr_and_off(void) {
    wpump_configure();
    TEST_ASSERT_TRUE((fake_ddrc & (1 << 7)) != 0); // Output
    TEST_ASSERT_TRUE((fake_portc & (1 << 7)) == 0); // Off
}

void test_wpump_start_turns_on_pump(void) {
    fake_portc = 0;
    wpump_start();
    // PC7 should be set high
    TEST_ASSERT_TRUE((fake_portc & (1 << 7)) != 0);
}

void test_wpump_stop_turns_off_pump(void) {
    fake_portc = 0xFF;
    wpump_stop();
    // PC7 should be cleared
    TEST_ASSERT_TRUE((fake_portc & (1 << 7)) == 0);
}


// --- Request-Based Control Tests ---

// Handler: Only start pump if amount_ml is in 1-500, else stop
void wpump_handle_request(int amount_ml) {
    if (amount_ml > 0 && amount_ml <= 500) {
        wpump_start();
    } else {
        wpump_stop();
    }
}

void test_wpump_handle_request_starts_pump(void) {
    fake_portc = 0;
    wpump_handle_request(1);   // Lower bound
    TEST_ASSERT_TRUE((fake_portc & (1 << 7)) != 0);

    fake_portc = 0;
    wpump_handle_request(250); // Mid-range
    TEST_ASSERT_TRUE((fake_portc & (1 << 7)) != 0);

    fake_portc = 0;
    wpump_handle_request(500); // Upper bound
    TEST_ASSERT_TRUE((fake_portc & (1 << 7)) != 0);
}

void test_wpump_handle_request_stops_pump(void) {
    fake_portc = 0xFF;
    wpump_handle_request(0);    // Zero
    TEST_ASSERT_TRUE((fake_portc & (1 << 7)) == 0);

    fake_portc = 0xFF;
    wpump_handle_request(-10);  // Negative
    TEST_ASSERT_TRUE((fake_portc & (1 << 7)) == 0);

    fake_portc = 0xFF;
    wpump_handle_request(600);  // Above max
    TEST_ASSERT_TRUE((fake_portc & (1 << 7)) == 0);
}


// --- Threshold-Based Control Tests (Soil Moisture) ---

// Handler: Only start pump if soil moisture is outside 200-600
void wpump_handle_soil_request(int soil_adc_value) {
    if (soil_adc_value < 200 || soil_adc_value > 600) {
        wpump_start();
    } else {
        wpump_stop();
    }
}

void test_wpump_handle_soil_request_starts_pump(void) {
    fake_portc = 0;
    wpump_handle_soil_request(100); // Too dry
    TEST_ASSERT_TRUE((fake_portc & (1 << 7)) != 0);

    fake_portc = 0;
    wpump_handle_soil_request(700); // Too wet
    TEST_ASSERT_TRUE((fake_portc & (1 << 7)) != 0);
}

void test_wpump_handle_soil_request_stops_pump(void) {
    fake_portc = 0xFF;
    wpump_handle_soil_request(200); // Lower bound
    TEST_ASSERT_TRUE((fake_portc & (1 << 7)) == 0);

    fake_portc = 0xFF;
    wpump_handle_soil_request(500); // In range
    TEST_ASSERT_TRUE((fake_portc & (1 << 7)) == 0);

    fake_portc = 0xFF;
    wpump_handle_soil_request(600); // Upper bound
    TEST_ASSERT_TRUE((fake_portc & (1 << 7)) == 0);
}

int main(void) {
    UNITY_BEGIN();

    // Basic driver tests
    RUN_TEST(test_wpump_configure_sets_ddr_and_off);
    RUN_TEST(test_wpump_start_turns_on_pump);
    RUN_TEST(test_wpump_stop_turns_off_pump);

    // Request-based control
    RUN_TEST(test_wpump_handle_request_starts_pump);
    RUN_TEST(test_wpump_handle_request_stops_pump);

    // Threshold-based control
    RUN_TEST(test_wpump_handle_soil_request_starts_pump);
    RUN_TEST(test_wpump_handle_soil_request_stops_pump);

    return UNITY_END();
}

// start/stop based on amount_ml
void wpump_handle_request(int amount_ml) {
    // Only start pump if amount_ml is in 1-500
    if (amount_ml > 0 && amount_ml <= 500) {
        wpump_start();
    } else {
        wpump_stop();
    }
}

void test_wpump_handle_request_starts_pump(void) {
    fake_portc = 0;
    wpump_handle_request(50); // Should start pump
    TEST_ASSERT_TRUE((fake_portc & (1 << 7)) != 0);

    fake_portc = 0;
    wpump_handle_request(500); // Should start pump (upper bound)
    TEST_ASSERT_TRUE((fake_portc & (1 << 7)) != 0);

    fake_portc = 0;
    wpump_handle_request(1); // Should start pump (lower bound)
    TEST_ASSERT_TRUE((fake_portc & (1 << 7)) != 0);
}

void test_wpump_handle_request_stops_pump(void) {
    fake_portc = 0xFF;
    wpump_handle_request(0); // Should stop pump
    TEST_ASSERT_TRUE((fake_portc & (1 << 7)) == 0);

    fake_portc = 0xFF;
    wpump_handle_request(-10); // Should stop pump (negative)
    TEST_ASSERT_TRUE((fake_portc & (1 << 7)) == 0);

    fake_portc = 0xFF;
    wpump_handle_request(600); // Should stop pump (above max)
    TEST_ASSERT_TRUE((fake_portc & (1 << 7)) == 0);
}
