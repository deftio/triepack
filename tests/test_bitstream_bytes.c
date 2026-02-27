/* test_bitstream_bytes.c — bytes tests for triepack bitstream */
#include "unity.h"
#include "triepack/triepack_bitstream.h"

void setUp(void) {}
void tearDown(void) {}

void test_placeholder(void) { TEST_PASS(); }

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_placeholder);
    return UNITY_END();
}
