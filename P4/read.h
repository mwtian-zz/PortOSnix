#ifndef __READ_PUBLIC_H__
#define __READ_PUBLIC_H__

/* synchronously reads a line from the keyboard into the given buffer.
 * If the input is longer than the length of the buffer, then up to len-1 bytes
 * are copied. Otherwise, the entire input inclusive of the newline character
 * is copied into the buffer. In both cases, the byte following the last input
 * character will be set to NULL, so the buffer contents will be a valid string.
 *
 * Returns the length of the string copied into the buffer.
 */
int miniterm_read(char* buffer, int len);

#endif __READ_PUBLIC_H__
