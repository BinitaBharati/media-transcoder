# Build

`docker build -t media-processor .`

# Deploy

`docker run -d --name media-processor media-processor`

# Run
* Log in to the container: `docker run exec -it media-processor /bin/bash`
* Copy the media resources manually from the host to the container. Sample command to copy from host : `docker cp ~/media-processing-assets/ media-processor:/workspace/input/assets`
* Compile the C program. `gcc src/media_processing.c -o out/media_processing.bin`
* Run the main program. `gcc out/media_processing.bin <input assets parent directory> <output assets parent directory>`. The output assets directory must be manually created before running the program. Sample command: `gcc out/media_processing.bin /workspace/input/assets/ /workspace/output/assets`
* In case, run fails, you can recompile teh program enabling ASAN, and then re-run the main program. It will give you exact line number in code where error has occured. `gcc -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer src/media_processing.c -o out/media_processing.bin`

# Observation
* It was noted that a single ffmpeg process uses 12 CPU cores. My host(actualy VM on a compute)  had 16 cores, so I did not attempt to parallelise the execution, as I dont have enough cores. But, if you have more than 16 cores, like 32, 64 cores etc, then parallelisation of the program may improve execution speed.
* 

