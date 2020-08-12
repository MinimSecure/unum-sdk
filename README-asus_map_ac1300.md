This applies to Asus asus_map_ac1300 agent build
(the agent for integration with the Asus asus_map_ac1300 firmware)

## Build environment setup:
https://github.com/violetatrium/asus/blob/asus_map_ac1300/README.md

## Build

 To start with, please refer unum-v2's [Integration document][1].

 Build is a two-step process for this platform.
 1. Build unum using the command:
    ./build.sh MODEL=asus_map_ac1300 . Unum tarball will be generated and the
    tarball path is dumped as the last line in build.sh's output.
 2. Get Asus SDK from, 
    https://github.com/violetatrium/asus/tree/asus_map_ac1300
    (Branch asus_map_ac1300)
    and trigger the build using the following command.
    ./build.sh --add &lt;path_to_unum.tgz&gt; asus_map_ac1300
    Firmware is copied into firmware/firmware_sysupgrade.bin
    if everything goes well with the build.

## Troubleshooting

* Where can I find log files for Unum?
  * Logs are written to `/var/log/unum.log`, `/var/log/http.log`, and 
    `/var/log/monitor.log`.

[1]: README-integrators.md
