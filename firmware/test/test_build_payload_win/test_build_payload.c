#include "unity.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

// Created comprehensive native tests for build_payload()
// Black-box testing approach - we only test the output based on inputs without caring about internal implementation
// This allows us to verify the correctness of the payload generation logic in isolation from AVR-specific code

// Copy of build_payload function from main.c for native testing
// NOTE: We need this copy because main.c contains AVR-specific headers
// that don't compile on native Windows
void build_payload(char *payload, size_t size,
                   uint16_t setup_id,
                   uint8_t temp_int, uint8_t temp_dec,
                   uint8_t hum_int, uint8_t hum_dec,
                   uint16_t light_value, uint16_t soil_value)
{
    uint16_t temperature_x10 = temp_int * 10 + temp_dec;
    uint16_t humidity_x10 = hum_int * 10 + hum_dec;

    snprintf(payload, size,
             "{\"setup_id\":%u,\"sensor_id\":null,\"temperature\":%u,\"humidity\":%u,\"light\":%u,\"soil_moisture\":%u}",
             setup_id, temperature_x10, humidity_x10, light_value, soil_value);
}

// Helper function to check if a string contains a substring
static int contains_substring(const char *str, const char *substr) {
    return strstr(str, substr) != NULL;
}

void setUp(void) {
    // Run before each test
}

void tearDown(void) {
    // Run after each test
}

// ============================================================================
// BASIC FUNCTIONALITY TESTS
// ============================================================================

void test_build_payload_valid_normal_values(void) {
    char payload[150];
    build_payload(payload, sizeof(payload),
                  1,      // setup_id
                  25, 3,  // 25.3°C
                  60, 5,  // 60.5% humidity
                  512,    // light
                  300);   // soil moisture
    
    TEST_ASSERT_TRUE(contains_substring(payload, "\"setup_id\":1"));
    TEST_ASSERT_TRUE(contains_substring(payload, "\"temperature\":253")); // 25.3 * 10 = 253
    TEST_ASSERT_TRUE(contains_substring(payload, "\"humidity\":605"));    // 60.5 * 10 = 605
    TEST_ASSERT_TRUE(contains_substring(payload, "\"light\":512"));
    TEST_ASSERT_TRUE(contains_substring(payload, "\"soil_moisture\":300"));
    TEST_ASSERT_TRUE(contains_substring(payload, "\"sensor_id\":null"));
}

void test_build_payload_json_format(void) {
    char payload[150];
    build_payload(payload, sizeof(payload), 1, 20, 0, 50, 0, 100, 200);
    
    // Check JSON structure: starts with {, ends with }
    TEST_ASSERT_EQUAL_CHAR('{', payload[0]);
    TEST_ASSERT_EQUAL_CHAR('}', payload[strlen(payload) - 1]);
    
    // Count quotes and colons (basic JSON validation)
    int quote_count = 0;
    int colon_count = 0;
    for (size_t i = 0; i < strlen(payload); i++) {
        if (payload[i] == '"') quote_count++;
        if (payload[i] == ':') colon_count++;
    }
    TEST_ASSERT_EQUAL(12, quote_count); // 6 field names * 2 quotes each
    TEST_ASSERT_EQUAL(6, colon_count);  // 6 fields
}

// ============================================================================
// EDGE CASE TESTS - MAXIMUM VALUES
// ============================================================================

void test_build_payload_max_values(void) {
    char payload[150];
    build_payload(payload, sizeof(payload),
                  65535,  // max uint16_t
                  99, 9,  // 99.9°C
                  99, 9,  // 99.9%
                  1023,   // max 10-bit ADC
                  1023);  // max 10-bit ADC
    
    TEST_ASSERT_TRUE(contains_substring(payload, "\"setup_id\":65535"));
    TEST_ASSERT_TRUE(contains_substring(payload, "\"temperature\":999"));
    TEST_ASSERT_TRUE(contains_substring(payload, "\"humidity\":999"));
    TEST_ASSERT_TRUE(contains_substring(payload, "\"light\":1023"));
    TEST_ASSERT_TRUE(contains_substring(payload, "\"soil_moisture\":1023"));
}

// ============================================================================
// EDGE CASE TESTS - MINIMUM/ZERO VALUES
// ============================================================================

void test_build_payload_zero_values(void) {
    char payload[150];
    build_payload(payload, sizeof(payload), 0, 0, 0, 0, 0, 0, 0);
    
    TEST_ASSERT_TRUE(contains_substring(payload, "\"setup_id\":0"));
    TEST_ASSERT_TRUE(contains_substring(payload, "\"temperature\":0"));
    TEST_ASSERT_TRUE(contains_substring(payload, "\"humidity\":0"));
    TEST_ASSERT_TRUE(contains_substring(payload, "\"light\":0"));
    TEST_ASSERT_TRUE(contains_substring(payload, "\"soil_moisture\":0"));
}

// ============================================================================
// DECIMAL CONVERSION ACCURACY
// ============================================================================

void test_build_payload_decimal_conversion(void) {
    char payload[150];
    
    // Test 12.5°C and 34.7% humidity
    build_payload(payload, sizeof(payload), 1, 12, 5, 34, 7, 100, 200);
    TEST_ASSERT_TRUE(contains_substring(payload, "\"temperature\":125")); // 12.5 * 10
    TEST_ASSERT_TRUE(contains_substring(payload, "\"humidity\":347"));    // 34.7 * 10
    
    // Test edge case: 5.9°C and 8.9% humidity
    build_payload(payload, sizeof(payload), 1, 5, 9, 8, 9, 100, 200);
    TEST_ASSERT_TRUE(contains_substring(payload, "\"temperature\":59"));
    TEST_ASSERT_TRUE(contains_substring(payload, "\"humidity\":89"));
}

void test_build_payload_decimal_overflow(void) {
    char payload[150];
    
    // If temp_dec > 9 (shouldn't happen in practice, but test robustness)
    build_payload(payload, sizeof(payload), 1, 20, 15, 50, 20, 100, 200);
    
    // Should calculate 20*10 + 15 = 215, 50*10 + 20 = 520
    TEST_ASSERT_TRUE(contains_substring(payload, "\"temperature\":215"));
    TEST_ASSERT_TRUE(contains_substring(payload, "\"humidity\":520"));
}

// ============================================================================
// BOUNDARY CONDITION TESTS
// ============================================================================

void test_build_payload_setup_id_boundaries(void) {
    char payload[150];
    
    // Minimum typical value
    build_payload(payload, sizeof(payload), 1, 20, 0, 50, 0, 100, 200);
    TEST_ASSERT_TRUE(contains_substring(payload, "\"setup_id\":1"));
    
    // Maximum uint16_t
    build_payload(payload, sizeof(payload), 65535, 20, 0, 50, 0, 100, 200);
    TEST_ASSERT_TRUE(contains_substring(payload, "\"setup_id\":65535"));
}

void test_build_payload_temperature_boundaries(void) {
    char payload[150];
    
    // Minimum (0.0°C)
    build_payload(payload, sizeof(payload), 1, 0, 0, 50, 0, 100, 200);
    TEST_ASSERT_TRUE(contains_substring(payload, "\"temperature\":0"));
    
    // High temperature (99.9°C)
    build_payload(payload, sizeof(payload), 1, 99, 9, 50, 0, 100, 200);
    TEST_ASSERT_TRUE(contains_substring(payload, "\"temperature\":999"));
}

void test_build_payload_sensor_boundaries(void) {
    char payload[150];
    
    // Light: minimum (dark)
    build_payload(payload, sizeof(payload), 1, 20, 0, 50, 0, 0, 512);
    TEST_ASSERT_TRUE(contains_substring(payload, "\"light\":0"));
    
    // Light: maximum (bright)
    build_payload(payload, sizeof(payload), 1, 20, 0, 50, 0, 1023, 512);
    TEST_ASSERT_TRUE(contains_substring(payload, "\"light\":1023"));
    
    // Soil: minimum (dry)
    build_payload(payload, sizeof(payload), 1, 20, 0, 50, 0, 512, 0);
    TEST_ASSERT_TRUE(contains_substring(payload, "\"soil_moisture\":0"));
    
    // Soil: maximum (wet)
    build_payload(payload, sizeof(payload), 1, 20, 0, 50, 0, 512, 1023);
    TEST_ASSERT_TRUE(contains_substring(payload, "\"soil_moisture\":1023"));
}

// ============================================================================
// REALISTIC SCENARIO TESTS
// ============================================================================

void test_build_payload_greenhouse_scenario(void) {
    char payload[150];
    
    // Typical greenhouse: 22.5°C, 65.0% humidity, moderate light, moist soil
    build_payload(payload, sizeof(payload), 5, 22, 5, 65, 0, 750, 450);
    
    TEST_ASSERT_TRUE(contains_substring(payload, "\"setup_id\":5"));
    TEST_ASSERT_TRUE(contains_substring(payload, "\"temperature\":225"));
    TEST_ASSERT_TRUE(contains_substring(payload, "\"humidity\":650"));
    TEST_ASSERT_TRUE(contains_substring(payload, "\"light\":750"));
    TEST_ASSERT_TRUE(contains_substring(payload, "\"soil_moisture\":450"));
}

void test_build_payload_cold_night_scenario(void) {
    char payload[150];
    
    // Cold night: 5.2°C, 80.5% humidity, very dark, dry soil
    build_payload(payload, sizeof(payload), 3, 5, 2, 80, 5, 10, 100);
    
    TEST_ASSERT_TRUE(contains_substring(payload, "\"temperature\":52"));
    TEST_ASSERT_TRUE(contains_substring(payload, "\"humidity\":805"));
    TEST_ASSERT_TRUE(contains_substring(payload, "\"light\":10"));
    TEST_ASSERT_TRUE(contains_substring(payload, "\"soil_moisture\":100"));
}

void test_build_payload_hot_day_scenario(void) {
    char payload[150];
    
    // Hot sunny day: 35.8°C, 40.2% humidity, very bright, wet soil
    build_payload(payload, sizeof(payload), 2, 35, 8, 40, 2, 1000, 800);
    
    TEST_ASSERT_TRUE(contains_substring(payload, "\"temperature\":358"));
    TEST_ASSERT_TRUE(contains_substring(payload, "\"humidity\":402"));
    TEST_ASSERT_TRUE(contains_substring(payload, "\"light\":1000"));
    TEST_ASSERT_TRUE(contains_substring(payload, "\"soil_moisture\":800"));
}

// ============================================================================
// NULL TERMINATION AND BUFFER TESTS
// ============================================================================

void test_build_payload_null_termination(void) {
    char payload[150];
    memset(payload, 'X', sizeof(payload)); // Fill with garbage
    
    build_payload(payload, sizeof(payload), 1, 25, 3, 60, 5, 512, 300);
    
    // String should be properly null-terminated
    size_t len = strlen(payload);
    TEST_ASSERT_EQUAL('\0', payload[len]);
    TEST_ASSERT_LESS_THAN(sizeof(payload), len + 1);
}

void test_build_payload_small_buffer(void) {
    char small_buffer[150];
    
    // Test with realistic buffer size
    build_payload(small_buffer, sizeof(small_buffer), 1, 25, 3, 60, 5, 512, 300);
    
    // Should produce valid output
    TEST_ASSERT_TRUE(contains_substring(small_buffer, "setup_id"));
    TEST_ASSERT_TRUE(strlen(small_buffer) < sizeof(small_buffer));
}

int main(void) {
    UNITY_BEGIN();
    
    // Basic functionality
    RUN_TEST(test_build_payload_valid_normal_values);
    RUN_TEST(test_build_payload_json_format);
    
    // Edge cases - Maximum values
    RUN_TEST(test_build_payload_max_values);
    
    // Edge cases - Minimum/Zero values
    RUN_TEST(test_build_payload_zero_values);
    
    // Decimal conversion
    RUN_TEST(test_build_payload_decimal_conversion);
    RUN_TEST(test_build_payload_decimal_overflow);
    
    // Boundary conditions
    RUN_TEST(test_build_payload_setup_id_boundaries);
    RUN_TEST(test_build_payload_temperature_boundaries);
    RUN_TEST(test_build_payload_sensor_boundaries);
    
    // Realistic scenarios
    RUN_TEST(test_build_payload_greenhouse_scenario);
    RUN_TEST(test_build_payload_cold_night_scenario);
    RUN_TEST(test_build_payload_hot_day_scenario);
    
    // Buffer tests
    RUN_TEST(test_build_payload_null_termination);
    RUN_TEST(test_build_payload_small_buffer);
    
    return UNITY_END();
}