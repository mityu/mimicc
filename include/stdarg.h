#ifndef __MIMIC_STDARG_H
#define __MIMIC_STDARG_H

#define va_start __builtin_va_start
#define va_end

typedef struct {
    int gp_offset;
    int fp_offset;
    void *overflow_arg_area;
    void *reg_save_area;
} va_list[1];

#endif
