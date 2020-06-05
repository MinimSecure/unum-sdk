# iwinfo-lite

Fork of the [OpenWrt][1] utility [iwinfo][2] with fewer dependencies.

iwinfo provides a uniform interface for interacting with different wireless
drivers.

Supported driver backends:

* nl80211 (aka cfg80211)
* wl (Broadcom)
* madwifi


## Overview

This project is a slightly modified version of iwinfo that:

* does not require ubus or uci
* does not require lua, by default
* works with both libnl-3 and libnl-tiny

This project produces `libiwinfo.so`, the `iwinfo` command line utility, and
an optional Lua module `iwinfo.so`.


## Building

iwinfo uses make to build and provides some options:

* `BACKENDS` (required) defines which backends to include support for.
  * Valid options (multiple can be specified): `nl80211`, `wl`, and `madwifi`.
  * When specifying `nl80211`, `LDFLAGS` must be also given to actually link to
    the desired netlink library.
* `LUA`, if specified, should be the name of the system installed lua.
  Usually `lua` or `lua5.1`. `CFLAGS` may be required to specify the correct
  include path.
* As described above, `CFLAGS`, `LDFLAGS` can be used to customize build 
  options for `cc` and `ld`, respectively.

### Build for nl-tiny

```bash
make BACKENDS=nl80211 LDFLAGS="-lnl-tiny"
```

### Build for nl-3

```bash
make BACKENDS=nl80211 CFLAGS="-I/usr/include/libnl3" LDFLAGS="-lnl-3 -lnl-genl-3"
```

### Specify multiple backends

```bash
make BACKENDS="madwifi nl80211" LDFLAGS="-lnl-tiny"
```

### Building the Lua module

Build the iwinfo lua module by specifying the `LUA` make option with the 
desired lua library (such as `lua5.1` or simply `lua`).

```bash
make BACKENDS=madwifi LUA="lua5.1"
```

## License

[Full license][3]

```
iwinfo - Wireless Information Library - Command line frontend

Copyright (C) 2011 Jo-Philipp Wich <xm@subsignal.org>

The iwinfo library is free software: you can redistribute it and/or
modify it under the terms of the GNU General Public License version 2
as published by the Free Software Foundation.

The iwinfo library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with the iwinfo library. If not, see http://www.gnu.org/licenses/.
```


[1]: https://openwrt.org
[2]: https://git.openwrt.org/project/iwinfo.git
[3]: COPYING