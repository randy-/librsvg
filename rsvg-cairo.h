/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 sts=4 expandtab: */
/*
   rsvg-cairo.h: Cairo rendering for SVG files

   Copyright (C) 2005 Red Hat, Inc.
   Copyright (C) 2026 Randy Butler

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.
*/

#if !defined (__RSVG_RSVG_H_INSIDE__) && !defined (RSVG_COMPILATION)
#warning "Including <librsvg/rsvg-cairo.h> directly is deprecated."
#endif

#ifndef RSVG_CAIRO_H
#define RSVG_CAIRO_H

#include <cairo.h>

G_BEGIN_DECLS

/**
 * @addtogroup rsvg_rendering
 * @{
 */

/**
 * rsvg_handle_render_cairo:
 * @handle: an #RsvgHandle
 * @cr: a Cairo context
 *
 * Draw the whole SVG to @cr.  The size is taken from the document;
 * you cannot pass a viewport.
 *
 * \deprecated Since 2.52: use rsvg_handle_render_document() instead.
 * \since 2.14
 */
RSVG_DEPRECATED_FOR(rsvg_handle_render_document)
gboolean rsvg_handle_render_cairo (RsvgHandle *handle, cairo_t *cr);

/**
 * rsvg_handle_render_cairo_sub:
 * @handle: an #RsvgHandle
 * @cr: a Cairo context
 * @id: (nullable): element id starting with `#`, or %NULL for the whole SVG
 *
 * Draw one element (and its children) to @cr.
 *
 * \deprecated Since 2.52: use rsvg_handle_render_layer() instead.
 * \since 2.14
 */
RSVG_DEPRECATED_FOR(rsvg_handle_render_layer)
gboolean rsvg_handle_render_cairo_sub (RsvgHandle *handle, cairo_t *cr, const char *id);

/**
 * Draw the whole SVG into @viewport on @cr.
 *
 * @handle: an #RsvgHandle
 * @cr: a Cairo context
 * @viewport: where to fit the SVG, in the current Cairo user units
 * @error: return location for a #GError
 *
 * Returns: %TRUE on success, %FALSE on error.
 *
 * \since 2.52
 */
RSVG_API
gboolean rsvg_handle_render_document (RsvgHandle          *handle,
                                      cairo_t             *cr,
                                      const RsvgRectangle *viewport,
                                      GError             **error);

/**
 * rsvg_handle_render_layer:
 * @handle: an #RsvgHandle
 * @cr: a Cairo context
 * @id: (nullable): element id starting with `#`, or %NULL for the whole SVG
 * @viewport: viewport for the whole document
 * @error: return location for a #GError
 *
 * Draw one element as if the whole SVG were fitted to @viewport.
 *
 * Returns: %TRUE on success, %FALSE on error.
 *
 * \since 2.52
 */
RSVG_API
gboolean rsvg_handle_render_layer (RsvgHandle          *handle,
                                   cairo_t             *cr,
                                   const char          *id,
                                   const RsvgRectangle *viewport,
                                   GError             **error);

/**
 * rsvg_handle_render_element:
 * @handle: an #RsvgHandle
 * @cr: a Cairo context
 * @id: (nullable): element id starting with `#`, or %NULL for the whole SVG
 * @element_viewport: size at which to draw that element
 * @error: return location for a #GError
 *
 * Draw one element into @element_viewport (the element’s own size, not
 * the whole document’s).
 *
 * Returns: %TRUE on success, %FALSE on error.
 *
 * \since 2.52
 */
RSVG_API
gboolean rsvg_handle_render_element (RsvgHandle          *handle,
                                     cairo_t             *cr,
                                     const char          *id,
                                     const RsvgRectangle *element_viewport,
                                     GError             **error);

/** @} */

/**
 * @addtogroup rsvg_geometry
 * @{
 */

/**
 * Geometry of an element (or the whole SVG) as if rendered to @viewport.
 *
 * @handle: an #RsvgHandle
 * @id: (nullable): element id starting with `#`, or %NULL for the whole SVG
 * @viewport: viewport the whole SVG would be fitted to
 * @out_ink_rect: (out)(optional): ink rectangle
 * @out_logical_rect: (out)(optional): logical rectangle
 * @error: return location for a #GError
 *
 * Returns: %TRUE on success, %FALSE on error.
 *
 * \since 2.46
 */
RSVG_API
gboolean rsvg_handle_get_geometry_for_layer (RsvgHandle          *handle,
                                             const char          *id,
                                             const RsvgRectangle *viewport,
                                             RsvgRectangle       *out_ink_rect,
                                             RsvgRectangle       *out_logical_rect,
                                             GError             **error);

/**
 * rsvg_handle_get_geometry_for_element:
 * @handle: an #RsvgHandle
 * @id: (nullable): element id starting with `#`, or %NULL for the whole SVG
 * @out_ink_rect: (out)(optional): ink rectangle
 * @out_logical_rect: (out)(optional): logical rectangle
 * @error: return location for a #GError
 *
 * Geometry of an element in SVG user units (no viewport fit).
 *
 * Returns: %TRUE on success, %FALSE on error.
 *
 * \since 2.46
 */
RSVG_API
gboolean rsvg_handle_get_geometry_for_element (RsvgHandle    *handle,
                                               const char    *id,
                                               RsvgRectangle *out_ink_rect,
                                               RsvgRectangle *out_logical_rect,
                                               GError       **error);

/** @} */

G_END_DECLS

#endif
