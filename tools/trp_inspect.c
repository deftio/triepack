/**
 * @file trp_inspect.c
 * @brief CLI tool for inspecting and converting .trp files.
 *
 * Commands:
 *   trp info     <file>                    Header metadata
 *   trp list     <file>                    All keys with types and values
 *   trp get      <file> <key>              Single key lookup
 *   trp dump     <file>                    Hex dump of header + structure
 *   trp search   <file> <prefix>           Prefix search
 *   trp validate <file>                    Validate integrity
 *   trp encode   <input.json> [-o out.trp] JSON -> TriePack
 *   trp decode   <input.trp> [-o out.json] TriePack -> JSON [--pretty]
 *   trp help                               Full usage
 *   trp version                            Print version
 *
 * Use '-' as the file argument to read from stdin.
 *
 * Copyright (c) 2026 M. A. Chatterjee <deftio at deftio dot com>
 * BSD-2-Clause -- see LICENSE.txt
 */

/* Expose POSIX isatty/fileno under strict C99 (must precede all includes) */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#include <unistd.h>
#endif

#include "triepack/triepack.h"
#include "triepack/triepack_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#endif

/* ── ANSI colour helpers ─────────────────────────────────────────────── */

static int use_color;

static void init_color(void)
{
    use_color = isatty(fileno(stdout));
}

static const char *c_reset(void)
{
    return use_color ? "\033[0m" : "";
}
static const char *c_bold(void)
{
    return use_color ? "\033[1m" : "";
}
static const char *c_dim(void)
{
    return use_color ? "\033[2m" : "";
}
static const char *c_green(void)
{
    return use_color ? "\033[32m" : "";
}
static const char *c_cyan(void)
{
    return use_color ? "\033[36m" : "";
}
static const char *c_yellow(void)
{
    return use_color ? "\033[33m" : "";
}
static const char *c_magenta(void)
{
    return use_color ? "\033[35m" : "";
}

/* ── File I/O ────────────────────────────────────────────────────────── */

static uint8_t *read_stdin(size_t *out_len)
{
    size_t cap = 4096;
    size_t len = 0;
    uint8_t *buf = (uint8_t *)malloc(cap);
    if (!buf) {
        fprintf(stderr, "trp: out of memory reading stdin\n");
        return NULL;
    }

#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
#endif

    for (;;) {
        size_t n = fread(buf + len, 1, cap - len, stdin);
        len += n;
        if (n == 0)
            break;
        if (len == cap) {
            cap *= 2;
            uint8_t *tmp = (uint8_t *)realloc(buf, cap);
            if (!tmp) {
                fprintf(stderr, "trp: out of memory reading stdin\n");
                free(buf);
                return NULL;
            }
            buf = tmp;
        }
    }

    if (ferror(stdin)) {
        fprintf(stderr, "trp: error reading stdin\n");
        free(buf);
        return NULL;
    }

    *out_len = len;
    return buf;
}

static int write_file(const char *path, const uint8_t *buf, size_t len)
{
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "trp: cannot open '%s' for writing: ", path);
        perror(NULL);
        return -1;
    }
    if (len > 0 && fwrite(buf, 1, len, f) != len) {
        fprintf(stderr, "trp: short write to '%s'\n", path);
        fclose(f);
        return -1;
    }
    fclose(f);
    return 0;
}

static uint8_t *read_file(const char *path, size_t *out_len)
{
    if (strcmp(path, "-") == 0)
        return read_stdin(out_len);

    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "trp: cannot open '%s': ", path);
        perror(NULL);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz < 0) {
        fprintf(stderr, "trp: cannot determine size of '%s'\n", path);
        fclose(f);
        return NULL;
    }

    uint8_t *buf = (uint8_t *)malloc((size_t)sz);
    if (!buf) {
        fprintf(stderr, "trp: out of memory reading '%s'\n", path);
        fclose(f);
        return NULL;
    }

    if (sz > 0 && fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        fprintf(stderr, "trp: short read on '%s'\n", path);
        free(buf);
        fclose(f);
        return NULL;
    }

    fclose(f);
    *out_len = (size_t)sz;
    return buf;
}

/* ── Value formatting ────────────────────────────────────────────────── */

static const char *type_name(tp_value_type t)
{
    switch (t) {
    case TP_NULL:
        return "null";
    case TP_BOOL:
        return "bool";
    case TP_INT:
        return "int";
    case TP_UINT:
        return "uint";
    case TP_FLOAT32:
        return "float32";
    case TP_FLOAT64:
        return "float64";
    case TP_STRING:
        return "string";
    case TP_BLOB:
        return "blob";
    case TP_ARRAY:
        return "array";
    case TP_DICT:
        return "dict";
    }
    return "unknown";
}

static const char *type_color(tp_value_type t)
{
    switch (t) {
    case TP_NULL:
        return c_dim();
    case TP_BOOL:
        return c_yellow();
    case TP_INT:
    case TP_UINT:
        return c_cyan();
    case TP_FLOAT32:
    case TP_FLOAT64:
        return c_magenta();
    case TP_STRING:
        return c_green();
    default:
        return c_reset();
    }
}

static void print_value(const tp_value *val)
{
    if (!val) {
        printf("%s(no value)%s", c_dim(), c_reset());
        return;
    }
    switch (val->type) {
    case TP_NULL:
        printf("%snull%s", c_dim(), c_reset());
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
        printf("%g", (double)val->data.float32_val);
        break;
    case TP_FLOAT64:
        printf("%g", val->data.float64_val);
        break;
    case TP_STRING:
        printf("\"%.*s\"", (int)val->data.string_val.str_len, val->data.string_val.str);
        break;
    case TP_BLOB:
        printf("<%zu bytes>", val->data.blob_val.len);
        break;
    case TP_ARRAY:
        printf("[%zu elements]", val->data.array_val.count);
        break;
    case TP_DICT:
        printf("{%zu bytes}", val->data.dict_val.dict_len);
        break;
    }
}

/* ── Addressing mode name ────────────────────────────────────────────── */

static const char *addr_mode_name(tp_addr_mode m)
{
    switch (m) {
    case TP_ADDR_BIT:
        return "bit";
    case TP_ADDR_BYTE:
        return "byte";
    case TP_ADDR_SYMBOL_FIXED:
        return "symbol-fixed";
    case TP_ADDR_SYMBOL_UTF8:
        return "symbol-utf8";
    }
    return "unknown";
}

static const char *checksum_name(tp_checksum_type c)
{
    switch (c) {
    case TP_CHECKSUM_CRC32:
        return "CRC-32";
    case TP_CHECKSUM_SHA256:
        return "SHA-256";
    case TP_CHECKSUM_XXHASH64:
        return "xxHash64";
    }
    return "unknown";
}

/* ── Commands ────────────────────────────────────────────────────────── */

static int cmd_info(const char *path)
{
    size_t len = 0;
    uint8_t *buf = read_file(path, &len);
    if (!buf)
        return 2;

    tp_dict *dict = NULL;
    tp_result rc = tp_dict_open(&dict, buf, len);
    if (rc != TP_OK) {
        fprintf(stderr, "trp: %s: %s\n", path, tp_result_str(rc));
        free(buf);
        return 2;
    }

    tp_dict_info info;
    tp_dict_get_info(dict, &info);

    printf("%sFile:%s       %s\n", c_bold(), c_reset(), path);
    printf("%sFormat:%s     TRP v%u.%u\n", c_bold(), c_reset(), info.format_version_major,
           info.format_version_minor);
    printf("%sKeys:%s       %u\n", c_bold(), c_reset(), info.num_keys);
    printf("%sSize:%s       %zu bytes\n", c_bold(), c_reset(), info.total_bytes);
    printf("%sChecksum:%s   %s (valid)\n", c_bold(), c_reset(), checksum_name(info.checksum_type));
    printf("%sTrie mode:%s  %s\n", c_bold(), c_reset(), addr_mode_name(info.trie_mode));
    printf("%sValue mode:%s %s\n", c_bold(), c_reset(), addr_mode_name(info.value_mode));
    printf("%sBPS:%s        %u\n", c_bold(), c_reset(), info.bits_per_symbol);
    printf("%sSuffix:%s     %s\n", c_bold(), c_reset(), info.has_suffix_table ? "yes" : "no");
    printf("%sCompact:%s    %s\n", c_bold(), c_reset(), info.compact_mode ? "yes" : "no");

    tp_dict_close(&dict);
    free(buf);
    return 0;
}

static int cmd_list(const char *path)
{
    size_t len = 0;
    uint8_t *buf = read_file(path, &len);
    if (!buf)
        return 2;

    tp_dict *dict = NULL;
    tp_result rc = tp_dict_open(&dict, buf, len);
    if (rc != TP_OK) {
        fprintf(stderr, "trp: %s: %s\n", path, tp_result_str(rc));
        free(buf);
        return 2;
    }

    tp_iterator *it = NULL;
    rc = tp_dict_iterate(dict, &it);
    if (rc != TP_OK) {
        fprintf(stderr, "trp: iterate: %s\n", tp_result_str(rc));
        tp_dict_close(&dict);
        free(buf);
        return 2;
    }

    const char *key = NULL;
    size_t key_len = 0;
    tp_value val;

    while (tp_iter_next(it, &key, &key_len, &val) == TP_OK) {
        printf("%-20.*s %s%-8s%s ", (int)key_len, key, type_color(val.type), type_name(val.type),
               c_reset());
        print_value(&val);
        printf("\n");
    }

    tp_iter_destroy(&it);
    tp_dict_close(&dict);
    free(buf);
    return 0;
}

static int cmd_get(const char *path, const char *key)
{
    size_t len = 0;
    uint8_t *buf = read_file(path, &len);
    if (!buf)
        return 2;

    tp_dict *dict = NULL;
    tp_result rc = tp_dict_open(&dict, buf, len);
    if (rc != TP_OK) {
        fprintf(stderr, "trp: %s: %s\n", path, tp_result_str(rc));
        free(buf);
        return 2;
    }

    tp_value val;
    rc = tp_dict_lookup(dict, key, &val);
    if (rc == TP_ERR_NOT_FOUND) {
        fprintf(stderr, "trp: key '%s' not found\n", key);
        tp_dict_close(&dict);
        free(buf);
        return 3;
    }
    if (rc != TP_OK) {
        fprintf(stderr, "trp: lookup: %s\n", tp_result_str(rc));
        tp_dict_close(&dict);
        free(buf);
        return 2;
    }

    printf("%stype:%s  %s%s%s\n", c_bold(), c_reset(), type_color(val.type), type_name(val.type),
           c_reset());
    printf("%svalue:%s ", c_bold(), c_reset());
    print_value(&val);
    printf("\n");

    tp_dict_close(&dict);
    free(buf);
    return 0;
}

static int cmd_dump(const char *path)
{
    size_t len = 0;
    uint8_t *buf = read_file(path, &len);
    if (!buf)
        return 2;

    /* Print hex dump of header (first 32 bytes or less) */
    size_t hdr_len = len < TP_HEADER_SIZE ? len : TP_HEADER_SIZE;
    printf("%s── Header (%zu bytes) ──%s\n", c_bold(), hdr_len, c_reset());

    for (size_t i = 0; i < hdr_len; i++) {
        if (i > 0 && i % 16 == 0)
            printf("\n");
        printf("%02x ", buf[i]);
    }
    printf("\n");

    if (len >= 4) {
        printf("\n%sMagic:%s %c%c%c\\0\n", c_bold(), c_reset(), (char)buf[0], (char)buf[1],
               (char)buf[2]);
    }

    /* Try to open and show summary */
    tp_dict *dict = NULL;
    tp_result rc = tp_dict_open(&dict, buf, len);
    if (rc == TP_OK) {
        tp_dict_info info;
        tp_dict_get_info(dict, &info);

        printf("\n%s── Structure ──%s\n", c_bold(), c_reset());
        printf("Version:    %u.%u\n", info.format_version_major, info.format_version_minor);
        printf("Keys:       %u\n", info.num_keys);
        printf("BPS:        %u\n", info.bits_per_symbol);
        printf("Trie mode:  %s\n", addr_mode_name(info.trie_mode));
        printf("Value mode: %s\n", addr_mode_name(info.value_mode));
        printf("Checksum:   %s\n", checksum_name(info.checksum_type));
        printf("Has values: %s\n", info.has_values ? "yes" : "no");
        printf("Total:      %zu bytes\n", info.total_bytes);

        tp_dict_close(&dict);
    } else {
        printf("\n%sNote:%s Could not parse structure: %s\n", c_bold(), c_reset(),
               tp_result_str(rc));
    }

    free(buf);
    return 0;
}

static int cmd_search(const char *path, const char *prefix)
{
    size_t len = 0;
    uint8_t *buf = read_file(path, &len);
    if (!buf)
        return 2;

    tp_dict *dict = NULL;
    tp_result rc = tp_dict_open(&dict, buf, len);
    if (rc != TP_OK) {
        fprintf(stderr, "trp: %s: %s\n", path, tp_result_str(rc));
        free(buf);
        return 2;
    }

    tp_iterator *it = NULL;
    rc = tp_dict_find_prefix(dict, prefix, &it);
    if (rc != TP_OK) {
        fprintf(stderr, "trp: prefix search: %s\n", tp_result_str(rc));
        tp_dict_close(&dict);
        free(buf);
        return 2;
    }

    const char *key = NULL;
    size_t key_len = 0;
    tp_value val;
    uint32_t count = 0;

    while (tp_iter_next(it, &key, &key_len, &val) == TP_OK) {
        printf("%-20.*s %s%-8s%s ", (int)key_len, key, type_color(val.type), type_name(val.type),
               c_reset());
        print_value(&val);
        printf("\n");
        count++;
    }

    if (count == 0) {
        printf("No keys matching prefix '%s'\n", prefix);
    } else {
        printf("\n%u key(s) found\n", count);
    }

    tp_iter_destroy(&it);
    tp_dict_close(&dict);
    free(buf);
    return 0;
}

/* ── Option parsing for encode/decode ────────────────────────────────── */

/**
 * Scan argv[start..argc-1] for -o <path> and --pretty.
 * Returns the index of the input file argument (first non-flag arg),
 * or -1 if no input file was found.
 */
static int parse_opts(int argc, char **argv, int start, const char **output, int *pretty)
{
    int input_idx = -1;
    *output = NULL;
    *pretty = 0;

    for (int i = start; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 < argc) {
                *output = argv[++i];
            } else {
                fprintf(stderr, "trp: -o requires a path argument\n");
            }
        } else if (strcmp(argv[i], "--pretty") == 0) {
            *pretty = 1;
        } else if (input_idx < 0) {
            input_idx = i;
        }
    }
    return input_idx;
}

static int cmd_encode(int argc, char **argv)
{
    const char *output = NULL;
    int pretty = 0;
    int input_idx = parse_opts(argc, argv, 2, &output, &pretty);

    if (input_idx < 0) {
        fprintf(stderr, "trp: encode requires an input file argument\n");
        return 1;
    }

    const char *input = argv[input_idx];
    size_t json_len = 0;
    uint8_t *json_buf = read_file(input, &json_len);
    if (!json_buf)
        return 2;

    uint8_t *trp_buf = NULL;
    size_t trp_len = 0;
    tp_result rc = tp_json_encode((const char *)json_buf, json_len, &trp_buf, &trp_len);
    free(json_buf);

    if (rc != TP_OK) {
        fprintf(stderr, "trp: encode: %s\n", tp_result_str(rc));
        return 2;
    }

    if (output) {
        if (write_file(output, trp_buf, trp_len) != 0) {
            free(trp_buf);
            return 2;
        }
        fprintf(stderr, "Encoded %zu bytes -> %s\n", trp_len, output);
    } else {
#ifdef _WIN32
        _setmode(_fileno(stdout), _O_BINARY);
#endif
        if (trp_len > 0)
            fwrite(trp_buf, 1, trp_len, stdout);
    }

    free(trp_buf);
    return 0;
}

static int cmd_decode(int argc, char **argv)
{
    const char *output = NULL;
    int pretty = 0;
    int input_idx = parse_opts(argc, argv, 2, &output, &pretty);

    if (input_idx < 0) {
        fprintf(stderr, "trp: decode requires an input file argument\n");
        return 1;
    }

    const char *input = argv[input_idx];
    size_t trp_len = 0;
    uint8_t *trp_buf = read_file(input, &trp_len);
    if (!trp_buf)
        return 2;

    char *json_str = NULL;
    size_t json_len = 0;
    tp_result rc;

    if (pretty)
        rc = tp_json_decode_pretty(trp_buf, trp_len, "  ", &json_str, &json_len);
    else
        rc = tp_json_decode(trp_buf, trp_len, &json_str, &json_len);

    free(trp_buf);

    if (rc != TP_OK) {
        fprintf(stderr, "trp: decode: %s\n", tp_result_str(rc));
        return 2;
    }

    if (output) {
        if (write_file(output, (const uint8_t *)json_str, json_len) != 0) {
            free(json_str);
            return 2;
        }
    } else {
        printf("%.*s\n", (int)json_len, json_str);
    }

    free(json_str);
    return 0;
}

static int cmd_validate(const char *path)
{
    size_t len = 0;
    uint8_t *buf = read_file(path, &len);
    if (!buf)
        return 2;

    tp_dict *dict = NULL;
    tp_result rc = tp_dict_open(&dict, buf, len);
    if (rc != TP_OK) {
        printf("%s: INVALID -- %s\n", path, tp_result_str(rc));
        free(buf);
        return 2;
    }

    tp_dict_info info;
    tp_dict_get_info(dict, &info);
    printf("%s: valid (%u keys, %zu bytes)\n", path, info.num_keys, info.total_bytes);

    tp_dict_close(&dict);
    free(buf);
    return 0;
}

static void print_usage(void)
{
    printf("Usage: trp <command> [args...]\n"
           "\n"
           "Inspection:\n"
           "  info     <file>            Show header metadata\n"
           "  list     <file>            List all keys with types and values\n"
           "  get      <file> <key>      Look up a single key\n"
           "  dump     <file>            Hex dump of header + structure summary\n"
           "  search   <file> <prefix>   Prefix search\n"
           "  validate <file>            Validate integrity (magic + checksum)\n"
           "\n"
           "Conversion:\n"
           "  encode   <input.json> [-o output.trp]   JSON -> TriePack\n"
           "  decode   <input.trp>  [-o output.json]  TriePack -> JSON [--pretty]\n"
           "\n"
           "General:\n"
           "  help                       Show this help\n"
           "  version                    Print version\n"
           "\n"
           "Use '-' as the file argument to read from stdin.\n"
           "\n"
           "Exit codes:\n"
           "  0  Success\n"
           "  1  Usage error\n"
           "  2  File or decode error\n"
           "  3  Key not found\n");
}

static void print_version(void)
{
    printf("trp 1.0.0 (TriePack CLI inspector)\n");
}

/* ── Main ────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    init_color();

    if (argc < 2) {
        print_usage();
        return 1;
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "--help") == 0 || strcmp(cmd, "-h") == 0) {
        print_usage();
        return 0;
    }

    if (strcmp(cmd, "version") == 0 || strcmp(cmd, "--version") == 0) {
        print_version();
        return 0;
    }

    if (strcmp(cmd, "info") == 0) {
        if (argc < 3) {
            fprintf(stderr, "trp: info requires a file argument\n");
            return 1;
        }
        return cmd_info(argv[2]);
    }

    if (strcmp(cmd, "list") == 0) {
        if (argc < 3) {
            fprintf(stderr, "trp: list requires a file argument\n");
            return 1;
        }
        return cmd_list(argv[2]);
    }

    if (strcmp(cmd, "get") == 0) {
        if (argc < 4) {
            fprintf(stderr, "trp: get requires <file> and <key> arguments\n");
            return 1;
        }
        return cmd_get(argv[2], argv[3]);
    }

    if (strcmp(cmd, "dump") == 0) {
        if (argc < 3) {
            fprintf(stderr, "trp: dump requires a file argument\n");
            return 1;
        }
        return cmd_dump(argv[2]);
    }

    if (strcmp(cmd, "search") == 0) {
        if (argc < 4) {
            fprintf(stderr, "trp: search requires <file> and <prefix> arguments\n");
            return 1;
        }
        return cmd_search(argv[2], argv[3]);
    }

    if (strcmp(cmd, "validate") == 0) {
        if (argc < 3) {
            fprintf(stderr, "trp: validate requires a file argument\n");
            return 1;
        }
        return cmd_validate(argv[2]);
    }

    if (strcmp(cmd, "encode") == 0) {
        return cmd_encode(argc, argv);
    }

    if (strcmp(cmd, "decode") == 0) {
        return cmd_decode(argc, argv);
    }

    fprintf(stderr, "trp: unknown command '%s' (try 'trp help')\n", cmd);
    return 1;
}
