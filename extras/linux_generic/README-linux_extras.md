# Generic Linux Extras

Collection of generalized utilities including:

- Automatic and assisted installation scripts for the Unum agent
- System-wide management scripts (unum "all-in-one" and minim-config)
- Shell script that augments login shells' PATH so that regular users
  can use the `unum` command.

> This guide assumes you have installed the Unum agent in `/opt/unum`.

## install.sh

Run the `install.sh` script as root to interactively configure the Unum
installation:

```bash
sudo /opt/unum/extras/install.sh
```

Several options are supported on the command line that affect the initial
choice state for different features during installation only. 

Supported options:

- `--profile` or `--no-profile`: Install (or do not) a script in 
  `/etc/profile.d` that adds the unum command and extras, if enabled, to users'
  PATH.
- `--init` or `--no-init`: Install (or do not) init scripts for the current
  system, automatically detected.
- `--extras` or `--no-extras`: Install (or do not) extra scripts that manage 
  various aspects of a Linux router.
- `--aio` or `--no-aio`: Install (or do not) the unum 'all-in-one' service and
  `minim-config` utility.

Supported alternate modes:

- `--no-interactive`: Do not prompt for user input. Use this mode with the
  uninstall mode or during install with options (listed below) to automate
  without letting the user choose themselves.
  ```bash
  sudo /opt/unum/extras/install.sh --no-interactive --extras --aio --init
  ```
- `--uninstall`: Uninstall the Unum agent and related installed files:
  ```bash
  sudo /opt/unum/extras/install.sh --uninstall
  ```
- `--purge`: Uninstall and remove configuration files, too.By default, 
  configuration files will be left intact. 
  Use `--purge` to remove these as well:
  ```bash
  sudo /opt/unum/extras/install.sh --purge
  ```


## unum_env.sh

This script is useful as a library script (to be sourced from another script)
that needs to interact with Unum in some way. It is used by most of the bash
scripts in the `sbin/` directory.


## minim-config

This command is used to manage an Unum "all-in-one" installation where hostapd,
dnsmasq, iptables, and unum are configured and started in the proper sequence.

This command interactively configures the host system based on user input. The
configuration is persisted across runs so that `minim-config` can be repeatedly
ran without overriding existing configuration.

Once configured, the `minim-config` command starts hostapd, dnsmasq, and 
finally unum, managing the configuration for all three as well as dhcpcd and
iptables.

Run this command with `--no-interactive` to skip prompting for user input, 
instead using the existing configuration stored in the `extras.conf.sh` file,
which is generated inside the Unum configuration directory (`/etc/opt/unum` by
default).


## Other tools

- [`minim-config`](#minim-config) (re)starts everything, first time run will also run 
  `config_interfaces.sh`.
- `config_interfaces.sh` will prompt the user for details to fully configure
  the system's network interfaces. This script generates files for
  `/etc/network/interfaces.d/` as well as an `extras.conf.sh` file used
  by the other scripts.
- `config_routing.sh` configures iptables and enables IP forwarding
- `config_dnsmasq.sh` generates a dnsmasq.conf file and starts dnsmasq
- `start_dnsmasq.sh` starts dnsmasq using the generated config
- `config_hostapd.sh` generates a hostapd.conf file, interactively prompting
  for configuring the wireless adapter, if any
- `start_hostapd.sh` starts hostapd using the generated config
- `start_unum.sh` starts unum as a daemon
- `stop_all.sh` stops all the services.
- [`/opt/unum/extras/sbin/unum_env.sh`](#unum-envsh) is a library script that can be sourced
  in bash sessions.
