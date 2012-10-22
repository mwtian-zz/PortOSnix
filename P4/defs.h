#ifndef _CONFIG_H_
#define _CONFIG_H_


#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/* if debuging is desired set value to 1 */
#define DEBUG 0
#define INTERRUPT_DEFER 0
#define INTERRUPT_DROP 1

/* for now kernel printfs are just regular printfs */
#define kprintf printf

#define SwitchToThread() sleep(0);


#define AbortOnCondition(cond,message) \
 if (cond) {\
    printf("Abort: %s:%d %d, MSG:%s\n", __FILE__, __LINE__, errno, message);\
    exit(EXIT_FAILURE);\
 }

#define AbortOnError(fctcall) \
   if (fctcall == 0) {\
    printf("Error: file %s line %d: code %ld.\n", __FILE__, __LINE__, errno);\
    exit(EXIT_FAILURE);\
  }

#endif /* _CONFIG_H_ */
