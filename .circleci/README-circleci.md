# CircleCI integration

The `.circleci` folder is used to configure [CircleCI][1] builds.

Check the [config.yml file][3] in this directory for the current configuration.


## Overview

At the time of writing, the CircleCI configuration produces 3 builds for both
"release" builds for tagged commits (version releases) and "snapshot" builds
for all commits.

* Debian (`.deb`) packages
  * armhf, built using `qemu-user-static` for armhf emulation within the amd64 
   CircleCI build environment.
  * amd64
* Docker images for [minimsecure/unum-builder][4] and [minimsecure/unum][5] on
  Docker Hub.


## Notes

CircleCI's `machine` executors run x86_64-based containers. Producing binaries 
for other platforms is done with [`qemu-user-static`][6] for emulation. Check
[the `build-armhf` job in config.yml][3] for an idea on how this works.

Debian-based Docker images that are used during CI to build Unum for a given
architecture are also published.
- [minimsecure/circleci-debian-amd64][7] (see also: [related Dockerfile][9])
- [minimsecure/circleci-debian-armhf][8] (see also: [related Dockerfile][10])

---

* [Unum SDK project on CircleCI][2]
* [CircleCI documentation][1]

---

Related notes: 
- [Distribution-specific builds][100] 
- [CircleCI integration][101] (this document)
- [GitHub integration][102]

[1]: https://circleci.com/docs/
[2]: https://circleci.com/gh/MinimSecure/unum-sdk
[3]: config.yml
[4]: https://hub.docker.com/r/minimsecure/unum-builder
[5]: https://hub.docker.com/r/minimsecure/unum
[6]: https://wiki.debian.org/QemuUserEmulation
[7]: https://hub.docker.com/r/minimsecure/circleci-debian-amd64
[8]: https://hub.docker.com/r/minimsecure/circleci-debian-armhf
[9]: debian-amd64/Dockerfile
[10]: debian-armhf/Dockerfile

[100]: ../dist/README-dist.md
[101]: ../.circleci/README-circleci.md
[102]: ../.github/README-github.md
