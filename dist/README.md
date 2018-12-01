# Distribution-specific builds

- [.deb (debian, ubuntu)](#debian)

## Debian

Prerequisites:

* unum-sdk build dependencies (see [README.md-linux_generic][1])
* `help2man` and `debhelper`:
  ```bash
  sudo apt install help2man debhelper
  ```

Build an architecture-specific `.deb` (for the current system's architecture)
with the `make_dotdeb.sh` script. From the SDK root directory:

```bash
sudo dist/make_dotdeb.sh
```


[1]: ../README-linux_generic.md#prerequisites