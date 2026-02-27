/* test_json_dom.c — dom tests for triepack json */
#include "unity.h"
#include "triepack/triepack_json.h"

void setUp(void) {}
void tearDown(void) {}

void test_placeholder(void) { TEST_PASS(); }

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_placeholder);
    return UNITY_END();
}
