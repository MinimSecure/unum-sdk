# Unum for Linux

The "linux_generic" target is suited for building the Unum agent for any
supported linux platforms including x86_64, i386, and armhf.

> Check [README-docker.md](extras/docker/README-docker.md) for
  instructions on running Unum in a Docker container with basic Linux
  router facilities.

Download [a pre-built binary for your platform][3] from the GitHub releases 
page.


## Prerequisites

The Unum agent securely communicates with [Minim][2] cloud servers and requires
a Minim Labs developer account.

Additionally, the configured LAN network adapter's MAC address is used as the
unique identifier for a given instance of the Unum agent. This MAC address must
be associated with an active account on Minim Labs for the agent to function.

Find out more and sign up for an account on the [Minim Labs website][1].


## Building

As far as build dependencies, building Unum requires make, gcc, libc6-pic, some
flavor of libcurl-dev and openssl-dev, libjansson-dev, and a netlink library 
like libnl-3-200 or libnl-tiny.

On a fresh Ubuntu or Debian install, for example:

```bash
sudo apt-get update
sudo apt-get install build-essential \
    libcurl4-openssl-dev gawk libjansson-dev \
    libnl-3-dev libnl-genl-3-dev libssl-dev
```

Use the make command to begin the build process. The default target will build 
and generate a tarball containing the resulting binaries and supporting files.

As a regular user, change to the directory containing the Unum SDK and then
invoke `make`:

```bash
make MODEL=linux_generic
```

The resulting tarball is placed into the `out/` directory relative to the
project root. The original files used to make the tarball exist below the
build directory, in `build/linux_generic/rfs`.

### Build options

By default, the SDK produces "release" binaries. This can be customized with
build options passed to make.

#### Debug mode

Enable debug mode to build Unum with verbose logging as well
as built-in test mode with `UNUM_DEBUG=1`:

```bash
make MODEL=linux_generic UNUM_DEBUG=1
```

#### Build Model

Use `MODEL=&lt;model_name&gt;` to alter the build model.

```bash
make MODEL=asus_map_ac1300
```

#### Bundled extras

By default, ["extra" utilities](extras/linux_generic/README-linux_extras.md) 
are installed. Disable this option with `INSTALL_EXTRAS=0`:

```bash
make INSTALL_EXTRAS=0
```


## Installing

The Unum agent requires libcurl (3 or later), libjansson4 (2.7 or later), and
libnl-3-200 and libnl-genl-3-200 to run correctly. Bash scripts included in 
"extras" additionally depends on gawk, iproute2, and ifupdown.

For Debian 9 or Ubuntu 18, for example:

```bash
sudo apt install -y gawk libcurl3 libjansson4
```

Building the SDK results in a tarball containing the `unum` binary and required
shared libraries, as well as default configuration files and some utility bash
scripts. The tarball is designed to be extracted inside a newly-created and 
empty directory.

The recommended location is `/opt/unum`. For example, while in the project
root directory:

```bash
sudo mkdir -p /opt/unum
sudo tar -C /opt/unum -xf out/linux_generic/*.tgz
```

After extracting the `.tgz`, either use the [guided installer script](#guided-install)
or [manually create](#manual-install) the necessary directories and files to
complete the installation. Both options are explained below.

### Guided install

Installation is simplified by using the interactive installer script that
allows the user to customize and automate the installation.

Run the script as root:

```bash
sudo /opt/unum/extras/install.sh
```

The user will be prompted for basic installation paths and whether to install
optional components.

Brief overview of supported options:

- Modify configuration and log file storage paths
- Optionally install a profile.d script that adds the `unum` command 
  to users' PATH.
- Optionally install a systemd init script so that Unum can be controlled
  with `service unum start|stop|status`
- Optionally install extra shell scripts and base configuration files that
  handle automatically configuring and starting hostapd, dnsmasq, unum, and
  necessary iptables rules for running a Linux router.
  > This is used in the Unum for Docker container and the Unum "all-in-one"
    package.
    Check [README-linux_extras.md](extras/linux_generic/README-linux_extras.md#other-tools)
    for a bit more information on these scripts. 
- Optionally install Unum "all-in-one" and `minim-config` utility.
  > Check [README-linux_extras.md](extras/linux_generic/README-linux_extras.md#minim-config)
    for more information about this option.
- Remove an existing installation:
  ```bash
  sudo /opt/unum/extras/install.sh --uninstall
  ```
  > Note: uninstalling will not remove configuration files. Use `--purge` to
    completely remove all traces of Unum.

> For more information on the `install.sh` script, check 
  [README-linux_extras.md](extras/linux_generic/README-linux_extras.md#installsh).


### Manual install

By default, Unum will store its configuration files in `/etc/opt/unum`.
Similarly, the agent will store logs and other temporary runtime files in
`/var/opt/unum`. These directories must be created:

```bash
sudo mkdir -p /var/opt/unum /etc/opt/unum
```

Included in the installation directory is a default configuration file.
Copy the bundled files into the system directory:

```bash
sudo cp -r /opt/unum/dist/etc/opt/unum/* /etc/opt/unum/
```

> Note that the `/var/opt/unum` directory must be writable by the user the 
  Unum agent is running as.

Next, the PATH and LD_LIBRARY_PATH environment variables must be modified 
to include the relevant Unum directories. Link the provided script that does
this into `/etc/profile.d/`:

```bash
sudo ln -sf /opt/unum/dist/etc/profile.d/unum.sh /etc/profile.d/unum.sh
```

### Uninstalling

Manually remove all traces of an installation:

```bash
sudo killall -9 unum
sudo rm -rvf /opt/unum /var/opt/unum /etc/opt/unum
sudo rm -vf /etc/profile.d/unum.sh
sudo rm -vf /etc/systemd/system/unum.service
```


## Usage

The `unum` command is a long-running daemon, meant to be started as a system
service.

> The environment variables `LD_LIBRARY_PATH` and `PATH` need to
  be modified for `unum` to function correctly. 
  See ["Installing"](#installing) for instructions on doing this.

During normal operation, a running Unum agent securely communicates with the 
Minim cloud to transfer network telemetry. When the agent connects to Minim
for the first time, a client certificate is generated and stored locally and
used to authenticate all subsequent requests.

To avoid potential misuse, Minim servers only accept first-time provisioning
from known devices-- the LAN network interface MAC address must be associated
with a Minim Labs developer account. Sign up for an account or find out more
on the [Minim Labs website][1].

> Currently, Minim generates a MAC address and the device running Unum must
  spoof its LAN network interface's MAC address to match. Doing this varies
  greatly from platform to platform, but as one example using `ifconfig`:

  ```bash
  # Replace 'wlan0' below, if necessary, and change '4e:00:00:12:34:56' to the 
  # assigned MAC address displayed in the Minim Labs portal.
  ifconfig wlan0 down hw ether 4e:00:00:12:34:56 up
  ```


### Runtime options

Unum for Linux uses a JSON file for runtime configuration. Each command line
option supported by the `unum` command may also be specified in the JSON
configuration file. Use the option's "long name" without preceding dashes as
a key in the configuration file.

Below is a minimal example JSON configuration that specifies `wlan0` as the 
main LAN network interface and `eth0` as the WAN interface:

```json
{
  "lan-if": "wlan0",
  "wan-if": "eth0"
}
```

The above example can be expressed with command line options instead:

```bash
unum --lan-if wlan0 --wan-if eth0
```


### Required options

At a minimum, the Unum agent requires these options be specified in either a
JSON configuration file or as a command line argument.

* A LAN network interface with the expected MAC address generated by the Minim
  servers.
  * Command line: `unum --lan-if wlan0`
  * JSON config file:
  ```json
  {
    "lan-if": "wlan0"
  }
  ```

* Unless built with ["AP mode"](#ap-mode), a WAN network interface.
  * Command line: `unum --wan-if eth0`
  * JSON config file:
  ```json
  {
    "wan-if": "eth0"
  }
  ```

* If using a configuration file, the `--cfg-file` (or `-f`) command line 
  option must be given with the full path to the file.
  ```bash
  unum --cfg-file /etc/opt/unum/config.json
  ```


### Troubleshooting

* The agent starts, but provisioning fails with "forbidden" or "unauthorized"
  * Verify the MAC address being sent by Unum matches the MAC address shown
    in the [Minim Labs portal][3]

* The agent is slowly running "conncheck" and does not come online in the portal
  * Check the network connection and restart the agent.
  
* Running `unum` results in "command not found"
  * Ensure PATH environment variable contains `/opt/unum/bin`, see 
    ["Installing"](#installing).

* Running `unum` as my regular user works, but running `unum` as root with 
  `sudo` results in "not found"
  * The included profile.d script does not run in non-login shells, which 
    `sudo` creates by default. Use `sudo -i` to start a login shell.


## Technical notes

Directory structure overview:

```
.                           Project root
|- README-linux_generic.md  This file
|- Makefile                 Main universal unum Makefile
|- dist/                    Files related to distributable build generation
|- dist/debian              Debian-specific files, used with debhelper to 
|                           create distributable .deb packages.
|- extras/docker/           Dockerfiles and related for building and running 
|                           Unum in a Docker container.
|- extras/linux_generic/    Utilities for managing a Linux router
|- rules/
|   - linux_generic.mk      Makefile for linux_generic
|- files/linux_generic/     Build and installation default files and scripts
|- libs/linux_generic/      Unused at time of writing
|- src/
|   - &lt;module&gt;/             Each module (such as iwinfo or unum)
|                           has a folder under `src/` with at least a
|       - Makefile          module-specific Makefile.
|       - [patches/]        Patches for the module, applied before building,
|       - [...]             as well as any other files used during the build.
|- build/linux_generic      Build output directory.
|   - obj/                  Staging directory containing sources as well as
|                           build-time artifacts.
|       - &lt;module&gt;/         Each module has a directory containing links to
|                           the module source code and necessary build files.
|   - rfs/                  Directory containing the built files used to
|                           generate the final tarball.
|- out/linux_generic/       Final distributable output directory.
```

### Configuration format (Key-Value Format)

The Unum agent transmits device configuration information and supports applying
changes (requested by the user from the Minim dashboard) to the device as well.

For "linux_generic", a basic configuration format is used to manage the device,
as defined below. All fields are required unless otherwise noted.

> Check the "extras" [read_conf.sh][6] and [apply_conf.sh][5] scripts for example 
  implementations that make use of this format.

##### WAN and primary LAN interface related fields

| field name | description
| ---------- | -----------
| ifname_wan | WAN interface name
| ifname_lan | Primary LAN interface name
| ipaddr_lan | LAN IP address (formatted 192.168.1.1)
| subnet_lan | LAN subnet mask (formatted 255.255.255.0)

##### Primary wireless adapter interface fields

| field name | description
| ---------- | -----------
| ifname_wlan | Wireless interface name. Empty if wireless is not configured.
| phyname_wlan | Wireless phy name. Empty if wireless is not configured.
| country_wlan | Configured country for the wireless adapter.
| hwmode_wlan | Hardware mode (either `11a` or `11g`)
| channel_wlan | Channel (valid number for the hwmode or `auto`)
| ssid_wlan | Wireless SSID
| passphrase_wlan | Wireless passphrase
| disabled_wlan | Optional. Non-empty if this interface is disabled.

##### Secondary wireless adapter interface fields

> Note that all `*_wlan1` fields are required when `ifname_wlan1` is specified.

| field name | description
| ---------- | -----------
| ifname_wlan1 | Secondary wireless interface name.
| phyname_wlan1 | Secondary wireless phy name.
| country_wlan1 | Configured country for secondary wireless adapter.
| hwmode_wlan1 | Secondary wireless adapter hardware mode (either `11a` or `11g`)
| channel_wlan1 | Channel (number or `auto`) for the secondary wireless interface.
| ssid_wlan1 | Wireless SSID for the secondary wireless interface.
| passphrase_wlan1 | Wireless passphrase for the secondary wireless interface.
| disabled_wlan1 | Optional. Non-empty if the secondary interface is disabled.

#### Example configuration

Each line should contain a single `key=value` pair. The key may contain letters,
numbers, and underscores. The value may be enclosed within single or double quotes.

> Check linux_generic "extras" linux_generic[read_conf.sh][6] and [apply_conf.sh][5] for a
  concrete implementation that uses this format.

```
ifname_wan=eth0
ifname_lan=wlan0
ipaddr_lan=192.168.11.1
subnet_lan=255.255.255.0
ifname_wlan=wlan0
phyname_wlan=phy0
country_wlan=US
hwmode_wlan=11g
channel_wlan=11
ssid_wlan=MinimSecure
passphrase_wlan="some passphrase"
```

### Configuration format (JSON)


```
{
  "radios": [
    {
      "kind": "wifi_5",                    // always use this kind for 5GHz radio
      "mode": "",
      "width": 0,
      "hwmode": "11ac",                    // 11ac or 11bgn
      "channel": "36",
      "country": "US",
      "enabled": true,          
      "is_mesh": true,
      "radio_name": "",
      "control_channel": 0,
      "secondary_channel": 0,
      "interface_name": "wifi_5",
      "access_points": [                  // supports n access points 
        {
          "key": "wifipassword-5",       
          "ssid": "wifissid-5",
          "is_guest": false,
          "key_management": "wpa-psk"
        }
      ]
    },
    {
      "kind": "wifi_2_4",
      "mode": "",
      "width": 0,
      "hwmode": "11bgn",
      "channel": "auto",
      "country": "US",
      "enabled": true,
      "is_mesh": false,
      "radio_name": "",
      "control_channel": 0,
      "secondary_channel": 0,
      "interface_name": "wifi_2_4",
      "access_points": [
        {
          "key": "wifipassword",
          "ssid": "wifissid",
          "is_guest": false,
          "key_management": "wpa-psk"
        }
      ]
    }
  ],
  "forwarded_ports": [],
  "dhcp_reservations": [],
  "minim_opmode": "gw",                   // gw (gateway) or ap (access point)
  "creds": {
    "username": "admin",                  // username for WebUI or SSH
    "password": "setecastronomy"          // password for WebUI or SSH
  },
  "dns_servers": {                        // user configured dns servers
    "ipv4": [
      "1.1.1.1",
      "8.8.4.4"
    ],
    "ipv6": []
  },
  "extras": {},
  "nat": {
    "hardware": false
  },
  "speed_tier_limits": {
    "upload_kbps_limit": null,
    "download_kbps_limit": null
  },
  "qos_limits": {
    "max_upload_limit_kbps": null,
    "max_download_limit_kbps": null
  },
  "lan_ip": "192.168.1.1",              // lan ip address of this device 
  "lan_mask": "255.255.255.0",          // lan subnet mask of this device
  "mesh": {
    "mesh_ssid": "Minim-XXXXXXXXXXXXXXXXXX",
    "mesh_password": "YYYYYYYYYYYYYYYYYY"
  }
}
```

### Configuration application process

Unum for Linux periodically executes the `read_conf.sh` script which should 
print the current device configuration to standard output. Unum tracks the 
contents of this output, and when it changes it is securely transmitted to 
Minim servers.

When the user triggers some functionality in the Minim dashboard that should 
change the device configuration, the server's copy of the configuration is 
updated and sent back to the running Unum agent. Unum then executes the 
`apply_conf.sh` script, writing the updated configuration to the script's 
standard input. The script is expected to perform any necessary changes to 
the device configuration, which should then be reflected the next time the
`read_conf.sh` script is executed.

By default, Unum will look in the `$UNUM_ISNTALL_ROOT/sbin` directory for both
scripts.

[1]: https://wwww.minim.co/labs
[2]: https://my.minim.co
[3]: https://github.com/MinimSecure/unum-sdk/releases
[4]: https://my.minim.co/labs
[5]: https://github.com/MinimSecure/unum-sdk/blob/master/extras/linux_generic/sbin/apply_conf.sh
[6]: https://github.com/MinimSecure/unum-sdk/blob/master/extras/linux_generic/sbin/read_conf.sh
