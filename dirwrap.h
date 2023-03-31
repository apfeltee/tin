
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#define TINDIR_PATHSIZE 1024
#if defined(__unix__) || defined(__linux__)
    #define TINDIR_ISUNIX
#elif defined(_WIN32) || defined(_WIN64)
    #define TINDIR_ISWINDOWS
#endif

#if defined(TINDIR_ISUNIX)
    #include <dirent.h>
#else
    #if defined(TINDIR_ISWINDOWS)
        #include <windows.h>
    #endif
#endif

#if defined (S_IFDIR) && !defined (S_ISDIR)
    #define	S_ISDIR(m)	(((m)&S_IFMT) == S_IFDIR)	/* directory */
#endif
#if defined (S_IFREG) && !defined (S_ISREG)
    #define	S_ISREG(m)	(((m)&S_IFMT) == S_IFREG)	/* file */
#endif

typedef struct TinDirReader TinDirReader;
typedef struct TinDirItem TinDirItem;

struct TinDirReader
{
    void* handle;
};

struct TinDirItem
{
    char name[TINDIR_PATHSIZE + 1];
    bool isdir;
    bool isfile;
};

bool tin_fs_diropen(TinDirReader* rd, const char* path)
{
    #if defined(TINDIR_ISUNIX)
        if((rd->handle = opendir(path)) == NULL)
        {
            return false;
        }
        return true;
    #endif
    return false;
}

bool tin_fs_dirread(TinDirReader* rd, TinDirItem* itm)
{
    itm->isdir = false;
    itm->isfile = false;
    memset(itm->name, 0, TINDIR_PATHSIZE);
    #if defined(TINDIR_ISUNIX)
        struct dirent* ent;
        if((ent = readdir((DIR*)(rd->handle))) == NULL)
        {
            return false;
        }
        if(ent->d_type == DT_DIR)
        {
            itm->isdir = true;
        }
        if(ent->d_type == DT_REG)
        {
            itm->isfile = true;
        }
        strcpy(itm->name, ent->d_name);
        return true;
    #endif
    return false;
}

bool tin_fs_dirclose(TinDirReader* rd)
{
    #if defined(TINDIR_ISUNIX)
        closedir((DIR*)(rd->handle));
    #endif
    return false;
}


