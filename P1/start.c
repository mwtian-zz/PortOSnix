/* 
 * Function to mark the start of the area of memory in 
 * which a running minithread may be preempted.
 *
 * YOU SHOULD NOT [NEED TO] MODIFY THIS FILE.
*/

#include <setjmp.h>
#include "defs.h"

unsigned int start(void) {  
  jmp_buf buf;

  setjmp(buf);
  /* for x86 processor PC is in position 6 in jmp_buf */
//  return buf[14];
}
