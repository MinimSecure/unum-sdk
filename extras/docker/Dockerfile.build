FROM debian:stretch-slim
ARG unum_version=2019.2.0

RUN apt-get update && \
    apt-get install -y \
        build-essential \
        gawk \
        gettext \
        git \
        libcurl4-openssl-dev \
        libjansson-dev \
        libnl-3-dev \
        libnl-genl-3-dev \
        libssl-dev \
        vim

WORKDIR /usr/local/src/unum
COPY . .

RUN make MODEL=linux_generic AGENT_VERSION=${unum_version}
