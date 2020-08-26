Unum v2 agent build system

## Building:

In order to build the agent for a specific hardware which model
(AKA hardware kind) is included into the open source repo run:
./build.sh MODEL=&lt;model&gt;

You can also use:
* make - build all for the default model (read from ~/unum-v2-default-model)
* make unum - compile only unum component for the default model
* make unum.clean - clean unum component build files
* Note: instead of unum it can be any other component available for the
      selected device model (for example "make MODEL=&lt;model&gt; curl")

Additional variables that can be specified on the command line:
* AGENT_VERSION=#.#.#.# - version string (by default all the dev builds have
                        the last version segment set to 0)
* UNUM_DEBUG=1 - build debug version of the agent, by default a release
               version is built, the release captures no log_dbg() output
               and runs process monitor restarting the main process in
               case of unhandled failures.
* ADD_TARGET=&lt;path&gt; - use this if the hardware kind files are not stored in
                    the open source repository (see below).

The hardware kind/model files can be stored within the open source repository
or separately. Typically the approach of storing them separately could be taken
by the 3rd parties integrating Minim agent into their firmware on their own.
The development can be started by adding the harware specific files to the open
source repo, then later the fles can be pulled into an external location using
the "./mk_hwtype.sh -X &lt;path&gt;" command and subsequently referred to during the
build by "ADD_TARGET=&lt;path&gt;" make argument.

You might have to make sure your system meets all the requrements
necessary for building for a specific device model/target. Most likely
the precompiled toolchain will have some requirements. Some components
might depend on the specific make or autotools version. The README files
should list the requirements.

## MacOS Users:
If running from a Mac through Vagrant, make sure this repo
is sync'd to a case sensitive file system! This means that until
High Sierra is released and we all live in the Eden of APFS-land,
you need to make a disk image that is formatted as "MacOS Extended
(Case-sensitive, Journaled)" and sync this repo into that.

Vagrant Workflow
 - vagrant up
 - vagrant ssh
 - ... (continue to building the agent and/or firmware)


## Source code:

Most of the content is located under
./&lt;content_type&gt;/&lt;model&gt;/
folders. For example the libs for Asus AC1300 should be under
./libs/asus_map_ac1300/

Source tree folders:
* kernel - kernel files necessary for building the agent components
* libs   - libraries the agent and its components depend on
* rules  - rules for building for the supported targets/models,
         naming conventions ./rules/&lt;model&gt;.mk
* src    - sources for the agent components (rules can specify what
         components to include to the specific model build),
         naming conventions ./src/&lt;component_name&gt;
* toolchains - toolchains for building the agent and its components
* files  - various non-buildable files (scripts, configuration, etc)
* extras - supplemental info, tools etc for the platforms

Generated folders:
* build - objects and agent (only the agent files) rootfs tree,
        object folders naming: ./build/&lt;model&gt;/obj/&lt;component&gt;
        agent's rootfs tree: ./build/&lt;model&gt;/rfs
* out   - rootfs tarball for extraction into the target platform
        rootfs tree.

## Adding new hardware type:

These instructions are for Minim engineers who have the necessary access
to Minim cloud. The 3rd party integrators can use it as a reference.
The officail support for 3rd party integrations is TBD. If you have
interest please contact Minim for details.

The amount of work for adding a new hardware type varies significantly.
If the platform is supported through OpenWRT/LEDE then follow the
instructions in OpenWRT/LEDE repository here:
https://github.com/violetatrium/lede/blob/master/README.txt
then continue from the step IV in the instructions below.

If the hardware type requires vendor specific integration then start
with the following document describing what we need from the vendor:
https://docs.google.com/document/d/12AgcU3-53aqWTult7zeA1Iob2EMCH2r1G8eU5hQudXY

When all the software, headers, libraries and other information is available
come up with the hardware type name following the existing naming conventions.
Then follow the steps outlined below:


### Vendor SDK repository

1. Create the vendor SDK repository for building the firmware;
2. Add README file describing the hardware: CPU type, clock speed, RAM size
   flash size, partition layout, serial console connector, e.t.c.
   Make sure to include the firmware building instructions. How long it takes
   to build the firmware. Where to download the toolchains (we store some of
   them in AWS S3 storage).
3. Change the SDK to allow adding unum tarball files to the firmware rootfs
   (the unum tarball is built here in the unum-sdk repository)
4. Add the standard build script:
   ./build.sh &lt;HARDWARE_TYPE&gt; [--no-changes | --add &lt;PATH_TO_UNUM_TARBALL&gt;]
   for building the firmware (if only one hardware type is supported then you
   do not need the first parameter). Follow the example of the build scripts
   in https://github.com/violetatrium/asus.
5. Add the firmware upgrade script:
   ./upgrade.sh &lt;IP_ADDR&gt; [PATH_TO_FIRMWARE_FILE]
   script for simple firmware upgrade from command line. Again, follow
   the examples in https://github.com/violetatrium/asus or
   https://github.com/violetatrium/lede repositiory.
6. Make sure you can build the firmware, flash it to the device and upgrade
   or downgrade again. Make sure that files (re)generated during the build are
   ignored by GIT. Make sure the build can be executed multiple times and
   the result does not change unless the underlying code is changed.
   Make sure that the repository can be cloned into a new folder and then
   built from scratch. Make sure you listed in the README all the
   dependencies one needs to install to build the firmware using your
   repository.

Note: when creating GIT repository for vendor SDK always create the first
      commit for the SDK as-it-is (unchanged). Then add your changes in
      additional commits. This assures that you can easily upgrade to
      newer versions of the vendor SDK by merging your changes on top of
      the new version.


### Adding the new hardware type to unum-sdk repo

1. Use script for generating the new hardware type template files:
   ./mk_hwtype.sh [-l] &lt;TARGET_TYPE&gt; &lt;SOURCE_TYPE&gt;
   See more options by running the script with "--help".
   Create the new type basing on one of the existent types that matches
   the most to the new hardware (particularly the wireless drivers).
   The option 'l' allows to create a lightweight hardware type.
   The lightweight hardware kinds are almost exactly the same as their source
   and almost all their folders and files are just symlinks. Sometimes
   (see Asus hw kinds) the base hardware kind isn't even buildable.
   The "asus_generic" is simply a base set of files asus_map_ac1300 and
   asus_map_ac1700 refer to. Note, that using a lightweight type
   as the source will make the target to be lightweight as well
   regardless of the options.
2. Use "git status" to see all the files and folders created by the
   script, examine and make changes necessary for supporting your new
   target platform. Note that the script creates the new type simply by
   copying the files and foldes from the source type. You might have to
   clean up or add something. Typically you'd start with the "rules"
   folder editing the location of the toolchain and the build options.
   After that the libraries and helper tools under "src" can be compiled
   and added to the tarball. At this point it might be useful to work on
   the startup files, dropbear and ./upgrade.sh script mentioned in I.5
   step above. After that it's time to go to "src/unum" and fix all the
   platform specific code in the agent to work for your new hardware type.
3. Test the agent functionality by building it with UNUM_DEBUG=1 and
   going through all the test options (you can see the list by running
   unum on the targer "./unum -m t").
4. Finish up the install section in rules makefile for the agent.
5. If the hardware kind cannot be open-sourced use the
   "./mk_hwtype.sh -X &lt;path_to_your_unum_folder&gt; &lt;hw_kind&gt;" command
   to export your hardware kind to &lt;path_to_your_unum_folder&gt;. After that
   you can delete it from the open source unum files and do the builds
   by adding "ADD_TARGET=&lt;path_to_your_unum_folder&gt;" make argument. Use
   "./mk_hwtype.sh -D &lt;hw_kind&gt;" to delete a hardware kind from the open
   source files tree. For consistency Minim developers should use "unum-v2"
   folder in the hardware vendor SDK repo as &lt;path_to_your_unum_folder&gt;.
   The file duplication has to be avoided, therefore the hardware kinds
   that share the same agent files, but have to be compiled with vendor
   SDKs in multiple repos should be pulled in into those repos "unum-v2"
   folders as GIT submodules.
6. Make the ./extras/&lt;hw_kind&gt; folder and add there info and tools for
   the new hardware type. You can see the examples of what is useful
   in the open sourced hardware kind subfolders. Typically here you'd save
   the examples of the configuratin, radio telemetry, the gdb (host) and
   gdbserver (target) for debugging.
   When capturing the configuration please provide an example of the
   configuration changes for each parameter configurable through the
   Minim cloud (for example cfg_bef_ssid_change.txt,
   cfg_aft_ssid_change.txt, cfg_bef_radio_off.txt, cfg_aft_radio_off.txt,
   cfg_bef_auth_open.txt, cfg_aft_auth_open.txt ...).


### Firmware modifications

1. Use the section "Firmware Changes Required for Integration" in the document
   https://docs.google.com/document/d/12AgcU3-53aqWTult7zeA1Iob2EMCH2r1G8eU5hQudXY
   to walk through the list of the necessary firmware modifications and make
   sure everything is done.


### Adding hardaware type to the cloud, part 1

1. Go to https://my.minim.co/admin/hardware_kinds and do step 1 of adding the
   new hardware type as instructed on that web page.
2. Implement the config file support on the cloud service side for the new
   hardware type.
3. Add new unum for your development device on the following page:
   https://my.minim.co/admin/unums or jsut add its MAC address to the inventory
   on the "Minim Prototypes" ISP page.
4. Enable/activate the new unum, make sure new LAN is created for the device
   and that device can connect and communicate with the cloud service.


### Build system

1. Go to Jenkins build system and set up a build for the new hardware type.
   Keep the promotion (the firmware upload to cloud disabled for now).
2. Make sure the build works as expected and is properly versioned.
3. Enable the firmware promotion step and publish a build to the cloud.


### Adding hardaware type to the cloud, part 2

1. Go to https://my.minim.co/admin/hardware_kinds and do remaining (2 - ...)
   steps for adding the new hardware type support (i.e. add, if necessary, and
   assing to the new hardware kind a command set, e.t.c.).


### Testing

1. Thoroughly test that all the cloud features work.
2. Make sure support portal works and that you can access web UI and shell
   through the portal.
3. Add the new hardware type support to the QA system.
4. Add the new hardware type device to the QA setup.
5. Check crash reports for the new hardware type.
6. Check that the mobile app works with the new hardware kind.
