#include <stdio.h>

// note this is not a proper basename implementation
char* basename(char* path)
{
    char *p = strrchr (path, '\\');
    if (p == NULL)
    {
        // sometimes we pass paths on win with "/" seperators
        p = strrchr(path, '/');
    }
    return p ? p + 1 : (char *) path;
}