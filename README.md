# Unum SDK

This repository contains the open source distribution of
[Minim][1]'s Unum agent and related, cumulatively known as the Unum SDK.

> Running the Unum agent requires a Minim Labs developer account. 
> [Sign up for an account][3] on the Minim website.

#### Status: in development

Currently, the Unum agent can be built and installed on OpenWrt compatible
devices, as well as Linux x86_64 targets. Tested with Ubuntu 16 and 18.


## Getting started

The Unum agent is designed to operate on a network gateway device or wireless
access point. Once installed, link your device with the Minim cloud to begin
gaining insight into your network.


### Docker Quick-start

A simple way to try out Minim and the Unum agent on Linux is using [Docker][4].

> Due to the Docker network configuration used (`macvlan`), only Linux hosts
> can run the container.

Check [the `extras/linux_generic` folder][5] for complete instructions.


## Building and Installing

Download or clone this repository, then check the platform-specific README
for detailed instructions for that platform.

Available platforms:

 - [OpenWrt][7]: build an installable package for routers running OpenWrt
 - [Linux aka `linux_generic`][6]: build Unum for running directly on Linux
 - [Docker for Linux][5]: run a Linux router with Unum in a Docker container


## Get in touch

The Minim team is always happy to chat. Send us a mail at [labs@minim.co][2] or 
[visit our website][1] to find out more!

[1]: https://minim.co
[2]: mailto:labs@minim.co
[3]: https://my.minim.co/developers/sign_up
[4]: https://www.docker.com
[5]: extras/linux_generic/README-docker
[6]: README-linux_generic
[7]: https://github.com/MinimSecure/minim-openwrt-feed
