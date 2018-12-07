ARG builder=minimsecure/unum-builder:ubuntu-16.04

FROM ${builder} as build

FROM ubuntu:16.04

RUN apt-get update && \
    apt-get install -y \
        bridge-utils \
        dnsmasq \
        dnsutils \
        gawk \
        hostapd \
        iptables \
        iputils-ping \
        iw \
        libcurl3 \
        libnl-3-200 \
        libnl-genl-3-200 \
        libjansson4 \
        net-tools \
        rfkill \
        usbutils \
        vim \
        wireless-tools

WORKDIR /root/
COPY --from=build /usr/local/src/unum/out/linux_generic/* .

RUN mkdir -p /opt/unum && \
    tar -C /opt/unum -xf /root/linux_generic*

RUN bash /opt/unum/extras/install.sh --no-interactive --extras --profile

RUN sed -i -e 's:wlan0:eth2:' -e 's:eth0:eth1:' /etc/opt/unum/config.json
