#include <windows.h>
#include <stdio.h>

#include <io.h>

int ftruncate(int fd, LONGLONG length)
{
    FILE* fp = fdopen(fd, "wb");
    LARGE_INTEGER lisize;
    lisize.QuadPart = length;
    if (lisize.QuadPart < 0) {
        return -1;
    }
    if (!fp) {
        return -1;
    }
    else if (SetFilePointerEx(fp, lisize, NULL, FILE_BEGIN) == 0 || SetEndOfFile(fp) == 0) {
        return -1;
    }
    return 0;
}