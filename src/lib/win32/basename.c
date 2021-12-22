#include <stdio.h>
#include <windows.h>
char* basename(char* path)
{
    char drive[MAX_PATH], dir[MAX_PATH], filename[MAX_PATH], ext[MAX_PATH]; 
    _splitpath(path, drive, dir, filename, ext);
    printf("%s, %s, %s, %s", drive, dir, filename, ext);

    char res[MAX_PATH];
    sprintf(res, "%s:\\%s", drive, dir);
    printf("Result: %s", res);
    return res;
}