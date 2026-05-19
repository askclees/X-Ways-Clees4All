#ifndef FILEOUTPUT_H_INCLUDED
#define FILEOUTPUT_H_INCLUDED

#include "X-Tension.h"

/** @defgroup fileoutput_errors File Output Error Codes */

/** @ingroup fileoutput_errors @brief Operation completed successfully. */
#define SUCCESS             0
/** @ingroup fileoutput_errors @brief Output file or X-Ways item handle could not be opened. */
#define ERROR_FILE_OPEN     1
/** @ingroup fileoutput_errors @brief Written file size does not match expected size. */
#define FILE_ERROR_SIZE     2
/** @ingroup fileoutput_errors @brief Error flushing output file to disk. */
#define FILE_ERROR_FLUSH    3

/** @ingroup fileoutput_errors @brief Return code: file open error (maps to ERROR_FILE_OPEN). */
#define RETERR_FILE_OPEN        100
/** @ingroup fileoutput_errors @brief Return code: file read or write error. */
#define RETERR_FILE_READ        101
/** @ingroup fileoutput_errors @brief Return code: written file size does not match X-Ways reported size. */
#define RETERR_SIZE_MISMATCH    102

int writeOutputFile(LONG nItemID, bool picFile, wchar_t* fileName, INT64 fileSize, HANDLE hdlCurrVol);
int generateRelativeFilePath(char* buffer, int sizeBuffer, wchar_t* fileName, bool escapedSlash = false);

#endif // FILEOUTPUT_H_INCLUDED
