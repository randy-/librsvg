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
 * Print intrinsic size and #dot geometry; optionally write a PNG.
 *
 * cc -o 04-geometry 04-geometry.c \
 *     $(pkg-config --cflags --libs librsvg-2.0 cairo cairo-png)
 */

#include <stdio.h>
#include <stdlib.h>
#include <librsvg/rsvg.h>
#include <cairo.h>

int
main (void)
{
    GError *error = NULL;
    RsvgHandle *handle;
    gboolean has_w, has_h, has_vb;
    RsvgLength w, h;
    RsvgRectangle vb, ink, logical, viewport;
    double px_w = 0.0, px_h = 0.0;
    cairo_surface_t *surface;
    cairo_t *cr;

    /* Load the SVG from disk into an RsvgHandle. */
    handle = rsvg_handle_new_from_file ("icon.svg", &error);
    if (handle == NULL) {
        g_printerr ("%s\n", error ? error->message : "load failed");
        g_clear_error (&error);
        return 1;
    }

    /* Intrinsic width/height/viewBox, then the same size in pixels. */
    rsvg_handle_get_intrinsic_dimensions (handle, &has_w, &w, &has_h, &h,
                                          &has_vb, &vb);
    rsvg_handle_get_intrinsic_size_in_pixels (handle, &px_w, &px_h);

    printf ("intrinsic px: %.1f x %.1f\n", px_w, px_h);
    if (has_w)
        printf ("width: %g (unit %d)\n", w.length, (int) w.unit);
    if (has_h)
        printf ("height: %g (unit %d)\n", h.length, (int) h.unit);
    if (has_vb)
        printf ("viewBox: %g %g %g %g\n", vb.x, vb.y, vb.width, vb.height);

    /* Viewport used when asking for layer geometry. */
    viewport.x = 0.0;
    viewport.y = 0.0;
    viewport.width = px_w > 0.0 ? px_w : 64.0;
    viewport.height = px_h > 0.0 ? px_h : 64.0;

    /* Ink and logical boxes for #dot at that viewport. */
    if (!rsvg_handle_get_geometry_for_layer (handle, "#dot", &viewport,
                                             &ink, &logical, &error)) {
        g_printerr ("%s\n", error ? error->message : "geometry failed");
        g_clear_error (&error);
        g_object_unref (handle);
        return 1;
    }

    printf ("#dot ink: %g %g %g %g\n",
            ink.x, ink.y, ink.width, ink.height);
    printf ("#dot logical: %g %g %g %g\n",
            logical.x, logical.y, logical.width, logical.height);

    /* Create an ARGB32 image the same size as that viewport. */
    surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                          (int) viewport.width,
                                          (int) viewport.height);
    cr = cairo_create (surface);

    /* Draw the whole document; still exit 0 if this optional render fails. */
    if (rsvg_handle_render_document (handle, cr, &viewport, &error)) {
        /* Write the surface as a PNG. */
        cairo_surface_write_to_png (surface, "04-out.png");
    } else {
        g_printerr ("%s\n", error ? error->message : "render failed");
        g_clear_error (&error);
    }

    /* Drop the Cairo objects and the handle. */
    cairo_destroy (cr);
    cairo_surface_destroy (surface);
    g_object_unref (handle);
    return 0;
}
