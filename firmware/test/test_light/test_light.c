#include "unity.h"
#include "light.h"


void setUp(void) {
    // This is run before EACH test
    // No special setup needed for light sensor tests
}

void tearDown(void) {
    // This is run after EACH test
    // No special teardown needed for light sensor tests
}

void test_light_init_success(void) {
    ADC_Error_t result = light_init();
    TEST_ASSERT_EQUAL(ADC_OK, result); // Should initialize successfully
}

void test_light_measure_raw_returns_valid_range(void) {
    // Initialize the light sensor first
    light_init();
    
    // Measure light level
    uint16_t light_level = light_measure_raw();
    
    // Valid values should be 0-1023 (10-bit ADC resolution)
    // Values > 1023 indicate an error from the ADC
    TEST_ASSERT_LESS_OR_EQUAL(1023, light_level); // Should be within valid range
}

void test_light_measure_raw_dark_vs_bright(void) {
    // This test verifies the inversion logic is working correctly
    // NOTE: This requires manual testing with actual hardware:
    // - Cover the sensor (should return low value near 0)
    // - Shine light on sensor (should return high value near 1023)
    
    light_init();
    uint16_t light_level = light_measure_raw();
    
    // At minimum, verify it returns a valid value
    TEST_ASSERT_LESS_OR_EQUAL(1023, light_level);
    
    // For automated testing, we can only verify the range
    // Manual verification needed to confirm:
    // - Dark conditions: light_level should be LOW (near 0)
    // - Bright conditions: light_level should be HIGH (near 1023)
}

void test_light_multiple_measurements(void) {
    // Verify that multiple consecutive measurements work correctly
    light_init();
    
    uint16_t reading1 = light_measure_raw();
    uint16_t reading2 = light_measure_raw();
    uint16_t reading3 = light_measure_raw();
    
    // All readings should be valid
    TEST_ASSERT_LESS_OR_EQUAL(1023, reading1);
    TEST_ASSERT_LESS_OR_EQUAL(1023, reading2);
    TEST_ASSERT_LESS_OR_EQUAL(1023, reading3);
    
    // Readings should be relatively stable (within reasonable variance)
    // Note: This might fail if light conditions change rapidly during test
    uint16_t max = reading1 > reading2 ? reading1 : reading2;
    max = max > reading3 ? max : reading3;
    uint16_t min = reading1 < reading2 ? reading1 : reading2;
    min = min < reading3 ? min : reading3;
    
    // Allow up to 50 units variance for stable conditions
    TEST_ASSERT_LESS_OR_EQUAL(50, max - min);
}

void test_light_measure_before_init(void) {
    // Test behavior when measuring without initialization
    // NOTE: Behavior depends on ADC implementation
    // This test documents expected behavior
    
    uint16_t light_level = light_measure_raw();
    
    // Should still return a value (ADC might auto-initialize or return error)
    // If ADC returns error value > 1023, this documents that behavior
    (void)light_level; // Mark as intentionally unused for documentation test
    TEST_MESSAGE("Measuring without init - behavior depends on ADC implementation");
}

int main(void) {
    UNITY_BEGIN();
    
    // Basic functionality tests
    RUN_TEST(test_light_init_success);
    RUN_TEST(test_light_measure_raw_returns_valid_range);
    RUN_TEST(test_light_multiple_measurements);
    
    // Behavioral tests (require hardware/manual verification)
    RUN_TEST(test_light_measure_raw_dark_vs_bright);
    
    // Edge case tests
    RUN_TEST(test_light_measure_before_init);
    
    return UNITY_END();
}