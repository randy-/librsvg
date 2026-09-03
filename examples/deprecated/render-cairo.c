/*
 * Copyright (C) 2026 Randy Butler
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, see LICENCE-LIB.
 *
 * Draw icon.svg with rsvg_handle_render_cairo (deprecated since 2.52).
 * rsvg_handle_render_document is the current API; it takes a viewport.
 *
 * cc -o render-cairo render-cairo.c \
 *     $(pkg-config --cflags --libs librsvg-2.0 cairo cairo-png)
 */

#define RSVG_DISABLE_DEPRECATION_WARNINGS
#include <stdlib.h>
#include <librsvg/rsvg.h>
#include <cairo.h>

int
main (void)
{
    GError *error = NULL;
    RsvgHandle *handle;
    cairo_surface_t *surface;
    cairo_t *cr;

    /* Load the SVG from disk into an RsvgHandle. */
    handle = rsvg_handle_new_from_file ("icon.svg", &error);
    if (handle == NULL) {
        g_printerr ("%s\n", error ? error->message : "load failed");
        g_clear_error (&error);
        return 1;
    }

    /* No viewport argument: size comes from the SVG (64×64 here). */
    /* Create an ARGB32 image matching that intrinsic size. */
    surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 64, 64);
    cr = cairo_create (surface);

    /* Draw with the deprecated call (no GError; prefer render_document). */
    if (!rsvg_handle_render_cairo (handle, cr)) {
        g_printerr ("render_cairo failed\n");
        cairo_destroy (cr);
        cairo_surface_destroy (surface);
        g_object_unref (handle);
        return 1;
    }

    /* Write the surface as a PNG. */
    cairo_surface_write_to_png (surface, "deprecated-out.png");

    /* Drop the Cairo objects and the handle. */
    cairo_destroy (cr);
    cairo_surface_destroy (surface);
    g_object_unref (handle);
    return 0;
}
