librsvg
=======

librsvg is a C library that renders Scalable Vector Graphics (SVG) to
Cairo surfaces.  The shared library, headers, and pkg-config module
are **librsvg-2.0**.  The soname is **librsvg-2.so.2**.

Version **2.62.3**.

Originally written by Raph Levien.

This tree is an independent continuation of the C implementation.
It is not an official GNOME release.  Project page:

    https://familybusinesssoftware.com/oss/librsvg

    https://github.com/randy-/librsvg

The public C API matches librsvg-2.0.  Viewport, stylesheet,
cancellable-render, and the other 2.46–2.59 entry points are present.
Programs written against librsvg 2.40 through 2.62 keep the same
headers and link line.

It renders non-animated SVG and SVGZ: CSS styling, filters, masks,
clip paths, and SVG `<image>` rasters (PNG, JPEG, GIF, WebP, BMP;
optional AVIF).  A gdk-pixbuf loader (`libpixbufloader-svg`) and
`rsvg-convert` are included.

rsvg-convert uses GLib GOption.  It accepts `--stylesheet` / `-s`,
stdin (`-` or no filename), `-f pdf` and `pdf1.4` … `pdf1.7`,
`--page-width` / `--page-height` / `--left` / `--top`, `-w` / `-h`
as CSS lengths (`100`, `100px`, `2in`, `50mm`), and
`-l` / `--accept-language` for `systemLanguage`.  Bash completion
installs as
`/usr/share/bash-completion/completions/rsvg-convert`.

Dependencies
------------

Required: GLib, GIO, libxml2, cairo, pango (pangocairo / pangoft2),
gdk-pixbuf.  Stylesheets use the in-tree **css/** engine.  `libcroco/`
is an unbuilt archive (not compiled, not linked, not packaged).
A system libcroco is not used.

Raster decoders (each **auto** at configure time):

| Format | Library                 | Disable             |
| ------ | ----------------------- | ------------------- |
| PNG    | libpng                  | `--disable-png`     |
| JPEG   | libjpeg / libjpeg-turbo | `--disable-jpeg`    |
| GIF    | giflib                  | `--disable-gif`     |
| WebP   | libwebp                 | `--disable-webp`    |
| BMP    | in-tree decoder         | `--disable-bmp`     |
| AVIF   | libavif                 | `--disable-avif`    |

`--enable-*` without the corresponding library is an error for
PNG/JPEG/GIF/WebP/AVIF.  `--enable-bmp` needs no extra library.
`--disable-*` omits that decoder.

Limits
------

SVG `<image>` rasters are limited to a maximum side of 32767 pixels and
32×1024×1024 pixels total.  Very large `data:` payloads or stylesheet
buffers may be rejected.

Defaults
--------

`RsvgHandle` and `rsvg-convert` default to 96 DPI.
Use `rsvg-convert -d 90 -p 90` for the previous CLI default.

Building
--------

See `INSTALL`.  Typical sequence:

    ./configure --prefix=/usr
    make
    make DESTDIR="$PWD/stage" install

Do not run plain `make install` onto a live system prefix.

Licence
-------

The library (`librsvg-2.so`) is GNU Library GPL v2 or later
(`LICENCE-LIB`): you may link it from other programs; the library
itself stays under that license.  The tools (`rsvg-convert` and
similar) are GNU GPL v2 only (`LICENCE`; COPYING does not add
“or later”).  `rsvg-convert --version` prints GPL-2.0-only.
See `COPYING` for this map.

Release
-------

Download and signature:

    https://familybusinesssoftware.com/oss/librsvg

    https://github.com/randy-/librsvg

    make help
    make pkg

writes `librsvg-c-2.62.3.tar.xz` only under `../pkg/`.
The tarball is source only (C tree, tests, autotools; `libcroco/`
is an unbuilt archive).  Sample packaging lives in git `dist/`,
not in the public source archive.
`.git/`, `.gitignore`, and maintainer key files are omitted from
the tarball.  The public key for signatures is on the project page
and in the git tree (`oss-familybusinesssoftware.com-pubkey.asc`).
`make dist-release` is an alias of `pkg`.  `make crux` writes under
`../pkg/`.  `make deb` writes Debian packages under `../debpkg/`.
Neither installs onto a live prefix or runs pkgmk.

    cd ../pkg
    gpg --detach-sign --armor librsvg-c-2.62.3.tar.xz
    sha256sum librsvg-c-2.62.3.tar.xz > SHA256SUMS
    gpg --detach-sign --armor SHA256SUMS

Further reading
---------------

- `INSTALL` — configure flags, DESTDIR/stage, generic Autotools notes
- `deb/INSTALL.md` — Debian/Devuan local-deb install (apt one-shot)
- `tests/README` — optional `make fuzz-css` / `fuzz-afl` / `fuzz-bmp` (not `make check`)
- `NEWS` — user-visible changes
- `COPYING` — which files use which license
- `LICENCE-LIB` — library license (GNU Library GPL v2 or later)
- `LICENCE` — tools license (GNU GPL v2)
- `doc/html/` — API reference (Doxygen; `doxygen doc/Doxyfile` or `make html`)
- `doc/html/examples.html` — curated C samples (`examples/` in the tree)
- `rsvg-convert(1)` — command-line converter
