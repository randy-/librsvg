# Installing C Library librsvg `(Debian / Devuan)`

These packages **replace** GNOME/Rust `librsvg2-*`. They keep the C API
and SONAME `librsvg-2.so.2`. CSS is the in-tree `css/` engine. libcroco
and GObject introspection (`gir1.2-rsvg-2.0`) are not shipped.

| File                           | C Package          | Replaces Rust     |
| ------------------------------ | ------------------ | ----------------- |
| `librsvg-c2_*_amd64.deb`       | `librsvg-c2`       | `librsvg2-2`      |
| `librsvg-c-common_*_amd64.deb` | `librsvg-c-common` | `librsvg2-common` |
| `librsvg-c-bin_*_amd64.deb`    | `librsvg-c-bin`    | `librsvg2-bin`    |
| `librsvg-c-dev_*_amd64.deb`    | `librsvg-c-dev`    | `librsvg2-dev`    |

`make deb` writes them to `../debpkg/`. Copy that directory to the
target machine.

**Runtime set (required):** `c2` + `common` + `bin`  
**Optional:** `-dev` (headers / pkg-config)

`librsvg-c2` Depends on `libgif7`. Apt will pull it if needed.

---

## 1. How to Install

Do **not** `dpkg -i` one package at a time on a desktop that still has GNOME `librsvg2-common`.  

Make sure to run  **apt-get update** and **apt-get upgrade** before runing the install.sh or the command line option.

### Preferred: `install.sh`

Put `install.sh` in the same directory as the `.deb` files.

```sh
cd /path/to/debs
sudo ./install.sh
```

Checksums (`sha256sum`) run by default. GPG is opt-in (see below).

### What you will see

During apt replace of GNOME `librsvg2-*`, dpkg may print “removing
anyway” / unsatisfied Depends on `librsvg2-2` / `librsvg2-common`
for packages such as `desktop-base`, `gnome-icon-theme`,
`xfce4-xkb-plugin`, `libavcodec*`, `gstreamer1.0-plugins-bad`.

Those lines appear while the same transaction is replacing the GNOME
packages. After configure, `librsvg-c2` / `librsvg-c-common` Provide
the old names. `apt-get check` should be clean. This is not a failed
install. Do not run `apt-get -f` because of those warnings.

### GPG (optional)

Integrity: `sha256sum` is the default (`sudo ./install.sh`).
Authenticity: `sudo ./install.sh --verify-gpg` with the published
pubkey (`oss-familybusinesssoftware.com-pubkey.asc`) and `*.asc`
next to the debs. That mode uses a temporary GnuPG home and does not
write the user keyring. Missing `-dev` is still not an error.

### Command line option

```sh
cd /path/to/debs
apt-get install -y \
  ./librsvg-c2_*.deb \
  ./librsvg-c-common_*.deb \
  ./librsvg-c-bin_*.deb \
  ./librsvg-c-dev_*.deb
```

Omit `./librsvg-c-dev_*.deb` if you do not need headers.

Runtime note: `librsvg-c2` Depends on **`libgif7`**. A typical desktop
already has cairo, pango, gdk-pixbuf, libxml2, libavif, libwebp,
libpng, and libjpeg; apt will still pull `libgif7` if it is missing.
