# Overview
This project was taken up to compare media transcoding quality using `ffmpeg` vs  `Adobe Media Encoder`.
There is a blog that talks about this in the FootNote section https://medium.com/@binitabharati/transcoding-video-and-images-with-adobe-media-encoder


# Build

`docker build -t media-processor .`

# Deploy

`docker run -d --name media-processor media-processor`

# Run
* Log in to the container: `docker exec -it media-processor /bin/bash`
* Copy the media resources manually from the host to the container. Sample command to copy from host : `docker cp ~/media-processing-assets media-processor:/workspace/input/assets`
* There are multiple C main files in this project.
  - media_sorter - This code will segregate a given input directory content by media resolution and media extension. You will need to provide the input directory that has the media resources,  the output directory where the segregated media resources should be saved, and the interesting file extension that you want to segregate for. 
  
    Compile media_sorter: `gcc src/media_sorter.c -o out/media_sorter.bin`.
    
    Run media_sorter: `out/media_sorter.bin /workspace/input/assets/assets/25.1-assets/ /workspace/output/sorted/ mp4`
    
  - media_processor - This code will transcode all the media resources inside the input directory to various resolutions. You will need to provide the input directory that has the segregated media resources,  the output directory where the transcoded media resources should be saved, and the interesting file resolution that you want to transcode for. 

  Compile media_processor: `gcc src/media_processing.c -o out/media_processing.bin`

  Run media_processor: `out/media_processing.bin /workspace/output/sorted/ /workspace/output/sorted 1920X1080`
                        Total Time Taken: 1575541877521 ns (1575541.878 ms)
                        Total Time Taken: 1452380697101 ns (1452380.697 ms)
                        Total Time Taken: 254021654176 ns (254021.654 ms)

  
* In case, any of the run fails, you can recompile the respective program enabling ASAN, and then re-run the main program. It may help you to find the faulty code. `gcc -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer src/media_processing.c -o out/media_processing.bin`

# Observation
* It was noted that a single ffmpeg process uses 12 CPU cores. My host(actualy VM on a compute)  had 16 cores, so I did not attempt to parallelise the execution, as I dont have enough cores. But, if you have more than 16 cores, like 32, 64 cores etc, then parallelisation of the program may improve execution speed.
  

