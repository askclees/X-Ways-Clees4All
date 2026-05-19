#ifndef GUI_H_INCLUDED
#define GUI_H_INCLUDED

#include "X-Tension.h"

/** @brief Registers the window class and opens the main extraction options dialog. */
int createWindow(WORD version);

/** @brief Frees heap memory allocated during GUI creation. Call when the X-Tension is unloaded. */
void cleanupGUI();

#endif // GUI_H_INCLUDED
