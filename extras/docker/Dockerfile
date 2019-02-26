ARG builder=minimsecure/unum-builder:debian-stretch-slim

FROM ${builder} as build

FROM debian:stretch-slim

RUN apt-get update && \
    apt-get install -y \
        bridge-utils \
        dnsmasq \
        dnsutils \
        gawk \
        hostapd \
        ifupdown \
        iptables \
        iputils-ping \
        iproute2 \
        iw \
        libcurl3 \
        libnl-3-200 \
        libnl-genl-3-200 \
        libjansson4 \
        rfkill \
        usbutils \
        vim \
        wireless-tools

WORKDIR /root/
COPY --from=build /usr/local/src/unum/out/linux_generic/linux_generic* .

RUN mkdir -p /opt/unum && \
    tar -C /opt/unum -xf /root/linux_generic*

RUN bash /opt/unum/extras/install.sh --no-interactive --extras --aio --profile && \
    echo 'service_type="manual"' >> /opt/unum/.installed

RUN sed -i -e 's:wlan0:eth2:' -e 's:eth0:eth1:' /etc/opt/unum/config.json
