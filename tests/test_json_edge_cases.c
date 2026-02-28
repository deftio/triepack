/* test_json_edge_cases.c — edge_cases tests for triepack json */
#include "triepack/triepack_json.h"
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
