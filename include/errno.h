#ifndef __MIMIC_ERRNO_H
#define __MIMIC_ERRNO_H

extern int *__errno_location(void);
#define errno (*__errno_location ())

#endif
