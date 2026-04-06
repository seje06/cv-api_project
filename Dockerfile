FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    libopencv-dev \
    libcurl4-openssl-dev \
    nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*

RUN git clone https://github.com/yhirose/cpp-httplib.git /tmp/httplib \
    && cp /tmp/httplib/httplib.h /usr/local/include/ \
    && rm -rf /tmp/httplib

WORKDIR /app
COPY . /app

RUN cmake -S . -B build && cmake --build build -j

EXPOSE 8080

CMD ["./build/ImageApiServer"]