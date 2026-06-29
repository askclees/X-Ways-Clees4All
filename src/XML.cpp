//standard headers
#include <cstdio>
#include <windows.h>

//project headers
#include "main.h"
#include "FileOutput.h"

static unsigned char BOM[]={0xFF,0xFE};

/**
 * @brief Creates a new XML output file and writes the UTF-16 header and root element opening tag.
 *
 * Currently no error checking is performed if the file cannot be created beyond returning NULL.
 *
 * @param filePath    NULL-terminated path for the file to be created.
 * @param progVersion Wide string containing the program version to embed in the root element.
 * @return FILE pointer to the opened XML file, or NULL on failure.
 *
 * @see closeXML
 */
FILE* createXML(const char* filePath, const wchar_t* progVersion)
{
    FILE* newFile;
    newFile = fopen(filePath,"wb");
    if (newFile == NULL) { return NULL; }
    fwrite(BOM,1,2,newFile);
    fwprintf(newFile,L"<?xml version=\"1.0\"  encoding=\"utf-16\"?>\r\n");
    fwprintf(newFile,L"\t<ReportIndex version=\"2.0\" source=\"X-Ways\" dll=\"Griffeye Extraction %ls\">\r\n", progVersion);
    return newFile;
}

/**
 * @brief Writes the XML root element closing tag and closes the file.
 *
 * @param xmlFile FILE pointer previously opened by createXML.
 *
 * @see createXML
 */
void closeXML(FILE* xmlFile)
{
    fwprintf(xmlFile,L"</ReportIndex>\r\n");
    fclose(xmlFile);
}

/**
 * @brief Writes a single XML record for a picture or video file to the output FILE.
 *
 * @param fr        Reference to the FileRecord containing the metadata to write.
 * @param picture   1 if the record is for a picture, 0 for a video.
 * @param tmpOutput FILE pointer to the XML output file.
 * @param counter   Numeric ID to assign to this media entry.
 * @return 0 always.
 *
 * @see createC4AllRecord
 */
LONG writeXML(FileRecord &fr,int picture, FILE* tmpOutput, INT64 counter)
{
    if (picture==1){
        fwprintf(tmpOutput,L"\t\t<Image>\r\n");
    }
    else{
        fwprintf(tmpOutput,L"\t\t<Movie>\r\n");
    }
    char path_buffer[128]={0};
    generateRelativeFilePath((char*)&path_buffer,128,(wchar_t*)&fr.hashValue,false);
    fwprintf(tmpOutput,L"\t\t\t<path><![CDATA[%s\\]]></path>\r\n",path_buffer);
    if (picture==1){
        fwprintf(tmpOutput,L"\t\t\t<picture>%ls</picture>\r\n",fr.hashValue);
    }
    else{
        fwprintf(tmpOutput,L"\t\t\t<movie>%ls</movie>\r\n",fr.hashValue);
    }
    fwprintf(tmpOutput,L"\t\t\t<category>0</category>\r\n");
    fwprintf(tmpOutput,L"\t\t\t<id>%llu</id>\r\n",counter);
    fwprintf(tmpOutput,L"\t\t\t<fileoffset>0</fileoffset>\r\n");
    fwprintf(tmpOutput,L"\t\t\t<fullpath><![CDATA[%ls]]></fullpath>\r\n",fr.fullPath);
    fwprintf(tmpOutput,L"\t\t\t<created>%llu</created>\r\n",fr.createdTime);
    fwprintf(tmpOutput,L"\t\t\t<accessed>%llu</accessed>\r\n",fr.accessedTime);
    fwprintf(tmpOutput,L"\t\t\t<written>%llu</written>\r\n",fr.modifiedTime);
    fwprintf(tmpOutput,L"\t\t\t<deleted>%llu</deleted>\r\n",fr.deletionTime);
    fwprintf(tmpOutput,L"\t\t\t<hash>%ls</hash>\r\n",fr.hashValue);
    fwprintf(tmpOutput,L"\t\t\t<encaseHash>0</encaseHash>\r\n");
    fwprintf(tmpOutput,L"\t\t\t<myDescription>%ls</myDescription>\r\n",fr.description);
    fwprintf(tmpOutput,L"\t\t\t<physicalLocation>%llu</physicalLocation>\r\n",fr.physicalSector);
    fwprintf(tmpOutput,L"\t\t\t<myUnique>0</myUnique>\r\n");
    fwprintf(tmpOutput,L"\t\t\t<tagged>0</tagged>\r\n");
    fwprintf(tmpOutput,L"\t\t\t<subCat></subCat>\r\n");
    fwprintf(tmpOutput,L"\t\t\t<notes></notes>\r\n");
    fwprintf(tmpOutput,L"\t\t\t<fileSize>%llu</fileSize>\r\n",fr.fileSize);
    if (picture==1){
        fwprintf(tmpOutput,L"\t\t\t<bitDepth></bitDepth>\r\n");
        fwprintf(tmpOutput,L"\t\t\t<aspectRatio></aspectRatio>\r\n");
        fwprintf(tmpOutput,L"\t\t</Image>\r\n");
    }
    else{
        fwprintf(tmpOutput,L"\t\t</Movie>\r\n");
    }
    return 0;

}

/**
 * @brief Replaces the character at a given position in a wide string with a replacement string.
 *
 * Frees the original buffer and returns a newly allocated buffer. The caller becomes responsible
 * for the returned pointer.
 *
 * @param strIn      Pointer to the wide character string to modify (freed by this function).
 * @param position   Zero-based index of the character to replace.
 * @param replaceStr Wide string to substitute in place of the character at position.
 * @return Pointer to a newly allocated wide string with the replacement applied.
 *
 * @see replaceInvalidXMLChars
 */
wchar_t* strReplaceChar(wchar_t* strIn, int position, const wchar_t* replaceStr)
{
    wchar_t* retStr = strIn;
    int originalLen = wcslen(strIn);
    wchar_t* tempStr;
    int increase = wcslen(replaceStr);
    tempStr = new wchar_t[wcslen(retStr)+increase+2];
    wcsncpy(tempStr,retStr,position);
    wcsncpy((wchar_t*)&tempStr[position],(wchar_t*)replaceStr,increase);
    wcsncpy((wchar_t*)&tempStr[position+increase],(wchar_t*)&retStr[position+1],originalLen - position);
    delete[] retStr;
    retStr = tempStr;
    return retStr;
}

/**
 * @brief Replaces characters that are invalid in XML with their entity references.
 *
 * Handles the five XML special characters: &lt; &gt; &quot; &apos; &amp;.
 * Already-escaped entities beginning with &amp; are not double-escaped.
 *
 * @param strIn Pointer to a wide character string to process (may be reallocated).
 * @return Pointer to a wide character string with XML-invalid characters replaced.
 *
 * @see createC4AllRecord
 * @see strReplaceChar
 */
wchar_t* replaceInvalidXMLChars(wchar_t* strIn)
{
    wchar_t* retStr = strIn;
    int length = wcslen(retStr);
    for (int i=0;i<length;i++)
    {
        if (retStr[i] == L'<')
        {
            retStr = strReplaceChar(retStr,i, L"&lt;");
            i += 3;
            length = wcslen(retStr);
        }
        else if (retStr[i] == L'>')
        {
            retStr = strReplaceChar(retStr,i, L"&gt;");
            i += 3;
            length = wcslen(retStr);
        }
        else if (retStr[i] == L'\"')
        {
            retStr = strReplaceChar(retStr,i, L"&quot;");
            i += 5;
            length = wcslen(retStr);
        }
        else if (retStr[i] == L'\'')
        {
            retStr = strReplaceChar(retStr,i, L"&apos;");
            i += 5;
            length = wcslen(retStr);
        }
        else if(retStr[i] == L'&')
        {
            //check its not an already-escaped entity
            bool alreadyEscaped = false;
            if (i + 3 < length && (retStr[i+1] == L'l' || retStr[i+1] == L'g') && retStr[i+2] == L't' && retStr[i+3] == L';')
                alreadyEscaped = true;
            if (i + 5 < length && retStr[i+1] == L'q' && retStr[i+2] == L'u' && retStr[i+3] == L'o' && retStr[i+4] == L't' && retStr[i+5] == L';')
                alreadyEscaped = true;
            if (i + 5 < length && retStr[i+1] == L'a' && retStr[i+2] == L'p' && retStr[i+3] == L'o' && retStr[i+4] == L's' && retStr[i+5] == L';')
                alreadyEscaped = true;
            if (i + 4 < length && retStr[i+1] == L'a' && retStr[i+2] == L'm' && retStr[i+3] == L'p' && retStr[i+4] == L';')
                alreadyEscaped = true;
            if (!alreadyEscaped)
            {
                retStr = strReplaceChar(retStr,i, L"&amp;");
                i += 4;
                length = wcslen(retStr);
            }
        }
    }
    return retStr;
}


/**
 * @brief Replaces non-printable and quote characters in a wide string with underscores, in place.
 *
 * Iterates through the string and substitutes any non-printable character or single/double
 * quote with an underscore.
 *
 * @param strIn Pointer to the wide character string to sanitise in place.
 *
 * @see createC4AllRecord
 */
void removeInvalidChars(wchar_t* strIn)
{
    int length = wcslen(strIn);
    for (int i=0;i<length;i++)
    {
        if (!iswprint(strIn[i]))
        {
            strIn[i]=L'_';
        }
        if (strIn[i] == L'\'' || strIn[i] == L'\"')
        {
            strIn[i]=L'_';
        }
    }
}
