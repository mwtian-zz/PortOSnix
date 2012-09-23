#ifndef _CONFIG_H_
#define _CONFIG_H_


#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

/* if debuging is desired set value to 1 */
#define DEBUG 0

/* for now kernel printfs are just regular printfs */
#define kprintf printf

#endif /* _CONFIG_H_ */
