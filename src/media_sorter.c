
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <sys/types.h>
#include <errno.h>

#include "util/common.h"

/**
 * This media sorter was written for the precise use case:
 * 
 * - Adobe Media Encoder can automatically add media files to queue when these files are all present inside a watch folder.
 * - In the case of watch folder, only one preset (config) can be applied to a folder.
 * - So, I am segregating all media files first by extension, and then by resolution.
 * - So, that all media files will reside as in below structure
 * <watchFolder>/mp4/1280X720/1.mp4
 *                  /960X540/2.mp4
 * 
 * <watchFolder>/png/1280X720/1.png
 *                  /960X540/2.png
 * <watchFolder>/jpeg/1280X720/1.jpeg
 * 
 * - Once, this segregation is done, the respective folder can be added as a watch folder and corresponding preset applied
 *   to that folder.
 * However, same logic can be used while using ffmpeg, as the output folder will get clear segregration.
 * 
 */

void process_path(const char *path, const char *outputpath, const char* intersting_extension, char** sixteen_eachto_nine_sizes, int sixteen_eachto_nine_sizes_length) {
    /**
     * To read a dir, we use openDir, readDir and finally closeDir methods from dirent.h
     * 
     */
    DIR *dir = opendir(path);
    if (!dir) {
        perror("opendir failed");
        return;
    }

    struct dirent *entry;
    char fullpath[4096];

    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
         /**
         * strcmp is similar to Java's String equals(), but it returns 0 on success instead of true.
         * 
         */
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0)
            continue;

        const char *filename = entry->d_name;
        char onlyNameWidoutExtension[256];

        get_name_without_ext(entry->d_name, onlyNameWidoutExtension, 256);

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
            process_path(fullpath, outputpath, intersting_extension, sixteen_eachto_nine_sizes, sixteen_eachto_nine_sizes_length);
            
            
        } else if (S_ISREG(st.st_mode)) {//is a regular file ? not a dir, symlink etc
             // Extract extension
             /**
              * strrchr is like Java String's lastIndexOf()
              * 
              */
            const char *ext = strrchr(entry->d_name, '.');
            if (!ext) {
                continue;
            }

            //we need to look for only .mp4, .png, .jpg files.       
            char lower_ext[32];
            strncpy(lower_ext, ext, sizeof(lower_ext) - 1);
            lower_ext[sizeof(lower_ext) - 1] = '\0';
            to_lowercase(lower_ext);

            if (strcmp(lower_ext + 1, intersting_extension) == 0){
                    int *width = malloc(sizeof(int)); 
                    int *height = malloc(sizeof(int)); 
                    get_resource_resolution(fullpath, width, height);
                    printf("resource resolution path = %s, width = %d, height = %d\n", fullpath, *width, *height);
                    char logLine[5000];
                    snprintf(
                        logLine, sizeof(logLine),
                        "Video resource path = %s, width = %d, height = %d\n",
                        fullpath,
                        *width,
                        *height
                        
                    );
                    printf("resource path = %s, resolution = %dX%d\n", fullpath,*width,*height);
                    if(is_aspect_ratio_16_9(*width,*height) == 1) {
                        char* tmp = calloc(20, sizeof(char));
                        snprintf(tmp, 20, "%dX%d", *width, *height);
                        if (is_size_already_present(sixteen_eachto_nine_sizes, sixteen_eachto_nine_sizes_length, tmp) == 0) {
                            for (int i = 0 ; i < sixteen_eachto_nine_sizes_length ; i++) {
                                if (sixteen_eachto_nine_sizes[i] == NULL) {
                                    sixteen_eachto_nine_sizes[i] = calloc(strlen(tmp)+1, sizeof(char));
                                    strcpy(sixteen_eachto_nine_sizes[i], tmp);
                                    break;
                                }
                            }
                        }
                       
                    }
                    char* dir_name_by_resolution = calloc(strlen(outputpath) + 100 , sizeof(char));                   

                    snprintf(dir_name_by_resolution, strlen(outputpath) + 100, "%s/%s/%dX%d",outputpath, lower_ext+1, *width, *height);
                    printf("mkdir if not exists for path = %s\n", dir_name_by_resolution);
                    mkdir_p(dir_name_by_resolution, 0777);

                    int abs_file_path_size = strlen(dir_name_by_resolution) + 1 + strlen(entry->d_name) + 1;
                    char* abs_file_path_by_resolution = calloc(abs_file_path_size , sizeof(char));
                    snprintf(abs_file_path_by_resolution, abs_file_path_size, "%s/%s", dir_name_by_resolution, entry->d_name);
                    copy_file(fullpath, abs_file_path_by_resolution);

                    free(dir_name_by_resolution);
                    free(abs_file_path_by_resolution);
                    
                    free(width);
                    free(height);                    
            }            
        }   
    }
    closedir(dir);
}

int main(int argc, char *argv[]){
     if (argc < 4) {
        printf("Usage: %s <input directory> <output directory> <interesting_file_extension>\n", argv[0]);//the input directory can contain medias in multiple directorries, recursive directory look up will be done.
        return 1;
    }

    char* assetInputDirectory = argv[1];
    char* assetOutputDirectory = argv[2];//ensure that output directory already exists by creating manually before invoking this program.
    char* intersting_extension = argv[3];
    printf("assetOutputDirectory = %s\n", assetOutputDirectory);
    char** sixteen_eachto_nine_sizes = calloc(100, sizeof(char*));
    process_path(assetInputDirectory, assetOutputDirectory, intersting_extension, sixteen_eachto_nine_sizes, 100);
    printf("Printing all 16:9 resource sizes\n");
    for(int i = 0 ; i < 100 ; i ++) {
        if(sixteen_eachto_nine_sizes[i] == NULL) {
            break;
        }
        printf("%s\n", sixteen_eachto_nine_sizes[i]);
    }
        printf("Printing all 16:9 resource sizes completed\n");

}