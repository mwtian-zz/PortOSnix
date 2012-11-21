#ifndef _RANDOM_H_
#define _RANDOM_H_

/* The NT ANSI random number generator is horrible. This
   one is much better.
*/

/* initializer for random number generator. Must be called
   once before calls to genrand()
*/
void sgenrand(unsigned long seed);

/* produce a uniformly distributed [0,1] double */
double genrand();

/* produce an integer between 1 and maxval */
unsigned int genintrand();

#endif /* _RANDOM_H_ */
