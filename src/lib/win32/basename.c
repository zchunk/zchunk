#include <stdio.h>
#include <windows.h>

char* basename(char* path)
{
    // note this is not a proper basename implementation
    char *p = strrchr (path, '\\');
    return p ? p + 1 : (char *) path;

    // char full_path[MAX_PATH], drive[MAX_PATH], dir[MAX_PATH], filename[MAX_PATH], ext[MAX_PATH];

    // printf("Input: %s", path);

    // _fullpath(full_path, path, MAX_PATH);
    // printf("Input: %s", full_path);

    // _splitpath(full_path, drive, dir, filename, ext);

    // printf("%s, %s, %s, %s", drive, dir, filename, ext);

    // const char* res = malloc(MAX_PATH);
    // sprintf(res, "%s%s", filename, ext);
    // printf("Result: %s", res);

    // return res;
}