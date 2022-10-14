#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <pwd.h>
#include <sys/stat.h>
#include <limits.h>
#include <errno.h>
#include <stdio.h>

const char* HOME_PREFIX = "~/";

/// @brief Resolves a path containing the home character to the full file path.
/// @param src A string defining the path to be resolved
/// @param buffer A buffer of at least PATH_MAX bytes where the resolved path is placed.
/// @return A pointer to the buffer upon success or null on failure
char* resolve_path(const char* src, char* buffer) {
    if(strlen(src) >= PATH_MAX)
        return 0;

    strcpy(buffer, src);

    if(strncmp(buffer, HOME_PREFIX, strlen(HOME_PREFIX)) == 0) {
        const char *homedir;

        if ((homedir = getenv("HOME")) == NULL) {
            homedir = getpwuid(getuid())->pw_dir;
        }

        if(strlen(buffer) + strlen(homedir) + 1 > PATH_MAX) {
            return 0;
        }

        size_t homelen = strlen(homedir);
        strcpy(buffer + strlen(homedir) + 1, buffer);
        memcpy(buffer, homedir, homelen);
        buffer[homelen + 1] = '/';
    }

    return buffer;
}

/// @brief Resolves a path to a regular file and validates that the path points to a regular file.
/// @param src A string defining the path to be resolved
/// @param buffer A buffer of at least PATH_MAX bytes where the resolved path is placed.
/// @return A pointer to the buffer upon success path resolution and when the path points to a regular
//          file, or null on failure for path resolution or when the path is not resolved to a regular file.
char* resolve_filepath(const char* src, char* buffer) {
    if(!resolve_path(src, buffer))
        return 0;
    
    struct stat stat_info;
    stat(buffer, &stat_info);

    if (!S_ISREG(stat_info.st_mode))
        return 0;

    return buffer;
}


/// @brief Resolves a path to a regular file and validates that the path points to a regular file.
/// @param src A string defining the path to be resolved
/// @param buffer A buffer of at least PATH_MAX bytes where the resolved path is placed.
/// @return A pointer to the buffer upon success path resolution and when the path points to a directory
//          or null on failure for path resolution or when the path is not resolved to a directory.
char* resolve_dirpath(const char* src, char* buffer) {
    if(!resolve_path(src, buffer))
        return 0;

    char workingPath[PATH_MAX];
    strcpy(workingPath, buffer);
    int pathlen = strlen(workingPath);

    int compLen = 0;
    for(int i = 0; i <= pathlen; i++) {
        if((i == pathlen || workingPath[i] == '/')) {
            if(compLen > 0) {
                compLen = 0;
                char old = workingPath[i];
                workingPath[i] = '\0';

                if(mkdir(workingPath, ALLPERMS) < 0) {
                    if(errno == EEXIST) {
                        struct stat statbuf;
                        if (stat(workingPath, &statbuf) != 0 || !S_ISDIR(statbuf.st_mode))
                            return 0;
                    } else
                        return 0;
                }
                workingPath[i] = old;
            }
        } else
            compLen += 1;
    }

    return buffer;
}