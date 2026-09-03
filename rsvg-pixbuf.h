/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 sts=4 expandtab: */
/*
   rsvg-pixbuf.h: GdkPixbuf rendering for SVG files

   Copyright (C) 2000 Eazel, Inc.
   Copyright (C) 2026 Randy Butler

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.
*/

#if !defined (__RSVG_RSVG_H_INSIDE__) && !defined (RSVG_COMPILATION)
#warning "Including <librsvg/rsvg-pixbuf.h> directly is deprecated."
#endif

#ifndef RSVG_PIXBUF_H
#define RSVG_PIXBUF_H

#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS

/**
 * @addtogroup rsvg_pixbuf
 * @{
 */

/**
 * rsvg_handle_get_pixbuf:
 * @handle: an #RsvgHandle
 *
 * Render the SVG to a #GdkPixbuf.  On failure this returns %NULL
 * without a #GError.
 *
 * \deprecated Since 2.58: use rsvg_handle_get_pixbuf_and_error() instead.
 * \since 2.0
 */
RSVG_DEPRECATED_FOR(rsvg_handle_get_pixbuf_and_error)
GdkPixbuf *rsvg_handle_get_pixbuf (RsvgHandle *handle);

/**
 * Render the SVG to a #GdkPixbuf.
 *
 * @handle: an #RsvgHandle
 * @error: return location for a #GError
 *
 * Size uses the handle DPI and the document’s intrinsic dimensions.
 *
 * Returns: (transfer full) (nullable): a new pixbuf, or %NULL on error.
 *
 * \since 2.58
 */
RSVG_API
GdkPixbuf *rsvg_handle_get_pixbuf_and_error (RsvgHandle *handle, GError **error);

/**
 * rsvg_handle_get_pixbuf_sub:
 * @handle: an #RsvgHandle
 * @id: (nullable): element id starting with `#`, or %NULL for the whole SVG
 *
 * Render one element into a pixbuf the size of the whole SVG.
 *
 * Returns: (transfer full) (nullable): a new pixbuf, or %NULL on error.
 *
 * \since 2.14
 */
RSVG_API
GdkPixbuf *rsvg_handle_get_pixbuf_sub (RsvgHandle *handle, const char *id);

/**
 * rsvg_pixbuf_from_file:
 * @filename: a file name
 * @error: return location for a #GError
 *
 * Load @filename and return a pixbuf.
 *
 * \deprecated Use rsvg_handle_new_from_file() and rsvg_handle_render_document().
 * \since 2.0
 */
RSVG_DEPRECATED
GdkPixbuf *rsvg_pixbuf_from_file (const gchar *filename, GError **error);

/**
 * rsvg_pixbuf_from_file_at_zoom:
 * \deprecated Use rsvg_handle_new_from_file() and rsvg_handle_render_document().
 * \since 2.0
 */
RSVG_DEPRECATED
GdkPixbuf *rsvg_pixbuf_from_file_at_zoom (const gchar *filename,
                                          double       x_zoom,
                                          double       y_zoom,
                                          GError     **error);

/**
 * rsvg_pixbuf_from_file_at_size:
 * \deprecated Use rsvg_handle_new_from_file() and rsvg_handle_render_document().
 * \since 2.0
 */
RSVG_DEPRECATED
GdkPixbuf *rsvg_pixbuf_from_file_at_size (const gchar *filename,
                                          gint         width,
                                          gint         height,
                                          GError     **error);

/**
 * rsvg_pixbuf_from_file_at_max_size:
 * \deprecated Use rsvg_handle_new_from_file() and rsvg_handle_render_document().
 * \since 2.0
 */
RSVG_DEPRECATED
GdkPixbuf *rsvg_pixbuf_from_file_at_max_size (const gchar *filename,
                                              gint         max_width,
                                              gint         max_height,
                                              GError     **error);

/**
 * rsvg_pixbuf_from_file_at_zoom_with_max:
 * \deprecated Use rsvg_handle_new_from_file() and rsvg_handle_render_document().
 * \since 2.0
 */
RSVG_DEPRECATED
GdkPixbuf *rsvg_pixbuf_from_file_at_zoom_with_max (const gchar *filename,
                                                   double       x_zoom,
                                                   double       y_zoom,
                                                   gint         max_width,
                                                   gint         max_height,
                                                   GError     **error);

/** @} */

G_END_DECLS

#endif
