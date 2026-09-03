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
 * Load SVG bytes with rsvg_handle_new_from_data and draw them.
 *
 * cc -o 05-from-memory 05-from-memory.c \
 *     $(pkg-config --cflags --libs librsvg-2.0 cairo cairo-png)
 */

#include <stdlib.h>
#include <string.h>
#include <librsvg/rsvg.h>
#include <cairo.h>

static const char svg[] =
    "<svg xmlns='http://www.w3.org/2000/svg' width='64' height='64'>"
    "<rect width='64' height='64' fill='#27ae60'/>"
    "<circle cx='32' cy='32' r='16' fill='#ecf0f1'/>"
    "</svg>";

int
main (void)
{
    GError *error = NULL;
    RsvgHandle *handle;
    RsvgRectangle viewport = { 0.0, 0.0, 64.0, 64.0 };
    cairo_surface_t *surface;
    cairo_t *cr;

    /* Load the SVG from a memory buffer, not a file. */
    handle = rsvg_handle_new_from_data ((const guint8 *) svg, strlen (svg),
                                        &error);
    if (handle == NULL) {
        g_printerr ("%s\n", error ? error->message : "load failed");
        g_clear_error (&error);
        return 1;
    }

    /* Viewport is where the SVG is fitted, in Cairo user units. */
    /* Create an ARGB32 image the same size as that viewport. */
    surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 64, 64);
    cr = cairo_create (surface);

    /* Draw the whole document into the viewport. */
    if (!rsvg_handle_render_document (handle, cr, &viewport, &error)) {
        g_printerr ("%s\n", error ? error->message : "render failed");
        g_clear_error (&error);
        cairo_destroy (cr);
        cairo_surface_destroy (surface);
        g_object_unref (handle);
        return 1;
    }

    /* Write the surface as a PNG. */
    cairo_surface_write_to_png (surface, "05-out.png");

    /* Drop the Cairo objects and the handle. */
    cairo_destroy (cr);
    cairo_surface_destroy (surface);
    g_object_unref (handle);
    return 0;
}
