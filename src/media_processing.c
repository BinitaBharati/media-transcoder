#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "util/fs_traverser.h"

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


//get resolution of a image or video file.
void get_resource_resolution(const char *input, int *width, int *height) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "ffprobe -v error -select_streams v:0 "
             "-show_entries stream=width,height "
             "-of csv=s=x:p=0 \"%s\"", input);//this command returns output like 1920x1080
    printf("executing resource resolution command = \n%s\n",cmd);

    /**
     *  You get a FILE stream whose output is the text printed by the command.

        fgets() is the safest way to read from any FILE* stream.
     * 
     */
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        perror("popen failed");
        exit(1);
    }

    char buffer[128];
    //fgets() is the safest way to read from any FILE* stream.
    if (fgets(buffer, sizeof(buffer), fp)) {
        //sscanf() parses a string and extracts numbers from it
        //sscanf() wants addresses to store the parsed integers
        //thats why in input arg , we passed pointer to int ie int *width, int *height
        sscanf(buffer, "%dx%d", width, height);
    } else {
        /**
         * fprintf works like printf, but instead of always printing to the console (stdout), it lets you choose which file or stream to write to.

        Common streams:

        stdout → console output
        stderr → error output
        FILE *fp → any file opened with fopen.

        In this case, since we are printing to stderr, we could have as well used printf.
         * 
         */
        fprintf(stderr, "Failed to read ffprobe output\n");
        pclose(fp);
        exit(1);
    }
    pclose(fp);
}

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

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <input directory> <output directory>\n", argv[0]);//the input directory can contain medias in multiple directorries, recursive directory look up will be done.
        return 1;
    }

    long long start = time_now_ns();

    char* assetInputDirectory = argv[1];
    char* assetOutputDirectory = argv[2];//ensure that output directory already exists by creating manually before invoking this program.

    char inputPathDiscoveryLogFile[100];//this file will list all media files (.mp4, .png. ,jpeg etc along with their resolutions. I did this so I know the details of the media I am dealing with.)
    snprintf(inputPathDiscoveryLogFile, sizeof(inputPathDiscoveryLogFile), "%s/%s", assetOutputDirectory, "inputPathDiscovery.log");
    printf("inputPathDiscoveryLogFile = %s\n",inputPathDiscoveryLogFile);
    FILE *inputPathDiscoveryLogFp = fopen(inputPathDiscoveryLogFile, "w");//inputPathDiscovery.log file will get created if not exists, parent directory should exist.
    if (!inputPathDiscoveryLogFp) return 1;

    void (*get_resource_resolution_fp)(const char *, int *, int *) = get_resource_resolution;//function pointer variable named get_resource_resolution_fp pointing to get_resource_resolution method
    void (*transcode_video_fp)(const char *, const char *, int, int, const char *) = transcode_video;//function pointer variable named transcode_video_fp pointing to transcode_video
    void (*encode_image_fp)(const char *, const char *, int, int) = encode_image;//function pointer variable named encode_image_fp pointing to encode_image
    int (*video_has_audio_fp)(const char *) = video_has_audio;//function pointer variable named video_has_audio_fp pointing to video_has_audio method

    process_path(argv[1], argv[2], get_resource_resolution_fp, transcode_video_fp, encode_image_fp, video_has_audio_fp, inputPathDiscoveryLogFp);
    
    fclose(inputPathDiscoveryLogFp);

    long long end = time_now_ns();
    long long duration_ns = end - start;
    printf("Total Time Taken: %lld ns (%.3f ms)\n",
           duration_ns,
           duration_ns / 1e6);
    return 0;
}

