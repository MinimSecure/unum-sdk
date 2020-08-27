# Unum SDK
[![CircleCI Build Status](https://img.shields.io/circleci/project/github/MinimSecure/unum-sdk.svg?style=flat-square)][9]
[![Latest Release](https://img.shields.io/github/release/MinimSecure/unum-sdk.svg?style=flat-square)][8]

Minim is an AI-driven smart home WiFi managment and security platform that offers IoT fingerprinting, security, managment, as well as operator and subscriber APIs that can be leveraged to build custom services. All of this is enabled by the Unum Agent which runs on the home gateway.

This repository contains the open source distribution of
[Minim][1]'s Unum agent and related libraries as well as build scripts and reference code, cumulatively known as the Unum SDK.

# Technical Background

Unum is a cross-platform C program designed for Linux and can be built and installed on
OpenWrt-compatible devices, Asus MAP AC1300, Asus MAP AC1700 as well as
Linux x86_64, i386, and armhf architectures.


## I am a...

### ...router manufacturer and want to port the agent to my device.

First, you should familiarize yourself with the requirements for the agent and the information you'll need to know to begin integrating the agent with your platform. You can review the following documents to understand the detailed requirements.

* [System and Hardware Requirements][18]
* [Integrator Information Checklist][17]

After confirming that you have everything you need and that the device meets all requirements, clone the agent code and review one of the reference implementations (such as Asus AC1300).

Once familiar with the source layout and with the platform specific functions that are required, you can follow the [Integration Guide][16] that Minim developers use to support new platforms.


### ...hobbyist and want to setup minim on my own my device.

> Running the Unum agent requires a Minim Labs developer account. 
> [Sign up for an account][3] on the Minim website.

The Unum agent is designed to operate on a network gateway device or wireless
access point. Once installed, link your device with the [Minim cloud][3] to 
begin gaining insight into your network.

- Read the "**[Integration Document][16]** and **Build the SDK** yourself for these supported platforms:
  - **[OpenWRT:][7]** build Unum for LEDE / OpenWRT distributions
  - **[Asus MAP AC1300:][14]** build Unum for Asus MAP AC1300 Router
  - **[Asus MAP AC1700:][15]** build Unum for Asus MAP AC1700 Router
  - **[Linux:][6]** build Unum for Linux and Linux-like distributions
  - **[Docker:][5]** run a Linux router with Unum in a Docker container
- For **guides and documentation**, check out the **[Unum SDK wiki][11]**.


### ...developer at Minim. 

For the step by step instructions for adding new hardware kinds, running unit tests, details on how to add the new hardware kind to the cloud as well as details about how to setup new automated builds, take a look at the instructions here:

[Minim Developer Gude][16]

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
[16]: README-integrators.md
[17]: https://docs.google.com/document/d/12AgcU3-53aqWTult7zeA1Iob2EMCH2r1G8eU5hQudXY
[18]: https://docs.google.com/document/d/1BUxOs0HjPbo2NGN--1TioqslomtKrvLTNm_CBdIbSQ8
