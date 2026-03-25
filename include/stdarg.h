#ifndef __MIMICC_STDARG_H
#define __MIMICC_STDARG_H

#include <string.h>

#define va_start(ap, fmt) __builtin_va_start(ap, fmt)
#define va_end(ap)
#define va_copy(dst, src) __builtin_va_copy(dst, src)
#define __builtin_va_copy(dst, src) (memcpy((dst), (src), sizeof(va_list)))

typedef struct {
    int gp_offset;
    int fp_offset;
    void *overflow_arg_area;
    void *reg_save_area;
} va_list[1];

#endif
