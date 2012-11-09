#ifndef _PID_RANDOM_H_
#define _PID_RANDOM_H_

/*
 * initializes the random number generator with the
 * process id of the minithreads process
 * It uses functions from random.c files
 */

void mt_random_init(void);

/*
 * function that returns a random number between O and maxvalue [0,maxvalue)
 */ 

int mt_random_number(unsigned int max);

#endif /* _PID_RANDOM_H_ */
