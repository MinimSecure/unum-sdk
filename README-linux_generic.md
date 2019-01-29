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
unique identifier for a given Unum. This MAC address must be associated with 
an active developer account on Minim Labs for the agent to start properly.

Sign up for a developer account on the [Minim Labs website][1].


## Building

As far as build dependencies, building Unum requires make, gcc, libc6-pic, some
flavor of libcurl-dev and libssl-dev, libjansson-dev, and a netlink library like libnl-3-200 
or libnl-tiny. Bash scripts included in "extras" additionally depend on gawk.

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
make
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
make UNUM_DEBUG=1
```

#### AP mode

For devices that are not acting as gateways (ie. access point only), the Unum
agent should be built in "access point mode" with `UNUM_AP_MODE=1`:

```bash
make UNUM_AP_MODE=1
```

Note that in AP mode, Unum will not collect connection telemetry. Wireless
device telemetry and system management functionality works as expected.

#### Build Model

Use `MODEL=<model_name>` to alter the build model. Defaults to `linux_generic`.

```bash
make MODEL=linux_generic
```

#### Bundled extras

By default, ["extra" utilities](extras/linux_generic/README-linux_extras.md) are
installed. Disable this option with `INSTALL_EXTRAS=0`:

```bash
make INSTALL_EXTRAS=0
```


## Installing

The Unum agent requires libcurl (3 or later), libjansson4 (2.7 or later), and
libnl-3-200 and libnl-genl-3-200 to run correctly.

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
  > This is used in the unum docker container, check 
    [README-docker.md](extras/docker/README-docker.md#technical-overview)
    for a bit more information on these scripts. 
- Optionally install Unum "all-in-one" and `minim-config` utility. 
- Remove an existing installation:
  ```bash
  sudo /opt/unum/extras/install.sh --uninstall
  ```

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
    in the [Minim Labs portal][1]

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
|- README-linux_generic     This file
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
|   - <module>/             Each module (such as iwinfo or unum)
|                           has a folder under `src/` with at least a
|       - Makefile          module-specific Makefile.
|       - [patches/]        Patches for the module, applied before building,
|       - [...]             as well as any other files used during the build.
|- build/linux_generic      Build output directory.
|   - obj/                  Staging directory containing sources as well as
|                           build-time artifacts.
|       - <module>/         Each module has a directory containing links to
|                           the module source code and necessary build files.
|   - rfs/                  Directory containing the built files used to
|                           generate the final tarball.
|- out/linux_generic/       Final distributable output directory.
```

[1]: https://my.minim.co/labs
[2]: https://my.minim.co
[3]: https://github.com/MinimSecure/unum-sdk/releases
