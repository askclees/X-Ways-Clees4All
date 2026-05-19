#ifndef OPTIONS_H_INCLUDED
#define OPTIONS_H_INCLUDED



#endif // OPTIONS_H_INCLUDED

extern int createOptionsWindow();
extern ExtractOptions loadOrCreateOptions(BOOL* success);
extern char* generateOptionsFolderString();

int writeExtractionDetails(ExtractionDetails record);

//1.50 added rad last settings function
int loadLastExtractionSettings(ExtractionDetails* record);
