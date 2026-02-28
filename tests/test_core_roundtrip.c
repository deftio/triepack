/* test_core_roundtrip.c — roundtrip tests for triepack core */
#include "triepack/triepack.h"
#include "unity.h"

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
