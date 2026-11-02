#ifndef WELCOME95_PROFILES_H
#define WELCOME95_PROFILES_H
void profiles_load(void);
int profiles_count(void);
const char *profiles_name(int index);
int profiles_remember(const char *name);
#endif
