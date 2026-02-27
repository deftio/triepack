/* test_core_lookup.c — lookup tests for triepack core */
#include "unity.h"
#include "triepack/triepack.h"

void setUp(void) {}
void tearDown(void) {}

void test_placeholder(void) { TEST_PASS(); }

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_placeholder);
    return UNITY_END();
}
