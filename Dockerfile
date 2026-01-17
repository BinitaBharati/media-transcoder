FROM ubuntu:22.04

RUN apt update
RUN apt install -y gcc
RUN apt install -y ffmpeg

WORKDIR /workspace
COPY ./src /workspace/src
RUN mkdir -p /workspace/input/assets

CMD ["tail", "-f", "/dev/null"]