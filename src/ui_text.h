#ifndef WELCOME95_UI_TEXT_H
#define WELCOME95_UI_TEXT_H
#include <windows.h>
/* Russian bytes are CP1251: Win95's ANSI controls do not decode UTF-8. */
static BOOL ui_russian(void)
{
#ifdef WELCOME95_TEST_RUSSIAN
    return WELCOME95_TEST_RUSSIAN;
#else
    return GetACP()==1251;
#endif
}
static const char *ui_text(const char *russian,const char *english)
{
    return ui_russian() ? russian : english;
}
#endif
