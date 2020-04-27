#!/bin/sh

# Simple docker-container from local build.
# No need for CI for this project.

cp -v ../build/pulsartests /usr/local/lib/libpulsar.so.2.6.0-SNAPSHOT .
docker build -t jgaafromnorth/pulsartests . &&\
docker push jgaafromnorth/pulsartests
