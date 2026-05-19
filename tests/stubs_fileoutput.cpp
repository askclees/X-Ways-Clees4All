#include "main.h"
#include "debugMessage.h"

// Globals defined in main.cpp — provide minimal versions for Tests
ExtractionDetails extractInfo = {};
ExtractOptions extractOpt = {};

// debugMessage stubs — debugMessage.cpp is excluded from the Tests target
void outputErrorMessage(const wchar_t*, LONG) {}
void outputErrorMessage(const wchar_t*) {}
void outputErrorMessage(const wchar_t*, wchar_t*) {}
void errorRaised(LONG, int) {}
int debugWriteDetails(LONG, const wchar_t*) { return 0; }
int debugWriteDetails(const char*) { return 0; }
int debugWriteDetails(LONG, const wchar_t*, const wchar_t*, varList) { return 0; }
