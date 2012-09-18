/* 
 * Function to mark the start of the area of memory in 
 * which a running minithread may be preempted.
 *
 * YOU SHOULD NOT [NEED TO] MODIFY THIS FILE.
*/

#include <setjmp.h>
#include "defs.h"

unsigned int end(void) {
  jmp_buf buf;
  setjmp(buf);
  /* for amd64 processor PC is in position 15 in jmp_buf (on macs) */
//  return buf[14]; 
}

