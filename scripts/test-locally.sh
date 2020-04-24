#!/bin/bash

docker_cpus=(1 4 8 16 32)
pulsar_cpus=(1 4)
message_rate=(100 600 1200)
duration=300
topics=(1 32 256)
asio_threads=4

set -x

for dcpu in $docker_cpus ; do
    for pcpu in $pulsar_cpus ; do
        for rate in $message_rate ; do
            for ntopic in $topics ;  do
                ./run-test.sh --docker-cpus $docker_cpus \
                    --pulsar-cpus $pulsar_cpus \
                    --messages-per-second $message_rate \
                    --topics $ntopic \
                    --duration $duration \
                    --disk-path /mnt/ramdisk/pulsar/pdata/ \
                    --restart-pulsar \
                    --storage ramdisk \
                    --asio-threads $asio_threads
                    --location "jgaaWS" >> tests.log
            done
        done
    done
done
