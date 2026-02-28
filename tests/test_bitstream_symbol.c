/* test_bitstream_symbol.c — symbol tests for triepack bitstream */
#include "triepack/triepack_bitstream.h"
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
