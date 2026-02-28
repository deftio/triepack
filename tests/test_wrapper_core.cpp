/* test_wrapper_core.cpp — C++ wrapper tests */
extern "C" {
#include "unity.h"
}
#include "triepack/triepack.hpp"

void setUp(void) {}
void tearDown(void) {}

void test_placeholder(void)
{
    TEST_PASS();
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_placeholder);
    return UNITY_END();
}
