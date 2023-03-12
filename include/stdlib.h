#ifndef __MIMIC_STDLIB_H
#define __MIMIC_STDLIB_H

#include <stddef.h>

void *malloc(size_t size);
void free(void *ptr);
_Noreturn void exit(int status);

#endif
