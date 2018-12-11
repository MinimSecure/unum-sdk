# Unum SDK

This repository contains the open source distribution of
[Minim][1]'s Unum agent and related, cumulatively known as the Unum SDK.

> Running the Unum agent requires a Minim Labs developer account. 
> [Sign up for an account][3] on the Minim website.

The Unum agent is a closs-platform program and can be built and installed on
OpenWrt-compatible devices, as well as Linux x86_64, i386, and armhf 
architectures.


## Getting started

The Unum agent is designed to operate on a network gateway device or wireless
access point. Once installed, link your device with the Minim cloud to begin
gaining insight into your network.

Download [a pre-built release for your platform][8] from the Releases page on
the Unum SDK GitHub.

Available platforms:

 - **[OpenWrt:][7]** build an installable package for routers running OpenWrt
 - **[Linux:][6]** build Unum for Linux and Linux-like distributions
 - **[Docker:][5]** run a Linux router with Unum in a Docker container


### Docker Quick-start

A simple way to try out Minim and the Unum agent is using [Docker][4].

Check [the `extras/docker` folder][5] for complete instructions.


## Get in touch

The Minim team is always happy to chat. Send us a mail at [labs@minim.co][2] or 
[visit our website][1] to find out more!

[1]: https://www.minim.co
[2]: mailto:labs@minim.co
[3]: https://my.minim.co/labs
[4]: https://www.docker.com
[5]: extras/docker/README-docker.md
[6]: README-linux_generic.md
[7]: https://github.com/MinimSecure/minim-openwrt-feed
[8]: https://github.com/MinimSecure/unum-sdk/releases