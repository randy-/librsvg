/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 sts=4 expandtab: */
/*
   rsvg.h: Public C API for librsvg.

   Copyright (C) 2000 Eazel, Inc.
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
*/

#ifndef RSVG_H
#define RSVG_H

#define __RSVG_RSVG_H_INSIDE__

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#ifndef RSVG_API
# define RSVG_API
#endif

#if defined(RSVG_DISABLE_DEPRECATION_WARNINGS) || !GLIB_CHECK_VERSION (2, 31, 0)
#define RSVG_DEPRECATED RSVG_API
#define RSVG_DEPRECATED_FOR(f) RSVG_API
#else
#define RSVG_DEPRECATED G_DEPRECATED RSVG_API
#define RSVG_DEPRECATED_FOR(f) G_DEPRECATED_FOR(f) RSVG_API
#endif

/**
 * @addtogroup rsvg_types
 * @{
 */

/**
 * RsvgError:
 * @RSVG_ERROR_FAILED: the request failed
 *
 * Domain for librsvg errors.
 *
 * \since 2.14
 */
typedef enum {
    RSVG_ERROR_FAILED
} RsvgError;

#define RSVG_ERROR (rsvg_error_quark ())

/**
 * rsvg_error_quark:
 *
 * Error domain for RSVG.
 *
 * \since 2.14
 */
RSVG_API
GQuark rsvg_error_quark (void) G_GNUC_CONST;

RSVG_API
GType rsvg_error_get_type (void);
#define RSVG_TYPE_ERROR (rsvg_error_get_type())

#define RSVG_TYPE_HANDLE                  (rsvg_handle_get_type ())
#define RSVG_HANDLE(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), RSVG_TYPE_HANDLE, RsvgHandle))
#define RSVG_HANDLE_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), RSVG_TYPE_HANDLE, RsvgHandleClass))
#define RSVG_IS_HANDLE(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RSVG_TYPE_HANDLE))
#define RSVG_IS_HANDLE_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), RSVG_TYPE_HANDLE))
#define RSVG_HANDLE_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), RSVG_TYPE_HANDLE, RsvgHandleClass))

RSVG_API
GType rsvg_handle_get_type (void);

#ifdef RSVG_COMPILATION
typedef struct RsvgHandlePrivate RsvgHandlePrivate;
#endif

typedef struct _RsvgHandle RsvgHandle;
typedef struct _RsvgHandleClass RsvgHandleClass;
typedef struct _RsvgDimensionData RsvgDimensionData;
typedef struct _RsvgPositionData RsvgPositionData;
typedef struct _RsvgRectangle RsvgRectangle;

/**
 * RsvgHandleClass:
 *
 * #GObjectClass for #RsvgHandle.
 *
 * \since 2.0
 */
struct _RsvgHandleClass {
    GObjectClass parent;

    /*< private >*/
    gpointer _abi_padding[15];
};

/**
 * RsvgHandle:
 *
 * An SVG document loaded in memory.  Create one with
 * rsvg_handle_new_from_file() or rsvg_handle_new_from_data(), then
 * draw with rsvg_handle_render_document().
 *
 * \since 2.0
 */
struct _RsvgHandle {
    GObject parent;

    /*< private >*/
#ifdef RSVG_COMPILATION
    RsvgHandlePrivate *priv;
    gpointer _abi_padding[15];
#else
    gpointer _abi_padding[16];
#endif
};

/**
 * RsvgDimensionData:
 *
 * Pixel size of a document.  Use
 * rsvg_handle_get_intrinsic_size_in_pixels() instead.
 *
 * \deprecated Since 2.46: use rsvg_handle_get_intrinsic_size_in_pixels().
 * \since 2.14
 */
struct _RsvgDimensionData {
    int width;
    int height;
    gdouble em;
    gdouble ex;
};

/**
 * RsvgPositionData:
 *
 * Position of a sub-element.  Use
 * rsvg_handle_get_geometry_for_layer() instead.
 *
 * \deprecated Since 2.46: use rsvg_handle_get_geometry_for_layer().
 * \since 2.22
 */
struct _RsvgPositionData {
    int x;
    int y;
};

/**
 * RsvgRectangle:
 *
 * Axis-aligned rectangle in user units (x, y, width, height).
 *
 * \since 2.46
 */
struct _RsvgRectangle {
    double x;
    double y;
    double width;
    double height;
};

/**
 * RsvgUnit:
 *
 * CSS length units for #RsvgLength.
 *
 * \since 2.46
 */
typedef enum {
    RSVG_UNIT_PERCENT,
    RSVG_UNIT_PX,
    RSVG_UNIT_EM,
    RSVG_UNIT_EX,
    RSVG_UNIT_IN,
    RSVG_UNIT_CM,
    RSVG_UNIT_MM,
    RSVG_UNIT_PT,
    RSVG_UNIT_PC,
    RSVG_UNIT_CH
} RsvgUnit;

/**
 * RsvgLength:
 *
 * CSS length (numeric part + unit).
 *
 * \since 2.46
 */
typedef struct {
    double   length;
    RsvgUnit unit;
} RsvgLength;

/** @} */

/**
 * @addtogroup rsvg_deprecated
 * @{
 */

/**
 * rsvg_cleanup:
 *
 * No-op.  GObject finalization is enough.
 *
 * \deprecated Since 2.36: do not call this.
 * \since 2.36
 */
RSVG_DEPRECATED
void rsvg_cleanup (void);

/**
 * rsvg_set_default_dpi:
 * \deprecated Since 2.42: use rsvg_handle_set_dpi() on a handle.
 * \since 2.8
 */
RSVG_DEPRECATED
void rsvg_set_default_dpi (double dpi);

/**
 * rsvg_set_default_dpi_x_y:
 * \deprecated Since 2.42: use rsvg_handle_set_dpi_x_y() on a handle.
 * \since 2.8
 */
RSVG_DEPRECATED
void rsvg_set_default_dpi_x_y (double dpi_x, double dpi_y);

/** @} */

/**
 * @addtogroup rsvg_loading
 * @{
 */

/**
 * rsvg_handle_set_dpi:
 * @handle: an #RsvgHandle
 * @dpi: dots per inch (both axes)
 *
 * Set the DPI used to resolve physical units.
 *
 * \since 2.8
 */
RSVG_API
void rsvg_handle_set_dpi (RsvgHandle *handle, double dpi);

/**
 * rsvg_handle_set_dpi_x_y:
 * @handle: an #RsvgHandle
 *
 * Set the DPI independently on each axis.
 *
 * \since 2.8
 */
RSVG_API
void rsvg_handle_set_dpi_x_y (RsvgHandle *handle, double dpi_x, double dpi_y);

/**
 * rsvg_handle_new:
 *
 * Empty handle.  Load data with rsvg_handle_read_stream_sync() or
 * the deprecated rsvg_handle_write() / rsvg_handle_close().
 *
 * \since 2.0
 */
RSVG_API
RsvgHandle *rsvg_handle_new (void);

/**
 * rsvg_handle_write:
 * \deprecated Use rsvg_handle_read_stream_sync().
 * \since 2.0
 */
RSVG_DEPRECATED_FOR(rsvg_handle_read_stream_sync)
gboolean rsvg_handle_write (RsvgHandle   *handle,
                            const guchar *buf,
                            gsize         count,
                            GError      **error);

/**
 * rsvg_handle_close:
 * \deprecated Use rsvg_handle_read_stream_sync().
 * \since 2.0
 */
RSVG_DEPRECATED_FOR(rsvg_handle_read_stream_sync)
gboolean rsvg_handle_close (RsvgHandle *handle, GError **error);

/**
 * rsvg_handle_get_base_uri:
 * \since 2.9
 */
RSVG_API
const char *rsvg_handle_get_base_uri (RsvgHandle *handle);

/**
 * rsvg_handle_set_base_uri:
 * @handle: an #RsvgHandle
 * @base_uri: base URI for resolving relative references
 *
 * Must be called before loading data.
 *
 * \since 2.8
 */
RSVG_API
void rsvg_handle_set_base_uri (RsvgHandle *handle, const char *base_uri);

/** @} */

/**
 * @addtogroup rsvg_geometry
 * @{
 */

/**
 * rsvg_handle_get_dimensions:
 * \deprecated Since 2.46: use rsvg_handle_get_intrinsic_size_in_pixels().
 * \since 2.14
 */
RSVG_DEPRECATED_FOR(rsvg_handle_get_intrinsic_size_in_pixels)
void rsvg_handle_get_dimensions (RsvgHandle *handle, RsvgDimensionData *dimension_data);

/**
 * rsvg_handle_get_dimensions_sub:
 * \deprecated Since 2.46: use rsvg_handle_get_geometry_for_layer().
 * \since 2.22
 */
RSVG_DEPRECATED_FOR(rsvg_handle_get_geometry_for_layer)
gboolean rsvg_handle_get_dimensions_sub (RsvgHandle        *handle,
                                         RsvgDimensionData *dimension_data,
                                         const char        *id);

/**
 * rsvg_handle_get_position_sub:
 * \deprecated Since 2.46: use rsvg_handle_get_geometry_for_layer().
 * \since 2.22
 */
RSVG_DEPRECATED_FOR(rsvg_handle_get_geometry_for_layer)
gboolean rsvg_handle_get_position_sub (RsvgHandle       *handle,
                                       RsvgPositionData *position_data,
                                       const char       *id);

/**
 * rsvg_handle_has_sub:
 * @handle: an #RsvgHandle
 * @id: element id starting with `#`
 *
 * Returns: %TRUE if @id exists in the document.
 *
 * \since 2.22
 */
RSVG_API
gboolean rsvg_handle_has_sub (RsvgHandle *handle, const char *id);

/** @} */

/**
 * @addtogroup rsvg_types
 * @{
 */

/**
 * RsvgHandleFlags:
 * @RSVG_HANDLE_FLAGS_NONE: no flags
 * @RSVG_HANDLE_FLAG_UNLIMITED: no limits on XML size (trusted input only)
 * @RSVG_HANDLE_FLAG_KEEP_IMAGE_DATA: keep compressed image bytes
 *
 * \since 2.36
 */
typedef enum /*< flags >*/
{
    RSVG_HANDLE_FLAGS_NONE           = 0,
    RSVG_HANDLE_FLAG_UNLIMITED       = 1 << 0,
    RSVG_HANDLE_FLAG_KEEP_IMAGE_DATA = 1 << 1
} RsvgHandleFlags;

RSVG_API
GType rsvg_handle_flags_get_type (void);
#define RSVG_TYPE_HANDLE_FLAGS (rsvg_handle_flags_get_type())

/** @} */

/**
 * @addtogroup rsvg_loading
 * @{
 */

/**
 * rsvg_handle_new_with_flags:
 * @flags: #RsvgHandleFlags
 *
 * \since 2.36
 */
RSVG_API
RsvgHandle *rsvg_handle_new_with_flags (RsvgHandleFlags flags);

/**
 * rsvg_handle_set_base_gfile:
 * @handle: an #RsvgHandle
 * @base_file: a #GFile used as the base for relative URIs
 *
 * \since 2.32
 */
RSVG_API
void rsvg_handle_set_base_gfile (RsvgHandle *handle, GFile *base_file);

/**
 * rsvg_handle_read_stream_sync:
 * @handle: an #RsvgHandle
 * @stream: SVG data
 * @cancellable: (nullable): a #GCancellable
 * @error: return location for a #GError
 *
 * Load SVG data from @stream.  Prefer this over rsvg_handle_write().
 *
 * \since 2.32
 */
RSVG_API
gboolean rsvg_handle_read_stream_sync (RsvgHandle   *handle,
                                       GInputStream *stream,
                                       GCancellable *cancellable,
                                       GError      **error);

/**
 * rsvg_handle_new_from_gfile_sync:
 * @gfile: a #GFile
 * @flags: #RsvgHandleFlags
 * @cancellable: (nullable): a #GCancellable
 * @error: return location for a #GError
 *
 * Load SVG from @gfile.
 *
 * \since 2.32
 */
RSVG_API
RsvgHandle *rsvg_handle_new_from_gfile_sync (GFile          *file,
                                             RsvgHandleFlags flags,
                                             GCancellable   *cancellable,
                                             GError        **error);

/**
 * rsvg_handle_new_from_stream_sync:
 *
 * Load SVG from a #GInputStream.
 *
 * \since 2.32
 */
RSVG_API
RsvgHandle *rsvg_handle_new_from_stream_sync (GInputStream   *input_stream,
                                              GFile          *base_file,
                                              RsvgHandleFlags flags,
                                              GCancellable   *cancellable,
                                              GError        **error);

/**
 * rsvg_handle_new_from_data:
 * @data: SVG bytes
 * @data_len: length of @data
 * @error: return location for a #GError
 *
 * Load SVG from memory.
 *
 * \since 2.14
 */
RSVG_API
RsvgHandle *rsvg_handle_new_from_data (const guint8 *data, gsize data_len, GError **error);

/**
 * rsvg_handle_new_from_file:
 * @filename: a file name
 * @error: return location for a #GError
 *
 * Load SVG from @filename.
 *
 * \since 2.14
 */
RSVG_API
RsvgHandle *rsvg_handle_new_from_file (const gchar *filename, GError **error);

/** @} */

/**
 * @addtogroup rsvg_stylesheet
 * @{
 */

/**
 * Set a CSS stylesheet for the SVG document.
 *
 * @handle: a #RsvgHandle
 * @css: (array length=css_len) (nullable): CSS data; must be valid UTF-8
 * @css_len: length of @css in bytes
 * @error: return location for a #GError
 *
 * @css_len is mandatory; this function will not compute the length of
 * @css.  A stylesheet read from a file can contain nul bytes.
 *
 * During the CSS cascade the stylesheet is applied after the document's
 * own rules (presentation attributes and &lt;style&gt;), so callers can
 * override fills used by symbolic icons.  Either order is accepted:
 * the stylesheet may be set before or after the document is loaded.
 *
 * `@import` rules are ignored except for `data:` URLs.
 *
 * Returns: %TRUE on success, %FALSE on error.
 *
 * \since 2.48
 */
RSVG_API
gboolean rsvg_handle_set_stylesheet (RsvgHandle   *handle,
                                     const guint8 *css,
                                     gsize         css_len,
                                     GError      **error);

/** @} */

/**
 * @addtogroup rsvg_cancel
 * @{
 */

/**
 * Interrupt rendering from another thread.
 *
 * @handle: A #RsvgHandle
 * @cancellable: (nullable): a #GCancellable or %NULL
 *
 * Sets a cancellable that can interrupt rendering while the handle is
 * being rendered in another thread.  Call g_cancellable_cancel() from
 * another thread to stop rendering.
 *
 * If rendering is interrupted, rsvg_handle_render_document() and the
 * other rendering functions return an error with domain #G_IO_ERROR
 * and code #G_IO_ERROR_CANCELLED.
 *
 * \since 2.59
 */
RSVG_API
void rsvg_handle_set_cancellable_for_rendering (RsvgHandle   *handle,
                                                GCancellable *cancellable);

/** @} */

/**
 * @addtogroup rsvg_geometry
 * @{
 */

/**
 * rsvg_handle_get_intrinsic_dimensions:
 * @handle: an #RsvgHandle
 * @out_has_width: (out)(optional): whether the SVG has a width
 * @out_width: (out)(optional): width as an #RsvgLength
 * @out_has_height: (out)(optional): whether the SVG has a height
 * @out_height: (out)(optional): height as an #RsvgLength
 * @out_has_viewbox: (out)(optional): whether the SVG has a viewBox
 * @out_viewbox: (out)(optional): viewBox as an #RsvgRectangle
 *
 * Unconverted width/height/viewBox from the SVG root.
 *
 * \since 2.46
 */
RSVG_API
void rsvg_handle_get_intrinsic_dimensions (RsvgHandle    *handle,
                                           gboolean      *out_has_width,
                                           RsvgLength    *out_width,
                                           gboolean      *out_has_height,
                                           RsvgLength    *out_height,
                                           gboolean      *out_has_viewbox,
                                           RsvgRectangle *out_viewbox);

/**
 * rsvg_handle_get_intrinsic_size_in_pixels:
 * @handle: an #RsvgHandle
 * @out_width: (out)(optional): width in pixels
 * @out_height: (out)(optional): height in pixels
 *
 * Convert intrinsic width and height to pixels using the handle DPI.
 *
 * Returns: %TRUE if both dimensions could be resolved to pixels.
 *
 * \since 2.46
 */
RSVG_API
gboolean rsvg_handle_get_intrinsic_size_in_pixels (RsvgHandle *handle,
                                                   gdouble    *out_width,
                                                   gdouble    *out_height);

/** @} */

#ifndef __GTK_DOC_IGNORE__
RSVG_API
void rsvg_handle_internal_set_testing (RsvgHandle *handle, gboolean testing);
#endif

/**
 * @addtogroup rsvg_deprecated
 * @{
 */

/**
 * rsvg_init:
 * \deprecated Use g_type_init() only on ancient GLib; otherwise do nothing.
 * \since 2.9
 */
RSVG_DEPRECATED_FOR(g_type_init)
void rsvg_init (void);

/**
 * rsvg_term:
 * \deprecated No-op.
 * \since 2.9
 */
RSVG_DEPRECATED
void rsvg_term (void);

/**
 * rsvg_handle_free:
 * \deprecated Use g_object_unref().
 * \since 2.0
 */
RSVG_DEPRECATED_FOR(g_object_unref)
void rsvg_handle_free (RsvgHandle *handle);

/**
 * RsvgSizeFunc:
 *
 * \deprecated Use a Cairo matrix and rsvg_handle_render_document().
 * \since 2.4
 */
typedef void (*RsvgSizeFunc) (gint *width, gint *height, gpointer user_data);

/**
 * rsvg_handle_set_size_callback:
 * \deprecated Use a Cairo matrix and rsvg_handle_render_document().
 * \since 2.4
 */
RSVG_DEPRECATED
void rsvg_handle_set_size_callback (RsvgHandle    *handle,
                                    RsvgSizeFunc   size_func,
                                    gpointer       user_data,
                                    GDestroyNotify user_data_destroy);

/**
 * rsvg_handle_get_title:
 * \deprecated SVG metadata is not exposed this way.
 * \since 2.4
 */
RSVG_DEPRECATED
const char *rsvg_handle_get_title (RsvgHandle *handle);

/**
 * rsvg_handle_get_desc:
 * \deprecated SVG metadata is not exposed this way.
 * \since 2.4
 */
RSVG_DEPRECATED
const char *rsvg_handle_get_desc (RsvgHandle *handle);

/**
 * rsvg_handle_get_metadata:
 * \deprecated SVG metadata is not exposed this way.
 * \since 2.9
 */
RSVG_DEPRECATED
const char *rsvg_handle_get_metadata (RsvgHandle *handle);

/** @} */

G_END_DECLS

#include "rsvg-version.h"
#include "rsvg-features.h"
#include "rsvg-cairo.h"

#if LIBRSVG_HAVE_PIXBUF
#include "rsvg-pixbuf.h"
#endif

#undef __RSVG_RSVG_H_INSIDE__

#endif /* RSVG_H */
