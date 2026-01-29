#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <sys/types.h>
#include <errno.h>

#include "util/common.h"

/**
 * Difference between popen vs system methods to invoke a system connand from C code.
 * system()	                        popen()
Runs command only	                Runs command AND allows communication with it
No output captured	                Output can be read
Returns exit code	                Returns FILE* stream

Example:
system("ls -l");          // prints on screen
popen("ls -l", "r");      // lets your C program read output, also you can send output to the command by using w mode instead of r mode

popen(cmd, mode):

mode	meaning
"r"	read from command output
"w"	write to command input

 * 
 * 
 */

int video_has_audio(const char *inputFile) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "ffprobe -v error -select_streams a:0 "
             "-show_entries stream=index -of csv=p=0 \"%s\" 2>/dev/null",
             inputFile);
    printf("checking if video has audio track %s\n",cmd);

    FILE *pipe = popen(cmd, "r");
    if (!pipe) {
        fprintf(stderr, "Failed to run ffprobe\n");
        return -1;  // error
    }

    char buffer[128];
    int hasAudio = 0;

    // If ffprobe prints anything, an audio stream exists
    if (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        hasAudio = 1;
    }
    pclose(pipe);
    return hasAudio;
}

void encode_image(const char *input, const char *output,
                  int targetW, int targetH) {

    char cmd[1024];

    snprintf(cmd, sizeof(cmd),
             "ffmpeg -y -i \"%s\" -vf scale=%d:%d \"%s\"",
             input, targetW, targetH, output);

    printf("Running resize command:\n%s\n", cmd);

    int ret = system(cmd);
    if (ret != 0) {
        fprintf(stderr, "FFmpeg failed with code %d\n", ret);
        exit(1);
    }
}

void transcode_video(const char *input, const char *output,
                     int targetW, int targetH, const char *bitrate) {
    char cmd[1024];
    /**
     * Here is the exact FFmpeg command that the C function generates:
     * ffmpeg -y -i "INPUT_FILE" -vf scale=TARGET_W:TARGET_H -b:v BITRATE -c:v libx264 -preset medium -c:a aac -b:a 128k "OUTPUT_FILE"
       
     -c:a copy is included, this will copy the audio track (if present) as is without any changes.
      What happens if the input has no audio track ?
      Then, FFmpeg will simply ignore the audio options; it will not fail.
     * 
     */
    snprintf(cmd, sizeof(cmd),
             "ffmpeg -y -i \"%s\" -vf scale=%d:%d "
             "-b:v %s -c:v libx264 -preset medium -c:a copy \"%s\"",
             input, targetW, targetH, bitrate, output);

    printf("Running transcode command:\n%s\n", cmd);

    int ret = system(cmd);
    if (ret != 0) {
        fprintf(stderr, "FFmpeg failed with code %d\n", ret);
        exit(1);
    }
}

/**
 Instead of having such verbose inpur args (owing to function pointers), you can simplify this by uisng typedefs like below:

typedef void (*resolution_fn)(const char *, int *, int *);
typedef void (*transcode_fn)(const char *, const char *, int, int, const char *);
typedef void (*encode_image_fn)(const char *, const char *, int, int);
typedef int  (*video_has_audio_fn)(const char *);

Now, you function declaration looks like below:
void process_path(const char *path, const char *outputpath,
                  resolution_fn get_resource_resolution_function_pointer,
                  transcode_fn transcode_video_fp,
                  encode_image_fn encode_image_fp,
                  video_has_audio_fn video_has_audio_fp,
                  FILE *inputPathDiscoveryLogFp)
 */
void process_path(const char *path, const char *outputpath, char* interesting_resolution,
    void (*transcode_video_fp)(const char *, const char *, int, int, const char *),
    void (*encode_image_fp)(const char *, const char *, int, int),
    int (*video_has_audio_fp)(const char *)) {
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
            process_path(fullpath, outputpath, interesting_resolution, transcode_video_fp, encode_image_fp, video_has_audio_fp);
        } else if (S_ISREG(st.st_mode)) {//is a regular file ? not a dir, symlink etc

            char* media_resolution = get_immediate_parent_dir(fullpath);
            if (strcmp(media_resolution, interesting_resolution) != 0) {
                closedir(dir);
                return;
            }

             // Extract extension
             /**
              * strrchr is like Java String's lastIndexOf()
              * 
              */
            const char *ext = strrchr(entry->d_name, '.');
            if (!ext) {
                closedir(dir);
                return;
            }

            int width, height;
            if (!sscanf(media_resolution, "%dX%d", &width, &height) == 2) {
                printf("Could not read resolution from path %s. Aborting further execution\n", fullpath);
                closedir(dir);
                return;
            }

             /* We need to look for only .mp4, .png, .jpg files.
               However, media_sorter has already segregated the files by extension and size.  

               But, we need to check the file extension, as video file encoding command is different from image file.
               In fact, ideally, indtaed of checking for extension, you should use ffprobe to find the exact file type,
               or even linux basic file command would do. But, for now, we assume that file extensions are true. 
            */   
            char lower_ext[32];
            strncpy(lower_ext, ext, sizeof(lower_ext) - 1);
            lower_ext[sizeof(lower_ext) - 1] = '\0';
            to_lowercase(lower_ext);
     
            if (strcmp(lower_ext, ".mp4") == 0) {//currently, VRS encoder only deals with video .mp4 files, no other video file types were found.
                bool hasAudio = video_has_audio_fp(fullpath) == 1 ? true : false;
                char logLine[5000];
                snprintf(
                    logLine, sizeof(logLine),
                    "Video resource path = %s, width = %d, height = %d, hasAudio = %s\n",
                    fullpath,
                    width,
                    height,
                    hasAudio ? "true" : "false"
                );
                printf("writing %s\n",logLine);
                /**
                 * Transcoding is only allowed from bigger to smaller size. Technically, other way can also be done, but output video quality wont be good.
                 * But, for now, I will allow everything, no checks done.
                 */
               
                for (int i = 0 ; i < 3 ; i++) {
                    VideoTranscoderConfig transcodeConfig = VIDEO_ENCODER_PRESETS[i];
                    char finalOutputParentDirPath[6096];
                    snprintf(finalOutputParentDirPath, sizeof(finalOutputParentDirPath), "%s/%s/%s", outputpath, lower_ext+1, media_resolution);
                    printf("finalOutputParentDirPath for transcode videos = %s\n",finalOutputParentDirPath);
                    mkdir_p(finalOutputParentDirPath, 0777);
                    char finalOutputFileName[2000];
                    snprintf(finalOutputFileName, sizeof(finalOutputFileName), "%s-%dx%d-%s%s", onlyNameWidoutExtension, transcodeConfig.width, transcodeConfig.height,transcodeConfig.bitRate,lower_ext);
                    printf("finalOutputFileName for transcode videos = %s\n",finalOutputFileName);
                    char finalOutputFilePath[8096];
                    snprintf(finalOutputFilePath, sizeof(finalOutputFilePath), "%s/%s", finalOutputParentDirPath, finalOutputFileName);
                    printf("finalOutputFilePath for transcode videos = %s\n",finalOutputFilePath);
                    transcode_video_fp(fullpath, finalOutputFilePath, transcodeConfig.width, transcodeConfig.height,transcodeConfig.bitRate);
                }
            }
            else {
                //image file
                char logLine[5000];
                snprintf(
                    logLine, sizeof(logLine),
                    "Image resource path = %s, width = %d, height =%d\n",
                    fullpath,
                    width,
                    height
                );
                for (int i = 0 ; i < 3 ; i++) {
                    VideoTranscoderConfig transcodeConfig = VIDEO_ENCODER_PRESETS[i];
                    char finalOutputParentDirPath[6096];
                    snprintf(finalOutputParentDirPath, sizeof(finalOutputParentDirPath), "%s/%s/%s", outputpath, lower_ext+1, media_resolution);
                    printf("finalOutputParentDirPath for transcode image = %s\n",finalOutputParentDirPath);
                    mkdir_p(finalOutputParentDirPath, 0777);
                    char finalOutputFileName[2000];
                    snprintf(finalOutputFileName, sizeof(finalOutputFileName), "%s-%dx%d-%s%s", onlyNameWidoutExtension, transcodeConfig.width, transcodeConfig.height,transcodeConfig.bitRate,lower_ext);
                    printf("finalOutputFileName for transcode image = %s\n",finalOutputFileName);
                    char finalOutputFilePath[8096];
                    snprintf(finalOutputFilePath, sizeof(finalOutputFilePath), "%s/%s", finalOutputParentDirPath, finalOutputFileName);
                    printf("finalOutputFilePath for transcode image = %s\n",finalOutputFilePath);
                    encode_image_fp(fullpath, finalOutputFilePath, transcodeConfig.width, transcodeConfig.height);
                }
            }                                
                      
        }   
    }
    closedir(dir);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <input directory> <output directory <interesting resolution>\n", argv[0]);//the input directory can contain medias in multiple directorries, recursive directory look up will be done.
        return 1;
    }

    long long start = time_now_ns();

    char* assetInputDirectory = argv[1];
    char* assetOutputDirectory = argv[2];//ensure that output directory already exists by creating manually before invoking this program.
    char* interesting_resolution = argv[3];//ensure that output directory already exists by creating manually before invoking this program.


    /**
     * you need not declare function pointer variable, in C function name (without parantheses) is a function pointer itself.
     * 
     */
    void (*transcode_video_fp)(const char *, const char *, int, int, const char *) = transcode_video;//function pointer variable named transcode_video_fp pointing to transcode_video
    void (*encode_image_fp)(const char *, const char *, int, int) = encode_image;//function pointer variable named encode_image_fp pointing to encode_image
    int (*video_has_audio_fp)(const char *) = video_has_audio;//function pointer variable named video_has_audio_fp pointing to video_has_audio method

    process_path(argv[1], argv[2], interesting_resolution, transcode_video_fp, encode_image_fp, video_has_audio_fp);
    
    long long end = time_now_ns();
    long long duration_ns = end - start;
    printf("Total Time Taken: %lld ns (%.3f ms)\n",
           duration_ns,
           duration_ns / 1e6);
    return 0;
}

