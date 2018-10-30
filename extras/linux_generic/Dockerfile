ARG builder=unum-builder-ubuntu:16.04

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
        net-tools \
        rfkill \
        usbutils \
        vim \
        wireless-tools

WORKDIR /root/
COPY --from=build /usr/local/src/unum/out/linux_generic/* .

RUN mkdir -p /opt/unum && \
    tar -C /opt/unum -xf /root/linux_generic*

RUN mkdir -p /var/opt/unum && \
    mkdir -p /etc/opt && \
    cp -r /opt/unum/dist/etc/opt/unum /etc/opt/unum

RUN bash /opt/unum/extras/install_extras.sh
