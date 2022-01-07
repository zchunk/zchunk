#ifndef UTIL_COMMON_H
#define UTIL_COMMON_H

#define ZCK_NAME "zchunk"
#define ZCK_COPYRIGHT_YEAR "2021"

#ifndef BUF_SIZE
#define BUF_SIZE 32768
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

void version();

#ifdef _WIN32
// add correct declaration for basename
char* basename(char*);
#endif

#ifdef _WIN32
#define LOG_ERROR(...) fprintf(stderr, __VA_ARGS__)
#else
#define LOG_ERROR(...) dprintf(STDERR_FILENO, __VA_ARGS__)
#endif

#endif
