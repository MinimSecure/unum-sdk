FROM ubuntu:16.04

RUN apt-get update && \
    apt-get install -y \
        build-essential \
        gawk \
        git \
        libcurl4-openssl-dev \
        libjansson-dev \
        libnl-3-dev \
        libnl-genl-3-dev \
        vim

WORKDIR /usr/local/src/unum
COPY . .

RUN make MODEL=linux_generic
