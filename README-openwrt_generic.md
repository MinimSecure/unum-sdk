# Unum for OpenWrt

The "openwrt_generic" target is designed to be called from an OpenWrt 
buildroot using the related [minim-openwrt-feed][1].

Download [a prebuilt OpenWrt image][2] for your router or install the Unum 
package on a device already running OpenWrt.


## Prerequisites

The Unum agent securely communicates with [Minim][3] cloud servers and requires
a Minim Labs developer account.

Additionally, the configured LAN network adapter's MAC address is used as the
unique identifier for a given instance of the Unum agent. This MAC address must
be associated with an active account on Minim Labs for the agent to function.

Sign up for an account on the [Minim Labs website][4].


## Installation

Install Unum on a device running OpenWrt using the `opkg` tool.


1. Register your OpenWrt device's LAN MAC address in the [Minim Labs 
   dashboard][5].
   
2. Download [an appropriate `.ipk` for your platform][2].

3. Copy the `.ipk` file to your device.
   ```bash
   # Be sure to modify the username and IP as needed.
   scp unum_*.ipk root@192.168.1.1:/tmp
   ```

4. Install the `.ipk` using `opkg install`.
   ```bash
   ssh root@192.168.1.1 opkg install /tmp/unum_*.ipk
   ```

5. Visit the Minim Labs dashboard, the device should appear online within a
   a minute or so. If it does not appear online in the portal, try rebooting
   the device.
   > Check the [Troubleshooting section](#troubleshooting) for more debugging
     steps.


## Building

Unum for OpenWrt is built using a standard OpenWrt buildroot with the Minim
feed installed.
 
> This document assumes you have an existing buildroot that is configured with
  your preferred platform and packages.

1. Add Minim's OpenWrt feed to your buildroot `feeds.conf`, then update the
   feed and enable the Unum package.
   ```bash
   echo 'src-git minim https://github.com/MinimSecure/minim-openwrt-feed' >> feeds.conf
   ./scripts/feeds update minim
   ./scripts/feeds install unum
   ```

2. Using `menuconfig` or `oldconfig`, go to the Network category then find and
   set Unum as either *built-in* (pre-installed on the image) or *module*.
   ```bash
   make menuconfig
   ```

3. Build the Unum package.
   ```bash
   make package/unum/compile
   ```
   > At this point you might instead build a full image and then flash your 
     device. Do this by running `make` with no arguments:
     ```bash
     make
     ```

4. Follow the [Installation instructions](#installation) with the newly built
   Unum ipk. It will be placed inside `bin/packages/<chipset>/minim/`, where
   `<chipset>` is replaced with the target device's chipset.
   ```bash
   ls -al bin/packages/arm_cortex-a7_neon-vfpv4/minim/unum_*.ipk
   ```
   > The folder `arm_cortex-a7_neon-vfpv4` corresponds to the target device's 
   chipset. In this example, the GL.iNet B1300 chipset is used.


## Troubleshooting

* Where can I find log files for Unum?
  * Logs are written to `/var/log/unum.log`, `/var/log/http.log`, and 
    `/var/log/monitor.log`.


[1]: https://github.com/MinimSecure/minim-openwrt-feed
[2]: https://github.com/MinimSecure/unum-sdk/releases/latest
[3]: https://www.minim.co
[4]: https://www.minim.co/labs
[5]: https://my.minim.co/labs
