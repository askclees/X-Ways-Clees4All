#ifndef XML_H_INCLUDED
#define XML_H_INCLUDED


//functions
FILE* createXML(const char* filePath, const wchar_t* progVersion);
void closeXML(FILE* xmlFile);
LONG writeXML(FileRecord &fr,int picture, FILE* tmpOutput, INT64 counter);
wchar_t* replaceInvalidXMLChars(wchar_t* strIn);

#endif // XML_H_INCLUDED
