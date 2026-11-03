#ifndef WELCOME95_FONTS_H
#define WELCOME95_FONTS_H
#include <windows.h>
int bundled_fonts_load(HINSTANCE instance);
void bundled_fonts_release(void);
const char *bundled_fonts_directory(void);
#endif
