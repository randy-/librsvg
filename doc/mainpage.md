# librsvg {#mainpage}

**librsvg** is a C library that loads SVG and SVGZ and draws it onto
a Cairo surface.  The shared library, headers, and pkg-config module
are **librsvg-2.0**.  This is the public C API for librsvg **2.62.3**.

It handles non-animated SVG, CSS styling, filters, masks, clip paths,
and SVG `<image>` rasters (PNG, JPEG, GIF, WebP, BMP; optional AVIF).  A
gdk-pixbuf loader and `rsvg-convert` are included.  See the
[examples](@ref examples).

## Limits

SVG `<image>` rasters: maximum side 32767, maximum 32×1024×1024
pixels.  Very large `data:` payloads or stylesheet buffers may be
rejected.

## API groups

- [Types](@ref rsvg_types) — #RsvgHandle, #RsvgRectangle, #RsvgLength, #RsvgUnit
- [Loading](@ref rsvg_loading) — `new_from_file`, `new_from_data`, GIO
- [Rendering](@ref rsvg_rendering) — `render_document` (prefer this over `render_cairo`)
- [Geometry](@ref rsvg_geometry) — `get_geometry_for_*`, `get_intrinsic_*`
- [Stylesheet](@ref rsvg_stylesheet) — `set_stylesheet`
- [Pixbuf](@ref rsvg_pixbuf) — `get_pixbuf_and_error`
- [Cancel](@ref rsvg_cancel) — `set_cancellable_for_rendering`
- [Version](@ref rsvg_version) — `LIBRSVG_CHECK_VERSION`, `LIBRSVG_VERSION`
- [Deprecated](@ref rsvg_deprecated) — older calls kept for compatibility

The last new public C function is `rsvg_handle_set_cancellable_for_rendering`
(2.59).  The library version is 2.62.3.

[Thank you](@ref thanks) — the people who built librsvg.

## Licence {#licence}

The **library** (`librsvg-2.so`) is licensed under the GNU Library
General Public License, version 2 or later.  You may link this library
from other programs; the library itself stays under this license.
The full text is <a href="LICENCE-LIB">LICENCE-LIB</a>.

The command-line tools (`rsvg-convert` and similar) are licensed under
the GNU General Public License, version 2.  The full text is
<a href="LICENCE">LICENCE</a>.

## Building this HTML

Doxygen is required.  From the project root:

    doxygen doc/Doxyfile

or `make html`.  Either command writes `doc/html/index.html`.
