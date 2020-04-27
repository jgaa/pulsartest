FROM debian:buster

LABEL maintainer="jgaa@jgaa.com"

RUN DEBIAN_FRONTEND="noninteractive" apt-get -q update &&\
    DEBIAN_FRONTEND="noninteractive" apt-get -y -q --no-install-recommends upgrade &&\
    DEBIAN_FRONTEND="noninteractive" apt-get install -y -q \
        libssl1.1 libboost-atomic1.67.0 libboost-chrono1.67.0 \
        libboost-context1.67.0 libboost-coroutine1.67.0 libboost-date-time1.67.0 libboost-filesystem1.67.0 libboost-log1.67.0 libboost-program-options1.67.0 \
        libboost-regex1.67.0 libboost-system1.67.0 libboost-thread1.67.0 \
        libprotobuf-lite17 libcurl4 libssl1.1 libunistring2 libgnutls30
        
COPY ./pulsartests /usr/local/bin
COPY ./libpulsar.so* /usr/local/lib

