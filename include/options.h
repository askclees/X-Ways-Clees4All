#ifndef OPTIONS_H_INCLUDED
#define OPTIONS_H_INCLUDED



#endif // OPTIONS_H_INCLUDED

/** @brief Creates and displays the extraction options GUI window. */
extern int createOptionsWindow();
/** @brief Loads options from the options database, creating it with defaults if it does not exist. */
extern ExtractOptions loadOrCreateOptions(BOOL* success);
/** @brief Returns a newly allocated string containing the path to the options database folder. */
extern char* generateOptionsFolderString();

/** @brief Persists the last-run extraction details to the options database. */
int writeExtractionDetails(ExtractionDetails record);

/** @brief Reads the last-run extraction settings from the options database into the provided record. */
int loadLastExtractionSettings(ExtractionDetails* record);
