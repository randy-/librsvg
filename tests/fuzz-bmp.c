/*
   tests/fuzz-bmp.c: Offline BMP / RLE decoder harness.

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
   License along with this program; if not, see LICENCE-LIB.
*/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <cairo.h>

#define RSVG_DISABLE_DEPRECATION_WARNINGS
#include "rsvg.h"

/* Feed a BMP buffer through SVG <image> data: URI. Must not abort. */
static void
try_bmp (const guint8 *data, gsize len, const char *name)
{
    gchar *b64;
    gchar *svg;
    RsvgHandle *handle;
    GError *error = NULL;
    cairo_surface_t *surface;
    cairo_t *cr;
    RsvgRectangle vp = { 0, 0, 8, 8 };

    b64 = g_base64_encode (data, len);
    svg = g_strdup_printf (
        "<svg xmlns='http://www.w3.org/2000/svg' width='8' height='8'>"
        "<rect width='8' height='8' fill='#00ff00'/>"
        "<image href='data:image/bmp;base64,%s' width='8' height='8'/>"
        "</svg>", b64);
    g_free (b64);

    handle = rsvg_handle_new_from_data ((const guint8 *) svg, strlen (svg), &error);
    g_free (svg);
    g_clear_error (&error);
    if (handle == NULL)
        return;

    surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 8, 8);
    cr = cairo_create (surface);
    rsvg_handle_render_document (handle, cr, &vp, &error);
    g_clear_error (&error);
    cairo_destroy (cr);
    cairo_surface_destroy (surface);
    g_object_unref (handle);
    (void) name;
}

static void
put_u16 (guint8 *p, guint16 v)
{
    p[0] = (guint8) v;
    p[1] = (guint8) (v >> 8);
}

static void
put_u32 (guint8 *p, guint32 v)
{
    p[0] = (guint8) v;
    p[1] = (guint8) (v >> 8);
    p[2] = (guint8) (v >> 16);
    p[3] = (guint8) (v >> 24);
}

static guint8 *
bmp_info (gint32 w, gint32 h, guint16 bits, guint32 comp,
          guint32 clrused, guint32 pix_off, const guint8 *extra,
          gsize extra_len, gsize *out_len)
{
    gsize n = 54 + extra_len;
    guint8 *p = g_malloc0 (n);

    p[0] = 'B';
    p[1] = 'M';
    put_u32 (p + 2, (guint32) n);
    put_u32 (p + 10, pix_off);
    put_u32 (p + 14, 40);
    put_u32 (p + 18, (guint32) w);
    put_u32 (p + 22, (guint32) h);
    put_u16 (p + 26, 1);
    put_u16 (p + 28, bits);
    put_u32 (p + 30, comp);
    put_u32 (p + 46, clrused);
    if (extra_len)
        memcpy (p + 54, extra, extra_len);
    *out_len = n;
    return p;
}

static guint8 *
bmp_rgb24_solid (int w, int h, guint8 r, guint8 g, guint8 b, gsize *out_len)
{
    gsize stride = ((gsize) w * 3 + 3) & ~(gsize) 3;
    gsize pix = stride * (gsize) h;
    guint8 *p;
    int y, x;

    p = bmp_info (w, h, 24, 0, 0, 54, NULL, 0, out_len);
    p = g_realloc (p, 54 + pix);
    memset (p + 54, 0, pix);
    put_u32 (p + 2, (guint32) (54 + pix));
    for (y = 0; y < h; y++) {
        guint8 *row = p + 54 + stride * (gsize) y;
        for (x = 0; x < w; x++) {
            row[x * 3 + 0] = b;
            row[x * 3 + 1] = g;
            row[x * 3 + 2] = r;
        }
    }
    *out_len = 54 + pix;
    return p;
}

static guint32
xorshift32 (guint32 *s)
{
    guint32 x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}

int
main (int argc, char **argv)
{
    int n_mut = 256;
    int i, n_tried = 0;
    gsize len;
    guint8 *buf;
    guint32 rng = 1;

#ifndef HAVE_BMP
    g_print ("fuzz-bmp: HAVE_BMP is off; skip\n");
    return 0;
#endif

    for (i = 1; i < argc; i++) {
        if (g_str_has_prefix (argv[i], "--mutate="))
            n_mut = atoi (argv[i] + 9);
        else if (strcmp (argv[i], "--mutate") == 0 && i + 1 < argc)
            n_mut = atoi (argv[++i]);
    }
    if (n_mut < 0)
        n_mut = 0;

    buf = bmp_rgb24_solid (8, 8, 255, 0, 0, &len);
    try_bmp (buf, len, "valid-rgb24-red");
    n_tried++;

    /* Truncated header. */
    try_bmp ((const guint8 *) "BM", 2, "truncated-bm");
    n_tried++;
    try_bmp (buf, 14, "truncated-file-header");
    n_tried++;
    try_bmp (buf, 40, "truncated-dib");
    n_tried++;

    /* Bad bfOffBits. */
    {
        guint8 *bad = bmp_info (8, 8, 24, 0, 0, 2, NULL, 0, &len);
        try_bmp (bad, len, "bad-offbits");
        n_tried++;
        g_free (bad);
    }

    /* Claimed huge dimensions, no pixel data. */
    {
        guint8 *huge = bmp_info (40000, 40000, 24, 0, 0, 54, NULL, 0, &len);
        try_bmp (huge, len, "huge-side");
        n_tried++;
        g_free (huge);
        huge = bmp_info (8192, 8192, 24, 0, 0, 54, NULL, 0, &len);
        try_bmp (huge, len, "huge-pixels");
        n_tried++;
        g_free (huge);
    }

    /* RLE8: 255-pixel encoded run on 8×8. */
    {
        guint8 extra[8 + 6];
        guint8 *rle;
        memset (extra, 0, sizeof (extra));
        extra[4] = 0;
        extra[5] = 0;
        extra[6] = 255; /* BGR red */
        extra[7] = 0;
        extra[8] = 255;
        extra[9] = 1;
        extra[10] = 0;
        extra[11] = 0;
        extra[12] = 0;
        extra[13] = 1;
        rle = bmp_info (8, 8, 8, 1, 2, 62, extra, sizeof (extra), &len);
        try_bmp (rle, len, "rle8-run-past-width");
        n_tried++;
        g_free (rle);
    }

    /* RLE4 encoded run past width. */
    {
        guint8 extra[8 + 4];
        guint8 *rle;
        memset (extra, 0, sizeof (extra));
        extra[6] = 255;
        extra[8] = 255;
        extra[9] = 0x11;
        extra[10] = 0;
        extra[11] = 1;
        rle = bmp_info (8, 8, 4, 2, 2, 62, extra, sizeof (extra), &len);
        try_bmp (rle, len, "rle4-run-past-width");
        n_tried++;
        g_free (rle);
    }

    /* RLE8 absolute mode: 0, count, bytes, pad. */
    {
        guint8 extra[8 + 10];
        guint8 *rle;
        memset (extra, 0, sizeof (extra));
        extra[6] = 255;
        extra[8] = 0;
        extra[9] = 3;
        extra[10] = 1;
        extra[11] = 1;
        extra[12] = 1;
        extra[13] = 0; /* pad */
        extra[14] = 0;
        extra[15] = 1;
        rle = bmp_info (8, 8, 8, 1, 2, 62, extra, sizeof (extra), &len);
        try_bmp (rle, len, "rle8-absolute");
        n_tried++;
        g_free (rle);
    }

    /* Absolute-mode delta + EOL + EOB. */
    {
        guint8 extra[8 + 8];
        guint8 *rle;
        memset (extra, 0, sizeof (extra));
        extra[6] = 255;
        extra[8] = 0;
        extra[9] = 2;
        extra[10] = 4;
        extra[11] = 4;
        extra[12] = 0;
        extra[13] = 0;
        extra[14] = 0;
        extra[15] = 1;
        rle = bmp_info (8, 8, 8, 1, 2, 62, extra, sizeof (extra), &len);
        try_bmp (rle, len, "rle8-delta");
        n_tried++;
        g_free (rle);
    }

    for (i = 0; i < n_mut; i++) {
        guint8 *m = g_malloc (len);
        memcpy (m, buf, len);
        int flips = 1 + (int) (xorshift32 (&rng) % 8);
        int f;
        for (f = 0; f < flips; f++) {
            gsize off = xorshift32 (&rng) % len;
            m[off] ^= (guint8) (1u << (xorshift32 (&rng) % 8));
        }
        try_bmp (m, len, "mutate");
        n_tried++;
        g_free (m);
    }

    g_free (buf);
    g_print ("fuzz-bmp: %d buffers (including %d mutations) survived\n",
             n_tried, n_mut);
    return 0;
}
