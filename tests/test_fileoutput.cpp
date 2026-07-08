#include "testharness.h"
#include "FileOutput.h"

static void test_relative_path_starts_with_files()
{
    char buffer[256] = {0};
    wchar_t md5[] = L"ABCDEF1234567890ABCDEF1234567890";
    generateRelativeFilePath(buffer, sizeof(buffer), md5, false);
    CHECK_CONTAINS(buffer, "Files");
}

static void test_relative_path_uses_first_two_chars_as_first_folder()
{
    char buffer[256] = {0};
    wchar_t md5[] = L"ABCDEF1234567890ABCDEF1234567890";
    generateRelativeFilePath(buffer, sizeof(buffer), md5, false);
    CHECK_CONTAINS(buffer, "\\AB\\");
}

static void test_relative_path_uses_next_two_chars_as_second_folder()
{
    char buffer[256] = {0};
    wchar_t md5[] = L"ABCDEF1234567890ABCDEF1234567890";
    generateRelativeFilePath(buffer, sizeof(buffer), md5, false);
    CHECK_CONTAINS(buffer, "\\CD");
}

static void test_relative_path_full_structure()
{
    char buffer[256] = {0};
    wchar_t md5[] = L"1234ABCD5678EFGH";
    generateRelativeFilePath(buffer, sizeof(buffer), md5, false);
    CHECK_STR_EQ(buffer, "Files\\12\\34");
}

static void test_vics_path_uses_escaped_backslashes()
{
    char buffer[256] = {0};
    wchar_t md5[] = L"ABCDEF1234567890ABCDEF1234567890";
    generateRelativeFilePath(buffer, sizeof(buffer), md5, true);
    CHECK_CONTAINS(buffer, "\\\\AB\\\\");
}

static void test_vics_path_starts_with_files()
{
    char buffer[256] = {0};
    wchar_t md5[] = L"ABCDEF1234567890ABCDEF1234567890";
    generateRelativeFilePath(buffer, sizeof(buffer), md5, true);
    CHECK_CONTAINS(buffer, "Files");
}

static void test_non_vics_path_has_no_double_backslash()
{
    char buffer[256] = {0};
    wchar_t md5[] = L"ABCDEF1234567890ABCDEF1234567890";
    generateRelativeFilePath(buffer, sizeof(buffer), md5, false);
    CHECK_NOT_CONTAINS(buffer, "\\\\");
}

static void test_buffer_not_overflowed_with_small_size()
{
    // Small buffer — should not write past end
    char buffer[10] = {0};
    buffer[9] = 0x7F;
    wchar_t md5[] = L"ABCDEF1234567890ABCDEF1234567890";
    generateRelativeFilePath(buffer, 9, md5, false);
    CHECK(buffer[9] == 0x7F);
}

static void test_lowercase_md5_chars()
{
    char buffer[256] = {0};
    wchar_t md5[] = L"abcdef1234567890abcdef1234567890";
    generateRelativeFilePath(buffer, sizeof(buffer), md5, false);
    CHECK_CONTAINS(buffer, "\\ab\\");
    CHECK_CONTAINS(buffer, "\\cd");
}

void run_fileoutput_tests()
{
    printf("FileOutput tests:\n");
    RUN(test_relative_path_starts_with_files);
    RUN(test_relative_path_uses_first_two_chars_as_first_folder);
    RUN(test_relative_path_uses_next_two_chars_as_second_folder);
    RUN(test_relative_path_full_structure);
    RUN(test_vics_path_uses_escaped_backslashes);
    RUN(test_vics_path_starts_with_files);
    RUN(test_non_vics_path_has_no_double_backslash);
    RUN(test_buffer_not_overflowed_with_small_size);
    RUN(test_lowercase_md5_chars);
}
