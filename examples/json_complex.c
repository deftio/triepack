/*
 * json_complex.c
 *
 * Demonstrates encoding a realistic nested JSON document with DOM-style
 * access. Shows objects, arrays, mixed nesting, all value types, path
 * lookups, and pretty-printed decode.
 *
 * Copyright (c) 2026 M. A. Chatterjee
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "triepack/triepack_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const JSON_INPUT =
    "{"
    "\"application\":{\"name\":\"TriePack Demo\",\"version\":\"2.1.0\",\"debug\":false},"
    "\"database\":{"
    "\"host\":\"db.example.com\",\"port\":5432,"
    "\"credentials\":{\"username\":\"admin\",\"password\":null},"
    "\"pool\":{\"min_connections\":2,\"max_connections\":20,\"timeout_ms\":5000}"
    "},"
    "\"features\":[\"auth\",\"logging\",\"metrics\",\"caching\"],"
    "\"thresholds\":[0.5,0.75,0.9,0.95,0.99],"
    "\"servers\":["
    "{\"name\":\"us-east-1\",\"weight\":3,\"active\":true,\"tags\":[\"primary\",\"production\"]},"
    "{\"name\":\"eu-west-1\",\"weight\":2,\"active\":true,\"tags\":[\"secondary\"]},"
    "{\"name\":\"ap-south-1\",\"weight\":1,\"active\":false,\"tags\":[\"disaster-recovery\"]}"
    "],"
    "\"limits\":{\"requests_per_second\":10000,"
    "\"rate_limit\":{\"window_seconds\":60,\"max_requests\":1000}}"
    "}";

static void print_value(const tp_value *val)
{
    switch (val->type) {
    case TP_NULL:
        printf("null");
        break;
    case TP_BOOL:
        printf("%s", val->data.bool_val ? "true" : "false");
        break;
    case TP_INT:
        printf("%lld", (long long)val->data.int_val);
        break;
    case TP_UINT:
        printf("%llu", (unsigned long long)val->data.uint_val);
        break;
    case TP_FLOAT32:
        printf("%.6f", (double)val->data.float32_val);
        break;
    case TP_FLOAT64:
        printf("%.6f", val->data.float64_val);
        break;
    case TP_STRING:
        printf("\"%.*s\"", (int)val->data.string_val.str_len, val->data.string_val.str);
        break;
    case TP_BLOB:
        printf("<blob %zu bytes>", val->data.blob_val.len);
        break;
    default:
        printf("<unknown>");
        break;
    }
}

static int lookup_and_print(const tp_json *j, const char *path)
{
    tp_value val;
    tp_result rc = tp_json_lookup_path(j, path, &val);
    if (rc == TP_OK) {
        printf("  %-35s = ", path);
        print_value(&val);
        printf("\n");
        return 0;
    }
    printf("  %-35s = ERROR: %s\n", path, tp_result_str(rc));
    return 1;
}

int main(void)
{
    printf("=== Complex JSON Example ===\n\n");

    /* ── Step 1: Encode ──────────────────────────────────────────────── */

    size_t json_len = strlen(JSON_INPUT);
    printf("Input JSON: %zu bytes\n", json_len);

    uint8_t *buf = NULL;
    size_t buf_len = 0;
    tp_result rc = tp_json_encode(JSON_INPUT, json_len, &buf, &buf_len);
    if (rc != TP_OK) {
        fprintf(stderr, "encode: %s\n", tp_result_str(rc));
        return 1;
    }
    printf("Encoded .trp: %zu bytes (%.1f%% of JSON)\n\n", buf_len,
           100.0 * (double)buf_len / (double)json_len);

    /* ── Step 2: DOM lookups ─────────────────────────────────────────── */

    tp_json *j = NULL;
    rc = tp_json_open(&j, buf, buf_len);
    if (rc != TP_OK) {
        fprintf(stderr, "open: %s\n", tp_result_str(rc));
        free(buf);
        return 1;
    }

    printf("DOM lookups:\n");
    lookup_and_print(j, "application.name");
    lookup_and_print(j, "application.version");
    lookup_and_print(j, "application.debug");
    lookup_and_print(j, "database.host");
    lookup_and_print(j, "database.port");
    lookup_and_print(j, "database.credentials.username");
    lookup_and_print(j, "database.credentials.password");
    lookup_and_print(j, "database.pool.max_connections");
    lookup_and_print(j, "features[0]");
    lookup_and_print(j, "features[3]");
    lookup_and_print(j, "thresholds[3]");
    lookup_and_print(j, "servers[0].name");
    lookup_and_print(j, "servers[1].name");
    lookup_and_print(j, "servers[2].active");
    lookup_and_print(j, "servers[0].tags[0]");
    lookup_and_print(j, "limits.requests_per_second");
    lookup_and_print(j, "limits.rate_limit.max_requests");

    tp_json_close(&j);

    /* ── Step 3: Full decode to pretty-printed JSON ──────────────────── */

    printf("\nPretty-printed decode:\n");
    char *pretty = NULL;
    size_t pretty_len = 0;
    rc = tp_json_decode_pretty(buf, buf_len, "  ", &pretty, &pretty_len);
    if (rc != TP_OK) {
        fprintf(stderr, "decode_pretty: %s\n", tp_result_str(rc));
        free(buf);
        return 1;
    }
    printf("%s\n", pretty);

    /* ── Step 4: Cleanup ─────────────────────────────────────────────── */

    free(pretty);
    free(buf);

    printf("Done.\n");
    return 0;
}
