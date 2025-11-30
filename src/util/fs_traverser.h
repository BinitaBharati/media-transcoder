#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <sys/types.h>
#include <errno.h>

typedef struct {
    char ext[32];
    int count;
} ExtCount;

typedef struct {
    const char *name;
    int width;
    int height;
    char* bitRate;//this is solely bit rate of the video stream (not audio track, in case video has audio too)
} VideoTranscoderConfig;

// Preset table
static const VideoTranscoderConfig VIDEO_ENCODER_PRESETS[] = {
    { "FHD",       1920, 1080, "15M" },
    { "HD",        1280,  720,  "8M" },
    { "SD",         960,  540,  "3M" },
    { "MOBILE_SD",  960,  540,  "0.5M" }
};

/**
 * total size of array in bytes
-------------------------------- = number of elements
size of one element

If you have:
static const int VIDEO_ENCODER_PRESETS[] = {1, 2, 3, 4, 5};

sizeof(VIDEO_ENCODER_PRESETS) → total bytes = 5 × 4 = 20 (since int is 4 bytes)

sizeof(VIDEO_ENCODER_PRESETS[0]) → size of one element = 4

division → 20 / 4 = 5

You can just hardcode the variable VIDEO_ENCODER_PRESET_COUNT's value to 4 also without doing these gymnastics.

 * 
 */
static const int VIDEO_ENCODER_PRESET_COUNT =
        sizeof(VIDEO_ENCODER_PRESETS) / sizeof(VIDEO_ENCODER_PRESETS[0]);

/**
 * in C, you can not retun a array as return type, array names are pointer to whatever type they point to.
 */
int getPossibleResourceTranscodingConfig(int inW, int inH, VideoTranscoderConfig **outArray)
{
    int count = 0;

    // First pass to count matches
    for (int i = 0; i < VIDEO_ENCODER_PRESET_COUNT; i++) {
        if (VIDEO_ENCODER_PRESETS[i].width  <= inW &&
            VIDEO_ENCODER_PRESETS[i].height <= inH) 
        {
            count++;
        }
    }

    if (count == 0) {
        *outArray = NULL;
        return 0;
    }

    // Allocate output array
    *outArray = malloc(sizeof(VideoTranscoderConfig) * count);//block allocate memory equal to number of fulfilling VideoTranscoderConfig.

    int idx = 0;

    // Second pass to copy matching presets
    for (int i = 0; i < VIDEO_ENCODER_PRESET_COUNT; i++) {
        if (VIDEO_ENCODER_PRESETS[i].width  <= inW &&
            VIDEO_ENCODER_PRESETS[i].height <= inH) 
        {
            (*outArray)[idx++] = VIDEO_ENCODER_PRESETS[i];
        }
    }

    return count;
}


long long time_now_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;//current clock time in ns
}

int mkdir_p(const char *path) {
    char tmp[1024];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);

    // Remove trailing slash
    if (tmp[len - 1] == '/')
        tmp[len - 1] = '\0';

    // Iterate over path components
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';

            if (mkdir(tmp, 0755) != 0) {
                if (errno != EEXIST) {
                    return -1; // unexpected error
                }
            }

            *p = '/';
        }
    }

    // Create final directory
    if (mkdir(tmp, 0755) != 0) {
        if (errno != EEXIST) {
            return -1;
        }
    }

    return 0;
}

int extCount = 0;

// Convert string to lowercase in-place
void to_lowercase(char *s) {
    while (*s) {
        *s = tolower(*s);
        s++;
    }
}

void writeLogsToFile(FILE *logFilePointer, char* logLine) {
    fprintf(logFilePointer, "%s", logLine);
}

// Add or increment extension in extList[]
void add_extension(const char *ext, ExtCount* extCountResult) {
    // If extension not present, ext="" means no extension
    for (int i = 0; i < extCount; i++) {
        if (strcmp(extCountResult[i].ext, ext) == 0) {
            extCountResult[i].count++;
            return;
        }
    }

    // New extension
    strcpy(extCountResult[extCount].ext, ext);
    extCountResult[extCount].count = 1;
    extCount++;
}

void get_extension_to_file_count(const char *path, ExtCount* extCountResult) {
    DIR *dir = opendir(path);
    if (!dir) {
        perror("opendir failed");
        return;
    }

    struct dirent *entry;
    char fullpath[4096];

    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0)
            continue;

        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);

        struct stat st;

        if (stat(fullpath, &st) == -1) {
            perror("stat failed");
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            // Recurse into directory
            add_extension(fullpath, extCountResult);
            
            
        } else if (S_ISREG(st.st_mode)) {
             // Extract extension
            const char *ext = strrchr(entry->d_name, '.');

            if (!ext) {
                add_extension("(no extension)", extCountResult);
            } else {
                char lower_ext[32];
                strncpy(lower_ext, ext, sizeof(lower_ext) - 1);
                lower_ext[sizeof(lower_ext) - 1] = '\0';
                to_lowercase(lower_ext);

                add_extension(lower_ext, extCountResult);
            }
        }   
    }
    closedir(dir);
}

void get_name_without_ext(const char *input, char *output, size_t outSize) {
    const char *dot = strchr(input, '.');  // find first '.'

    size_t len;
    if (dot != NULL) {
        len = dot - input;  // number of chars before '.'. Does pointer arithmetic, and gives the length of teh file name part in input.
    } else {
        // no dot → copy entire string
        len = strlen(input);
    }

    // ensure we don't overflow output buffer
    if (len >= outSize)
        len = outSize - 1;

    strncpy(output, input, len);
    output[len] = '\0';     // null terminate
    printf("get_name_without_ext: input = %s, output = %s\n",input, output);
}

void process_path(const char *path, const char *outputpath,
    void (*get_resource_resolution_function_pointer)(const char *, int *, int *),
    void (*transcode_video_fp)(const char *, const char *, int, int, const char *),
    void (*encode_image_fp)(const char *, const char *, int, int),
    int (*video_has_audio_fp)(const char *),
    FILE *inputPathDiscoveryLogFp) {
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
            process_path(fullpath, outputpath, get_resource_resolution_function_pointer, transcode_video_fp, encode_image_fp, video_has_audio_fp, inputPathDiscoveryLogFp);
            
            
        } else if (S_ISREG(st.st_mode)) {//is a regular file ? not a dir, symlink etc
             // Extract extension
             /**
              * strrchr is like Java String's lastIndexOf()
              * 
              */
            const char *ext = strrchr(entry->d_name, '.');

            //we need to look for only .mp4, .png, .jpg files.       
            char lower_ext[32];
            strncpy(lower_ext, ext, sizeof(lower_ext) - 1);
            lower_ext[sizeof(lower_ext) - 1] = '\0';
            to_lowercase(lower_ext);

            if (strcmp(lower_ext, ".mp4") == 0  || 
                strcmp(lower_ext, ".png") == 0  ||
                strcmp(lower_ext, ".jpg") == 0 ) {
                    int *width = malloc(sizeof(int)); 
                    int *height = malloc(sizeof(int)); 
                    get_resource_resolution_function_pointer(fullpath, width, height);
                    printf("re4source esolution path = %s, width = %d, height = %d\n", fullpath, *width, *height);
                    if (strcmp(lower_ext, ".mp4") == 0) {
                        bool hasAudio = video_has_audio_fp(fullpath) == 1 ? true : false;
                        char logLine[5000];
                        snprintf(
                            logLine, sizeof(logLine),
                            "Video resource path = %s, width = %d, height = %d, hasAudio = %s\n",
                            fullpath,
                            *width,
                            *height,
                            hasAudio ? "true" : "false"
                        );
                        printf("writing %s\n",logLine);
                        writeLogsToFile(inputPathDiscoveryLogFp, logLine);
                        //Transcoding is only allowed from bigger to smaller size. Technically, other way can also  be done, but output video quality wont be good.
                        VideoTranscoderConfig *matches = NULL;
                        int availableTranscodeConfigCount = getPossibleResourceTranscodingConfig(*width, *height, &matches);
                        printf("Video path = %s, resolution = %dX%d, found %d matching presets:\n", fullpath,*width,*height,availableTranscodeConfigCount);
                       for (int i = 0 ; i < availableTranscodeConfigCount ; i++) {
                            VideoTranscoderConfig transcodeConfig = matches[i];
                            char finalOutputParentDirPath[6096];
                            snprintf(finalOutputParentDirPath, sizeof(finalOutputParentDirPath), "%s/%s", outputpath, path);
                            printf("finalOutputParentDirPath for transcode videos = %s\n",finalOutputParentDirPath);
                            mkdir_p(finalOutputParentDirPath);
                            char finalOutputFileName[2000];
                            snprintf(finalOutputFileName, sizeof(finalOutputFileName), "%s-%dx%d%s", onlyNameWidoutExtension, transcodeConfig.width, transcodeConfig.height,lower_ext);
                            printf("finalOutputFileName for transcode videos = %s\n",finalOutputFileName);
                            char finalOutputFilePath[8096];
                            snprintf(finalOutputFilePath, sizeof(finalOutputFilePath), "%s/%s", finalOutputParentDirPath, finalOutputFileName);
                            printf("finalOutputFilePath for transcode videos = %s\n",finalOutputFilePath);
                            transcode_video_fp(fullpath, finalOutputFilePath, transcodeConfig.width, transcodeConfig.height,transcodeConfig.bitRate);
                        }
                        free(matches);
                    }
                    else {
                        //image file
                        char logLine[5000];
                        snprintf(
                            logLine, sizeof(logLine),
                            "Image resource path = %s, width = %d, height =%d\n",
                            fullpath,
                            *width,
                            *height
                        );
                        writeLogsToFile(inputPathDiscoveryLogFp, logLine);
                        VideoTranscoderConfig *matches = NULL;
                        int availableTranscodeConfigCount = getPossibleResourceTranscodingConfig(*width, *height, &matches);
                        printf("Image path = %s, resolution = %dX%d, found %d matching presets:\n", fullpath,*width,*height,availableTranscodeConfigCount);
                       for (int i = 0 ; i < availableTranscodeConfigCount ; i++) {
                            VideoTranscoderConfig transcodeConfig = matches[i];
                            char finalOutputParentDirPath[6096];
                            snprintf(finalOutputParentDirPath, sizeof(finalOutputParentDirPath), "%s/%s", outputpath, path);
                            printf("finalOutputParentDirPath for transcode videos = %s\n",finalOutputParentDirPath);
                            mkdir_p(finalOutputParentDirPath);
                            char finalOutputFileName[2000];
                            snprintf(finalOutputFileName, sizeof(finalOutputFileName), "%s-%dx%d%s", onlyNameWidoutExtension, transcodeConfig.width, transcodeConfig.height,lower_ext);
                            printf("finalOutputFileName for transcode videos = %s\n",finalOutputFileName);
                            char finalOutputFilePath[8096];
                            snprintf(finalOutputFilePath, sizeof(finalOutputFilePath), "%s/%s", finalOutputParentDirPath, finalOutputFileName);
                            printf("finalOutputFilePath for transcode videos = %s\n",finalOutputFilePath);
                            encode_image_fp(fullpath, finalOutputFilePath, transcodeConfig.width, transcodeConfig.height);
                        }
                        free(matches);

                    }                 
                    free(width);
                    free(height);                    
            }            
        }   
    }
    closedir(dir);
}

/*
int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <directory>\n", argv[0]);
        return 1;
    }

   
    //The below assigment makes:
    //assuming there will be 10 different extensions ? yes. you can also do dynamic allocation, but in that case the struct has to be slightly changed for next item.

    //all ext strings = empty ""
    //all count fields = 0 
    //You need to do this, as in C, local variables are not initialized like Java, only global static vars get initialized, not local vars.
    
    ExtCount extList[10] = {0};   // sets entire array to zero

    get_extension_to_file_count(argv[1], extList);

    printf("\nExtension Counts:\n");
    for (int i = 0; i < extCount; i++) {
        printf("%s -> %d\n", extList[i].ext, extList[i].count);
    }

    return 0;
} */
