#ifndef __MIMICC_ERRNO_H
#define __MIMICC_ERRNO_H

extern int *__errno_location(void);
#define errno (*__errno_location())

#endif
