#pragma once
#include <stdio.h>
#include <string.h>

extern int _tc_pass, _tc_fail;

#define CHECK(expr) \
    do { \
        if (expr) { _tc_pass++; } \
        else { _tc_fail++; printf("    FAIL line %d: %s\n", __LINE__, #expr); } \
    } while(0)

#define CHECK_CONTAINS(str, needle) \
    do { \
        if ((str) && strstr((str), (needle))) { _tc_pass++; } \
        else { _tc_fail++; printf("    FAIL line %d: expected \"%s\" in output\n", __LINE__, (needle)); } \
    } while(0)

#define CHECK_NOT_CONTAINS(str, needle) \
    do { \
        if (!(str) || !strstr((str), (needle))) { _tc_pass++; } \
        else { _tc_fail++; printf("    FAIL line %d: did not expect \"%s\" in output\n", __LINE__, (needle)); } \
    } while(0)

#define RUN(fn) \
    do { printf("  " #fn "\n"); fn(); } while(0)

#define SUMMARY() \
    do { \
        printf("\n=== Results: %d passed, %d failed ===\n", _tc_pass, _tc_fail); \
        return _tc_fail > 0 ? 1 : 0; \
    } while(0)
