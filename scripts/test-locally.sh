#!/bin/bash

docker_cpus="1 4 8 16 32"
pulsar_cpus="1 4"
message_rate="100 600 1200"
topics="1 32 256"

# docker_cpus="1 4 8"
# pulsar_cpus="16"
# message_rate="1000 5000 10000"
# topics="200 300"

duration=300
asio_threads=4

#set -x

for dcpu in ${docker_cpus} ; do
    for pcpu in $pulsar_cpus ; do
        for rate in $message_rate ; do
            for ntopic in $topics ;  do
                echo "test $dcpu $pcpu $rate $ntopic"
                ./run-test.sh --docker-cpus $dcpu \
                    --pulsar-cpus $pcpu \
                    --messages-per-second $rate \
                    --topics $ntopic \
                    --duration $duration \
                    --disk-path /mnt/ramdisk/pulsar/pdata/ \
                    --restart-pulsar \
                    --storage ramdisk \
                    --asio-threads $asio_threads \
                    --location `hostname` >> tests.log
            done
        done
    done
done
