#ifndef RSVG_API_GROUPS_H
#define RSVG_API_GROUPS_H

/**
 * @defgroup rsvg librsvg
 * @brief Public C API for librsvg 2.62.3 (librsvg-2.0).
 *
 * Render SVG documents to Cairo surfaces.  The groups below list the
 * functions and types, not the header files.
 */

/**
 * @defgroup rsvg_types Types
 * @ingroup rsvg
 * @brief Handle, rectangles, lengths, units, errors, and flags.
 */

/**
 * @defgroup rsvg_loading Loading
 * @ingroup rsvg
 * @brief Create an #RsvgHandle from a file, memory, or a GIO stream.
 *
 * Prefer rsvg_handle_new_from_file(), rsvg_handle_new_from_data(), or
 * the GIO constructors.  rsvg_handle_write() / rsvg_handle_close()
 * remain public but are deprecated.
 */

/**
 * @defgroup rsvg_rendering Rendering
 * @ingroup rsvg
 * @brief Draw an SVG onto a Cairo context.
 *
 * Prefer rsvg_handle_render_document().  rsvg_handle_render_cairo()
 * is deprecated.
 */

/**
 * @defgroup rsvg_geometry Geometry
 * @ingroup rsvg
 * @brief Intrinsic size and element geometry.
 *
 * Prefer rsvg_handle_get_intrinsic_dimensions(),
 * rsvg_handle_get_intrinsic_size_in_pixels(), and
 * rsvg_handle_get_geometry_for_layer().
 */

/**
 * @defgroup rsvg_stylesheet Stylesheet
 * @ingroup rsvg
 * @brief Inject a CSS stylesheet into a document.
 */

/**
 * @defgroup rsvg_pixbuf Pixbuf
 * @ingroup rsvg
 * @brief Render to a GdkPixbuf.
 *
 * Prefer rsvg_handle_get_pixbuf_and_error().  rsvg_handle_get_pixbuf()
 * is deprecated.
 */

/**
 * @defgroup rsvg_cancel Cancel
 * @ingroup rsvg
 * @brief Interrupt rendering from another thread.
 */

/**
 * @defgroup rsvg_version Version
 * @ingroup rsvg
 * @brief Compile-time and run-time version macros.
 *
 * The library release is 2.62.3.  The last new public C function
 * is rsvg_handle_set_cancellable_for_rendering() (2.59).
 */

/**
 * @defgroup rsvg_deprecated Deprecated
 * @ingroup rsvg
 * @brief Older entry points kept for compatibility.
 */

#endif /* RSVG_API_GROUPS_H */
