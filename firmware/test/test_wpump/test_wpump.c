#include "unity.h"
#include "wpump.h"
#include <stdint.h>

// Mocks for hardware registers
uint8_t fake_ddrc = 0;
uint8_t fake_portc = 0;

// Redefine macros to use fakes for unit test
#undef wpump_ddrc
#undef wpump_port
#define wpump_ddrc fake_ddrc
#define wpump_port fake_portc

void setUp(void) {
    fake_ddrc = 0;
    fake_portc = 0xFF; // Assume all high initially
}

void tearDown(void) {}

void test_wpump_configure_sets_ddr_and_off(void) {
    wpump_configure();
    // PC7 should be set as output
    TEST_ASSERT_TRUE((fake_ddrc & (1 << 7)) != 0);
    // PC7 should be powered off (active high, so bit cleared)
    TEST_ASSERT_TRUE((fake_portc & (1 << 7)) == 0);
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

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_wpump_configure_sets_ddr_and_off);
    RUN_TEST(test_wpump_start_turns_on_pump);
    RUN_TEST(test_wpump_stop_turns_off_pump);
    return UNITY_END();
}
