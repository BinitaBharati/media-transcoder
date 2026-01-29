#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#define BUF_SIZE 8192

typedef struct {
    char ext[32];
    int count;
} ExtCount;

typedef struct {
    const char *name;
    int width;
    int height;
    char* bitRate;//this is solely bit rate of the video stream (not audio track, in case video has audio too). audiot streams also have bit rate, but we are not concerened about that here.
} VideoTranscoderConfig;

// Preset table
static const VideoTranscoderConfig VIDEO_ENCODER_PRESETS[] = {
    /*
    All below sizes have the same aspect ratio. So, you can upscale and downscale without causing distortion.
    */
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

long long time_now_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;//current clock time in ns
}


//mkdir recursive
int mkdir_p(const char *path, mode_t mode)
{
    char tmp[PATH_MAX];
    char *p = NULL;
    size_t len;

    if (!path || !*path)
        return -1;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);

    /* remove trailing slash */
    if (tmp[len - 1] == '/')
        tmp[len - 1] = '\0';

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';

            if (mkdir(tmp, mode) != 0) {
                if (errno != EEXIST)
                    return -1;
            }

            *p = '/';
        }
    }

    /* final directory */
    if (mkdir(tmp, mode) != 0) {
        if (errno != EEXIST)
            return -1;
    }

    return 0;
}

int copy_file(const char *src, const char *dst)
{
    int in_fd = open(src, O_RDONLY);
    if (in_fd < 0) {
        perror("open src");
        return -1;
    }

    int out_fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out_fd < 0) {
        perror("open dst");
        close(in_fd);
        return -1;
    }

    char buf[BUF_SIZE];
    ssize_t bytes;

    while ((bytes = read(in_fd, buf, sizeof(buf))) > 0) {
        ssize_t written = write(out_fd, buf, bytes);
        if (written != bytes) {
            perror("write");
            close(in_fd);
            close(out_fd);
            return -1;
        }
    }

    if (bytes < 0) {
        perror("read");
    }

    close(in_fd);
    close(out_fd);
    return (bytes < 0) ? -1 : 0;
}

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

void get_name_without_ext(const char *input, char *output, size_t outSize) {
    /**
     * C’s strchr() and Java’s String.indexOf() do a similar job (find a character in a string), but their return values and behavior are different.
     * Returns a char pointer to the first occurrence
     * 
     */
    const char *dot = strchr(input, '.');  // find first '.'

    size_t len;
    if (dot != NULL) {
        len = dot - input;  // number of chars before '.'. Does pointer arithmetic, and gives the length of the file name part in input.
    } else {
        // no dot → copy entire string
        len = strlen(input);//strlen is same as Java's String length
    }

    // ensure we don't overflow output buffer
    if (len >= outSize)
        len = outSize - 1;

    strncpy(output, input, len);
    output[len] = '\0';     // null terminate
    printf("get_name_without_ext: input = %s, output = %s\n",input, output);
}

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

/**
 * The Euclidean rule is: gcd(a, b) = gcd(b, a % b)
 * 
 * This code repeatedly replaces (a, b) with (b, a % b) until b becomes 0.
   At that point, a is the GCD. This rule does not make any assumption about a being > b.

   gcd(20, 6)
    → gcd(6, 20 % 6)
    → gcd(6, 2) = gcd(2, 0) = answer is 2.
    
 */
int gcd(int a, int b) {
    while (b != 0) {
        int t = b;
        b = a % b;
        a = t;
    }
    return a;
}

int is_aspect_ratio_16_9(int width, int height) {
    int g = gcd(width, height);
    printf("Aspect Ratio: %d:%d\n", width / g, height / g);
    if (width / g == 16 && height / g == 9) {
        return 1;
    }
    return 0;
   
}

int is_size_already_present(char** sixteen_eachto_nine_sizes, int sixteen_eachto_nine_sizes_length, char* input_size) {
    for (int i = 0 ; i < sixteen_eachto_nine_sizes_length ; i++) {
        if (sixteen_eachto_nine_sizes[i] != NULL) {
            if (strcmp(sixteen_eachto_nine_sizes[i], input_size) == 0) {
                printf("resolution %s already present\n", input_size);
                return 1;
            }
        }
    }
    printf("resolution %s not present\n", input_size);
    return 0;
}

char* get_immediate_parent_dir(char* full_path) {
    if (!full_path) {
        return NULL;
    }

    char* last_index_slash = strrchr(full_path, '/');
    if (!last_index_slash) return NULL;

    if(last_index_slash == full_path) return NULL;
    //find immediate slash in path preceding last_index_slash.
    char* prev_slash_index = last_index_slash - 1;
    while (prev_slash_index != full_path && *prev_slash_index != '/') {
        prev_slash_index--;
    }

    if (*prev_slash_index != '/') return NULL;
    char* parent_dir_name = calloc(32, sizeof(char));

    size_t len = last_index_slash - prev_slash_index - 1;
    if (len >= 32) return NULL;

    //strncpy(parent_dir_name, prev_slash_index + 1, last_index_slash - prev_slash_index - 1);//null terminator will be appended by strncpy, as char* of 32 chars is big enough to hold all type of media resolution
    //return parent_dir_name;

    memcpy(parent_dir_name, prev_slash_index + 1, len);
    parent_dir_name[len] = '\0';
    return parent_dir_name;
}