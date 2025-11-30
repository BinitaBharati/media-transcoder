#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>

int has_valid_extension(const char *filename) {
    /**
     * @strrchr is like Java's String lastIndexOf 
     * 
     */
    const char *ext = strrchr(filename, '.');
    if (!ext) return 0;

    // Convert to lowercase for consistent comparison
    char lower_ext[10];
    int i = 0;
    while (ext[i] && i < 9) {
        lower_ext[i] = tolower(ext[i]);
        i++;
    }
    lower_ext[i] = '\0';

    /**
     * strcmp is similar to Java's String equals(), but it returns 0 on success instead of true.
     * 
     */
    return strcmp(lower_ext, ".mp4") == 0 ||
           strcmp(lower_ext, ".jpg") == 0 ||
           strcmp(lower_ext, ".png") == 0;
}

void traverse(const char *path) {
    DIR *dir = opendir(path);
    if (!dir)//same as if (dir == NULL) 
    {
        /*
        perror() prints a message based on the global variable errno, so it is only meaningful after a function call that sets errno.
        opendir set this global errno variable.

        */
        perror("opendir failed");
        return;
    }

    struct dirent *entry;
    char fullpath[4096];

    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || //d->name only gives the name of the direct child of dir. That child could be a file, dir, or a hidden file/dir etc.
            strcmp(entry->d_name, "..") == 0)  //
            continue;

        /**
         * snprintf is like Java's String.format API.
         * 
         */
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);

        /**
         * stat() to get information about the file or directory pointed to by fullpath.stat method returns 0 on success and -1 on failure.
         * the struct st will receive information about the file, such as:

            file type (directory, regular file, symlink…)
            size
            permissions
            timestamps (access, modify)
            inode number etc
         * 
         */
        struct stat st;

        if (stat(fullpath, &st) == -1) {
            perror("stat failed");
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            // Recurse into directory
            traverse(fullpath);
        } else if (S_ISREG(st.st_mode)) {
            // Check extension
            if (has_valid_extension(entry->d_name)) {
                printf("%s : %ld bytes\n", fullpath, st.st_size);
            }
        }
    }

    closedir(dir);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <directory>\n", argv[0]);
        return 1;
    }

    traverse(argv[1]);
    return 0;
}
