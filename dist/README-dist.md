# Distribution-specific builds

Maintainer notes for building and publishing various "finished product"
produced by or with the Unum SDK.

- [Debian, Ubuntu (.deb)][1]
- [Unum for Docker][3]

## .deb for Debian-like

Prerequisites on Debian or Ubuntu:

* unum-sdk build dependencies (see [README.md-linux_generic][4])
* `debhelper`:
  ```bash
  sudo apt install debhelper
  ```

Build an architecture-specific `.deb` for the current system's architecture
with the [make_dotdeb.sh script][6] in this directory.

From the SDK root directory, run the make_dotdeb.sh script:

```bash
sudo dist/make_dotdeb.sh
```

The newly built `.deb` will be in the `out/` directory relative to the Unum 
SDK root directory.


## Unum for Docker

Unum for Docker is [built on CircleCI][101] and published on Docker Hub under the
[minimsecure organization][7]. Both images produced bythe bundled Docker extras
are built and published:
- [minimsecure/unum][8]
- [minimsecure/unum-builder][9]

### Other Docker images

Unum SDK's [CircleCI configuration][101] also creates, publishes, and uses some 
lightly specialized containers:
- [minimsecure/circleci-debian-amd64][10]
- [minimsecure/circleci-debian-armhf][11]


---

Related: 
- [Distribution-specific builds][100] (this document) 
- [CircleCI integration][101] 
- [GitHub integration][102]

[1]: #deb-for-debian-like
[3]: #docker-images
[4]: ../README-linux_generic.md#prerequisites
[6]: make_dotdeb.sh
[7]: https://hub.docker.com/u/minimsecure
[8]: https://hub.docker.com/r/minimsecure/unum
[9]: https://hub.docker.com/r/minimsecure/unum-builder
[10]: https://hub.docker.com/r/minimsecure/circleci-debian-amd64
[11]: https://hub.docker.com/r/minimsecure/circleci-debian-armhf

[100]: ../dist/README-dist.md
[101]: ../.circleci/README-circleci.md
[102]: ../.github/README-github.md
