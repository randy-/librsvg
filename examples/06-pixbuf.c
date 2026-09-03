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
 * Load icon.svg and write a PNG with rsvg_handle_get_pixbuf_and_error.
 *
 * cc -o 06-pixbuf 06-pixbuf.c \
 *     $(pkg-config --cflags --libs librsvg-2.0)
 */

#include <stdlib.h>
#include <librsvg/rsvg.h>

int
main (void)
{
    GError *error = NULL;
    RsvgHandle *handle;
    GdkPixbuf *pixbuf;

    /* Load the SVG from disk into an RsvgHandle. */
    handle = rsvg_handle_new_from_file ("icon.svg", &error);
    if (handle == NULL) {
        g_printerr ("%s\n", error ? error->message : "load failed");
        g_clear_error (&error);
        return 1;
    }

    /* Size comes from handle DPI (default 96) and intrinsic dimensions. */
    pixbuf = rsvg_handle_get_pixbuf_and_error (handle, &error);
    if (pixbuf == NULL) {
        g_printerr ("%s\n", error ? error->message : "pixbuf failed");
        g_clear_error (&error);
        g_object_unref (handle);
        return 1;
    }

    if (!gdk_pixbuf_save (pixbuf, "06-out.png", "png", &error, NULL)) {
        g_printerr ("%s\n", error ? error->message : "save failed");
        g_clear_error (&error);
        g_object_unref (pixbuf);
        g_object_unref (handle);
        return 1;
    }

    g_object_unref (pixbuf);
    g_object_unref (handle);
    return 0;
}
