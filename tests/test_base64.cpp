#include "testharness.h"
#include "base64.h"

static void test_null_input_returns_null()
{
    char* result = b64Encode(NULL, 10);
    CHECK(result == NULL);
}

static void test_zero_length_returns_null()
{
    unsigned char data[] = "A";
    char* result = b64Encode(data, 0);
    CHECK(result == NULL);
}

static void test_single_byte()
{
    unsigned char data[] = { 'M' };
    char* result = b64Encode(data, 1);
    CHECK(result != NULL);
    CHECK_STR_EQ(result, "TQ==");
    delete[] result;
}

static void test_two_bytes()
{
    unsigned char data[] = { 'M', 'a' };
    char* result = b64Encode(data, 2);
    CHECK(result != NULL);
    CHECK_STR_EQ(result, "TWE=");
    delete[] result;
}

static void test_three_bytes_no_padding()
{
    unsigned char data[] = { 'M', 'a', 'n' };
    char* result = b64Encode(data, 3);
    CHECK(result != NULL);
    CHECK_STR_EQ(result, "TWFu");
    delete[] result;
}

static void test_rfc4648_vector()
{
    // RFC 4648 test vector: "foobar" -> "Zm9vYmFy"
    unsigned char data[] = "foobar";
    char* result = b64Encode(data, 6);
    CHECK(result != NULL);
    CHECK_STR_EQ(result, "Zm9vYmFy");
    delete[] result;
}

static void test_result_is_null_terminated()
{
    unsigned char data[] = "test";
    char* result = b64Encode(data, 4);
    CHECK(result != NULL);
    size_t len = strlen(result);
    CHECK(result[len] == '\0');
    delete[] result;
}

static void test_output_length_multiple_of_four()
{
    unsigned char data[] = "hello world";
    char* result = b64Encode(data, 11);
    CHECK(result != NULL);
    CHECK(strlen(result) % 4 == 0);
    delete[] result;
}

static void test_known_all_zeros()
{
    unsigned char data[] = { 0, 0, 0 };
    char* result = b64Encode(data, 3);
    CHECK(result != NULL);
    CHECK_STR_EQ(result, "AAAA");
    delete[] result;
}

void run_base64_tests()
{
    printf("base64 tests:\n");
    RUN(test_null_input_returns_null);
    RUN(test_zero_length_returns_null);
    RUN(test_single_byte);
    RUN(test_two_bytes);
    RUN(test_three_bytes_no_padding);
    RUN(test_rfc4648_vector);
    RUN(test_result_is_null_terminated);
    RUN(test_output_length_multiple_of_four);
    RUN(test_known_all_zeros);
}
