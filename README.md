# Unum SDK

This repository contains the open source distribution of
[Minim][1]'s Unum agent and related, cumulatively known as the Unum SDK.

> Running the Unum agent requires a Minim Labs developer account. 
> [Sign up for an account][3] on the Minim website.

Unum is a cross-platform program and can be built and installed on
OpenWrt-compatible devices, Asus MAP AC1300, Asus MAP AC1700 as well as
Linux x86_64, i386, and armhf architectures.

[![CircleCI Build Status](https://img.shields.io/circleci/project/github/MinimSecure/unum-sdk.svg?style=flat-square)][9]
[![Latest Release](https://img.shields.io/github/release/MinimSecure/unum-sdk.svg?style=flat-square)][8]


## Getting started

The Unum agent is designed to operate on a network gateway device or wireless
access point. Once installed, link your device with the [Minim cloud][3] to 
begin gaining insight into your network.

- **Download [a pre-built release for your platform][8]** from the Releases page
- **Build the SDK** yourself for these supported platforms:
  - **[OpenWRT:][7]** build Unum for LEDE / OpenWRT distributions
  - **[Asus MAP AC1300:][14]** build Unum for Asus MAP AC1300 Router
  - **[Asus MAP AC1700:][15]** build Unum for Asus MAP AC1700 Router
  - **[Linux:][6]** build Unum for Linux and Linux-like distributions
  - **[Docker:][5]** run a Linux router with Unum in a Docker container
- For **guides and documentation**, check out the **[Unum SDK wiki][11]**.

## Build and Test

Build and Test instructions are captured in [Build][16].

## Contributing

Guidelines around contributing to Unum SDK are [outlined in
CONTRIBUTING.md][10]. 

Check the [Developer Guide on the wiki][12] for more
information on the technical aspects of the SDK.


## Get in touch

The Minim team is always happy to chat. Send us a mail at [labs@minim.co][2] or 
[visit our website][1] to find out more!

[1]: https://www.minim.co
[2]: mailto:labs@minim.co
[3]: https://www.minim.co/labs
[4]: https://www.docker.com
[5]: extras/docker/README-docker.md
[6]: README-linux_generic.md
[7]: README-lede_generic.md
[8]: https://github.com/MinimSecure/unum-sdk/releases/latest
[9]: https://circleci.com/gh/MinimSecure/unum-sdk/tree/master
[10]: CONTRIBUTING.md
[11]: https://github.com/MinimSecure/unum-sdk/wiki
[12]: https://github.com/MinimSecure/unum-sdk/wiki/Developer-Guide
[13]: LICENSE
[14]: README-asus_map_ac1300.md
[15]: README-asus_map_ac1700.md
[16]: Build.md
