#!/bin/bash

echo "Starting pulsar"

# docker run --rm -it --name pulsar -d \
#   -p 6650:6650 \
#   -p 8080:8080 \
#   --mount source=pulsardata,target=/pulsar/data \
#   --mount source=pulsarconf,target=/pulsar/conf \
#   apachepulsar/pulsar:2.5.0 \
#   bin/pulsar standalone > pulsar.log
  
docker run --rm -it --name pulsar -d \
  -p 6650:6650 \
  -p 8080:8080 \
  --mount type=bind,src=/mnt/ramdisk/pulsar/pdata,dst=/pulsar/data \
  --mount source=pulsarconf,target=/pulsar/conf \
  apachepulsar/pulsar:2.5.0 \
  bin/pulsar standalone 
  #> pulsar.log
  
# docker run --rm -it --name pulsar -d \
#   -p 6650:6650 \
#   -p 8080:8080 \
#   -v /mnt/ramdisk/pulsar/pdata:/pulsar/data \
#   -v /mnt/ramdisk/pulsar/pconf:/pulsar/conf \
#   apachepulsar/pulsar:2.5.0 \
#   bin/pulsar standalone > pulsar.log
#   
