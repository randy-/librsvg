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
 * Inject CSS so currentColor on icon.svg is set, then render.
 *
 * cc -o 03-stylesheet 03-stylesheet.c \
 *     $(pkg-config --cflags --libs librsvg-2.0 cairo cairo-png)
 */

#include <stdlib.h>
#include <string.h>
#include <librsvg/rsvg.h>
#include <cairo.h>

int
main (void)
{
    const char *css = "* { color: #c0392b; }";
    GError *error = NULL;
    RsvgHandle *handle;
    RsvgRectangle viewport = { 0.0, 0.0, 256.0, 256.0 };
    cairo_surface_t *surface;
    cairo_t *cr;

    /* Load the SVG from disk into an RsvgHandle. */
    handle = rsvg_handle_new_from_file ("icon.svg", &error);
    if (handle == NULL) {
        g_printerr ("%s\n", error ? error->message : "load failed");
        g_clear_error (&error);
        return 1;
    }

    /* Apply a user stylesheet; length is required (not strlen-implied). */
    if (!rsvg_handle_set_stylesheet (handle, (const guint8 *) css,
                                     strlen (css), &error)) {
        g_printerr ("%s\n", error ? error->message : "stylesheet failed");
        g_clear_error (&error);
        g_object_unref (handle);
        return 1;
    }

    /* Viewport is where the SVG is fitted, in Cairo user units. */
    /* Create an ARGB32 image the same size as that viewport. */
    surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 256, 256);
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
    cairo_surface_write_to_png (surface, "03-out.png");

    /* Drop the Cairo objects and the handle. */
    cairo_destroy (cr);
    cairo_surface_destroy (surface);
    g_object_unref (handle);
    return 0;
}
