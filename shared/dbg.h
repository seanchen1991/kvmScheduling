#ifndef __dbg_h__
#define __dbg_h__

#include <stdio.h>
#include <errno.h>
#include <string.h>

#ifdef NDEBUG
#define debug(M, ...)
#else
#define debug(M, ...) fprintf(stderr, "DEBUG %s:%d: " M "\n",\
    __FILE__, __LINE__, ##__VA_ARGS__)
#endif

#define clean_errno() (errno == 0 ? "None" : strerror(errno))

#define log_err(M, ...) fprintf(stderr,\
    "[ERROR] (%s:%d: errno: %s) " M "\n", __FILE__, __LINE__,\
    clean_errno(), ##__VA_ARGS__)

#define log_warn(M, ...) fprintf(stderr,\
    "[WARN] (%s:%d: errno: %s) " M "\n",\
    __FILE__, __LINE__, clean_errno(), ##__VA_ARGS__)

#define log_info(M, ...) fprintf(stderr, "[INFO] (%s:%d) " M "\n",\
    __FILE__, __LINE__, ##__VA_ARGS__)

#define check(A, M, ...) if(!(A)) {\
    log_err(M, ##__VA_ARGS__); errno=0; goto error; }
#endif

#ifdef __cplusplus__
#include <iostream>
#include <string>

void clearScreen()
{
	cout << string( 100, '\n' );
}

#else
void clearScreen()
{
	int n;
	for (n = 0; n < 10; n++)
		printf( "\n\n\n\n\n\n\n\n\n\n" );
}
#endif
// Pathetic, but I don't want to add more dependencies
