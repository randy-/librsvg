/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 sts=4 ts=4 expandtab: */
/*
   rsvg-image.c: Image loading and displaying

   Copyright (C) 2000 Eazel, Inc.
   Copyright (C) 2002, 2003, 2004, 2005 Dom Lachowicz <cinamod@hotmail.com>
   Copyright (C) 2003, 2004, 2005 Caleb Moore <c.moore@student.unsw.edu.au>
   Copyright (C) 2026 Randy Butler

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Raph Levien <raph@artofcode.com>, 
            Dom Lachowicz <cinamod@hotmail.com>, 
            Caleb Moore <c.moore@student.unsw.edu.au>
*/

#include "config.h"

#include "rsvg-image.h"
#include <string.h>
#include <math.h>
#include <errno.h>
#include "rsvg-css.h"
#include "rsvg-io.h"

#ifdef HAVE_LIBWEBP
#include <webp/decode.h>
#endif

#ifdef HAVE_LIBPNG
#include <png.h>
#include <setjmp.h>
#endif

#ifdef HAVE_LIBJPEG
#include <stdio.h>
#ifndef HAVE_LIBPNG
#include <setjmp.h>
#endif
#include <jpeglib.h>
#endif

#ifdef HAVE_LIBGIF
#include <gif_lib.h>
#endif

#ifdef HAVE_LIBAVIF
#include <avif/avif.h>
#endif

/* Cairo ARGB32 premultiplication, same as rsvg_cairo_surface_from_pixbuf. */
#define RSVG_PREMUL(d,c,a,t) G_STMT_START { t = (c) * (a) + 0x7f; (d) = (((t) >> 8) + (t)) >> 8; } G_STMT_END

/* Untrusted <image> rasters. Cairo ARGB32 max side is 32767.
 * Pixel cap avoids 32767² × 4 ≈ 4 GiB from a tiny header. */
#define RSVG_MAX_IMAGE_DIMENSION 32767
#define RSVG_MAX_IMAGE_PIXELS    (32 * 1024 * 1024)

static gboolean
rsvg_image_dims_ok (gint64 width, gint64 height)
{
    if (width <= 0 || height <= 0)
        return FALSE;
    if (width > RSVG_MAX_IMAGE_DIMENSION || height > RSVG_MAX_IMAGE_DIMENSION)
        return FALSE;
    if (width > (gint64) RSVG_MAX_IMAGE_PIXELS / height)
        return FALSE;
    return TRUE;
}

static gboolean
rsvg_image_rgba_nbytes (gint64 width, gint64 height, gsize *out_bytes)
{
    gsize n;

    if (!rsvg_image_dims_ok (width, height))
        return FALSE;
    n = (gsize) width * (gsize) height;
    if (n > G_MAXSIZE / 4)
        return FALSE;
    if (out_bytes)
        *out_bytes = n * 4;
    return TRUE;
}

static gboolean
rsvg_mime_is (const char *mime_type, const char *wanted)
{
    gsize n;

    if (mime_type == NULL || wanted == NULL)
        return FALSE;

    n = strlen (wanted);
    if (g_ascii_strncasecmp (mime_type, wanted, n) != 0)
        return FALSE;

    return mime_type[n] == '\0' || mime_type[n] == ';';
}

static gboolean
rsvg_bytes_look_like_webp (const char *data, gsize data_len)
{
    /* RIFF....WEBP */
    return data != NULL &&
        data_len >= 12 &&
        memcmp (data, "RIFF", 4) == 0 &&
        memcmp (data + 8, "WEBP", 4) == 0;
}

static gboolean
rsvg_href_has_extension (const char *href, const char *ext)
{
    const char *end, *p;
    gsize n;

    if (href == NULL || ext == NULL || g_ascii_strncasecmp (href, "data:", 5) == 0)
        return FALSE;

    end = href + strlen (href);
    for (p = href; *p; p++) {
        if (*p == '?' || *p == '#') {
            end = p;
            break;
        }
    }

    n = strlen (ext);
    if ((gsize) (end - href) < n)
        return FALSE;

    return g_ascii_strncasecmp (end - n, ext, n) == 0;
}

static gboolean
rsvg_href_has_webp_extension (const char *href)
{
    return rsvg_href_has_extension (href, ".webp");
}

static gboolean
rsvg_href_has_png_extension (const char *href)
{
    return rsvg_href_has_extension (href, ".png");
}

static gboolean
rsvg_href_has_jpeg_extension (const char *href)
{
    return rsvg_href_has_extension (href, ".jpg") ||
        rsvg_href_has_extension (href, ".jpeg");
}

static gboolean
rsvg_href_has_gif_extension (const char *href)
{
    return rsvg_href_has_extension (href, ".gif");
}

static gboolean
rsvg_href_has_avif_extension (const char *href)
{
    return rsvg_href_has_extension (href, ".avif");
}

static gboolean
rsvg_href_has_bmp_extension (const char *href)
{
    return rsvg_href_has_extension (href, ".bmp");
}

static gboolean
rsvg_bytes_look_like_png (const char *data, gsize data_len)
{
    static const guint8 sig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };

    return data != NULL && data_len >= 8 && memcmp (data, sig, 8) == 0;
}

static gboolean
rsvg_bytes_look_like_jpeg (const char *data, gsize data_len)
{
    const guint8 *b = (const guint8 *) data;

    return data != NULL && data_len >= 3 &&
        b[0] == 0xff && b[1] == 0xd8 && b[2] == 0xff;
}

static gboolean
rsvg_bytes_look_like_gif (const char *data, gsize data_len)
{
    return data != NULL && data_len >= 6 &&
        (memcmp (data, "GIF87a", 6) == 0 || memcmp (data, "GIF89a", 6) == 0);
}

/* ISO BMFF ftyp box; major or compatible brand avif/avis. */
static gboolean
rsvg_bytes_look_like_avif (const char *data, gsize data_len)
{
    const guint8 *b = (const guint8 *) data;
    guint32 size;
    gsize i, end;

    if (data == NULL || data_len < 16)
        return FALSE;
    if (memcmp (b + 4, "ftyp", 4) != 0)
        return FALSE;
    if (memcmp (b + 8, "avif", 4) == 0 || memcmp (b + 8, "avis", 4) == 0)
        return TRUE;

    size = ((guint32) b[0] << 24) | ((guint32) b[1] << 16) |
        ((guint32) b[2] << 8) | (guint32) b[3];
    if (size < 16 || (gsize) size > data_len)
        size = (guint32) data_len;
    end = size;
    for (i = 16; i + 4 <= end; i += 4) {
        if (memcmp (b + i, "avif", 4) == 0 || memcmp (b + i, "avis", 4) == 0)
            return TRUE;
    }
    return FALSE;
}

/* Windows BMP: 'B' 'M' */
static gboolean
rsvg_bytes_look_like_bmp (const char *data, gsize data_len)
{
    return data != NULL && data_len >= 2 &&
        data[0] == 'B' && data[1] == 'M';
}

/* Known raster/SVG types that must not be sniffed as another format. */
static gboolean
rsvg_mime_is_other_known (const char *mime_type, const char *ours)
{
    static const char *const types[] = {
        "image/webp",
        "image/png",
        "image/jpeg",
        "image/jpg",
        "image/gif",
        "image/avif",
        "image/bmp",
        "image/x-ms-bmp",
        "image/svg+xml",
        NULL
    };
    int i;

    if (mime_type == NULL)
        return FALSE;

    for (i = 0; types[i]; i++) {
        if (rsvg_mime_is (mime_type, types[i]) && !rsvg_mime_is (ours, types[i]))
            return TRUE;
    }

    return FALSE;
}

/* MIME first (like 2.62 document.rs), then magic, then .webp. */
static gboolean
rsvg_data_is_webp (const char *data,
                   gsize data_len,
                   const char *mime_type,
                   const char *href)
{
    if (rsvg_mime_is (mime_type, "image/webp"))
        return TRUE;

    /* Known non-WebP types stay on their own decoder (or gdk-pixbuf). */
    if (rsvg_mime_is_other_known (mime_type, "image/webp"))
        return FALSE;

    if (rsvg_bytes_look_like_webp (data, data_len))
        return TRUE;

    return rsvg_href_has_webp_extension (href);
}

/* MIME first (like 2.62 document.rs), then magic, then .png. */
static gboolean
rsvg_data_is_png (const char *data,
                  gsize data_len,
                  const char *mime_type,
                  const char *href)
{
    if (rsvg_mime_is (mime_type, "image/png"))
        return TRUE;

    if (rsvg_mime_is_other_known (mime_type, "image/png"))
        return FALSE;

    if (rsvg_bytes_look_like_png (data, data_len))
        return TRUE;

    return rsvg_href_has_png_extension (href);
}

/* MIME first, then SOI sniff, then .jpg/.jpeg. */
static gboolean
rsvg_data_is_jpeg (const char *data,
                   gsize data_len,
                   const char *mime_type,
                   const char *href)
{
    if (rsvg_mime_is (mime_type, "image/jpeg") || rsvg_mime_is (mime_type, "image/jpg"))
        return TRUE;

    if (rsvg_mime_is_other_known (mime_type, "image/jpeg") ||
        rsvg_mime_is_other_known (mime_type, "image/jpg"))
        return FALSE;

    if (rsvg_bytes_look_like_jpeg (data, data_len))
        return TRUE;

    return rsvg_href_has_jpeg_extension (href);
}

/* MIME first, then GIF87a/GIF89a sniff, then .gif. */
static gboolean
rsvg_data_is_gif (const char *data,
                  gsize data_len,
                  const char *mime_type,
                  const char *href)
{
    if (rsvg_mime_is (mime_type, "image/gif"))
        return TRUE;

    if (rsvg_mime_is_other_known (mime_type, "image/gif"))
        return FALSE;

    if (rsvg_bytes_look_like_gif (data, data_len))
        return TRUE;

    return rsvg_href_has_gif_extension (href);
}

/* MIME first, then ftyp/avif sniff, then .avif. */
static gboolean
rsvg_data_is_avif (const char *data,
                   gsize data_len,
                   const char *mime_type,
                   const char *href)
{
    if (rsvg_mime_is (mime_type, "image/avif"))
        return TRUE;

    if (rsvg_mime_is_other_known (mime_type, "image/avif"))
        return FALSE;

    if (rsvg_bytes_look_like_avif (data, data_len))
        return TRUE;

    return rsvg_href_has_avif_extension (href);
}

/* MIME first, then BM sniff, then .bmp. */
static gboolean
rsvg_data_is_bmp (const char *data,
                  gsize data_len,
                  const char *mime_type,
                  const char *href)
{
    if (rsvg_mime_is (mime_type, "image/bmp") ||
        rsvg_mime_is (mime_type, "image/x-ms-bmp"))
        return TRUE;

    if (rsvg_mime_is_other_known (mime_type, "image/bmp") ||
        rsvg_mime_is_other_known (mime_type, "image/x-ms-bmp"))
        return FALSE;

    if (rsvg_bytes_look_like_bmp (data, data_len))
        return TRUE;

    return rsvg_href_has_bmp_extension (href);
}

static cairo_surface_t *
rsvg_cairo_surface_from_rgba (const guint8 *rgba,
                              int width,
                              int height,
                              const char *href,
                              const char *kind,
                              GError **error)
{
    cairo_surface_t *surface;
    guint8 *cairo_pixels;
    int cairo_stride;
    int y;

    if (rgba == NULL || !rsvg_image_dims_ok (width, height)) {
        g_set_error (error,
                     GDK_PIXBUF_ERROR,
                     GDK_PIXBUF_ERROR_FAILED,
                     _("Failed to decode %s image '%s'"),
                     kind ? kind : "raster",
                     href ? href : "");
        return NULL;
    }

    surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
    if (cairo_surface_status (surface) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy (surface);
        g_set_error (error,
                     GDK_PIXBUF_ERROR,
                     GDK_PIXBUF_ERROR_FAILED,
                     _("Failed to allocate surface for %s image '%s'"),
                     kind ? kind : "raster",
                     href ? href : "");
        return NULL;
    }

    cairo_pixels = cairo_image_surface_get_data (surface);
    cairo_stride = cairo_image_surface_get_stride (surface);

    for (y = 0; y < height; y++) {
        const guint8 *p = rgba + (gsize) y * (gsize) width * 4;
        guint8 *q = cairo_pixels + (gsize) y * (gsize) cairo_stride;
        const guint8 *end = p + 4 * width;
        guint t1, t2, t3;

        while (p < end) {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
            RSVG_PREMUL (q[0], p[2], p[3], t1);
            RSVG_PREMUL (q[1], p[1], p[3], t2);
            RSVG_PREMUL (q[2], p[0], p[3], t3);
            q[3] = p[3];
#else
            q[0] = p[3];
            RSVG_PREMUL (q[1], p[0], p[3], t1);
            RSVG_PREMUL (q[2], p[1], p[3], t2);
            RSVG_PREMUL (q[3], p[2], p[3], t3);
#endif
            p += 4;
            q += 4;
        }
    }

    cairo_surface_mark_dirty (surface);
    return surface;
}

#ifdef HAVE_LIBWEBP
static cairo_surface_t *
rsvg_cairo_surface_from_webp (const guint8 *data,
                              gsize data_len,
                              const char *href,
                              GError **error)
{
    uint8_t *rgba;
    int width = 0, height = 0;
    cairo_surface_t *surface;

    if (data == NULL || data_len == 0) {
        g_set_error (error,
                     GDK_PIXBUF_ERROR,
                     GDK_PIXBUF_ERROR_FAILED,
                     _("Failed to decode WebP image '%s': no image data"),
                     href ? href : "");
        return NULL;
    }

    if (!WebPGetInfo (data, (size_t) data_len, &width, &height) ||
        !rsvg_image_dims_ok (width, height)) {
        g_set_error (error,
                     GDK_PIXBUF_ERROR,
                     GDK_PIXBUF_ERROR_FAILED,
                     _("Failed to decode WebP image '%s'"),
                     href ? href : "");
        return NULL;
    }

    rgba = WebPDecodeRGBA (data, (size_t) data_len, &width, &height);
    if (rgba == NULL || !rsvg_image_dims_ok (width, height)) {
        if (rgba)
            WebPFree (rgba);
        g_set_error (error,
                     GDK_PIXBUF_ERROR,
                     GDK_PIXBUF_ERROR_FAILED,
                     _("Failed to decode WebP image '%s'"),
                     href ? href : "");
        return NULL;
    }

    surface = rsvg_cairo_surface_from_rgba (rgba, width, height, href, "WebP", error);
    WebPFree (rgba);
    return surface;
}
#endif /* HAVE_LIBWEBP */

#ifdef HAVE_LIBPNG
typedef struct {
    const guint8 *data;
    gsize len;
    gsize pos;
} RsvgPngMem;

static void
rsvg_png_read (png_structp png, png_bytep out, png_size_t n)
{
    RsvgPngMem *m = png_get_io_ptr (png);

    if (m == NULL || m->pos + n > m->len)
        png_error (png, "truncated PNG");
    memcpy (out, m->data + m->pos, n);
    m->pos += n;
}

static cairo_surface_t *
rsvg_cairo_surface_from_png (const guint8 *data,
                             gsize data_len,
                             const char *href,
                             GError **error)
{
    png_structp png = NULL;
    png_infop info = NULL;
    RsvgPngMem mem;
    guint8 *rgba = NULL;
    png_bytep *rows = NULL;
    png_uint_32 width = 0, height = 0;
    int y;
    cairo_surface_t *surface = NULL;

    if (data == NULL || data_len == 0) {
        g_set_error (error,
                     GDK_PIXBUF_ERROR,
                     GDK_PIXBUF_ERROR_FAILED,
                     _("Failed to decode PNG image '%s': no image data"),
                     href ? href : "");
        return NULL;
    }

    mem.data = data;
    mem.len = data_len;
    mem.pos = 0;

    png = png_create_read_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png == NULL)
        goto fail;
    info = png_create_info_struct (png);
    if (info == NULL)
        goto fail;

    if (setjmp (png_jmpbuf (png)))
        goto fail;

    png_set_read_fn (png, &mem, rsvg_png_read);
#ifdef PNG_SET_USER_LIMITS_SUPPORTED
    png_set_user_limits (png, (png_uint_32) RSVG_MAX_IMAGE_DIMENSION,
                         (png_uint_32) RSVG_MAX_IMAGE_DIMENSION);
#endif
    png_read_info (png, info);

    /* Expand to 8-bit RGBA, same as gdk-pixbuf's PNG loader (no extra
     * sRGB/gamma pass — the simplified png_image API was shifting
     * smiley.png and failing filters-image-04/05). */
    png_set_expand (png);
    png_set_strip_16 (png);
    png_set_gray_to_rgb (png);
    png_set_add_alpha (png, 0xff, PNG_FILLER_AFTER);
    png_set_packing (png);
#ifdef PNG_READ_INTERLACING_SUPPORTED
    png_set_interlace_handling (png);
#endif
    png_read_update_info (png, info);

    width = png_get_image_width (png, info);
    height = png_get_image_height (png, info);
    if (!rsvg_image_dims_ok ((gint64) width, (gint64) height))
        goto fail;
    if (png_get_rowbytes (png, info) != (png_size_t) width * 4)
        goto fail;

    {
        gsize nbytes;

        if (!rsvg_image_rgba_nbytes ((gint64) width, (gint64) height, &nbytes))
            goto fail;
        rgba = g_try_malloc (nbytes);
    }
    rows = g_try_malloc (sizeof (png_bytep) * (gsize) height);
    if (rgba == NULL || rows == NULL)
        goto fail;

    for (y = 0; y < (int) height; y++)
        rows[y] = rgba + (gsize) y * (gsize) width * 4;

    png_read_image (png, rows);
    png_read_end (png, NULL);
    png_destroy_read_struct (&png, &info, NULL);
    g_free (rows);
    rows = NULL;

    surface = rsvg_cairo_surface_from_rgba (rgba, (int) width, (int) height,
                                            href, "PNG", error);
    g_free (rgba);
    return surface;

  fail:
    if (png)
        png_destroy_read_struct (&png, info ? &info : NULL, NULL);
    g_free (rows);
    g_free (rgba);
    g_set_error (error,
                 GDK_PIXBUF_ERROR,
                 GDK_PIXBUF_ERROR_FAILED,
                 _("Failed to decode PNG image '%s'"),
                 href ? href : "");
    return NULL;
}
#endif /* HAVE_LIBPNG */

#ifdef HAVE_LIBJPEG
typedef struct {
    struct jpeg_error_mgr pub;
    jmp_buf setjmp_buffer;
} RsvgJpegError;

static void
rsvg_jpeg_error_exit (j_common_ptr cinfo)
{
    RsvgJpegError *err = (RsvgJpegError *) cinfo->err;

    longjmp (err->setjmp_buffer, 1);
}

static void
rsvg_jpeg_output_message (j_common_ptr cinfo)
{
    (void) cinfo;
}

static cairo_surface_t *
rsvg_cairo_surface_from_jpeg (const guint8 *data,
                              gsize data_len,
                              const char *href,
                              GError **error)
{
    struct jpeg_decompress_struct cinfo;
    RsvgJpegError jerr;
    guint8 *rgba = NULL;
    JSAMPARRAY buffer;
    int row_stride;
    int y;
    cairo_surface_t *surface = NULL;
    gboolean created = FALSE;

    if (data == NULL || data_len == 0) {
        g_set_error (error,
                     GDK_PIXBUF_ERROR,
                     GDK_PIXBUF_ERROR_FAILED,
                     _("Failed to decode JPEG image '%s': no image data"),
                     href ? href : "");
        return NULL;
    }

    memset (&cinfo, 0, sizeof (cinfo));
    cinfo.err = jpeg_std_error (&jerr.pub);
    jerr.pub.error_exit = rsvg_jpeg_error_exit;
    jerr.pub.output_message = rsvg_jpeg_output_message;

    if (setjmp (jerr.setjmp_buffer)) {
        if (created)
            jpeg_destroy_decompress (&cinfo);
        g_free (rgba);
        g_set_error (error,
                     GDK_PIXBUF_ERROR,
                     GDK_PIXBUF_ERROR_FAILED,
                     _("Failed to decode JPEG image '%s'"),
                     href ? href : "");
        return NULL;
    }

    jpeg_create_decompress (&cinfo);
    created = TRUE;
    jpeg_mem_src (&cinfo, data, (unsigned long) data_len);

    if (jpeg_read_header (&cinfo, TRUE) != JPEG_HEADER_OK) {
        longjmp (jerr.setjmp_buffer, 1);
    }

    if (!rsvg_image_dims_ok ((gint64) cinfo.image_width,
                             (gint64) cinfo.image_height)) {
        longjmp (jerr.setjmp_buffer, 1);
    }

    cinfo.out_color_space = JCS_RGB;
    jpeg_start_decompress (&cinfo);

    if (cinfo.output_width <= 0 || cinfo.output_height <= 0 ||
        cinfo.output_components != 3) {
        longjmp (jerr.setjmp_buffer, 1);
    }

    {
        int width = (int) cinfo.output_width;
        int height = (int) cinfo.output_height;
        gsize nbytes;

        if (!rsvg_image_rgba_nbytes (width, height, &nbytes))
            longjmp (jerr.setjmp_buffer, 1);

        row_stride = width * 3;
        rgba = g_try_malloc (nbytes);
        if (rgba == NULL)
            longjmp (jerr.setjmp_buffer, 1);

        buffer = (*cinfo.mem->alloc_sarray) ((j_common_ptr) &cinfo, JPOOL_IMAGE,
                                             (JDIMENSION) row_stride, 1);

        y = 0;
        while (cinfo.output_scanline < cinfo.output_height) {
            const guint8 *src;
            guint8 *dst;
            int x;

            jpeg_read_scanlines (&cinfo, buffer, 1);
            src = buffer[0];
            dst = rgba + (gsize) y * (gsize) width * 4;
            for (x = 0; x < width; x++) {
                dst[0] = src[0];
                dst[1] = src[1];
                dst[2] = src[2];
                dst[3] = 255;
                src += 3;
                dst += 4;
            }
            y++;
        }

        jpeg_finish_decompress (&cinfo);
        jpeg_destroy_decompress (&cinfo);
        created = FALSE;

        surface = rsvg_cairo_surface_from_rgba (rgba, width, height, href, "JPEG", error);
        g_free (rgba);
        return surface;
    }
}
#endif /* HAVE_LIBJPEG */

#ifdef HAVE_LIBGIF
typedef struct {
    const guint8 *data;
    gsize len;
    gsize pos;
} RsvgGifMem;

static int
rsvg_gif_read (GifFileType *gif, GifByteType *out, int n)
{
    RsvgGifMem *m;

    if (gif == NULL || out == NULL || n <= 0)
        return 0;
    m = gif->UserData;
    if (m == NULL || m->pos >= m->len)
        return 0;
    if ((gsize) n > m->len - m->pos)
        n = (int) (m->len - m->pos);
    memcpy (out, m->data + m->pos, (gsize) n);
    m->pos += (gsize) n;
    return n;
}

static cairo_surface_t *
rsvg_cairo_surface_from_gif (const guint8 *data,
                             gsize data_len,
                             const char *href,
                             GError **error)
{
    RsvgGifMem mem;
    GifFileType *gif = NULL;
    int err = 0;
    SavedImage *img;
    ColorMapObject *cmap;
    GraphicsControlBlock gcb;
    int trans = NO_TRANSPARENT_COLOR;
    int width, height, left, top, fw, fh;
    guint8 *rgba = NULL;
    cairo_surface_t *surface;
    int x, y;

    if (data == NULL || data_len == 0) {
        g_set_error (error,
                     GDK_PIXBUF_ERROR,
                     GDK_PIXBUF_ERROR_FAILED,
                     _("Failed to decode GIF image '%s': no image data"),
                     href ? href : "");
        return NULL;
    }

    mem.data = data;
    mem.len = data_len;
    mem.pos = 0;

    gif = DGifOpen (&mem, rsvg_gif_read, &err);
    if (gif == NULL)
        goto fail;
    if (gif->UserData == NULL)
        gif->UserData = &mem;

    if (gif->SWidth > 0 && gif->SHeight > 0 &&
        !rsvg_image_dims_ok (gif->SWidth, gif->SHeight))
        goto fail;

    if (DGifSlurp (gif) != GIF_OK || gif->ImageCount < 1)
        goto fail;

    img = &gif->SavedImages[0];
    cmap = img->ImageDesc.ColorMap ? img->ImageDesc.ColorMap : gif->SColorMap;
    if (cmap == NULL || cmap->Colors == NULL || img->RasterBits == NULL)
        goto fail;

    /* First frame only. rust 2.62 still <image> is image 0.25
     * GifDecoder::read_image (ColorOutput::RGBA, logical screen,
     * uncovered pixels [0,0,0,0], LSD background ignored). Local
     * colormap then global; GCB transparent index skipped.
     * DGifSlurp deinterlaces RasterBits. Animation/disposal and
     * painting the LSD background are out of scope. */
    if (DGifSavedExtensionToGCB (gif, 0, &gcb) == GIF_OK)
        trans = gcb.TransparentColor;

    width = gif->SWidth;
    height = gif->SHeight;
    if (width <= 0 || height <= 0) {
        width = img->ImageDesc.Width;
        height = img->ImageDesc.Height;
    }
    if (!rsvg_image_dims_ok (width, height))
        goto fail;

    {
        gsize nbytes;

        if (!rsvg_image_rgba_nbytes (width, height, &nbytes))
            goto fail;
        rgba = g_try_malloc0 (nbytes);
    }
    if (rgba == NULL)
        goto fail;

    left = img->ImageDesc.Left;
    top = img->ImageDesc.Top;
    fw = img->ImageDesc.Width;
    fh = img->ImageDesc.Height;
    if (fw < 0 || fh < 0)
        goto fail;
    if ((fw > 0 && fw > RSVG_MAX_IMAGE_DIMENSION) ||
        (fh > 0 && fh > RSVG_MAX_IMAGE_DIMENSION))
        goto fail;

    for (y = 0; y < fh; y++) {
        int cy = top + y;

        if (cy < 0 || cy >= height)
            continue;
        for (x = 0; x < fw; x++) {
            int cx = left + x;
            int idx;
            GifColorType *c;
            guint8 *dst;

            if (cx < 0 || cx >= width)
                continue;
            idx = img->RasterBits[(gsize) y * (gsize) fw + (gsize) x];
            if (idx == trans)
                continue;
            if (idx < 0 || idx >= cmap->ColorCount)
                continue;
            c = &cmap->Colors[idx];
            dst = rgba + ((gsize) cy * (gsize) width + (gsize) cx) * 4;
            dst[0] = c->Red;
            dst[1] = c->Green;
            dst[2] = c->Blue;
            dst[3] = 255;
        }
    }

    {
        int close_err = 0;

        DGifCloseFile (gif, &close_err);
        gif = NULL;
    }

    surface = rsvg_cairo_surface_from_rgba (rgba, width, height, href, "GIF", error);
    g_free (rgba);
    return surface;

  fail:
    if (gif) {
        int close_err = 0;

        DGifCloseFile (gif, &close_err);
    }
    g_free (rgba);
    g_set_error (error,
                 GDK_PIXBUF_ERROR,
                 GDK_PIXBUF_ERROR_FAILED,
                 _("Failed to decode GIF image '%s'"),
                 href ? href : "");
    return NULL;
}
#endif /* HAVE_LIBGIF */

#ifdef HAVE_BMP
/* Windows DIB in SVG <image>. Uncompressed 1/4/8/24/32-bit and
 * BI_RLE8 / BI_RLE4. Known BMP never falls through to gdk-pixbuf. */
#define RSVG_BMP_BI_RGB       0
#define RSVG_BMP_BI_RLE8      1
#define RSVG_BMP_BI_RLE4      2
#define RSVG_BMP_BI_BITFIELDS 3

static guint16
rsvg_bmp_u16 (const guint8 *p)
{
    return (guint16) p[0] | ((guint16) p[1] << 8);
}

static guint32
rsvg_bmp_u32 (const guint8 *p)
{
    return (guint32) p[0] | ((guint32) p[1] << 8) |
        ((guint32) p[2] << 16) | ((guint32) p[3] << 24);
}

static gint32
rsvg_bmp_i32 (const guint8 *p)
{
    return (gint32) rsvg_bmp_u32 (p);
}

static void
rsvg_bmp_put (guint8 *rgba, int width, int height, int x, int y,
              guint8 r, guint8 g, guint8 b, guint8 a)
{
    guint8 *dst;

    if (x < 0 || y < 0 || x >= width || y >= height)
        return;
    dst = rgba + ((gsize) y * (gsize) width + (gsize) x) * 4;
    dst[0] = r;
    dst[1] = g;
    dst[2] = b;
    dst[3] = a;
}

static gboolean
rsvg_bmp_decode_rle (const guint8 *src, gsize src_len,
                     guint8 *rgba, int width, int height,
                     gboolean top_down, int bits,
                     const guint8 *pal, int ncolors)
{
    int x = 0;
    int y = top_down ? 0 : height - 1;
    int ydir = top_down ? 1 : -1;
    gsize i = 0;

    while (i + 1 < src_len) {
        guint8 b0 = src[i++];
        guint8 b1 = src[i++];

        if (b0 == 0) {
            if (b1 == 0) {
                x = 0;
                y += ydir;
            } else if (b1 == 1) {
                return TRUE;
            } else if (b1 == 2) {
                gint64 nx, ny;

                if (i + 1 >= src_len)
                    return FALSE;
                nx = (gint64) x + src[i++];
                ny = (gint64) y + (gint64) ydir * src[i++];
                if (nx < 0)
                    nx = 0;
                if (nx > width)
                    nx = width;
                if (ny < -1)
                    ny = -1;
                if (ny > height)
                    ny = height;
                x = (int) nx;
                y = (int) ny;
            } else {
                int n = b1;
                int k;

                for (k = 0; k < n; k++) {
                    guint8 idx;

                    if (bits == 8) {
                        if (i >= src_len)
                            return FALSE;
                        idx = src[i++];
                    } else {
                        guint8 packed;

                        if (i >= src_len)
                            return FALSE;
                        packed = src[i];
                        if ((k & 1) == 0)
                            idx = packed >> 4;
                        else {
                            idx = packed & 0x0f;
                            i++;
                        }
                    }
                    if (idx < ncolors) {
                        const guint8 *c = pal + (gsize) idx * 4;
                        rsvg_bmp_put (rgba, width, height, x, y,
                                      c[2], c[1], c[0], 255);
                    }
                    if (x < width)
                        x++;
                }
                if (bits == 8) {
                    if ((n & 1) && i < src_len)
                        i++;
                } else {
                    int bytes = (n + 1) / 2;

                    if ((n & 1) && i < src_len)
                        i++;
                    if ((bytes & 1) && i < src_len)
                        i++;
                }
            }
        } else {
            int n = b0;
            int k;

            for (k = 0; k < n; k++) {
                guint8 idx;

                if (bits == 8)
                    idx = b1;
                else if ((k & 1) == 0)
                    idx = b1 >> 4;
                else
                    idx = b1 & 0x0f;
                if (idx < ncolors) {
                    const guint8 *c = pal + (gsize) idx * 4;
                    rsvg_bmp_put (rgba, width, height, x, y,
                                  c[2], c[1], c[0], 255);
                }
                if (x < width)
                    x++;
            }
        }
        if (y < 0 || y >= height)
            break;
    }
    return TRUE;
}

static cairo_surface_t *
rsvg_cairo_surface_from_bmp (const guint8 *data,
                             gsize data_len,
                             const char *href,
                             GError **error)
{
    guint32 pix_off, dib_size, compression, clrused;
    gint32 width, height_raw;
    int height, bits, planes;
    gboolean top_down = FALSE;
    gsize pal_off, pal_entry, pal_bytes, ncolors_max;
    int ncolors = 0;
    const guint8 *pal = NULL;
    guint8 pal_buf[256 * 4];
    guint8 *rgba = NULL;
    cairo_surface_t *surface;
    int y;

    if (data == NULL || data_len < 14 + 12 ||
        data[0] != 'B' || data[1] != 'M')
        goto fail;

    pix_off = rsvg_bmp_u32 (data + 10);
    dib_size = rsvg_bmp_u32 (data + 14);
    if (dib_size < 12 || dib_size > data_len - 14)
        goto fail;

    compression = RSVG_BMP_BI_RGB;
    clrused = 0;
    if (dib_size == 12) {
        width = (gint32) rsvg_bmp_u16 (data + 18);
        height_raw = (gint32) rsvg_bmp_u16 (data + 20);
        planes = rsvg_bmp_u16 (data + 22);
        bits = rsvg_bmp_u16 (data + 24);
        pal_off = 14 + 12;
        pal_entry = 3;
    } else if (dib_size >= 40) {
        width = rsvg_bmp_i32 (data + 18);
        height_raw = rsvg_bmp_i32 (data + 22);
        planes = rsvg_bmp_u16 (data + 26);
        bits = rsvg_bmp_u16 (data + 28);
        compression = rsvg_bmp_u32 (data + 30);
        clrused = rsvg_bmp_u32 (data + 46);
        pal_off = 14 + dib_size;
        pal_entry = 4;
        if (compression == RSVG_BMP_BI_BITFIELDS && dib_size == 40)
            pal_off += 12;
    } else {
        goto fail;
    }

    if (width <= 0 || planes != 1)
        goto fail;
    if (height_raw < 0) {
        top_down = TRUE;
        height = -height_raw;
    } else {
        height = height_raw;
    }
    if (!rsvg_image_dims_ok (width, height))
        goto fail;

    if (compression == RSVG_BMP_BI_RLE8) {
        if (bits != 8)
            goto fail;
    } else if (compression == RSVG_BMP_BI_RLE4) {
        if (bits != 4)
            goto fail;
    } else if (compression == RSVG_BMP_BI_RGB ||
               compression == RSVG_BMP_BI_BITFIELDS) {
        if (bits != 1 && bits != 4 && bits != 8 && bits != 24 && bits != 32)
            goto fail;
    } else {
        goto fail;
    }

    pal_bytes = 0;
    if (bits <= 8) {
        ncolors_max = (gsize) 1 << bits;
        if (clrused == 0 || clrused > ncolors_max)
            ncolors = (int) ncolors_max;
        else
            ncolors = (int) clrused;
        if (ncolors > 256)
            goto fail;
        pal_bytes = (gsize) ncolors * pal_entry;
        if (pal_off + pal_bytes > data_len)
            goto fail;
        pal = data + pal_off;
        if (pal_entry == 3) {
            int i;

            for (i = 0; i < ncolors; i++) {
                pal_buf[i * 4 + 0] = pal[i * 3 + 0];
                pal_buf[i * 4 + 1] = pal[i * 3 + 1];
                pal_buf[i * 4 + 2] = pal[i * 3 + 2];
                pal_buf[i * 4 + 3] = 0;
            }
            pal = pal_buf;
        }
    }

    if (pix_off < 14 || pix_off >= data_len)
        goto fail;
    if ((gsize) pix_off < pal_off)
        goto fail;
    if (pal_bytes > 0 && (gsize) pix_off < pal_off + pal_bytes)
        goto fail;

    {
        gsize nbytes;

        if (!rsvg_image_rgba_nbytes (width, height, &nbytes))
            goto fail;
        rgba = g_try_malloc0 (nbytes);
    }
    if (rgba == NULL)
        goto fail;

    if (compression == RSVG_BMP_BI_RLE8 || compression == RSVG_BMP_BI_RLE4) {
        int rle_bits = compression == RSVG_BMP_BI_RLE8 ? 8 : 4;

        if (bits != rle_bits || pal == NULL)
            goto fail;
        if (!rsvg_bmp_decode_rle (data + pix_off, data_len - pix_off,
                                  rgba, width, height, top_down, rle_bits,
                                  pal, ncolors))
            goto fail;
    } else if (compression == RSVG_BMP_BI_RGB ||
               compression == RSVG_BMP_BI_BITFIELDS) {
        gsize row_bytes;
        gsize stride;

        if (bits == 24)
            row_bytes = (gsize) width * 3;
        else if (bits == 32)
            row_bytes = (gsize) width * 4;
        else if (bits == 8)
            row_bytes = (gsize) width;
        else if (bits == 4)
            row_bytes = ((gsize) width + 1) / 2;
        else if (bits == 1)
            row_bytes = ((gsize) width + 7) / 8;
        else
            goto fail;
        stride = (row_bytes + 3) & ~(gsize) 3;

        for (y = 0; y < height; y++) {
            int src_y = top_down ? y : (height - 1 - y);
            const guint8 *row;
            int x;

            if (stride > 0 &&
                (gsize) src_y > (data_len - pix_off) / stride)
                goto fail;
            if (pix_off + stride * (gsize) src_y + row_bytes > data_len)
                goto fail;
            row = data + pix_off + stride * (gsize) src_y;

            for (x = 0; x < width; x++) {
                guint8 r = 0, g = 0, b = 0, a = 255;
                guint8 idx = 0;

                if (bits == 24) {
                    b = row[x * 3 + 0];
                    g = row[x * 3 + 1];
                    r = row[x * 3 + 2];
                } else if (bits == 32) {
                    b = row[x * 4 + 0];
                    g = row[x * 4 + 1];
                    r = row[x * 4 + 2];
                    /* BI_RGB 32-bit is BGRX (opaque). V4+ alpha is rare
                     * in still <image>; treat the fourth byte as unused. */
                    a = 255;
                } else if (bits == 8) {
                    idx = row[x];
                } else if (bits == 4) {
                    guint8 packed = row[x / 2];
                    idx = (x & 1) ? (packed & 0x0f) : (packed >> 4);
                } else {
                    guint8 packed = row[x / 8];
                    idx = (packed >> (7 - (x & 7))) & 1;
                }
                if (bits <= 8) {
                    if (pal == NULL || idx >= ncolors)
                        continue;
                    b = pal[(gsize) idx * 4 + 0];
                    g = pal[(gsize) idx * 4 + 1];
                    r = pal[(gsize) idx * 4 + 2];
                }
                rsvg_bmp_put (rgba, width, height, x, y, r, g, b, a);
            }
        }
    } else {
        goto fail;
    }

    surface = rsvg_cairo_surface_from_rgba (rgba, width, height, href, "BMP", error);
    g_free (rgba);
    return surface;

  fail:
    g_free (rgba);
    g_set_error (error,
                 GDK_PIXBUF_ERROR,
                 GDK_PIXBUF_ERROR_FAILED,
                 _("Failed to decode BMP image '%s'"),
                 href ? href : "");
    return NULL;
}
#endif /* HAVE_BMP */

#ifdef HAVE_LIBAVIF
static cairo_surface_t *
rsvg_cairo_surface_from_avif (const guint8 *data,
                              gsize data_len,
                              const char *href,
                              GError **error)
{
    avifDecoder *decoder = NULL;
    avifRGBImage rgb;
    avifResult r;
    guint8 *rgba = NULL;
    cairo_surface_t *surface = NULL;
    int width, height, y;

    if (data == NULL || data_len == 0) {
        g_set_error (error,
                     GDK_PIXBUF_ERROR,
                     GDK_PIXBUF_ERROR_FAILED,
                     _("Failed to decode AVIF image '%s': no image data"),
                     href ? href : "");
        return NULL;
    }

    memset (&rgb, 0, sizeof (rgb));
    decoder = avifDecoderCreate ();
    if (decoder == NULL)
        goto fail;

    r = avifDecoderSetIOMemory (decoder, data, (size_t) data_len);
    if (r != AVIF_RESULT_OK)
        goto fail;
    r = avifDecoderParse (decoder);
    if (r != AVIF_RESULT_OK)
        goto fail;
    r = avifDecoderNextImage (decoder);
    if (r != AVIF_RESULT_OK || decoder->image == NULL)
        goto fail;

    width = (int) decoder->image->width;
    height = (int) decoder->image->height;
    if (!rsvg_image_dims_ok (width, height))
        goto fail;

    avifRGBImageSetDefaults (&rgb, decoder->image);
    rgb.format = AVIF_RGB_FORMAT_RGBA;
    rgb.depth = 8;
    /* 0.11 returns void; 1.x returns avifResult. Check pixels either way. */
    (void) avifRGBImageAllocatePixels (&rgb);
    if (rgb.pixels == NULL)
        goto fail;
    if (rgb.rowBytes < (guint32) width * 4)
        goto fail;
    r = avifImageYUVToRGB (decoder->image, &rgb);
    if (r != AVIF_RESULT_OK)
        goto fail;

    {
        gsize nbytes;

        if (!rsvg_image_rgba_nbytes (width, height, &nbytes))
            goto fail;
        rgba = g_try_malloc (nbytes);
    }
    if (rgba == NULL)
        goto fail;
    for (y = 0; y < height; y++) {
        memcpy (rgba + (gsize) y * (gsize) width * 4,
                rgb.pixels + (gsize) y * (gsize) rgb.rowBytes,
                (gsize) width * 4);
    }

    avifRGBImageFreePixels (&rgb);
    memset (&rgb, 0, sizeof (rgb));
    avifDecoderDestroy (decoder);
    decoder = NULL;

    surface = rsvg_cairo_surface_from_rgba (rgba, width, height, href, "AVIF", error);
    g_free (rgba);
    return surface;

  fail:
    if (rgb.pixels)
        avifRGBImageFreePixels (&rgb);
    if (decoder)
        avifDecoderDestroy (decoder);
    g_free (rgba);
    g_set_error (error,
                 GDK_PIXBUF_ERROR,
                 GDK_PIXBUF_ERROR_FAILED,
                 _("Failed to decode AVIF image '%s'"),
                 href ? href : "");
    return NULL;
}
#endif /* HAVE_LIBAVIF */

cairo_surface_t *
rsvg_cairo_surface_new_from_href (RsvgHandle *handle,
                                  const char *href,
                                  GError **error)
{
    char *data;
    gsize data_len;
    char *mime_type = NULL;
    GdkPixbufLoader *loader = NULL;
    GdkPixbuf *pixbuf = NULL;
    cairo_surface_t *surface = NULL;

    data = _rsvg_handle_acquire_data (handle, href, &mime_type, &data_len, error);
    if (data == NULL)
        return NULL;

    if (data_len > RSVG_MAX_DATA_URI_BYTES) {
        g_set_error (error,
                     GDK_PIXBUF_ERROR,
                     GDK_PIXBUF_ERROR_FAILED,
                     _("Image '%s' is too large"),
                     href ? href : "");
        goto out;
    }

    if (rsvg_data_is_webp (data, data_len, mime_type, href)) {
#ifdef HAVE_LIBWEBP
        surface = rsvg_cairo_surface_from_webp ((const guint8 *) data, data_len, href, error);
        if (surface == NULL)
            goto out;

        if (!rsvg_mime_is (mime_type, "image/webp")) {
            g_free (mime_type);
            mime_type = g_strdup ("image/webp");
        }
        goto attach_mime;
#else
        g_set_error (error,
                     GDK_PIXBUF_ERROR,
                     GDK_PIXBUF_ERROR_UNKNOWN_TYPE,
                     _("WebP support is not enabled; cannot load image '%s'"),
                     href ? href : "");
        goto out;
#endif
    }

    if (rsvg_data_is_png (data, data_len, mime_type, href)) {
#ifdef HAVE_LIBPNG
        surface = rsvg_cairo_surface_from_png ((const guint8 *) data, data_len, href, error);
        if (surface == NULL)
            goto out;

        if (!rsvg_mime_is (mime_type, "image/png")) {
            g_free (mime_type);
            mime_type = g_strdup ("image/png");
        }
        goto attach_mime;
#else
        g_set_error (error,
                     GDK_PIXBUF_ERROR,
                     GDK_PIXBUF_ERROR_UNKNOWN_TYPE,
                     _("PNG support is not enabled; cannot load image '%s'"),
                     href ? href : "");
        goto out;
#endif
    }

    if (rsvg_data_is_jpeg (data, data_len, mime_type, href)) {
#ifdef HAVE_LIBJPEG
        surface = rsvg_cairo_surface_from_jpeg ((const guint8 *) data, data_len, href, error);
        if (surface == NULL)
            goto out;

        if (!rsvg_mime_is (mime_type, "image/jpeg") &&
            !rsvg_mime_is (mime_type, "image/jpg")) {
            g_free (mime_type);
            mime_type = g_strdup ("image/jpeg");
        }
        goto attach_mime;
#else
        g_set_error (error,
                     GDK_PIXBUF_ERROR,
                     GDK_PIXBUF_ERROR_UNKNOWN_TYPE,
                     _("JPEG support is not enabled; cannot load image '%s'"),
                     href ? href : "");
        goto out;
#endif
    }

    if (rsvg_data_is_gif (data, data_len, mime_type, href)) {
#ifdef HAVE_LIBGIF
        surface = rsvg_cairo_surface_from_gif ((const guint8 *) data, data_len, href, error);
        if (surface == NULL)
            goto out;

        if (!rsvg_mime_is (mime_type, "image/gif")) {
            g_free (mime_type);
            mime_type = g_strdup ("image/gif");
        }
        goto attach_mime;
#else
        g_set_error (error,
                     GDK_PIXBUF_ERROR,
                     GDK_PIXBUF_ERROR_UNKNOWN_TYPE,
                     _("GIF support is not enabled; cannot load image '%s'"),
                     href ? href : "");
        goto out;
#endif
    }

    if (rsvg_data_is_avif (data, data_len, mime_type, href)) {
#ifdef HAVE_LIBAVIF
        surface = rsvg_cairo_surface_from_avif ((const guint8 *) data, data_len, href, error);
        if (surface == NULL)
            goto out;

        if (!rsvg_mime_is (mime_type, "image/avif")) {
            g_free (mime_type);
            mime_type = g_strdup ("image/avif");
        }
        goto attach_mime;
#else
        g_set_error (error,
                     GDK_PIXBUF_ERROR,
                     GDK_PIXBUF_ERROR_UNKNOWN_TYPE,
                     _("AVIF support is not enabled; cannot load image '%s'"),
                     href ? href : "");
        goto out;
#endif
    }

    if (rsvg_data_is_bmp (data, data_len, mime_type, href)) {
#ifdef HAVE_BMP
        surface = rsvg_cairo_surface_from_bmp ((const guint8 *) data, data_len, href, error);
        if (surface == NULL)
            goto out;

        if (!rsvg_mime_is (mime_type, "image/bmp") &&
            !rsvg_mime_is (mime_type, "image/x-ms-bmp")) {
            g_free (mime_type);
            mime_type = g_strdup ("image/bmp");
        }
        goto attach_mime;
#else
        g_set_error (error,
                     GDK_PIXBUF_ERROR,
                     GDK_PIXBUF_ERROR_UNKNOWN_TYPE,
                     _("BMP support is not enabled; cannot load image '%s'"),
                     href ? href : "");
        goto out;
#endif
    }

    if (mime_type) {
        loader = gdk_pixbuf_loader_new_with_mime_type (mime_type, error);
    } else {
        loader = gdk_pixbuf_loader_new ();
    }

    if (loader == NULL)
        goto out;

    if (!gdk_pixbuf_loader_write (loader, (guchar *) data, data_len, error)) {
        gdk_pixbuf_loader_close (loader, NULL);
        goto out;
    }

    if (!gdk_pixbuf_loader_close (loader, error))
        goto out;

    pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);

    if (!pixbuf) {
        g_set_error (error,
                     GDK_PIXBUF_ERROR,
                     GDK_PIXBUF_ERROR_FAILED,
                      _("Failed to load image '%s': reason not known, probably a corrupt image file"),
                      href);
        goto out;
    }

    if (!rsvg_image_dims_ok (gdk_pixbuf_get_width (pixbuf),
                             gdk_pixbuf_get_height (pixbuf))) {
        g_set_error (error,
                     GDK_PIXBUF_ERROR,
                     GDK_PIXBUF_ERROR_FAILED,
                     _("Image '%s' is too large"),
                     href ? href : "");
        goto out;
    }

    surface = rsvg_cairo_surface_from_pixbuf (pixbuf);

    if (mime_type == NULL) {
        /* Try to get the information from the loader */
        GdkPixbufFormat *format;
        char **mime_types;

        if ((format = gdk_pixbuf_loader_get_format (loader)) != NULL) {
            mime_types = gdk_pixbuf_format_get_mime_types (format);

            if (mime_types != NULL)
                mime_type = g_strdup (mime_types[0]);
            g_strfreev (mime_types);
        }
    }

  attach_mime:
    if ((handle->priv->flags & RSVG_HANDLE_FLAG_KEEP_IMAGE_DATA) != 0 &&
        mime_type != NULL &&
        surface != NULL &&
        cairo_surface_set_mime_data (surface, mime_type, (guchar *) data,
                                     data_len, g_free, data) == CAIRO_STATUS_SUCCESS) {
        data = NULL; /* transferred to the surface */
    }

  out:
    if (loader)
        g_object_unref (loader);
    g_free (mime_type);
    g_free (data);

    return surface;
}

void
rsvg_preserve_aspect_ratio (unsigned int aspect_ratio, double width,
                            double height, double *w, double *h, double *x, double *y)
{
    double neww, newh;
    if (aspect_ratio & ~RSVG_ASPECT_RATIO_SLICE) {
        neww = *w;
        newh = *h;
        if ((height * *w > width * *h) == ((aspect_ratio & RSVG_ASPECT_RATIO_SLICE) == 0)) {
            neww = width * *h / height;
        } else {
            newh = height * *w / width;
        }

        if (aspect_ratio & RSVG_ASPECT_RATIO_XMIN_YMIN ||
            aspect_ratio & RSVG_ASPECT_RATIO_XMIN_YMID ||
            aspect_ratio & RSVG_ASPECT_RATIO_XMIN_YMAX) {
        } else if (aspect_ratio & RSVG_ASPECT_RATIO_XMID_YMIN ||
                   aspect_ratio & RSVG_ASPECT_RATIO_XMID_YMID ||
                   aspect_ratio & RSVG_ASPECT_RATIO_XMID_YMAX)
            *x -= (neww - *w) / 2;
        else
            *x -= neww - *w;

        if (aspect_ratio & RSVG_ASPECT_RATIO_XMIN_YMIN ||
            aspect_ratio & RSVG_ASPECT_RATIO_XMID_YMIN ||
            aspect_ratio & RSVG_ASPECT_RATIO_XMAX_YMIN) {
        } else if (aspect_ratio & RSVG_ASPECT_RATIO_XMIN_YMID ||
                   aspect_ratio & RSVG_ASPECT_RATIO_XMID_YMID ||
                   aspect_ratio & RSVG_ASPECT_RATIO_XMAX_YMID)
            *y -= (newh - *h) / 2;
        else
            *y -= newh - *h;

        *w = neww;
        *h = newh;
    }
}

static void
rsvg_node_image_free (RsvgNode * self)
{
    RsvgNodeImage *z = (RsvgNodeImage *) self;
    rsvg_state_finalize (z->super.state);
    g_free (z->super.state);
    z->super.state = NULL;
    if (z->surface)
        cairo_surface_destroy (z->surface);
    _rsvg_node_free(self);
}

static void
rsvg_node_image_draw (RsvgNode * self, RsvgDrawingCtx * ctx, int dominate)
{
    RsvgNodeImage *z = (RsvgNodeImage *) self;
    unsigned int aspect_ratio = z->preserve_aspect_ratio;
    gdouble x, y, w, h;
    cairo_surface_t *surface = z->surface;

    if (surface == NULL)
        return;

    x = _rsvg_css_normalize_length (&z->x, ctx, 'h');
    y = _rsvg_css_normalize_length (&z->y, ctx, 'v');
    w = _rsvg_css_normalize_length (&z->w, ctx, 'h');
    h = _rsvg_css_normalize_length (&z->h, ctx, 'v');

    rsvg_state_reinherit_top (ctx, z->super.state, dominate);

    rsvg_push_discrete_layer (ctx);

    if (!rsvg_current_state (ctx)->overflow && (aspect_ratio & RSVG_ASPECT_RATIO_SLICE)) {
        rsvg_add_clipping_rect (ctx, x, y, w, h);
    }

    rsvg_preserve_aspect_ratio (aspect_ratio, 
                                (double) cairo_image_surface_get_width (surface),
                                (double) cairo_image_surface_get_height (surface), 
                                &w, &h, &x, &y);

    rsvg_render_surface (ctx, surface, x, y, w, h);

    rsvg_pop_discrete_layer (ctx);
}

static void
rsvg_node_image_set_atts (RsvgNode * self, RsvgHandle * ctx, RsvgPropertyBag * atts)
{
    const char *klazz = NULL, *id = NULL, *value;
    RsvgNodeImage *image = (RsvgNodeImage *) self;

    if (rsvg_property_bag_size (atts)) {
        if ((value = rsvg_property_bag_lookup (atts, "x")))
            image->x = _rsvg_css_parse_length (value);
        if ((value = rsvg_property_bag_lookup (atts, "y")))
            image->y = _rsvg_css_parse_length (value);
        if ((value = rsvg_property_bag_lookup (atts, "width")))
            image->w = _rsvg_css_parse_length (value);
        if ((value = rsvg_property_bag_lookup (atts, "height")))
            image->h = _rsvg_css_parse_length (value);
        /* path is used by some older adobe illustrator versions */
        if ((value = rsvg_property_bag_lookup (atts, "path"))
            || (value = rsvg_property_bag_lookup_href (atts))) {
            image->surface = rsvg_cairo_surface_new_from_href (ctx,
                                                               value, 
                                                               NULL);

            if (!image->surface) {
#ifdef G_ENABLE_DEBUG
                g_warning ("Couldn't load image: %s\n", value);
#endif
            }
        }
        if ((value = rsvg_property_bag_lookup (atts, "class")))
            klazz = value;
        if ((value = rsvg_property_bag_lookup (atts, "id"))) {
            id = value;
            rsvg_defs_register_name (ctx->priv->defs, id, &image->super);
        }
        if ((value = rsvg_property_bag_lookup (atts, "preserveAspectRatio")))
            image->preserve_aspect_ratio = rsvg_css_parse_aspect_ratio (value);

        rsvg_parse_style_attrs (ctx, image->super.state, "image", klazz, id, atts);
    }
}

RsvgNode *
rsvg_new_image (void)
{
    RsvgNodeImage *image;
    image = g_new (RsvgNodeImage, 1);
    _rsvg_node_init (&image->super, RSVG_NODE_TYPE_IMAGE);
    g_assert (image->super.state);
    image->surface = NULL;
    image->preserve_aspect_ratio = RSVG_ASPECT_RATIO_XMID_YMID;
    image->x = image->y = image->w = image->h = _rsvg_css_parse_length ("0");
    image->super.free = rsvg_node_image_free;
    image->super.draw = rsvg_node_image_draw;
    image->super.set_atts = rsvg_node_image_set_atts;
    return &image->super;
}
