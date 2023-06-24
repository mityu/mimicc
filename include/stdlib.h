#ifndef __MIMICC_STDLIB_H
#define __MIMICC_STDLIB_H

#include <stddef.h>

void *malloc(size_t size);
void free(void *ptr);
_Noreturn void exit(int status);

#endif
