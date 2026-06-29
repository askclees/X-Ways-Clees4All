#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "VICS.h"
#include "cJSON.h"
#include "testharness.h"

int _tc_pass = 0, _tc_fail = 0;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static FILE* openTmp()
{
    return fopen("_vics_test.tmp", "w+b");
}

// Rewinds f, reads full contents into a heap buffer, closes f. Caller must
// delete[] the returned pointer.
static char* readTmp(FILE* f)
{
    fflush(f);
    rewind(f);
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);
    char* buf = new char[len + 1];
    fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);
    return buf;
}

// Returns a VICSRecord with MD5 pre-filled (minimum required for a valid record).
static VICSRecord makeRecord()
{
    VICSRecord rec;
    initializeVICSRecord(rec);
    wcscpy(rec.vMedia.MD5, L"AABBCCDDEEFF00112233445566778899");
    return rec;
}

// Returns a heap-allocated VICSMediaFile with mandatory fields filled.
static VICSMediaFile* makeMediaFile(const wchar_t* md5, const wchar_t* name, const wchar_t* path)
{
    VICSMediaFile* f = new VICSMediaFile;
    initializeMediaFileRecord(*f);
    wcscpy(f->MD5, md5);
    f->fileName = new wchar_t[wcslen(name) + 1];
    wcscpy(f->fileName, name);
    f->filePath = new wchar_t[wcslen(path) + 1];
    wcscpy(f->filePath, path);
    return f;
}

static void freeMediaFile(VICSMediaFile* f)
{
    deallocateMediaFileRecord(*f);
    delete f;
}

// ---------------------------------------------------------------------------
// Tests: writeMediaRecord NULL / empty guards
// ---------------------------------------------------------------------------

static void test_null_file()
{
    VICSRecord rec = makeRecord();
    CHECK(writeMediaRecord(NULL, &rec) == -1);
}

static void test_null_record()
{
    FILE* f = openTmp();
    CHECK(writeMediaRecord(f, NULL) == -1);
    fclose(f);
}

static void test_empty_md5_rejected()
{
    FILE* f = openTmp();
    VICSRecord rec;
    initializeVICSRecord(rec);
    // MD5 left empty by initializeVICSRecord — mandatory field absent
    CHECK(writeMediaRecord(f, &rec) == -1);
    fclose(f);
}

// ---------------------------------------------------------------------------
// Tests: JSON structure
// ---------------------------------------------------------------------------

static void test_output_is_valid_json()
{
    FILE* f = openTmp();
    VICSRecord rec = makeRecord();
    rec.vMedia.MediaID   = 1;
    rec.vMedia.MediaSize = 2048;
    rec.vMedia.Category  = 1;
    CHECK(writeMediaRecord(f, &rec) == 0);
    char* json = readTmp(f);
    cJSON* parsed = cJSON_Parse(json);
    CHECK(parsed != NULL);
    if (parsed) cJSON_Delete(parsed);
    delete[] json;
}

static void test_basic_keys_present()
{
    FILE* f = openTmp();
    VICSRecord rec = makeRecord();
    rec.vMedia.MediaID = 7;
    wcscpy(rec.vMedia.SHA1, L"DA39A3EE5E6B4B0D3255BFEF95601890AFD80709");
    writeMediaRecord(f, &rec);
    char* json = readTmp(f);
    CHECK_CONTAINS(json, "\"MediaID\"");
    CHECK_CONTAINS(json, "\"MD5\"");
    CHECK_CONTAINS(json, "\"SHA1\"");
    CHECK_CONTAINS(json, "AABBCCDDEEFF00112233445566778899");
    CHECK_CONTAINS(json, "DA39A3EE5E6B4B0D3255BFEF95601890AFD80709");
    delete[] json;
}

// ---------------------------------------------------------------------------
// Tests: INT64 precision (the key regression guard)
// ---------------------------------------------------------------------------

static void test_mediaid_written_as_integer()
{
    FILE* f = openTmp();
    VICSRecord rec = makeRecord();
    rec.vMedia.MediaID = 42;
    writeMediaRecord(f, &rec);
    char* json = readTmp(f);
    // "MediaID": 42  — exact integer, not 42.0 or scientific notation
    CHECK_CONTAINS(json, "\"MediaID\"");
    CHECK_CONTAINS(json, "42");
    CHECK_NOT_CONTAINS(json, "42.0");
    delete[] json;
}

static void test_mediasize_large_int64_exact()
{
    // 2^53 + 1 = 9007199254740993. Doubles cannot represent this value
    // exactly — they round it to 9007199254740992. cjsonAddInt64 must
    // preserve the exact digit string via cJSON_CreateRaw.
    FILE* f = openTmp();
    VICSRecord rec = makeRecord();
    rec.vMedia.MediaSize = 9007199254740993LL;
    writeMediaRecord(f, &rec);
    char* json = readTmp(f);
    CHECK_CONTAINS(json,     "9007199254740993");  // exact value
    CHECK_NOT_CONTAINS(json, "9007199254740992");  // double-rounded value
    delete[] json;
}

static void test_physicallocation_large_int64_exact()
{
    FILE* f = openTmp();
    VICSRecord rec = makeRecord();
    rec.vMediaFiles = makeMediaFile(
        L"AABBCCDDEEFF00112233445566778899",
        L"image.jpg",
        L"C:\\Evidence\\image.jpg"
    );
    rec.vMediaFiles->physicalLocation = 9007199254740993LL;
    rec.noMediaFiles = 1;
    writeMediaRecord(f, &rec);
    char* json = readTmp(f);
    CHECK_CONTAINS(json,     "9007199254740993");
    CHECK_NOT_CONTAINS(json, "9007199254740992");
    freeMediaFile(rec.vMediaFiles);
    delete[] json;
}

// ---------------------------------------------------------------------------
// Tests: Optional fields
// ---------------------------------------------------------------------------

static void test_category_zero_omitted()
{
    FILE* f = openTmp();
    VICSRecord rec = makeRecord();
    rec.vMedia.Category = 0;
    writeMediaRecord(f, &rec);
    char* json = readTmp(f);
    CHECK_NOT_CONTAINS(json, "\"Category\"");
    delete[] json;
}

static void test_category_nonzero_included()
{
    FILE* f = openTmp();
    VICSRecord rec = makeRecord();
    rec.vMedia.Category = 3;
    writeMediaRecord(f, &rec);
    char* json = readTmp(f);
    CHECK_CONTAINS(json, "\"Category\"");
    delete[] json;
}

static void test_sha1_absent_when_empty()
{
    FILE* f = openTmp();
    VICSRecord rec = makeRecord();
    // SHA1[0] == L'\0' from initializeVICSRecord
    writeMediaRecord(f, &rec);
    char* json = readTmp(f);
    CHECK_NOT_CONTAINS(json, "\"SHA1\"");
    delete[] json;
}

static void test_sha1_present_when_set()
{
    FILE* f = openTmp();
    VICSRecord rec = makeRecord();
    wcscpy(rec.vMedia.SHA1, L"DA39A3EE5E6B4B0D3255BFEF95601890AFD80709");
    writeMediaRecord(f, &rec);
    char* json = readTmp(f);
    CHECK_CONTAINS(json, "\"SHA1\"");
    CHECK_CONTAINS(json, "DA39A3EE5E6B4B0D3255BFEF95601890AFD80709");
    delete[] json;
}

static void test_mediasize_zero_omitted()
{
    FILE* f = openTmp();
    VICSRecord rec = makeRecord();
    rec.vMedia.MediaSize = 0;
    writeMediaRecord(f, &rec);
    char* json = readTmp(f);
    CHECK_NOT_CONTAINS(json, "\"MediaSize\"");
    delete[] json;
}

// ---------------------------------------------------------------------------
// Tests: Boolean flags
// ---------------------------------------------------------------------------

static void test_victimid_written_when_true()
{
    FILE* f = openTmp();
    VICSRecord rec = makeRecord();
    rec.vMedia.VictimID = TRUE;
    writeMediaRecord(f, &rec);
    char* json = readTmp(f);
    CHECK_CONTAINS(json, "\"VictimIdentified\"");
    delete[] json;
}

static void test_victimid_absent_when_false()
{
    FILE* f = openTmp();
    VICSRecord rec = makeRecord();
    rec.vMedia.VictimID = FALSE;
    writeMediaRecord(f, &rec);
    char* json = readTmp(f);
    CHECK_NOT_CONTAINS(json, "\"VictimIdentified\"");
    delete[] json;
}

static void test_offenderid_written_when_true()
{
    FILE* f = openTmp();
    VICSRecord rec = makeRecord();
    rec.vMedia.OffenderID = TRUE;
    writeMediaRecord(f, &rec);
    char* json = readTmp(f);
    CHECK_CONTAINS(json, "\"OffenderIdentified\"");
    delete[] json;
}

static void test_isdistributed_written_when_true()
{
    FILE* f = openTmp();
    VICSRecord rec = makeRecord();
    rec.vMedia.IsDistributed = TRUE;
    writeMediaRecord(f, &rec);
    char* json = readTmp(f);
    CHECK_CONTAINS(json, "\"IsDistributed\"");
    delete[] json;
}

static void test_precat_fields_written_when_set()
{
    FILE* f = openTmp();
    VICSRecord rec = makeRecord();
    rec.vMedia.IsPreCat = TRUE;
    rec.vMedia.PrecatSource = new wchar_t[32];
    wcscpy(rec.vMedia.PrecatSource, L"NCMEC");
    writeMediaRecord(f, &rec);
    char* json = readTmp(f);
    CHECK_CONTAINS(json, "\"IsPrecategorized\"");
    CHECK_CONTAINS(json, "\"PrecategorizationSource\"");
    CHECK_CONTAINS(json, "NCMEC");
    delete[] rec.vMedia.PrecatSource;
    rec.vMedia.PrecatSource = NULL;
    delete[] json;
}

static void test_precat_fields_absent_when_not_set()
{
    FILE* f = openTmp();
    VICSRecord rec = makeRecord();
    rec.vMedia.IsPreCat = FALSE;
    writeMediaRecord(f, &rec);
    char* json = readTmp(f);
    CHECK_NOT_CONTAINS(json, "\"IsPrecategorized\"");
    CHECK_NOT_CONTAINS(json, "\"PrecategorizationSource\"");
    delete[] json;
}

// ---------------------------------------------------------------------------
// Tests: MediaFiles array
// ---------------------------------------------------------------------------

static void test_mediafiles_array_present_with_files()
{
    FILE* f = openTmp();
    VICSRecord rec = makeRecord();
    rec.vMediaFiles = makeMediaFile(
        L"AABBCCDDEEFF00112233445566778899",
        L"photo.jpg",
        L"C:\\Evidence\\photo.jpg"
    );
    rec.noMediaFiles = 1;
    writeMediaRecord(f, &rec);
    char* json = readTmp(f);
    CHECK_CONTAINS(json, "\"MediaFiles\"");
    CHECK_CONTAINS(json, "photo.jpg");
    // Verify it parses as a non-empty array
    cJSON* parsed = cJSON_Parse(json);
    if (parsed) {
        cJSON* files = cJSON_GetObjectItem(parsed, "MediaFiles");
        CHECK(files != NULL);
        if (files) CHECK(cJSON_GetArraySize(files) == 1);
        cJSON_Delete(parsed);
    }
    freeMediaFile(rec.vMediaFiles);
    delete[] json;
}

static void test_mediafiles_array_absent_when_none()
{
    FILE* f = openTmp();
    VICSRecord rec = makeRecord();
    // noMediaFiles == 0 from initializeVICSRecord
    writeMediaRecord(f, &rec);
    char* json = readTmp(f);
    CHECK_NOT_CONTAINS(json, "\"MediaFiles\"");
    delete[] json;
}

static void test_mediafile_skipped_when_md5_empty()
{
    FILE* f = openTmp();
    VICSRecord rec = makeRecord();
    rec.vMediaFiles = new VICSMediaFile[1];
    initializeMediaFileRecord(rec.vMediaFiles[0]);
    // MD5 left empty — should be rejected by buildMediaFileCJSON
    rec.vMediaFiles[0].fileName = new wchar_t[16];
    wcscpy(rec.vMediaFiles[0].fileName, L"bad.jpg");
    rec.vMediaFiles[0].filePath = new wchar_t[16];
    wcscpy(rec.vMediaFiles[0].filePath, L"C:\\bad.jpg");
    rec.noMediaFiles = 1;
    writeMediaRecord(f, &rec);
    char* json = readTmp(f);
    cJSON* parsed = cJSON_Parse(json);
    if (parsed) {
        cJSON* files = cJSON_GetObjectItem(parsed, "MediaFiles");
        if (files) CHECK(cJSON_GetArraySize(files) == 0);
        cJSON_Delete(parsed);
    }
    delete[] rec.vMediaFiles[0].fileName;
    delete[] rec.vMediaFiles[0].filePath;
    delete[] rec.vMediaFiles;
    delete[] json;
}

static void test_mediafile_skipped_when_filename_null()
{
    FILE* f = openTmp();
    VICSRecord rec = makeRecord();
    rec.vMediaFiles = new VICSMediaFile[1];
    initializeMediaFileRecord(rec.vMediaFiles[0]);
    wcscpy(rec.vMediaFiles[0].MD5, L"AABBCCDDEEFF00112233445566778899");
    // fileName left NULL — should be rejected
    rec.vMediaFiles[0].filePath = new wchar_t[16];
    wcscpy(rec.vMediaFiles[0].filePath, L"C:\\test.jpg");
    rec.noMediaFiles = 1;
    writeMediaRecord(f, &rec);
    char* json = readTmp(f);
    cJSON* parsed = cJSON_Parse(json);
    if (parsed) {
        cJSON* files = cJSON_GetObjectItem(parsed, "MediaFiles");
        if (files) CHECK(cJSON_GetArraySize(files) == 0);
        cJSON_Delete(parsed);
    }
    delete[] rec.vMediaFiles[0].filePath;
    delete[] rec.vMediaFiles;
    delete[] json;
}

// ---------------------------------------------------------------------------
// Tests: Special characters in strings
// ---------------------------------------------------------------------------

static void test_special_chars_produce_valid_json()
{
    // Backslash and double-quote in a path must be escaped by cJSON.
    // If escaping fails, cJSON_Parse will return NULL.
    FILE* f = openTmp();
    VICSRecord rec = makeRecord();
    rec.vMedia.RelativeFilePath = new wchar_t[64];
    wcscpy(rec.vMedia.RelativeFilePath, L"folder\\sub\\file \"name\".jpg");
    writeMediaRecord(f, &rec);
    char* json = readTmp(f);
    CHECK_CONTAINS(json, "\"RelativeFilePath\"");
    cJSON* parsed = cJSON_Parse(json);
    CHECK(parsed != NULL);
    if (parsed) cJSON_Delete(parsed);
    delete[] rec.vMedia.RelativeFilePath;
    rec.vMedia.RelativeFilePath = NULL;
    delete[] json;
}

static void test_mimetype_written_correctly()
{
    FILE* f = openTmp();
    VICSRecord rec = makeRecord();
    rec.vMedia.MimeType = new wchar_t[16];
    wcscpy(rec.vMedia.MimeType, L"image/jpeg");
    writeMediaRecord(f, &rec);
    char* json = readTmp(f);
    CHECK_CONTAINS(json, "\"MimeType\"");
    CHECK_CONTAINS(json, "image/jpeg");
    delete[] rec.vMedia.MimeType;
    rec.vMedia.MimeType = NULL;
    delete[] json;
}

// ---------------------------------------------------------------------------
// Tests: validFiletime
// ---------------------------------------------------------------------------

static void test_validfiletime_zero_rejected()
{
    FILETIME ft = {0, 0};
    CHECK(!validFiletime(ft));
}

static void test_validfiletime_below_minimum_rejected()
{
    // dwHighDateTime below minTime (0x015fffff)
    FILETIME ft = {0x00100000, 0};
    CHECK(!validFiletime(ft));
}

static void test_validfiletime_valid_accepted()
{
    // 2023-06-15 — well within the valid range
    SYSTEMTIME st = {};
    st.wYear = 2023; st.wMonth = 6; st.wDay = 15;
    FILETIME ft;
    SystemTimeToFileTime(&st, &ft);
    CHECK(validFiletime(ft));
}

static void test_validfiletime_far_future_rejected()
{
    // 2090 — more than 2 years in the future
    SYSTEMTIME st = {};
    st.wYear = 2090; st.wMonth = 1; st.wDay = 1;
    FILETIME ft;
    SystemTimeToFileTime(&st, &ft);
    CHECK(!validFiletime(ft));
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

void run_base64_tests();
void run_fileoutput_tests();

int main()
{
    printf("=== VICS JSON Tests ===\n\n");

    printf("NULL / empty guards:\n");
    RUN(test_null_file);
    RUN(test_null_record);
    RUN(test_empty_md5_rejected);

    printf("\nJSON structure:\n");
    RUN(test_output_is_valid_json);
    RUN(test_basic_keys_present);

    printf("\nINT64 precision:\n");
    RUN(test_mediaid_written_as_integer);
    RUN(test_mediasize_large_int64_exact);
    RUN(test_physicallocation_large_int64_exact);

    printf("\nOptional fields:\n");
    RUN(test_category_zero_omitted);
    RUN(test_category_nonzero_included);
    RUN(test_sha1_absent_when_empty);
    RUN(test_sha1_present_when_set);
    RUN(test_mediasize_zero_omitted);

    printf("\nBoolean flags:\n");
    RUN(test_victimid_written_when_true);
    RUN(test_victimid_absent_when_false);
    RUN(test_offenderid_written_when_true);
    RUN(test_isdistributed_written_when_true);
    RUN(test_precat_fields_written_when_set);
    RUN(test_precat_fields_absent_when_not_set);

    printf("\nMediaFiles array:\n");
    RUN(test_mediafiles_array_present_with_files);
    RUN(test_mediafiles_array_absent_when_none);
    RUN(test_mediafile_skipped_when_md5_empty);
    RUN(test_mediafile_skipped_when_filename_null);

    printf("\nSpecial characters:\n");
    RUN(test_special_chars_produce_valid_json);
    RUN(test_mimetype_written_correctly);

    printf("\nvalidFiletime:\n");
    RUN(test_validfiletime_zero_rejected);
    RUN(test_validfiletime_below_minimum_rejected);
    RUN(test_validfiletime_valid_accepted);
    RUN(test_validfiletime_far_future_rejected);

    remove("_vics_test.tmp");

    printf("\n=== Base64 Tests ===\n\n");
    run_base64_tests();

    printf("\n=== FileOutput Tests ===\n\n");
    run_fileoutput_tests();

    SUMMARY();
}
