/*
   tests/api.c: C API tests.

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

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <cairo.h>
#include <gio/gio.h>

#define RSVG_DISABLE_DEPRECATION_WARNINGS
#include "rsvg.h"
#include "rsvg-private.h"
#include "librsvg-features.h"
#include "test-utils.h"

static char *
get_test_filename (const char *basename)
{
    return g_build_filename (test_utils_get_test_data_path (),
                             "api",
                             basename,
                             NULL);
}

static RsvgHandle *
load_test_document (const char *basename)
{
    char *filename = get_test_filename (basename);
    GError *error = NULL;
    RsvgHandle *handle = rsvg_handle_new_from_file (filename, &error);

    g_free (filename);
    g_assert_nonnull (handle);
    g_assert_no_error (error);
    return handle;
}

static void
handle_has_correct_type_info (void)
{
    GTypeQuery q;
    RsvgHandle *handle;

    g_type_query (RSVG_TYPE_HANDLE, &q);
    g_assert (q.type == RSVG_TYPE_HANDLE);
    g_assert (q.type == rsvg_handle_get_type ());
    g_assert_cmpstr (q.type_name, ==, "RsvgHandle");

    /* Public layout is GObject + 16 pointer slots. Private data is extra. */
    g_assert_cmpuint (sizeof (RsvgHandle), ==, sizeof (GObject) + 16 * sizeof (gpointer));
    g_assert_cmpuint (sizeof (RsvgHandleClass), ==, sizeof (GObjectClass) + 15 * sizeof (gpointer));
    g_assert_cmpuint (q.instance_size, >=, sizeof (RsvgHandle));
    g_assert_cmpuint (q.class_size, >=, sizeof (RsvgHandleClass));

    handle = rsvg_handle_new ();
    g_assert (G_OBJECT_TYPE (handle) == RSVG_TYPE_HANDLE);
    g_object_unref (handle);
}

static void
public_types_exist (void)
{
    RsvgRectangle rect = { 0.0, 0.0, 1.0, 2.0 };
    RsvgLength len;
    RsvgUnit unit = RSVG_UNIT_PX;
    RsvgDimensionData dim;
    RsvgPositionData pos;

    len.length = 10.0;
    len.unit = RSVG_UNIT_MM;

    g_assert_cmpfloat (rect.width, ==, 1.0);
    g_assert_cmpfloat (rect.height, ==, 2.0);
    g_assert_cmpint (unit, ==, RSVG_UNIT_PX);
    g_assert_cmpint (len.unit, ==, RSVG_UNIT_MM);
    g_assert_cmpint (RSVG_UNIT_CH, ==, 9);

    memset (&dim, 0, sizeof (dim));
    memset (&pos, 0, sizeof (pos));
    g_assert_cmpint (dim.width, ==, 0);
    g_assert_cmpint (pos.x, ==, 0);
}

static void
assert_flags_value_matches (GFlagsValue *v,
                            guint value,
                            const char *value_name,
                            const char *value_nick)
{
    g_assert_cmpint (v->value, ==, value);
    g_assert_cmpstr (v->value_name, ==, value_name);
    g_assert_cmpstr (v->value_nick, ==, value_nick);
}

static void
flags_registration (void)
{
    GType ty = RSVG_TYPE_HANDLE_FLAGS;
    GTypeQuery q;
    GTypeClass *type_class;
    GFlagsClass *flags_class;

    g_assert (ty != G_TYPE_INVALID);
    g_type_query (ty, &q);
    g_assert (G_TYPE_IS_FLAGS (q.type));
    g_assert_cmpstr (q.type_name, ==, "RsvgHandleFlags");

    type_class = g_type_class_ref (ty);
    flags_class = G_FLAGS_CLASS (type_class);
    g_assert_cmpint (flags_class->n_values, ==, 3);

    assert_flags_value_matches (&flags_class->values[0],
                                RSVG_HANDLE_FLAGS_NONE,
                                "RSVG_HANDLE_FLAGS_NONE",
                                "flags-none");
    assert_flags_value_matches (&flags_class->values[1],
                                RSVG_HANDLE_FLAG_UNLIMITED,
                                "RSVG_HANDLE_FLAG_UNLIMITED",
                                "flag-unlimited");
    assert_flags_value_matches (&flags_class->values[2],
                                RSVG_HANDLE_FLAG_KEEP_IMAGE_DATA,
                                "RSVG_HANDLE_FLAG_KEEP_IMAGE_DATA",
                                "flag-keep-image-data");

    g_type_class_unref (type_class);
}

static void
error_registration (void)
{
    GType ty = RSVG_TYPE_ERROR;
    GTypeQuery q;
    GTypeClass *type_class;
    GEnumClass *enum_class;

    g_assert_cmpint (RSVG_ERROR, !=, 0);
    g_type_query (ty, &q);
    g_assert (G_TYPE_IS_ENUM (q.type));
    g_assert_cmpstr (q.type_name, ==, "RsvgError");

    type_class = g_type_class_ref (ty);
    enum_class = G_ENUM_CLASS (type_class);
    g_assert_cmpint (enum_class->n_values, ==, 1);
    g_assert_cmpint (enum_class->values[0].value, ==, RSVG_ERROR_FAILED);
    g_assert_cmpstr (enum_class->values[0].value_name, ==, "RSVG_ERROR_FAILED");
    g_assert_cmpstr (enum_class->values[0].value_nick, ==, "failed");
    g_type_class_unref (type_class);
}

static void
noops (void)
{
    rsvg_init ();
    rsvg_term ();
    rsvg_cleanup ();
}

static void
set_dpi (void)
{
    RsvgHandle *handle;
    RsvgDimensionData dim;

    handle = load_test_document ("dpi.svg");
    rsvg_handle_set_dpi (handle, 100.0);
    rsvg_handle_get_dimensions (handle, &dim);
    g_assert_cmpint (dim.width, ==, 100);
    g_assert_cmpint (dim.height, ==, 400);

    rsvg_handle_set_dpi (handle, 200.0);
    rsvg_handle_get_dimensions (handle, &dim);
    g_assert_cmpint (dim.width, ==, 200);
    g_assert_cmpint (dim.height, ==, 800);
    g_object_unref (handle);

    handle = load_test_document ("dpi.svg");
    rsvg_handle_set_dpi_x_y (handle, 400.0, 300.0);
    rsvg_handle_get_dimensions (handle, &dim);
    g_assert_cmpint (dim.width, ==, 400);
    g_assert_cmpint (dim.height, ==, 1200);
    g_object_unref (handle);
}

static void
default_dpi_is_96 (void)
{
    RsvgHandle *handle;
    gdouble dpi_x = 0.0, dpi_y = 0.0;

    handle = rsvg_handle_new ();
    g_object_get (handle, "dpi-x", &dpi_x, "dpi-y", &dpi_y, NULL);
    g_assert_cmpfloat (dpi_x, ==, 96.0);
    g_assert_cmpfloat (dpi_y, ==, 96.0);
    g_object_unref (handle);
}

static void
handle_has_sub (void)
{
    RsvgHandle *handle = load_test_document ("example.svg");

    g_assert_true (rsvg_handle_has_sub (handle, "#one"));
    g_assert_true (rsvg_handle_has_sub (handle, "#two"));
    g_assert_false (rsvg_handle_has_sub (handle, "#nonexistent"));
    g_object_unref (handle);
}

static void
handle_new_from_data (void)
{
    char *filename = get_test_filename ("example.svg");
    char *data;
    gsize length;
    GError *error = NULL;
    RsvgHandle *handle;

    g_assert_true (g_file_get_contents (filename, &data, &length, &error));
    g_free (filename);
    g_assert_no_error (error);

    handle = rsvg_handle_new_from_data ((guint8 *) data, length, &error);
    g_assert_nonnull (handle);
    g_assert_no_error (error);
    g_object_unref (handle);
    g_free (data);
}

static void
render_cairo_sub (void)
{
    RsvgHandle *handle = load_test_document ("example.svg");
    cairo_surface_t *surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 100, 400);
    cairo_t *cr = cairo_create (surface);

    g_assert_true (rsvg_handle_render_cairo (handle, cr));
    g_assert_true (rsvg_handle_render_cairo_sub (handle, cr, "#one"));

    cairo_destroy (cr);
    cairo_surface_destroy (surface);
    g_object_unref (handle);
}

static void
handle_get_pixbuf (void)
{
    RsvgHandle *handle = load_test_document ("example.svg");
    GdkPixbuf *pixbuf = rsvg_handle_get_pixbuf (handle);

    g_assert_nonnull (pixbuf);
    g_assert_cmpint (gdk_pixbuf_get_width (pixbuf), ==, 100);
    g_assert_cmpint (gdk_pixbuf_get_height (pixbuf), ==, 400);
    g_object_unref (pixbuf);
    g_object_unref (handle);
}

static void
library_version_defines (void)
{
    gchar *version = g_strdup_printf ("%u.%u.%u",
                                      LIBRSVG_MAJOR_VERSION,
                                      LIBRSVG_MINOR_VERSION,
                                      LIBRSVG_MICRO_VERSION);
    g_assert_cmpstr (version, ==, LIBRSVG_VERSION);
    g_assert_cmpstr (LIBRSVG_VERSION, ==, "2.62.3");
    g_free (version);
}

static void
library_version_check (void)
{
    g_assert_true (LIBRSVG_CHECK_VERSION (2, 0, 0));
    g_assert_true (LIBRSVG_CHECK_VERSION (2, 46, 0));
    g_assert_true (LIBRSVG_CHECK_VERSION (2, 52, 0));
    g_assert_true (LIBRSVG_CHECK_VERSION (2, 59, 0));
    g_assert_true (LIBRSVG_CHECK_VERSION (2, 62, 0));
    g_assert_true (LIBRSVG_CHECK_VERSION (2, 62, 3));
    g_assert_false (LIBRSVG_CHECK_VERSION (2, 62, 4));
    g_assert_false (LIBRSVG_CHECK_VERSION (2, 63, 0));
    g_assert_false (LIBRSVG_CHECK_VERSION (3, 0, 0));
}

static void
library_version_constants (void)
{
    g_assert_cmpuint (rsvg_major_version, ==, LIBRSVG_MAJOR_VERSION);
    g_assert_cmpuint (rsvg_minor_version, ==, LIBRSVG_MINOR_VERSION);
    g_assert_cmpuint (rsvg_micro_version, ==, LIBRSVG_MICRO_VERSION);
    g_assert_cmpuint (librsvg_major_version, ==, LIBRSVG_MAJOR_VERSION);
    g_assert_cmpstr (librsvg_version, ==, LIBRSVG_VERSION);
}

static void
compat_features_header (void)
{
    /* librsvg-features.h is a wrapper; these macros must still resolve. */
    g_assert_true (LIBRSVG_HAVE_SVGZ);
    g_assert_true (LIBRSVG_HAVE_CSS);
    g_assert_true (LIBRSVG_HAVE_PIXBUF);
}

static void
get_intrinsic_dimensions (void)
{
    RsvgHandle *handle = load_test_document ("example.svg");
    gboolean has_width, has_height, has_viewbox;
    RsvgLength width, height;
    RsvgRectangle viewbox;

    rsvg_handle_get_intrinsic_dimensions (handle,
                                          &has_width, &width,
                                          &has_height, &height,
                                          &has_viewbox, &viewbox);

    g_assert_true (has_width);
    g_assert_cmpfloat (width.length, ==, 100.0);
    g_assert_cmpint (width.unit, ==, RSVG_UNIT_PX);
    g_assert_true (has_height);
    g_assert_cmpfloat (height.length, ==, 400.0);
    g_assert_cmpint (height.unit, ==, RSVG_UNIT_PX);
    g_assert_true (has_viewbox);
    g_assert_cmpfloat (viewbox.x, ==, 0.0);
    g_assert_cmpfloat (viewbox.y, ==, 0.0);
    g_assert_cmpfloat (viewbox.width, ==, 100.0);
    g_assert_cmpfloat (viewbox.height, ==, 400.0);
    g_object_unref (handle);
}

static void
get_intrinsic_dimensions_missing_values (void)
{
    RsvgHandle *handle = load_test_document ("no-viewbox.svg");
    gboolean has_width, has_height, has_viewbox;
    RsvgLength width, height;
    RsvgRectangle viewbox;

    rsvg_handle_get_intrinsic_dimensions (handle,
                                          &has_width, &width,
                                          &has_height, &height,
                                          &has_viewbox, &viewbox);
    g_assert_true (has_width);
    g_assert_true (has_height);
    g_assert_false (has_viewbox);
    g_object_unref (handle);
}

static void
get_intrinsic_size_in_pixels_yes (void)
{
    RsvgHandle *handle = load_test_document ("size.svg");
    gdouble width, height;

    rsvg_handle_set_dpi (handle, 96.0);
    g_assert_true (rsvg_handle_get_intrinsic_size_in_pixels (handle, NULL, NULL));
    g_assert_true (rsvg_handle_get_intrinsic_size_in_pixels (handle, &width, &height));
    g_assert_cmpfloat (width, ==, 192.0);
    g_assert_cmpfloat (height, ==, 288.0);
    g_object_unref (handle);
}

static void
get_intrinsic_size_in_pixels_no (void)
{
    RsvgHandle *handle = load_test_document ("no-size.svg");
    gdouble width = 99, height = 99;

    rsvg_handle_set_dpi (handle, 96.0);
    g_assert_false (rsvg_handle_get_intrinsic_size_in_pixels (handle, &width, &height));
    g_assert_cmpfloat (width, ==, 0.0);
    g_assert_cmpfloat (height, ==, 0.0);
    g_object_unref (handle);
}

static void
render_document (void)
{
    RsvgHandle *handle = load_test_document ("document.svg");
    cairo_surface_t *output;
    cairo_t *cr;
    RsvgRectangle viewport = { 50.0, 50.0, 50.0, 50.0 };
    GError *error = NULL;

    output = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 150, 150);
    cr = cairo_create (output);
    g_assert_true (rsvg_handle_render_document (handle, cr, &viewport, &error));
    g_assert_no_error (error);
    g_assert_cmpint (cairo_status (cr), ==, CAIRO_STATUS_SUCCESS);
    cairo_destroy (cr);
    cairo_surface_destroy (output);
    g_object_unref (handle);
}

static void
get_geometry_for_layer (void)
{
    RsvgHandle *handle = load_test_document ("geometry.svg");
    RsvgRectangle viewport = { 0.0, 0.0, 100.0, 400.0 };
    RsvgRectangle ink_rect, logical_rect;
    GError *error = NULL;

    g_assert_false (rsvg_handle_get_geometry_for_layer (handle, "#nonexistent", &viewport,
                                                        &ink_rect, &logical_rect, &error));
    g_assert_nonnull (error);
    g_clear_error (&error);

    g_assert_true (rsvg_handle_get_geometry_for_layer (handle, "#two", &viewport,
                                                       &ink_rect, &logical_rect, &error));
    g_assert_no_error (error);

    g_assert_cmpfloat (ink_rect.x, ==, 5.0);
    g_assert_cmpfloat (ink_rect.y, ==, 195.0);
    g_assert_cmpfloat (ink_rect.width, ==, 90.0);
    g_assert_cmpfloat (ink_rect.height, ==, 110.0);
    g_assert_cmpfloat (logical_rect.x, ==, 10.0);
    g_assert_cmpfloat (logical_rect.y, ==, 200.0);
    g_assert_cmpfloat (logical_rect.width, ==, 80.0);
    g_assert_cmpfloat (logical_rect.height, ==, 100.0);
    g_object_unref (handle);
}

static void
render_layer (void)
{
    RsvgHandle *handle = load_test_document ("layers.svg");
    cairo_surface_t *output;
    cairo_t *cr;
    RsvgRectangle viewport = { 100.0, 100.0, 100.0, 100.0 };
    GError *error = NULL;

    output = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 300, 300);
    cr = cairo_create (output);
    g_assert_true (rsvg_handle_render_layer (handle, cr, "#bar", &viewport, &error));
    g_assert_no_error (error);
    cairo_destroy (cr);
    cairo_surface_destroy (output);
    g_object_unref (handle);
}

static void
untransformed_element (void)
{
    RsvgHandle *handle = load_test_document ("geometry-element.svg");
    RsvgRectangle ink_rect, logical_rect;
    cairo_surface_t *output;
    cairo_t *cr;
    RsvgRectangle viewport = { 100.0, 100.0, 100.0, 100.0 };
    GError *error = NULL;

    g_assert_false (rsvg_handle_get_geometry_for_element (handle, "#nonexistent",
                                                          &ink_rect, &logical_rect, &error));
    g_assert_nonnull (error);
    g_clear_error (&error);

    g_assert_true (rsvg_handle_get_geometry_for_element (handle, "#foo",
                                                         &ink_rect, &logical_rect, &error));
    g_assert_no_error (error);

    g_assert_cmpfloat (ink_rect.x, ==, 0.0);
    g_assert_cmpfloat (ink_rect.y, ==, 0.0);
    g_assert_cmpfloat (ink_rect.width, ==, 40.0);
    g_assert_cmpfloat (ink_rect.height, ==, 50.0);
    g_assert_cmpfloat (logical_rect.x, ==, 5.0);
    g_assert_cmpfloat (logical_rect.y, ==, 5.0);
    g_assert_cmpfloat (logical_rect.width, ==, 30.0);
    g_assert_cmpfloat (logical_rect.height, ==, 40.0);

    output = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 300, 300);
    cr = cairo_create (output);
    g_assert_true (rsvg_handle_render_element (handle, cr, "#foo", &viewport, &error));
    g_assert_no_error (error);
    cairo_destroy (cr);
    cairo_surface_destroy (output);
    g_object_unref (handle);
}

static void
get_pixbuf_and_error (void)
{
    RsvgHandle *handle = load_test_document ("example.svg");
    GError *error = NULL;
    GdkPixbuf *pixbuf = rsvg_handle_get_pixbuf_and_error (handle, &error);

    g_assert_nonnull (pixbuf);
    g_assert_no_error (error);
    g_assert_cmpint (gdk_pixbuf_get_width (pixbuf), ==, 100);
    g_assert_cmpint (gdk_pixbuf_get_height (pixbuf), ==, 400);
    g_object_unref (pixbuf);
    g_object_unref (handle);
}

static guint32
pixel_argb (cairo_surface_t *surface, int x, int y)
{
    unsigned char *data;
    int stride;

    cairo_surface_flush (surface);
    data = cairo_image_surface_get_data (surface);
    stride = cairo_image_surface_get_stride (surface);
    return *(guint32 *) (data + y * stride + x * 4);
}

static void
render_to_surface (RsvgHandle *handle, cairo_surface_t *surface)
{
    cairo_t *cr = cairo_create (surface);
    RsvgRectangle viewport;
    GError *error = NULL;

    viewport.x = 0;
    viewport.y = 0;
    viewport.width = cairo_image_surface_get_width (surface);
    viewport.height = cairo_image_surface_get_height (surface);

    g_assert_true (rsvg_handle_render_document (handle, cr, &viewport, &error));
    g_assert_no_error (error);
    cairo_destroy (cr);
}

static void
set_stylesheet (void)
{
    const char *css = "rect { fill: #00ff00; }";
    RsvgHandle *handle = load_test_document ("stylesheet.svg");
    cairo_surface_t *output;
    GError *error = NULL;

    g_assert_true (rsvg_handle_set_stylesheet (handle, (const guint8 *) css,
                                               strlen (css), &error));
    g_assert_no_error (error);

    output = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 100, 100);
    render_to_surface (handle, output);

    /* Interior of the 10,20 30x40 rect should be opaque green. */
    g_assert_cmphex (pixel_argb (output, 25, 40), ==, 0xff00ff00);

    cairo_surface_destroy (output);
    g_object_unref (handle);
}

static void
set_stylesheet_before_load (void)
{
    const char *css = "rect { fill: #00ff00; }";
    char *filename = get_test_filename ("stylesheet.svg");
    gchar *contents = NULL;
    gsize length = 0;
    RsvgHandle *handle;
    cairo_surface_t *output;
    GError *error = NULL;

    g_assert_true (g_file_get_contents (filename, &contents, &length, &error));
    g_assert_no_error (error);
    g_free (filename);

    handle = rsvg_handle_new ();
    g_assert_true (rsvg_handle_set_stylesheet (handle, (const guint8 *) css,
                                               strlen (css), &error));
    g_assert_no_error (error);
    g_assert_true (rsvg_handle_write (handle, (const guchar *) contents, length, &error));
    g_assert_no_error (error);
    g_assert_true (rsvg_handle_close (handle, &error));
    g_assert_no_error (error);
    g_free (contents);

    output = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 100, 100);
    render_to_surface (handle, output);
    g_assert_cmphex (pixel_argb (output, 25, 40), ==, 0xff00ff00);

    cairo_surface_destroy (output);
    g_object_unref (handle);
}

static void
set_stylesheet_current_color (void)
{
    const char *css = "* { color: #ff0000; }";
    RsvgHandle *handle = load_test_document ("currentcolor.svg");
    cairo_surface_t *output;
    GError *error = NULL;

    g_assert_true (rsvg_handle_set_stylesheet (handle, (const guint8 *) css,
                                               strlen (css), &error));
    g_assert_no_error (error);

    output = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 40, 40);
    render_to_surface (handle, output);
    g_assert_cmphex (pixel_argb (output, 20, 20), ==, 0xffff0000);

    cairo_surface_destroy (output);
    g_object_unref (handle);
}

static void
set_stylesheet_context_fill (void)
{
    const char *css = "* { color: #00ff00; }";
    RsvgHandle *handle = load_test_document ("context-fill.svg");
    cairo_surface_t *output;
    GError *error = NULL;

    g_assert_true (rsvg_handle_set_stylesheet (handle, (const guint8 *) css,
                                               strlen (css), &error));
    g_assert_no_error (error);

    output = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 40, 40);
    render_to_surface (handle, output);
    g_assert_cmphex (pixel_argb (output, 20, 20), ==, 0xff00ff00);

    cairo_surface_destroy (output);
    g_object_unref (handle);
}

static void
context_fill_from_use (void)
{
    RsvgHandle *handle = load_test_document ("context-use.svg");
    cairo_surface_t *output;

    output = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 80, 40);
    render_to_surface (handle, output);

    g_assert_cmphex (pixel_argb (output, 20, 20), ==, 0xffff0000);
    g_assert_cmphex (pixel_argb (output, 60, 20), ==, 0xff0000ff);

    cairo_surface_destroy (output);
    g_object_unref (handle);
}

static void
set_stylesheet_oversized_does_not_abort (void)
{
    RsvgHandle *handle = rsvg_handle_new ();
    gsize n = RSVG_MAX_STYLESHEET_BYTES + 1;
    guint8 *css;
    GError *error = NULL;

    css = g_malloc (n);
    memset (css, ' ', n);
    g_assert_false (rsvg_handle_set_stylesheet (handle, css, n, &error));
    g_assert_nonnull (error);
    g_clear_error (&error);
    g_free (css);
    g_object_unref (handle);
}

static void
set_stylesheet_many_rules_does_not_abort (void)
{
    GString *css;
    RsvgHandle *handle;
    cairo_surface_t *output;
    GError *error = NULL;
    int i;

    css = g_string_new ("rect { fill: #00ff00; }");
    for (i = 0; i < 5000; i++)
        g_string_append (css, "r{x:0}");

    handle = load_test_document ("stylesheet.svg");
    g_assert_true (rsvg_handle_set_stylesheet (handle, (const guint8 *) css->str,
                                               css->len, &error));
    g_assert_no_error (error);
    g_string_free (css, TRUE);

    output = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 100, 100);
    render_to_surface (handle, output);
    g_assert_cmphex (pixel_argb (output, 25, 40), ==, 0xff00ff00);

    cairo_surface_destroy (output);
    g_object_unref (handle);
}

static void
set_stylesheet_invalid_utf8 (void)
{
    RsvgHandle *handle = rsvg_handle_new ();
    const guint8 bad[] = { 0xff, 0xfe, 0xfd };
    GError *error = NULL;

    g_assert_false (rsvg_handle_set_stylesheet (handle, bad, sizeof (bad), &error));
    g_assert_nonnull (error);
    g_clear_error (&error);
    g_object_unref (handle);
}

static void
set_stylesheet_ignores_file_import (void)
{
    const char *css = "@import url(\"file:///etc/passwd\"); rect { fill: #00ff00; }";
    RsvgHandle *handle = load_test_document ("stylesheet.svg");
    cairo_surface_t *output;
    GError *error = NULL;

    g_assert_true (rsvg_handle_set_stylesheet (handle, (const guint8 *) css,
                                               strlen (css), &error));
    g_assert_no_error (error);

    output = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 100, 100);
    render_to_surface (handle, output);
    g_assert_cmphex (pixel_argb (output, 25, 40), ==, 0xff00ff00);

    cairo_surface_destroy (output);
    g_object_unref (handle);
}

static void
css_engine_load_style_survives (const char *css)
{
    GString *svg;
    RsvgHandle *handle;
    GError *error = NULL;

    svg = g_string_new ("<svg xmlns='http://www.w3.org/2000/svg' width='8' height='8'>");
    g_string_append (svg, "<style>");
    g_string_append (svg, css);
    g_string_append (svg, "</style><rect width='8' height='8' fill='green'/></svg>");
    alarm (8);
    handle = rsvg_handle_new_from_data ((const guint8 *) svg->str, svg->len, &error);
    alarm (0);
    g_string_free (svg, TRUE);
    g_assert_no_error (error);
    g_assert_nonnull (handle);
    g_object_unref (handle);
}

static void
css_deep_not_does_not_abort (void)
{
    GString *css = g_string_new (NULL);
    int i;

    for (i = 0; i < 40; i++)
        g_string_append (css, ":not(");
    g_string_append (css, "*");
    for (i = 0; i < 40; i++)
        g_string_append (css, ")");
    g_string_append (css, "{fill:red}");
    css_engine_load_style_survives (css->str);
    g_string_free (css, TRUE);
}

static void
css_long_combinator_does_not_abort (void)
{
    GString *css = g_string_new (NULL);
    int i;

    for (i = 0; i < 200; i++)
        g_string_append (css, "a ");
    g_string_append (css, "{fill:red}");
    css_engine_load_style_survives (css->str);
    g_string_free (css, TRUE);
}

static void
css_huge_nth_does_not_abort (void)
{
    css_engine_load_style_survives (
        "*:nth-child(999999999999999999n+999999999999999999){fill:red}");
}

static void
css_long_attr_selector_does_not_abort (void)
{
    GString *css = g_string_new ("*[id='");
    int i;

    for (i = 0; i < 8000; i++)
        g_string_append_c (css, 'x');
    g_string_append (css, "']{fill:red}");
    css_engine_load_style_survives (css->str);
    g_string_free (css, TRUE);
}

static void
css_style_http_import_does_not_abort (void)
{
    css_engine_load_style_survives (
        "@import url(http://127.0.0.1:1/x.css); rect{fill:green}");
}

static void
css_deep_decl_parens_does_not_abort (void)
{
    GString *css = g_string_new ("rect{filter:");
    int i;

    for (i = 0; i < 200; i++)
        g_string_append_c (css, '(');
    g_string_append (css, "x");
    for (i = 0; i < 200; i++)
        g_string_append_c (css, ')');
    g_string_append (css, "}");
    css_engine_load_style_survives (css->str);
    g_string_free (css, TRUE);
}

static void
css_stray_close_brace_does_not_abort (void)
{
    /* fuzz-css hang: parse_stylesheet stalled on a stray `}`. */
    css_engine_load_style_survives ("}");
    css_engine_load_style_survives ("? }\n.symboliC0; color: #00ff00; }\n#");
}

static void
css_deep_tree_does_not_abort (void)
{
    GString *svg;
    RsvgHandle *handle;
    GError *error = NULL;
    cairo_surface_t *output;
    int i;
    const int depth = RSVG_MAX_CSS_TREE_DEPTH + 64;

    /* Nest past cascade_walk's cap. Presentation fill still paints;
     * author CSS on the deep leaf may not. Process must stay up. */
    svg = g_string_new ("<svg xmlns='http://www.w3.org/2000/svg' width='8' height='8'>");
    g_string_append (svg, "<style>rect{fill:lime}</style>");
    for (i = 0; i < depth; i++)
        g_string_append (svg, "<g>");
    g_string_append (svg, "<rect width='8' height='8' fill='green'/>");
    for (i = 0; i < depth; i++)
        g_string_append (svg, "</g>");
    g_string_append (svg, "</svg>");

    alarm (8);
    handle = rsvg_handle_new_from_data ((const guint8 *) svg->str, svg->len, &error);
    g_string_free (svg, TRUE);
    g_assert_no_error (error);
    g_assert_nonnull (handle);

    output = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 8, 8);
    render_to_surface (handle, output);
    alarm (0);
    g_assert_cmpuint ((pixel_argb (output, 4, 4) >> 24) & 0xff, >, 0);

    cairo_surface_destroy (output);
    g_object_unref (handle);
}

static void
href_use (void)
{
    RsvgHandle *handle = load_test_document ("href-use.svg");
    cairo_surface_t *output = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 20, 20);

    render_to_surface (handle, output);
    g_assert_cmphex (pixel_argb (output, 10, 10), ==, 0xff00ff00);

    cairo_surface_destroy (output);
    g_object_unref (handle);
}

static void
xlink_href_use_still_works (void)
{
    RsvgHandle *handle = load_test_document ("xlink-href-use.svg");
    cairo_surface_t *output = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 20, 20);

    render_to_surface (handle, output);
    g_assert_cmphex (pixel_argb (output, 10, 10), ==, 0xff00ff00);

    cairo_surface_destroy (output);
    g_object_unref (handle);
}

static void
href_overrides_xlink_href (void)
{
    RsvgHandle *handle = load_test_document ("href-prefers-href.svg");
    cairo_surface_t *output = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 20, 20);

    render_to_surface (handle, output);
    g_assert_cmphex (pixel_argb (output, 10, 10), ==, 0xff00ff00);

    cairo_surface_destroy (output);
    g_object_unref (handle);
}

static void
paint_order_stroke_then_fill (void)
{
    RsvgHandle *handle = load_test_document ("paint-order.svg");
    cairo_surface_t *output = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 20, 20);

    render_to_surface (handle, output);
    /* Fat stroke would cover the fill unless fill is painted last. */
    g_assert_cmphex (pixel_argb (output, 10, 10), ==, 0xff00ff00);

    cairo_surface_destroy (output);
    g_object_unref (handle);
}

static void
mix_blend_mode_multiply (void)
{
    RsvgHandle *handle = load_test_document ("mix-blend-mode.svg");
    cairo_surface_t *output = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 20, 20);

    render_to_surface (handle, output);
    g_assert_cmphex (pixel_argb (output, 10, 10), ==, 0xff000000);

    cairo_surface_destroy (output);
    g_object_unref (handle);
}

static void
mask_type_alpha (void)
{
    RsvgHandle *handle = load_test_document ("mask-type-alpha.svg");
    cairo_surface_t *output = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 20, 20);

    render_to_surface (handle, output);
    /* Opaque black mask + mask-type=alpha keeps the red overlay. */
    g_assert_cmphex (pixel_argb (output, 10, 10), ==, 0xffff0000);

    cairo_surface_destroy (output);
    g_object_unref (handle);
}

static void
vector_effect_non_scaling_stroke (void)
{
    RsvgHandle *handle = load_test_document ("vector-effect.svg");
    cairo_surface_t *output = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 40, 20);
    guint32 on_stroke, off_stroke;

    render_to_surface (handle, output);
    on_stroke = pixel_argb (output, 10, 10);
    off_stroke = pixel_argb (output, 10, 0);
    /* 1 device-pixel stroke around y=10; y=0 must stay empty. */
    g_assert_cmpuint ((on_stroke >> 24) & 0xff, >, 0);
    g_assert_cmphex (off_stroke, ==, 0x00000000);

    cairo_surface_destroy (output);
    g_object_unref (handle);
}

static void
remote_image_href_is_ignored (void)
{
    const char *svg =
        "<svg xmlns='http://www.w3.org/2000/svg' width='10' height='10'>"
        "<rect width='10' height='10' fill='#00ff00'/>"
        "<image href='http://example.com/missing.png' width='10' height='10'/>"
        "<image href='file://evil.example/missing.png' width='10' height='10'/>"
        "</svg>";
    GError *error = NULL;
    RsvgHandle *handle;
    cairo_surface_t *output;

    handle = rsvg_handle_new_from_data ((const guint8 *) svg, strlen (svg), &error);
    g_assert_nonnull (handle);
    g_assert_no_error (error);

    output = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 10, 10);
    render_to_surface (handle, output);
    g_assert_cmphex (pixel_argb (output, 5, 5), ==, 0xff00ff00);

    cairo_surface_destroy (output);
    g_object_unref (handle);
}

static void
set_cancellable_for_rendering (void)
{
    RsvgHandle *handle = load_test_document ("layers.svg");
    cairo_surface_t *output = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 300, 300);
    cairo_t *cr = cairo_create (output);
    RsvgRectangle viewport = { 100.0, 100.0, 100.0, 100.0 };
    GError *error = NULL;
    GCancellable *cancellable = g_cancellable_new ();

    g_cancellable_cancel (cancellable);
    rsvg_handle_set_cancellable_for_rendering (handle, cancellable);

    g_assert_false (rsvg_handle_render_layer (handle, cr, "#bar", &viewport, &error));
    g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);

    g_clear_error (&error);
    cairo_destroy (cr);
    cairo_surface_destroy (output);
    g_object_unref (cancellable);
    g_object_unref (handle);
}

static void
file_url_query_is_denied (void)
{
    const char *svg =
        "<svg xmlns='http://www.w3.org/2000/svg' width='10' height='10'>"
        "<rect width='10' height='10' fill='#00ff00'/>"
        "<image href='sibling.png?../../../../etc/passwd' width='10' height='10'/>"
        "<image href='sibling.png#frag' width='10' height='10'/>"
        "</svg>";
    GError *error = NULL;
    RsvgHandle *handle;
    cairo_surface_t *output;
    RsvgRectangle viewport = { 0, 0, 10, 10 };
    char *base;

    handle = rsvg_handle_new ();
    base = g_build_filename (test_utils_get_test_data_path (), "api", "example.svg", NULL);
    rsvg_handle_set_base_uri (handle, base);
    g_free (base);
    g_assert_true (rsvg_handle_write (handle, (const guint8 *) svg, strlen (svg), &error));
    g_assert_true (rsvg_handle_close (handle, &error));
    g_assert_no_error (error);

    output = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 10, 10);
    {
        cairo_t *cr = cairo_create (output);
        g_assert_true (rsvg_handle_render_document (handle, cr, &viewport, &error));
        cairo_destroy (cr);
    }
    g_assert_no_error (error);
    g_assert_cmphex (pixel_argb (output, 5, 5), ==, 0xff00ff00);

    cairo_surface_destroy (output);
    g_object_unref (handle);
}

static void
write_text_file (const char *path, const char *contents)
{
    GError *error = NULL;

    g_assert_true (g_file_set_contents (path, contents, -1, &error));
    g_assert_no_error (error);
}

static RsvgHandle *
handle_from_path (const char *path)
{
    GError *error = NULL;
    RsvgHandle *handle = rsvg_handle_new_from_file (path, &error);

    g_assert_nonnull (handle);
    g_assert_no_error (error);
    return handle;
}

static guint32
center_pixel_of_handle (RsvgHandle *handle)
{
    cairo_surface_t *output = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 8, 8);
    guint32 p;

    render_to_surface (handle, output);
    p = pixel_argb (output, 4, 4);
    cairo_surface_destroy (output);
    return p;
}


/* CVE-2023-38633-style: xi:include / image href must not walk past the
 * document directory. Secret lives in a sibling of the doc dir, not
 * /etc/passwd. */
static void
href_path_traversal_is_denied (void)
{
    GError *error = NULL;
    char *root, *outside, *docdir;
    char *secret_xml, *sibling_xml, *secret_png, *sibling_png;
    char *svg_sib, *svg_esc, *svg_query, *svg_img_esc, *svg_img_query, *svg_img_sib;
    char *red_png;
    gchar *png_bytes = NULL;
    gsize png_len = 0;
    RsvgHandle *handle;
    gboolean xinclude_ok;

    root = g_dir_make_tmp ("librsvg-href-XXXXXX", &error);
    g_assert_no_error (error);
    g_assert_nonnull (root);
    outside = g_build_filename (root, "outside", NULL);
    docdir = g_build_filename (root, "doc", NULL);
    g_assert_cmpint (g_mkdir_with_parents (outside, 0700), ==, 0);
    g_assert_cmpint (g_mkdir_with_parents (docdir, 0700), ==, 0);

    secret_xml = g_build_filename (outside, "secret.xml", NULL);
    sibling_xml = g_build_filename (docdir, "sibling.xml", NULL);
    secret_png = g_build_filename (outside, "secret.png", NULL);
    sibling_png = g_build_filename (docdir, "sibling.png", NULL);
    svg_sib = g_build_filename (docdir, "include-sibling.svg", NULL);
    svg_esc = g_build_filename (docdir, "include-escape.svg", NULL);
    svg_query = g_build_filename (docdir, "include-query.svg", NULL);
    svg_img_esc = g_build_filename (docdir, "image-escape.svg", NULL);
    svg_img_query = g_build_filename (docdir, "image-query.svg", NULL);
    svg_img_sib = g_build_filename (docdir, "image-sibling.svg", NULL);

    write_text_file (secret_xml,
                     "<rect id='secret-rect' width='8' height='8' fill='#ff0000'/>");
    write_text_file (sibling_xml,
                     "<rect id='sibling-rect' width='8' height='8' fill='#0000ff'/>");

    red_png = g_build_filename (test_utils_get_test_data_path (), "api", "red.png", NULL);
    if (g_file_get_contents (red_png, &png_bytes, &png_len, NULL) && png_len > 0) {
        g_assert_true (g_file_set_contents (secret_png, png_bytes, (gssize) png_len, &error));
        g_assert_no_error (error);
        g_assert_true (g_file_set_contents (sibling_png, png_bytes, (gssize) png_len, &error));
        g_assert_no_error (error);
    }
    g_free (red_png);
    g_free (png_bytes);

    write_text_file (svg_sib,
                     "<svg xmlns='http://www.w3.org/2000/svg'"
                     " xmlns:xi='http://www.w3.org/2001/XInclude'"
                     " width='8' height='8'>"
                     "<rect width='8' height='8' fill='#00ff00'/>"
                     "<xi:include href='sibling.xml' parse='xml'/>"
                     "</svg>");
    write_text_file (svg_esc,
                     "<svg xmlns='http://www.w3.org/2000/svg'"
                     " xmlns:xi='http://www.w3.org/2001/XInclude'"
                     " width='8' height='8'>"
                     "<rect width='8' height='8' fill='#00ff00'/>"
                     "<xi:include href='../outside/secret.xml' parse='xml'/>"
                     "</svg>");
    write_text_file (svg_query,
                     "<svg xmlns='http://www.w3.org/2000/svg'"
                     " xmlns:xi='http://www.w3.org/2001/XInclude'"
                     " width='8' height='8'>"
                     "<rect width='8' height='8' fill='#00ff00'/>"
                     "<xi:include href='.?../outside/secret.xml' parse='xml'/>"
                     "</svg>");
    write_text_file (svg_img_esc,
                     "<svg xmlns='http://www.w3.org/2000/svg' width='8' height='8'>"
                     "<rect width='8' height='8' fill='#00ff00'/>"
                     "<image href='../outside/secret.png' width='8' height='8'/>"
                     "</svg>");
    write_text_file (svg_img_query,
                     "<svg xmlns='http://www.w3.org/2000/svg' width='8' height='8'>"
                     "<rect width='8' height='8' fill='#00ff00'/>"
                     "<image href='.?../outside/secret.png' width='8' height='8'/>"
                     "</svg>");
    write_text_file (svg_img_sib,
                     "<svg xmlns='http://www.w3.org/2000/svg' width='8' height='8'>"
                     "<rect width='8' height='8' fill='#00ff00'/>"
                     "<image href='sibling.png' width='8' height='8'/>"
                     "</svg>");

    handle = handle_from_path (svg_sib);
    xinclude_ok = rsvg_handle_has_sub (handle, "#sibling-rect");
    if (xinclude_ok)
        g_assert_cmphex (center_pixel_of_handle (handle), ==, 0xff0000ff);
    else
        g_assert_cmphex (center_pixel_of_handle (handle), ==, 0xff00ff00);
    g_object_unref (handle);

    handle = handle_from_path (svg_esc);
    g_assert_false (rsvg_handle_has_sub (handle, "#secret-rect"));
    g_assert_cmphex (center_pixel_of_handle (handle), ==, 0xff00ff00);
    g_object_unref (handle);

    handle = handle_from_path (svg_query);
    g_assert_false (rsvg_handle_has_sub (handle, "#secret-rect"));
    g_assert_cmphex (center_pixel_of_handle (handle), ==, 0xff00ff00);
    g_object_unref (handle);

    handle = handle_from_path (svg_img_esc);
    g_assert_cmphex (center_pixel_of_handle (handle), ==, 0xff00ff00);
    g_object_unref (handle);

    handle = handle_from_path (svg_img_query);
    g_assert_cmphex (center_pixel_of_handle (handle), ==, 0xff00ff00);
    g_object_unref (handle);

#ifdef HAVE_LIBPNG
    handle = handle_from_path (svg_img_sib);
    g_assert_cmphex (center_pixel_of_handle (handle), ==, 0xffff0000);
    g_object_unref (handle);
#endif

    g_remove (svg_sib);
    g_remove (svg_esc);
    g_remove (svg_query);
    g_remove (svg_img_esc);
    g_remove (svg_img_query);
    g_remove (svg_img_sib);
    g_remove (secret_xml);
    g_remove (sibling_xml);
    g_remove (secret_png);
    g_remove (sibling_png);
    g_rmdir (docdir);
    g_rmdir (outside);
    g_rmdir (root);

    g_free (secret_xml);
    g_free (sibling_xml);
    g_free (secret_png);
    g_free (sibling_png);
    g_free (svg_sib);
    g_free (svg_esc);
    g_free (svg_query);
    g_free (svg_img_esc);
    g_free (svg_img_query);
    g_free (svg_img_sib);
    g_free (outside);
    g_free (docdir);
    g_free (root);
}

/* 8×8 lossless red WebP / PNG generated with Pillow. */
#define WEBP_RED_DATA_URI \
    "data:image/webp;base64,UklGRhwAAABXRUJQVlA4TA8AAAAvB8ABAAcQ/Y/+ByKi/wEA"
#define WEBP_RED_SNIFF_URI \
    "data:application/octet-stream;base64,UklGRhwAAABXRUJQVlA4TA8AAAAvB8ABAAcQ/Y/+ByKi/wEA"
#define WEBP_BAD_DATA_URI \
    "data:image/webp;base64,UklGRggAAABXRUJQ"
#define PNG_RED_DATA_URI \
    "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAgAAAAICAIAAABLbSncAAAAEklEQVR4nGP8z4AdMOEQH6QSAM1BAQ/oQeJvAAAAAElFTkSuQmCC"
#define PNG_RED_SNIFF_URI \
    "data:application/octet-stream;base64,iVBORw0KGgoAAAANSUhEUgAAAAgAAAAICAIAAABLbSncAAAAEklEQVR4nGP8z4AdMOEQH6QSAM1BAQ/oQeJvAAAAAElFTkSuQmCC"
#define PNG_BAD_DATA_URI \
    "data:image/png;base64,iVBORw0KGgo="
#define JPEG_RED_DATA_URI \
    "data:image/jpeg;base64,/9j/4AAQSkZJRgABAQAAAQABAAD/2wBDAAMCAgMCAgMDAwMEAwMEBQgFBQQEBQoHBwYIDAoMDAsKCwsNDhIQDQ4RDgsLEBYQERMUFRUVDA8XGBYUGBIUFRT/2wBDAQMEBAUEBQkFBQkUDQsNFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBT/wAARCAAIAAgDAREAAhEBAxEB/8QAFAABAAAAAAAAAAAAAAAAAAAACP/EABQQAQAAAAAAAAAAAAAAAAAAAAD/xAAVAQEBAAAAAAAAAAAAAAAAAAAHCf/EABQRAQAAAAAAAAAAAAAAAAAAAAD/2gAMAwEAAhEDEQA/ADoDFU3/2Q=="
#define JPEG_RED_SNIFF_URI \
    "data:application/octet-stream;base64,/9j/4AAQSkZJRgABAQAAAQABAAD/2wBDAAMCAgMCAgMDAwMEAwMEBQgFBQQEBQoHBwYIDAoMDAsKCwsNDhIQDQ4RDgsLEBYQERMUFRUVDA8XGBYUGBIUFRT/2wBDAQMEBAUEBQkFBQkUDQsNFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBT/wAARCAAIAAgDAREAAhEBAxEB/8QAFAABAAAAAAAAAAAAAAAAAAAACP/EABQQAQAAAAAAAAAAAAAAAAAAAAD/xAAVAQEBAAAAAAAAAAAAAAAAAAAHCf/EABQRAQAAAAAAAAAAAAAAAAAAAAD/2gAMAwEAAhEDEQA/ADoDFU3/2Q=="
#define JPEG_BAD_DATA_URI \
    "data:image/jpeg;base64,/9j/4AAQSkZJRg=="
#define GIF_RED_DATA_URI \
    "data:image/gif;base64,R0lGODdhCAAIAIEAAP8AAAAAAAAAAAAAACwAAAAACAAIAAAIDwABCBxIsKDBgwgTKkwYEAA7"
#define GIF_RED_SNIFF_URI \
    "data:application/octet-stream;base64,R0lGODdhCAAIAIEAAP8AAAAAAAAAAAAAACwAAAAACAAIAAAIDwABCBxIsKDBgwgTKkwYEAA7"
#define GIF_BAD_DATA_URI \
    "data:image/gif;base64,R0lGODlh"
#define BMP_RED_DATA_URI \
    "data:image/bmp;base64,Qk32AAAAAAAAADYAAAAoAAAACAAAAAgAAAABABgAAAAAAMAAAAAAAAAAAAAAAAAAAAAAAAAAAAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/"
#define BMP_RED_SNIFF_URI \
    "data:application/octet-stream;base64,Qk32AAAAAAAAADYAAAAoAAAACAAAAAgAAAABABgAAAAAAMAAAAAAAAAAAAAAAAAAAAAAAAAAAAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/AAD/"
#define BMP_BAD_DATA_URI \
    "data:image/bmp;base64,Qk0="
/* 54-byte BITMAPINFOHEADER, 40000×40000 24-bit, no pixel data. */
#define BMP_OVERSIZE_DIM_URI \
    "data:image/bmp;base64,Qk02AAAAAAAAADYAAAAoAAAAQJwAAECcAAABABgAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
/* 8192×8192 24-bit: under the 32767 side cap, over the 32MP pixel cap. */
#define BMP_OVERSIZE_PIXELS_URI \
    "data:image/bmp;base64,Qk02AAAAAAAAADYAAAAoAAAAACAAAAAgAAABABgAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
#define BMP_BAD_OFFBITS_URI \
    "data:image/bmp;base64,Qk02AAAAAAAAAAIAAAAoAAAACAAAAAgAAAABABgAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
/* 8×8 BI_RLE8 with a 255-pixel encoded run (must clip, not abort). */
#define BMP_RLE_OOB_URI \
    "data:image/bmp;base64,Qk1EAAAAAAAAAD4AAAAoAAAACAAAAAgAAAABAAgAAQAAAAAAAAAAAAAAAAAAAAIAAAAAAAAAAAAAAAAA/wD/AQAAAAE="
#define PNG_OVERSIZE_DIM_URI \
    "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAnEAAAJxACAIAAADebplSAAAAAElFTkSuQmCC"
/* rust/rsvg/tests/fixtures/reftests/rectangle.avif — 10×10 #7eff02 */
#define AVIF_RECT_DATA_URI \
    "data:image/avif;base64,AAAAHGZ0eXBhdmlmAAAAAGF2aWZtaWYxbWlhZgAAAOptZXRhAAAAAAAAACFoZGxyAAAAAAAAAABwaWN0AAAAAAAAAAAAAAAAAAAAAA5waXRtAAAAAAABAAAAImlsb2MAAAAAREAAAQABAAAAAAEOAAEAAAAAAAAAIQAAACNpaW5mAAAAAAABAAAAFWluZmUCAAAAAAEAAGF2MDEAAAAAamlwcnAAAABLaXBjbwAAABNjb2xybmNseAABAA0ABoAAAAAMYXYxQ4EADAAAAAAUaXNwZQAAAAAAAAAKAAAACgAAABBwaXhpAAAAAAMICAgAAAAXaXBtYQAAAAAAAAABAAEEAYIDBAAAACltZGF0EgAKCRgM5lggIaDQgDISGAAKKKKEAACpjMK7oRKCHmG8"
#define AVIF_RECT_SNIFF_URI \
    "data:application/octet-stream;base64,AAAAHGZ0eXBhdmlmAAAAAGF2aWZtaWYxbWlhZgAAAOptZXRhAAAAAAAAACFoZGxyAAAAAAAAAABwaWN0AAAAAAAAAAAAAAAAAAAAAA5waXRtAAAAAAABAAAAImlsb2MAAAAAREAAAQABAAAAAAEOAAEAAAAAAAAAIQAAACNpaW5mAAAAAAABAAAAFWluZmUCAAAAAAEAAGF2MDEAAAAAamlwcnAAAABLaXBjbwAAABNjb2xybmNseAABAA0ABoAAAAAMYXYxQ4EADAAAAAAUaXNwZQAAAAAAAAAKAAAACgAAABBwaXhpAAAAAAMICAgAAAAXaXBtYQAAAAAAAAABAAEEAYIDBAAAACltZGF0EgAKCRgM5lggIaDQgDISGAAKKKKEAACpjMK7oRKCHmG8"
#define AVIF_BAD_DATA_URI \
    "data:image/avif;base64,AAAAHGZ0eXBhdmlm"

static RsvgHandle *
handle_from_svg_string (const char *svg)
{
    GError *error = NULL;
    RsvgHandle *handle;

    handle = rsvg_handle_new_from_data ((const guint8 *) svg, strlen (svg), &error);
    g_assert_nonnull (handle);
    g_assert_no_error (error);
    return handle;
}

static void
assert_href_paints_center (const char *href, guint32 expected)
{
    char *svg;
    RsvgHandle *handle;
    cairo_surface_t *output;

    svg = g_strdup_printf (
        "<svg xmlns='http://www.w3.org/2000/svg' width='8' height='8'>"
        "<rect width='8' height='8' fill='#00ff00'/>"
        "<image href='%s' width='8' height='8'/>"
        "</svg>", href);
    handle = handle_from_svg_string (svg);
    g_free (svg);

    output = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 8, 8);
    render_to_surface (handle, output);
    g_assert_cmphex (pixel_argb (output, 4, 4), ==, expected);

    cairo_surface_destroy (output);
    g_object_unref (handle);
}

#ifdef HAVE_LIBWEBP
static void
webp_image_data_uri_renders (void)
{
    assert_href_paints_center (WEBP_RED_DATA_URI, 0xffff0000);
}

static void
webp_image_sniff_without_mime_renders (void)
{
    assert_href_paints_center (WEBP_RED_SNIFF_URI, 0xffff0000);
}

static void
webp_image_file_href_renders (void)
{
    RsvgHandle *handle = load_test_document ("webp-image.svg");
    cairo_surface_t *output = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 8, 8);

    render_to_surface (handle, output);
    g_assert_cmphex (pixel_argb (output, 4, 4), ==, 0xffff0000);

    cairo_surface_destroy (output);
    g_object_unref (handle);
}

static void
webp_decode_failure_does_not_abort (void)
{
    /* RIFF/WEBP sniff matches; payload is truncated. Image is dropped. */
    assert_href_paints_center (WEBP_BAD_DATA_URI, 0xff00ff00);
}
#else
static void
webp_without_libwebp_is_ignored (void)
{
    assert_href_paints_center (WEBP_RED_DATA_URI, 0xff00ff00);
}
#endif

#ifdef HAVE_LIBPNG
static void
png_image_data_uri_renders (void)
{
    assert_href_paints_center (PNG_RED_DATA_URI, 0xffff0000);
}

static void
png_image_sniff_without_mime_renders (void)
{
    assert_href_paints_center (PNG_RED_SNIFF_URI, 0xffff0000);
}

static void
png_image_file_href_renders (void)
{
    RsvgHandle *handle = load_test_document ("png-image.svg");
    cairo_surface_t *output = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 8, 8);

    render_to_surface (handle, output);
    g_assert_cmphex (pixel_argb (output, 4, 4), ==, 0xffff0000);

    cairo_surface_destroy (output);
    g_object_unref (handle);
}

static void
png_decode_failure_does_not_abort (void)
{
    assert_href_paints_center (PNG_BAD_DATA_URI, 0xff00ff00);
}

static void
png_oversized_dimensions_does_not_abort (void)
{
    assert_href_paints_center (PNG_OVERSIZE_DIM_URI, 0xff00ff00);
}
#else
static void
png_without_libpng_is_ignored (void)
{
    assert_href_paints_center (PNG_RED_DATA_URI, 0xff00ff00);
}
#endif

#ifdef HAVE_LIBJPEG
/* JPEG is lossy (YCbCr). A solid #ff0000 source decodes near-red,
 * not 0xffff0000. Require opaque red, not the green fallback. */
static void
assert_href_paints_near_red (const char *href)
{
    char *svg;
    RsvgHandle *handle;
    cairo_surface_t *output;
    guint32 p;
    guint8 a, r, g, b;

    svg = g_strdup_printf (
        "<svg xmlns='http://www.w3.org/2000/svg' width='8' height='8'>"
        "<rect width='8' height='8' fill='#00ff00'/>"
        "<image href='%s' width='8' height='8'/>"
        "</svg>", href);
    handle = handle_from_svg_string (svg);
    g_free (svg);

    output = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 8, 8);
    render_to_surface (handle, output);
    p = pixel_argb (output, 4, 4);
    a = (guint8) (p >> 24);
    r = (guint8) (p >> 16);
    g = (guint8) (p >> 8);
    b = (guint8) p;
    g_assert_cmpint (a, ==, 0xff);
    g_assert_cmpint (r, >=, 0xf0);
    g_assert_cmpint (g, <=, 0x08);
    g_assert_cmpint (b, <=, 0x08);

    cairo_surface_destroy (output);
    g_object_unref (handle);
}

static void
jpeg_image_data_uri_renders (void)
{
    assert_href_paints_near_red (JPEG_RED_DATA_URI);
}

static void
jpeg_image_sniff_without_mime_renders (void)
{
    assert_href_paints_near_red (JPEG_RED_SNIFF_URI);
}

static void
jpeg_image_file_href_renders (void)
{
    RsvgHandle *handle = load_test_document ("jpeg-image.svg");
    cairo_surface_t *output = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 8, 8);
    guint32 p;
    guint8 a, r, g, b;

    render_to_surface (handle, output);
    p = pixel_argb (output, 4, 4);
    a = (guint8) (p >> 24);
    r = (guint8) (p >> 16);
    g = (guint8) (p >> 8);
    b = (guint8) p;
    g_assert_cmpint (a, ==, 0xff);
    g_assert_cmpint (r, >=, 0xf0);
    g_assert_cmpint (g, <=, 0x08);
    g_assert_cmpint (b, <=, 0x08);

    cairo_surface_destroy (output);
    g_object_unref (handle);
}

static void
jpeg_decode_failure_does_not_abort (void)
{
    assert_href_paints_center (JPEG_BAD_DATA_URI, 0xff00ff00);
}
#else
static void
jpeg_without_libjpeg_is_ignored (void)
{
    assert_href_paints_center (JPEG_RED_DATA_URI, 0xff00ff00);
}
#endif

#ifdef HAVE_LIBGIF
static void
gif_image_data_uri_renders (void)
{
    assert_href_paints_center (GIF_RED_DATA_URI, 0xffff0000);
}

static void
gif_image_sniff_without_mime_renders (void)
{
    assert_href_paints_center (GIF_RED_SNIFF_URI, 0xffff0000);
}

static void
gif_image_file_href_renders (void)
{
    RsvgHandle *handle = load_test_document ("gif-image.svg");
    cairo_surface_t *output = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 8, 8);

    render_to_surface (handle, output);
    g_assert_cmphex (pixel_argb (output, 4, 4), ==, 0xffff0000);

    cairo_surface_destroy (output);
    g_object_unref (handle);
}

static void
gif_decode_failure_does_not_abort (void)
{
    assert_href_paints_center (GIF_BAD_DATA_URI, 0xff00ff00);
}
#else
static void
gif_without_libgif_is_ignored (void)
{
    assert_href_paints_center (GIF_RED_DATA_URI, 0xff00ff00);
}
#endif

#ifdef HAVE_BMP
static void
bmp_image_data_uri_renders (void)
{
    assert_href_paints_center (BMP_RED_DATA_URI, 0xffff0000);
}

static void
bmp_image_sniff_without_mime_renders (void)
{
    assert_href_paints_center (BMP_RED_SNIFF_URI, 0xffff0000);
}

static void
bmp_image_file_href_renders (void)
{
    RsvgHandle *handle = load_test_document ("bmp-image.svg");
    cairo_surface_t *output = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 8, 8);

    render_to_surface (handle, output);
    g_assert_cmphex (pixel_argb (output, 4, 4), ==, 0xffff0000);

    cairo_surface_destroy (output);
    g_object_unref (handle);
}

static void
bmp_decode_failure_does_not_abort (void)
{
    assert_href_paints_center (BMP_BAD_DATA_URI, 0xff00ff00);
}

static void
bmp_oversized_dimensions_does_not_abort (void)
{
    assert_href_paints_center (BMP_OVERSIZE_DIM_URI, 0xff00ff00);
}

static void
bmp_oversized_pixels_does_not_abort (void)
{
    assert_href_paints_center (BMP_OVERSIZE_PIXELS_URI, 0xff00ff00);
}

static void
bmp_bad_offbits_does_not_abort (void)
{
    assert_href_paints_center (BMP_BAD_OFFBITS_URI, 0xff00ff00);
}

static void
bmp_rle_oob_does_not_abort (void)
{
    assert_href_paints_center (BMP_RLE_OOB_URI, 0xff00ff00);
}
#else
static void
bmp_without_decoder_is_ignored (void)
{
    assert_href_paints_center (BMP_RED_DATA_URI, 0xff00ff00);
}
#endif

#ifdef HAVE_LIBAVIF
/* rectangle.avif is a 10×10 #7eff02 fill (rust avif-image-1003-ref). */
static void
assert_href_paints_avif_green (const char *href)
{
    char *svg;
    RsvgHandle *handle;
    cairo_surface_t *output;
    guint32 p;
    guint8 a, r, g, b;

    svg = g_strdup_printf (
        "<svg xmlns='http://www.w3.org/2000/svg' width='10' height='10'>"
        "<rect width='10' height='10' fill='#0000ff'/>"
        "<image href='%s' width='10' height='10'/>"
        "</svg>", href);
    handle = handle_from_svg_string (svg);
    g_free (svg);

    output = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 10, 10);
    render_to_surface (handle, output);
    p = pixel_argb (output, 5, 5);
    a = (guint8) (p >> 24);
    r = (guint8) (p >> 16);
    g = (guint8) (p >> 8);
    b = (guint8) p;
    g_assert_cmpint (a, ==, 0xff);
    g_assert_cmpint (r, >=, 0x60);
    g_assert_cmpint (g, >=, 0xe0);
    g_assert_cmpint (b, <=, 0x20);

    cairo_surface_destroy (output);
    g_object_unref (handle);
}

static void
avif_image_data_uri_renders (void)
{
    assert_href_paints_avif_green (AVIF_RECT_DATA_URI);
}

static void
avif_image_sniff_without_mime_renders (void)
{
    assert_href_paints_avif_green (AVIF_RECT_SNIFF_URI);
}

static void
avif_image_file_href_renders (void)
{
    RsvgHandle *handle = load_test_document ("avif-image.svg");
    cairo_surface_t *output = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 10, 10);
    guint32 p;
    guint8 a, r, g, b;

    render_to_surface (handle, output);
    p = pixel_argb (output, 5, 5);
    a = (guint8) (p >> 24);
    r = (guint8) (p >> 16);
    g = (guint8) (p >> 8);
    b = (guint8) p;
    g_assert_cmpint (a, ==, 0xff);
    g_assert_cmpint (r, >=, 0x60);
    g_assert_cmpint (g, >=, 0xe0);
    g_assert_cmpint (b, <=, 0x20);

    cairo_surface_destroy (output);
    g_object_unref (handle);
}

static void
avif_decode_failure_does_not_abort (void)
{
    assert_href_paints_center (AVIF_BAD_DATA_URI, 0xff00ff00);
}
#else
static void
avif_disabled_without_libavif (void)
{
    g_test_skip ("libavif not found (pkg-config libavif); AVIF in <image> stays off");
}
#endif

static void
pixbuf_png_image_data_uri_renders (void)
{
    /* Still named for the old pixbuf path; with libpng this is the
     * standalone decoder. Without libpng the image is omitted (green). */
#ifdef HAVE_LIBPNG
    assert_href_paints_center (PNG_RED_DATA_URI, 0xffff0000);
#else
    assert_href_paints_center (PNG_RED_DATA_URI, 0xff00ff00);
#endif
}

static void
pixbuf_png_image_file_href_renders (void)
{
    RsvgHandle *handle = load_test_document ("png-image.svg");
    cairo_surface_t *output = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 8, 8);

#ifdef HAVE_LIBPNG
    render_to_surface (handle, output);
    g_assert_cmphex (pixel_argb (output, 4, 4), ==, 0xffff0000);
#else
    render_to_surface (handle, output);
    g_assert_cmphex (pixel_argb (output, 4, 4), ==, 0xff00ff00);
#endif

    cairo_surface_destroy (output);
    g_object_unref (handle);
}

static void
render_unloaded_returns_error (void)
{
    RsvgHandle *handle = rsvg_handle_new ();
    cairo_surface_t *surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 10, 10);
    cairo_t *cr = cairo_create (surface);
    RsvgRectangle viewport = { 0, 0, 10, 10 };
    GError *error = NULL;

    g_assert_false (rsvg_handle_render_document (handle, cr, &viewport, &error));
    g_assert_nonnull (error);
    g_clear_error (&error);

    cairo_destroy (cr);
    cairo_surface_destroy (surface);
    g_object_unref (handle);
}

static void
oversized_number_list_does_not_abort (void)
{
    GString *vals;
    char *svg;
    RsvgHandle *handle;
    cairo_surface_t *output;
    int i;

    vals = g_string_new ("0");
    for (i = 1; i < 5000; i++)
        g_string_append (vals, " 0");

    svg = g_strdup_printf (
        "<svg xmlns='http://www.w3.org/2000/svg' width='8' height='8'>"
        "<filter id='f'>"
        "<feComponentTransfer>"
        "<feFuncR type='table' tableValues='%s'/>"
        "</feComponentTransfer>"
        "</filter>"
        "<rect width='8' height='8' fill='#00ff00' filter='url(#f)'/>"
        "</svg>", vals->str);
    g_string_free (vals, TRUE);

    handle = handle_from_svg_string (svg);
    g_free (svg);
    output = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 8, 8);
    render_to_surface (handle, output);
    g_assert_cmphex (pixel_argb (output, 4, 4) >> 24, ==, 0xff);

    cairo_surface_destroy (output);
    g_object_unref (handle);
}

static void
oversized_dasharray_does_not_abort (void)
{
    GString *dash;
    char *svg;
    RsvgHandle *handle;
    cairo_surface_t *output;
    int i;

    dash = g_string_new ("1");
    for (i = 1; i < 5000; i++)
        g_string_append (dash, ",1");

    svg = g_strdup_printf (
        "<svg xmlns='http://www.w3.org/2000/svg' width='8' height='8'>"
        "<rect width='8' height='8' fill='#00ff00'"
        " stroke='#0000ff' stroke-width='2' stroke-dasharray='%s'/>"
        "</svg>", dash->str);
    g_string_free (dash, TRUE);

    handle = handle_from_svg_string (svg);
    g_free (svg);
    output = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 8, 8);
    render_to_surface (handle, output);
    g_assert_cmphex (pixel_argb (output, 4, 4) >> 24, ==, 0xff);

    cairo_surface_destroy (output);
    g_object_unref (handle);
}

static void
color_matrix_20_values_still_applies (void)
{
    const char *svg =
        "<svg xmlns='http://www.w3.org/2000/svg' width='8' height='8'>"
        "<filter id='f'>"
        "<feColorMatrix type='matrix'"
        " values='1 0 0 0 0  0 1 0 0 0  0 0 1 0 0  0 0 0 1 0'/>"
        "</filter>"
        "<rect width='8' height='8' fill='#ff0000' filter='url(#f)'/>"
        "</svg>";
    RsvgHandle *handle = handle_from_svg_string (svg);
    cairo_surface_t *output = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 8, 8);

    render_to_surface (handle, output);
    g_assert_cmphex (pixel_argb (output, 4, 4), ==, 0xffff0000);

    cairo_surface_destroy (output);
    g_object_unref (handle);
}

static void
oversized_data_uri_does_not_abort (void)
{
    const gsize n = (gsize) 64 * 1024 * 1024 + 1;
    const char *pre =
        "<svg xmlns='http://www.w3.org/2000/svg' width='8' height='8'>"
        "<rect width='8' height='8' fill='#00ff00'/>"
        "<image href='data:text/plain,";
    const char *post = "' width='8' height='8'/></svg>";
    gsize pre_len = strlen (pre);
    gsize post_len = strlen (post);
    char *svg;
    RsvgHandle *handle;
    GError *error = NULL;
    cairo_surface_t *output;

    svg = g_try_malloc (pre_len + n + post_len + 1);
    g_assert_nonnull (svg);
    memcpy (svg, pre, pre_len);
    memset (svg + pre_len, 'A', n);
    memcpy (svg + pre_len + n, post, post_len);
    svg[pre_len + n + post_len] = '\0';

    handle = rsvg_handle_new_with_flags (RSVG_HANDLE_FLAG_UNLIMITED);
    g_assert_nonnull (handle);
    {
        gsize off = 0;
        gsize total = pre_len + n + post_len;
        gboolean ok = TRUE;

        while (off < total) {
            gsize chunk = total - off;
            if (chunk > 1024 * 1024)
                chunk = 1024 * 1024;
            if (!rsvg_handle_write (handle, (const guint8 *) svg + off, chunk, &error)) {
                ok = FALSE;
                break;
            }
            off += chunk;
        }
        if (ok && !rsvg_handle_close (handle, &error))
            ok = FALSE;
        g_free (svg);
        if (!ok) {
            /* XML layer may still refuse; must not abort. */
            g_test_message ("oversized data: load skipped: %s",
                            error ? error->message : "unknown");
            g_clear_error (&error);
            g_object_unref (handle);
            return;
        }
    }
    g_assert_no_error (error);

    output = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 8, 8);
    render_to_surface (handle, output);
    g_assert_cmphex (pixel_argb (output, 4, 4), ==, 0xff00ff00);

    cairo_surface_destroy (output);
    g_object_unref (handle);
}

#ifdef CONVERT_PROGRAM
static gchar *
convert_temp_png (void)
{
    gchar *path = NULL;
    gint fd;

    fd = g_file_open_tmp ("rsvg-convert-XXXXXX.png", &path, NULL);
    g_assert_cmpint (fd, >=, 0);
    close (fd);
    return path;
}

static void
convert_stylesheet_file (void)
{
    gchar *png = convert_temp_png ();
    gchar *svg = get_test_filename ("stylesheet.svg");
    gchar *css = get_test_filename ("stylesheet-green.css");
    cairo_surface_t *s;
    GError *error = NULL;
    gint status = -1;
    gchar *argv[] = {
        (gchar *) CONVERT_PROGRAM, "--stylesheet", css, "-o", png, svg, NULL
    };

    g_assert_true (g_spawn_sync (NULL, argv, NULL, G_SPAWN_DEFAULT,
                                 NULL, NULL, NULL, NULL, &status, &error));
    g_assert_no_error (error);
#ifndef G_OS_WIN32
    g_assert_true (g_spawn_check_exit_status (status, &error));
    g_assert_no_error (error);
#else
    g_assert_cmpint (status, ==, 0);
#endif

    s = cairo_image_surface_create_from_png (png);
    g_assert_cmpint (cairo_surface_status (s), ==, CAIRO_STATUS_SUCCESS);
    g_assert_cmphex (pixel_argb (s, 25, 40), ==, 0xff00ff00);
    cairo_surface_destroy (s);
    g_unlink (png);
    g_free (png);
    g_free (svg);
    g_free (css);
}

static void
convert_stdin_dash (void)
{
    gchar *png = convert_temp_png ();
    gchar *svg = get_test_filename ("stylesheet.svg");
    gchar *contents = NULL;
    gsize len = 0;
    cairo_surface_t *s;
    GError *error = NULL;
    GSubprocess *proc;
    GOutputStream *stdin_stream;
    GBytes *bytes;

    g_assert_true (g_file_get_contents (svg, &contents, &len, &error));
    g_assert_no_error (error);

    proc = g_subprocess_new (G_SUBPROCESS_FLAGS_STDIN_PIPE, &error,
                             CONVERT_PROGRAM, "-o", png, "-", NULL);
    g_assert_no_error (error);
    g_assert_nonnull (proc);

    stdin_stream = g_subprocess_get_stdin_pipe (proc);
    bytes = g_bytes_new_take (contents, len);
    contents = NULL;
    g_assert_true (g_output_stream_write_all (stdin_stream,
                                              g_bytes_get_data (bytes, NULL),
                                              g_bytes_get_size (bytes),
                                              NULL, NULL, &error));
    g_assert_no_error (error);
    g_output_stream_close (stdin_stream, NULL, NULL);
    g_bytes_unref (bytes);

    g_assert_true (g_subprocess_wait_check (proc, NULL, &error));
    g_assert_no_error (error);
    g_object_unref (proc);

    s = cairo_image_surface_create_from_png (png);
    g_assert_cmpint (cairo_surface_status (s), ==, CAIRO_STATUS_SUCCESS);
    /* Default black fill of the 30x40 rect. */
    g_assert_cmphex (pixel_argb (s, 25, 40), ==, 0xff000000);
    cairo_surface_destroy (s);
    g_unlink (png);
    g_free (png);
    g_free (svg);
}

static gchar *
convert_temp_pdf (void)
{
    gchar *path = NULL;
    gint fd;

    fd = g_file_open_tmp ("rsvg-convert-XXXXXX.pdf", &path, NULL);
    g_assert_cmpint (fd, >=, 0);
    close (fd);
    return path;
}

static void
convert_pdf_version (const char *format, const char *header)
{
    gchar *pdf = convert_temp_pdf ();
    gchar *svg = get_test_filename ("stylesheet.svg");
    gchar *contents = NULL;
    gsize len = 0;
    GError *error = NULL;
    gint status = -1;
    gchar *argv[] = {
        (gchar *) CONVERT_PROGRAM, "-f", (gchar *) format, "-o", pdf, svg, NULL
    };

    g_assert_true (g_spawn_sync (NULL, argv, NULL, G_SPAWN_DEFAULT,
                                 NULL, NULL, NULL, NULL, &status, &error));
    g_assert_no_error (error);
#ifndef G_OS_WIN32
    g_assert_true (g_spawn_check_exit_status (status, &error));
    g_assert_no_error (error);
#else
    g_assert_cmpint (status, ==, 0);
#endif

    g_assert_true (g_file_get_contents (pdf, &contents, &len, &error));
    g_assert_no_error (error);
    g_assert_cmpuint (len, >=, strlen (header));
    g_assert_true (g_str_has_prefix (contents, header));
    g_free (contents);
    g_unlink (pdf);
    g_free (pdf);
    g_free (svg);
}

static void
convert_pdf1_4 (void)
{
    convert_pdf_version ("pdf1.4", "%PDF-1.4");
}

static void
convert_pdf1_7 (void)
{
    convert_pdf_version ("pdf1.7", "%PDF-1.7");
}

static gboolean
convert_spawn (gchar **argv, gchar **stderr_text, gint *status, GError **error)
{
    return g_spawn_sync (NULL, argv, NULL, G_SPAWN_DEFAULT,
                         NULL, NULL, NULL, stderr_text, status, error);
}

static gchar **
convert_env_lang (const char *lang)
{
    gchar **env = g_get_environ ();

    env = g_environ_setenv (env, "LANG", lang, TRUE);
    env = g_environ_setenv (env, "LANGUAGE", lang, TRUE);
    env = g_environ_unsetenv (env, "LC_ALL");
    env = g_environ_unsetenv (env, "LC_MESSAGES");
    env = g_environ_unsetenv (env, "RSVG_CONVERT_ACCEPT_LANGUAGE");
    return env;
}

static gboolean
convert_spawn_env (gchar **argv, gchar **envp, gchar **stderr_text, gint *status, GError **error)
{
    return g_spawn_sync (NULL, argv, envp, G_SPAWN_DEFAULT,
                         NULL, NULL, NULL, stderr_text, status, error);
}

static void
convert_assert_success (gint status, GError *error)
{
    g_assert_no_error (error);
#ifndef G_OS_WIN32
    {
        GError *exit_error = NULL;

        g_assert_true (g_spawn_check_exit_status (status, &exit_error));
        g_assert_no_error (exit_error);
    }
#else
    g_assert_cmpint (status, ==, 0);
#endif
}

static void
convert_assert_failure (gint status, GError *error)
{
    g_assert_no_error (error);
#ifndef G_OS_WIN32
    {
        GError *exit_error = NULL;

        g_assert_false (g_spawn_check_exit_status (status, &exit_error));
        g_clear_error (&exit_error);
    }
#else
    g_assert_cmpint (status, !=, 0);
#endif
}

static void
convert_page_default_png (void)
{
    gchar *png = convert_temp_png ();
    gchar *svg = get_test_filename ("stylesheet.svg");
    cairo_surface_t *s;
    GError *error = NULL;
    gint status = -1;
    gchar *argv[] = {
        (gchar *) CONVERT_PROGRAM, "-o", png, svg, NULL
    };

    g_assert_true (convert_spawn (argv, NULL, &status, &error));
    convert_assert_success (status, error);

    s = cairo_image_surface_create_from_png (png);
    g_assert_cmpint (cairo_surface_status (s), ==, CAIRO_STATUS_SUCCESS);
    g_assert_cmpint (cairo_image_surface_get_width (s), ==, 100);
    g_assert_cmpint (cairo_image_surface_get_height (s), ==, 100);
    g_assert_cmphex (pixel_argb (s, 25, 40), ==, 0xff000000);
    cairo_surface_destroy (s);
    g_unlink (png);
    g_free (png);
    g_free (svg);
}

static void
convert_page_size_png (void)
{
    gchar *png = convert_temp_png ();
    gchar *svg = get_test_filename ("stylesheet.svg");
    cairo_surface_t *s;
    GError *error = NULL;
    gint status = -1;
    gchar *argv[] = {
        (gchar *) CONVERT_PROGRAM,
        "--page-width", "200",
        "--page-height", "150",
        "-o", png, svg, NULL
    };

    g_assert_true (convert_spawn (argv, NULL, &status, &error));
    convert_assert_success (status, error);

    s = cairo_image_surface_create_from_png (png);
    g_assert_cmpint (cairo_surface_status (s), ==, CAIRO_STATUS_SUCCESS);
    g_assert_cmpint (cairo_image_surface_get_width (s), ==, 200);
    g_assert_cmpint (cairo_image_surface_get_height (s), ==, 150);
    /* Zero offset: image sits at the origin; extra page is empty. */
    g_assert_cmphex (pixel_argb (s, 25, 40), ==, 0xff000000);
    g_assert_cmphex (pixel_argb (s, 180, 140), ==, 0x00000000);
    cairo_surface_destroy (s);
    g_unlink (png);
    g_free (png);
    g_free (svg);
}

static void
convert_page_offset_png (void)
{
    gchar *png = convert_temp_png ();
    gchar *svg = get_test_filename ("stylesheet.svg");
    cairo_surface_t *s;
    GError *error = NULL;
    gint status = -1;
    gchar *argv[] = {
        (gchar *) CONVERT_PROGRAM,
        "--page-width", "200",
        "--page-height", "150",
        "--left", "50",
        "--top", "30",
        "-o", png, svg, NULL
    };

    g_assert_true (convert_spawn (argv, NULL, &status, &error));
    convert_assert_success (status, error);

    s = cairo_image_surface_create_from_png (png);
    g_assert_cmpint (cairo_surface_status (s), ==, CAIRO_STATUS_SUCCESS);
    g_assert_cmpint (cairo_image_surface_get_width (s), ==, 200);
    g_assert_cmpint (cairo_image_surface_get_height (s), ==, 150);
    /* Rect at SVG (10,20) 30×40, shifted by left=50 top=30.
     * Interior sample (25,40) was black; it moves to (75,70). */
    g_assert_cmphex (pixel_argb (s, 25, 40), ==, 0x00000000);
    g_assert_cmphex (pixel_argb (s, 75, 70), ==, 0xff000000);
    cairo_surface_destroy (s);
    g_unlink (png);
    g_free (png);
    g_free (svg);
}

static const char *
find_bytes (const char *hay, gsize hay_len, const char *needle)
{
    gsize n = strlen (needle);
    gsize i;

    if (n == 0 || hay_len < n)
        return NULL;
    for (i = 0; i + n <= hay_len; i++) {
        if (memcmp (hay + i, needle, n) == 0)
            return hay + i;
    }
    return NULL;
}

static gboolean
pdf_mediabox_is (const char *pdf, gsize len, double w, double h)
{
    const char *p;
    const char *end;
    char buf[128];
    gsize remain;
    double x0, y0, x1, y1;

    p = find_bytes (pdf, len, "/MediaBox");
    if (p == NULL)
        return FALSE;
    end = pdf + len;
    while (p < end && *p != '[')
        p++;
    if (p >= end || *p != '[')
        return FALSE;
    p++;
    remain = (gsize) (end - p);
    if (remain >= sizeof (buf))
        remain = sizeof (buf) - 1;
    memcpy (buf, p, remain);
    buf[remain] = '\0';
    if (sscanf (buf, "%lf %lf %lf %lf", &x0, &y0, &x1, &y1) != 4)
        return FALSE;
    return x0 == 0.0 && y0 == 0.0
        && fabs (x1 - w) < 0.01 && fabs (y1 - h) < 0.01;
}

static void
convert_page_default_pdf (void)
{
    gchar *pdf = convert_temp_pdf ();
    gchar *svg = get_test_filename ("stylesheet.svg");
    gchar *contents = NULL;
    gsize len = 0;
    GError *error = NULL;
    gint status = -1;
    gchar *argv[] = {
        (gchar *) CONVERT_PROGRAM, "-f", "pdf1.4", "-o", pdf, svg, NULL
    };

    g_assert_true (convert_spawn (argv, NULL, &status, &error));
    convert_assert_success (status, error);

    g_assert_true (g_file_get_contents (pdf, &contents, &len, &error));
    g_assert_no_error (error);
    g_assert_true (g_str_has_prefix (contents, "%PDF-1.4"));
    /* Default media = rendered size (100×100 cairo units).
     * pdf1.4 keeps /MediaBox in the page dict (plain -f pdf may not). */
    g_assert_true (pdf_mediabox_is (contents, len, 100.0, 100.0));
    g_free (contents);
    g_unlink (pdf);
    g_free (pdf);
    g_free (svg);
}

static void
convert_page_size_pdf (void)
{
    gchar *pdf = convert_temp_pdf ();
    gchar *svg = get_test_filename ("stylesheet.svg");
    gchar *contents = NULL;
    gsize len = 0;
    GError *error = NULL;
    gint status = -1;
    gchar *argv[] = {
        (gchar *) CONVERT_PROGRAM,
        "-f", "pdf1.4",
        "--page-width", "200",
        "--page-height", "300",
        "-o", pdf, svg, NULL
    };

    g_assert_true (convert_spawn (argv, NULL, &status, &error));
    convert_assert_success (status, error);

    g_assert_true (g_file_get_contents (pdf, &contents, &len, &error));
    g_assert_no_error (error);
    g_assert_true (pdf_mediabox_is (contents, len, 200.0, 300.0));
    g_free (contents);
    g_unlink (pdf);
    g_free (pdf);
    g_free (svg);
}

static void
convert_page_width_only_fails (void)
{
    gchar *png = convert_temp_png ();
    gchar *svg = get_test_filename ("stylesheet.svg");
    gchar *err = NULL;
    GError *error = NULL;
    gint status = -1;
    gchar *argv[] = {
        (gchar *) CONVERT_PROGRAM,
        "--page-width", "200",
        "-o", png, svg, NULL
    };

    g_assert_true (convert_spawn (argv, &err, &status, &error));
    convert_assert_failure (status, error);
    g_assert_nonnull (err);
    g_assert_true (strstr (err, "page-width") != NULL);
    g_assert_true (strstr (err, "page-height") != NULL);
    g_free (err);
    g_unlink (png);
    g_free (png);
    g_free (svg);
}

static void
convert_page_negative_fails (void)
{
    gchar *png = convert_temp_png ();
    gchar *svg = get_test_filename ("stylesheet.svg");
    gchar *err = NULL;
    GError *error = NULL;
    gint status = -1;
    gchar *argv[] = {
        (gchar *) CONVERT_PROGRAM,
        "--page-width", "-10",
        "--page-height", "100",
        "-o", png, svg, NULL
    };

    g_assert_true (convert_spawn (argv, &err, &status, &error));
    convert_assert_failure (status, error);
    g_assert_nonnull (err);
    g_assert_true (strstr (err, "page-width") != NULL);
    g_free (err);
    g_unlink (png);
    g_free (png);
    g_free (svg);
}

static void
convert_width_bare_100 (void)
{
    gchar *png = convert_temp_png ();
    gchar *svg = get_test_filename ("stylesheet.svg");
    cairo_surface_t *s;
    GError *error = NULL;
    gint status = -1;
    gchar *argv[] = {
        (gchar *) CONVERT_PROGRAM, "-w", "100", "-o", png, svg, NULL
    };

    g_assert_true (convert_spawn (argv, NULL, &status, &error));
    convert_assert_success (status, error);

    s = cairo_image_surface_create_from_png (png);
    g_assert_cmpint (cairo_surface_status (s), ==, CAIRO_STATUS_SUCCESS);
    g_assert_cmpint (cairo_image_surface_get_width (s), ==, 100);
    g_assert_cmpint (cairo_image_surface_get_height (s), ==, 100);
    g_assert_cmphex (pixel_argb (s, 25, 40), ==, 0xff000000);
    cairo_surface_destroy (s);
    g_unlink (png);
    g_free (png);
    g_free (svg);
}

static void
convert_width_2in_png (void)
{
    gchar *png = convert_temp_png ();
    gchar *svg = get_test_filename ("stylesheet.svg");
    cairo_surface_t *s;
    GError *error = NULL;
    gint status = -1;
    gchar *argv[] = {
        (gchar *) CONVERT_PROGRAM, "-w", "2in", "-h", "2in", "-o", png, svg, NULL
    };

    g_assert_true (convert_spawn (argv, NULL, &status, &error));
    convert_assert_success (status, error);

    s = cairo_image_surface_create_from_png (png);
    g_assert_cmpint (cairo_surface_status (s), ==, CAIRO_STATUS_SUCCESS);
    /* 2in at default 96dpi = 192px. */
    g_assert_cmpint (cairo_image_surface_get_width (s), ==, 192);
    g_assert_cmpint (cairo_image_surface_get_height (s), ==, 192);
    cairo_surface_destroy (s);
    g_unlink (png);
    g_free (png);
    g_free (svg);
}

static void
convert_height_50mm_png (void)
{
    gchar *png = convert_temp_png ();
    gchar *svg = get_test_filename ("stylesheet.svg");
    cairo_surface_t *s;
    GError *error = NULL;
    gint status = -1;
    int expect;
    gchar *argv[] = {
        (gchar *) CONVERT_PROGRAM, "-h", "50mm", "-o", png, svg, NULL
    };

    expect = (int) ceil (50.0 / 25.4 * 96.0);

    g_assert_true (convert_spawn (argv, NULL, &status, &error));
    convert_assert_success (status, error);

    s = cairo_image_surface_create_from_png (png);
    g_assert_cmpint (cairo_surface_status (s), ==, CAIRO_STATUS_SUCCESS);
    g_assert_cmpint (cairo_image_surface_get_height (s), ==, expect);
    g_assert_cmpint (cairo_image_surface_get_width (s), ==, expect);
    cairo_surface_destroy (s);
    g_unlink (png);
    g_free (png);
    g_free (svg);
}

static void
convert_width_2in_pdf (void)
{
    gchar *pdf = convert_temp_pdf ();
    gchar *svg = get_test_filename ("stylesheet.svg");
    gchar *contents = NULL;
    gsize len = 0;
    GError *error = NULL;
    gint status = -1;
    gchar *argv[] = {
        (gchar *) CONVERT_PROGRAM,
        "-f", "pdf1.4",
        "-w", "2in",
        "-h", "1in",
        "-o", pdf, svg, NULL
    };

    g_assert_true (convert_spawn (argv, NULL, &status, &error));
    convert_assert_success (status, error);

    g_assert_true (g_file_get_contents (pdf, &contents, &len, &error));
    g_assert_no_error (error);
    /* 2in = 144pt, 1in = 72pt (rust to_points). */
    g_assert_true (pdf_mediabox_is (contents, len, 144.0, 72.0));
    g_free (contents);
    g_unlink (pdf);
    g_free (pdf);
    g_free (svg);
}

static void
convert_height_50mm_pdf (void)
{
    gchar *pdf = convert_temp_pdf ();
    gchar *svg = get_test_filename ("stylesheet.svg");
    gchar *contents = NULL;
    gsize len = 0;
    GError *error = NULL;
    gint status = -1;
    double expect;
    gchar *argv[] = {
        (gchar *) CONVERT_PROGRAM,
        "-f", "pdf1.4",
        "-w", "50mm",
        "-h", "50mm",
        "-o", pdf, svg, NULL
    };

    expect = 50.0 / 25.4 * 72.0;

    g_assert_true (convert_spawn (argv, NULL, &status, &error));
    convert_assert_success (status, error);

    g_assert_true (g_file_get_contents (pdf, &contents, &len, &error));
    g_assert_no_error (error);
    g_assert_true (pdf_mediabox_is (contents, len, expect, expect));
    g_free (contents);
    g_unlink (pdf);
    g_free (pdf);
    g_free (svg);
}

static void
convert_width_bad_unit_fails (void)
{
    gchar *png = convert_temp_png ();
    gchar *svg = get_test_filename ("stylesheet.svg");
    gchar *err = NULL;
    GError *error = NULL;
    gint status = -1;
    gchar *argv[] = {
        (gchar *) CONVERT_PROGRAM, "-w", "200ex", "-o", png, svg, NULL
    };

    g_assert_true (convert_spawn (argv, &err, &status, &error));
    convert_assert_failure (status, error);
    g_assert_nonnull (err);
    g_assert_true (strstr (err, "supported units") != NULL);
    g_free (err);
    g_unlink (png);
    g_free (png);
    g_free (svg);
}

static cairo_surface_t *
convert_accept_language_png (const char *lang_env, const char *accept_opt)
{
    gchar *png = convert_temp_png ();
    gchar *svg = get_test_filename ("accept-language.svg");
    gchar **env = convert_env_lang (lang_env);
    cairo_surface_t *s;
    GError *error = NULL;
    gint status = -1;
    gchar *argv_with_l[] = {
        (gchar *) CONVERT_PROGRAM, "-l", (gchar *) accept_opt, "-o", png, svg, NULL
    };
    gchar *argv_no_l[] = {
        (gchar *) CONVERT_PROGRAM, "-o", png, svg, NULL
    };
    gchar **argv = accept_opt ? argv_with_l : argv_no_l;

    g_assert_true (convert_spawn_env (argv, env, NULL, &status, &error));
    convert_assert_success (status, error);
    g_strfreev (env);

    s = cairo_image_surface_create_from_png (png);
    g_assert_cmpint (cairo_surface_status (s), ==, CAIRO_STATUS_SUCCESS);
    g_unlink (png);
    g_free (png);
    g_free (svg);
    return s;
}

static void
convert_accept_language_default_en (void)
{
    cairo_surface_t *s = convert_accept_language_png ("en_US.UTF-8", NULL);

    /* Host en: en branch (green), not de. */
    g_assert_cmphex (pixel_argb (s, 10, 10), ==, 0xff00ff00);
    cairo_surface_destroy (s);
}

static void
convert_accept_language_de (void)
{
    cairo_surface_t *s = convert_accept_language_png ("en_US.UTF-8", "de");

    /* -l de overrides LANG=en: de branch (red). */
    g_assert_cmphex (pixel_argb (s, 10, 10), ==, 0xffff0000);
    cairo_surface_destroy (s);
}

static void
convert_accept_language_list (void)
{
    cairo_surface_t *s = convert_accept_language_png ("en_US.UTF-8", "fr,de,en");

    /* First switch child that matches the list is de. */
    g_assert_cmphex (pixel_argb (s, 10, 10), ==, 0xffff0000);
    cairo_surface_destroy (s);
}

static void
convert_accept_language_invalid_fails (void)
{
    gchar *png = convert_temp_png ();
    gchar *svg = get_test_filename ("accept-language.svg");
    gchar *err = NULL;
    GError *error = NULL;
    gint status = -1;
    gchar *argv[] = {
        (gchar *) CONVERT_PROGRAM, "-l", "foo_bar", "-o", png, svg, NULL
    };

    g_assert_true (convert_spawn (argv, &err, &status, &error));
    convert_assert_failure (status, error);
    g_assert_nonnull (err);
    g_assert_true (strstr (err, "invalid language tag") != NULL);
    g_free (err);
    g_unlink (png);
    g_free (png);
    g_free (svg);
}

static void
convert_assert_license_notice (const char *text)
{
    g_assert_nonnull (text);
    g_assert_true (strstr (text, "rsvg-convert " VERSION) != NULL);
    g_assert_true (strstr (text, "Copyright (C) 1999-2026") != NULL);
    g_assert_true (strstr (text, "2001-2026") == NULL);
    g_assert_true (strstr (text, "GPL-2.0-only") != NULL);
    g_assert_true (strstr (text, "LGPL-2.1") == NULL);
    g_assert_true (strstr (text, "NO WARRANTY") != NULL);
    g_assert_true (strstr (text, "Gnomovision") == NULL);
}

static void
convert_version_license_notice (void)
{
    gchar *out = NULL;
    GError *error = NULL;
    gint status = -1;
    gchar *argv[] = {
        (gchar *) CONVERT_PROGRAM, "--version", NULL
    };

    g_assert_true (g_spawn_sync (NULL, argv, NULL, G_SPAWN_DEFAULT,
                                 NULL, NULL, &out, NULL, &status, &error));
    convert_assert_success (status, error);
    convert_assert_license_notice (out);
    g_free (out);
}

static void
convert_help_license_notice (void)
{
    gchar *out = NULL;
    GError *error = NULL;
    gint status = -1;
    gchar *argv[] = {
        (gchar *) CONVERT_PROGRAM, "--help", NULL
    };

    g_assert_true (g_spawn_sync (NULL, argv, NULL, G_SPAWN_DEFAULT,
                                 NULL, NULL, &out, NULL, &status, &error));
    convert_assert_success (status, error);
    convert_assert_license_notice (out);
    g_free (out);
}

static void
convert_stdin_pipe_no_tty_warning (void)
{
    gchar *png = convert_temp_png ();
    gchar *svg = get_test_filename ("stylesheet.svg");
    gchar *contents = NULL;
    gsize len = 0;
    gchar *err = NULL;
    GError *error = NULL;
    GSubprocess *proc;
    GBytes *input;
    GBytes *stderr_bytes = NULL;

    g_assert_true (g_file_get_contents (svg, &contents, &len, &error));
    g_assert_no_error (error);
    input = g_bytes_new_take (contents, len);

    proc = g_subprocess_new (G_SUBPROCESS_FLAGS_STDIN_PIPE
                             | G_SUBPROCESS_FLAGS_STDERR_PIPE,
                             &error,
                             CONVERT_PROGRAM, "-o", png, "-", NULL);
    g_assert_no_error (error);
    g_assert_true (g_subprocess_communicate (proc, input, NULL, NULL,
                                             &stderr_bytes, &error));
    g_assert_no_error (error);
    g_assert_true (g_subprocess_wait_check (proc, NULL, &error));
    g_assert_no_error (error);
    g_bytes_unref (input);
    g_object_unref (proc);

    if (stderr_bytes != NULL) {
        gsize elen = 0;
        err = g_strndup (g_bytes_get_data (stderr_bytes, &elen), elen);
        g_bytes_unref (stderr_bytes);
    }
    g_assert_true (err == NULL || strstr (err, "Reading from standard input") == NULL);
    g_free (err);
    g_unlink (png);
    g_free (png);
    g_free (svg);
}
#endif

int
main (int argc, char **argv)
{
    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/api/handle_has_correct_type_info", handle_has_correct_type_info);
    g_test_add_func ("/api/public_types_exist", public_types_exist);
    g_test_add_func ("/api/flags_registration", flags_registration);
    g_test_add_func ("/api/error_registration", error_registration);
    g_test_add_func ("/api/noops", noops);
    g_test_add_func ("/api/set_dpi", set_dpi);
    g_test_add_func ("/api/default_dpi_is_96", default_dpi_is_96);
    g_test_add_func ("/api/handle_has_sub", handle_has_sub);
    g_test_add_func ("/api/handle_new_from_data", handle_new_from_data);
    g_test_add_func ("/api/render_cairo_sub", render_cairo_sub);
    g_test_add_func ("/api/handle_get_pixbuf", handle_get_pixbuf);
    g_test_add_func ("/api/library_version_defines", library_version_defines);
    g_test_add_func ("/api/library_version_check", library_version_check);
    g_test_add_func ("/api/library_version_constants", library_version_constants);
    g_test_add_func ("/api/compat_features_header", compat_features_header);
    g_test_add_func ("/api/get_intrinsic_dimensions", get_intrinsic_dimensions);
    g_test_add_func ("/api/get_intrinsic_dimensions_missing_values", get_intrinsic_dimensions_missing_values);
    g_test_add_func ("/api/get_intrinsic_size_in_pixels/yes", get_intrinsic_size_in_pixels_yes);
    g_test_add_func ("/api/get_intrinsic_size_in_pixels/no", get_intrinsic_size_in_pixels_no);
    g_test_add_func ("/api/render_document", render_document);
    g_test_add_func ("/api/get_geometry_for_layer", get_geometry_for_layer);
    g_test_add_func ("/api/render_layer", render_layer);
    g_test_add_func ("/api/untransformed_element", untransformed_element);
    g_test_add_func ("/api/get_pixbuf_and_error", get_pixbuf_and_error);
    g_test_add_func ("/api/set_stylesheet", set_stylesheet);
    g_test_add_func ("/api/set_stylesheet_before_load", set_stylesheet_before_load);
    g_test_add_func ("/api/set_stylesheet_oversized_does_not_abort", set_stylesheet_oversized_does_not_abort);
    g_test_add_func ("/api/set_stylesheet_many_rules_does_not_abort", set_stylesheet_many_rules_does_not_abort);
    g_test_add_func ("/api/set_stylesheet_current_color", set_stylesheet_current_color);
    g_test_add_func ("/api/set_stylesheet_context_fill", set_stylesheet_context_fill);
    g_test_add_func ("/api/context_fill_from_use", context_fill_from_use);
    g_test_add_func ("/api/set_stylesheet_invalid_utf8", set_stylesheet_invalid_utf8);
    g_test_add_func ("/api/set_stylesheet_ignores_file_import", set_stylesheet_ignores_file_import);
    g_test_add_func ("/api/css_deep_not_does_not_abort", css_deep_not_does_not_abort);
    g_test_add_func ("/api/css_long_combinator_does_not_abort", css_long_combinator_does_not_abort);
    g_test_add_func ("/api/css_huge_nth_does_not_abort", css_huge_nth_does_not_abort);
    g_test_add_func ("/api/css_long_attr_selector_does_not_abort", css_long_attr_selector_does_not_abort);
    g_test_add_func ("/api/css_style_http_import_does_not_abort", css_style_http_import_does_not_abort);
    g_test_add_func ("/api/css_deep_decl_parens_does_not_abort", css_deep_decl_parens_does_not_abort);
    g_test_add_func ("/api/css_stray_close_brace_does_not_abort", css_stray_close_brace_does_not_abort);
    g_test_add_func ("/api/css_deep_tree_does_not_abort", css_deep_tree_does_not_abort);
    g_test_add_func ("/api/href_use", href_use);
    g_test_add_func ("/api/xlink_href_use_still_works", xlink_href_use_still_works);
    g_test_add_func ("/api/href_overrides_xlink_href", href_overrides_xlink_href);
    g_test_add_func ("/api/paint_order_stroke_then_fill", paint_order_stroke_then_fill);
    g_test_add_func ("/api/mix_blend_mode_multiply", mix_blend_mode_multiply);
    g_test_add_func ("/api/mask_type_alpha", mask_type_alpha);
    g_test_add_func ("/api/vector_effect_non_scaling_stroke", vector_effect_non_scaling_stroke);
    g_test_add_func ("/api/remote_image_href_is_ignored", remote_image_href_is_ignored);
    g_test_add_func ("/api/set_cancellable_for_rendering", set_cancellable_for_rendering);
    g_test_add_func ("/api/file_url_query_is_denied", file_url_query_is_denied);
    g_test_add_func ("/api/href_path_traversal_is_denied", href_path_traversal_is_denied);
#ifdef HAVE_LIBWEBP
    g_test_add_func ("/api/webp_image_data_uri_renders", webp_image_data_uri_renders);
    g_test_add_func ("/api/webp_image_sniff_without_mime_renders", webp_image_sniff_without_mime_renders);
    g_test_add_func ("/api/webp_image_file_href_renders", webp_image_file_href_renders);
    g_test_add_func ("/api/webp_decode_failure_does_not_abort", webp_decode_failure_does_not_abort);
#else
    g_test_add_func ("/api/webp_without_libwebp_is_ignored", webp_without_libwebp_is_ignored);
#endif
#ifdef HAVE_LIBPNG
    g_test_add_func ("/api/png_image_data_uri_renders", png_image_data_uri_renders);
    g_test_add_func ("/api/png_image_sniff_without_mime_renders", png_image_sniff_without_mime_renders);
    g_test_add_func ("/api/png_image_file_href_renders", png_image_file_href_renders);
    g_test_add_func ("/api/png_decode_failure_does_not_abort", png_decode_failure_does_not_abort);
    g_test_add_func ("/api/png_oversized_dimensions_does_not_abort", png_oversized_dimensions_does_not_abort);
#else
    g_test_add_func ("/api/png_without_libpng_is_ignored", png_without_libpng_is_ignored);
#endif
#ifdef HAVE_LIBJPEG
    g_test_add_func ("/api/jpeg_image_data_uri_renders", jpeg_image_data_uri_renders);
    g_test_add_func ("/api/jpeg_image_sniff_without_mime_renders", jpeg_image_sniff_without_mime_renders);
    g_test_add_func ("/api/jpeg_image_file_href_renders", jpeg_image_file_href_renders);
    g_test_add_func ("/api/jpeg_decode_failure_does_not_abort", jpeg_decode_failure_does_not_abort);
#else
    g_test_add_func ("/api/jpeg_without_libjpeg_is_ignored", jpeg_without_libjpeg_is_ignored);
#endif
#ifdef HAVE_LIBGIF
    g_test_add_func ("/api/gif_image_data_uri_renders", gif_image_data_uri_renders);
    g_test_add_func ("/api/gif_image_sniff_without_mime_renders", gif_image_sniff_without_mime_renders);
    g_test_add_func ("/api/gif_image_file_href_renders", gif_image_file_href_renders);
    g_test_add_func ("/api/gif_decode_failure_does_not_abort", gif_decode_failure_does_not_abort);
#else
    g_test_add_func ("/api/gif_without_libgif_is_ignored", gif_without_libgif_is_ignored);
#endif
#ifdef HAVE_BMP
    g_test_add_func ("/api/bmp_image_data_uri_renders", bmp_image_data_uri_renders);
    g_test_add_func ("/api/bmp_image_sniff_without_mime_renders", bmp_image_sniff_without_mime_renders);
    g_test_add_func ("/api/bmp_image_file_href_renders", bmp_image_file_href_renders);
    g_test_add_func ("/api/bmp_decode_failure_does_not_abort", bmp_decode_failure_does_not_abort);
    g_test_add_func ("/api/bmp_oversized_dimensions_does_not_abort", bmp_oversized_dimensions_does_not_abort);
    g_test_add_func ("/api/bmp_oversized_pixels_does_not_abort", bmp_oversized_pixels_does_not_abort);
    g_test_add_func ("/api/bmp_bad_offbits_does_not_abort", bmp_bad_offbits_does_not_abort);
    g_test_add_func ("/api/bmp_rle_oob_does_not_abort", bmp_rle_oob_does_not_abort);
#else
    g_test_add_func ("/api/bmp_without_decoder_is_ignored", bmp_without_decoder_is_ignored);
#endif
#ifdef HAVE_LIBAVIF
    g_test_add_func ("/api/avif_image_data_uri_renders", avif_image_data_uri_renders);
    g_test_add_func ("/api/avif_image_sniff_without_mime_renders", avif_image_sniff_without_mime_renders);
    g_test_add_func ("/api/avif_image_file_href_renders", avif_image_file_href_renders);
    g_test_add_func ("/api/avif_decode_failure_does_not_abort", avif_decode_failure_does_not_abort);
#else
    g_test_add_func ("/api/avif_disabled_without_libavif", avif_disabled_without_libavif);
#endif
    g_test_add_func ("/api/pixbuf_png_image_data_uri_renders", pixbuf_png_image_data_uri_renders);
    g_test_add_func ("/api/pixbuf_png_image_file_href_renders", pixbuf_png_image_file_href_renders);
    g_test_add_func ("/api/render_unloaded_returns_error", render_unloaded_returns_error);
    g_test_add_func ("/api/oversized_number_list_does_not_abort", oversized_number_list_does_not_abort);
    g_test_add_func ("/api/oversized_dasharray_does_not_abort", oversized_dasharray_does_not_abort);
    g_test_add_func ("/api/color_matrix_20_values_still_applies", color_matrix_20_values_still_applies);
    g_test_add_func ("/api/oversized_data_uri_does_not_abort", oversized_data_uri_does_not_abort);
#ifdef CONVERT_PROGRAM
    g_test_add_func ("/convert/stylesheet", convert_stylesheet_file);
    g_test_add_func ("/convert/stdin_dash", convert_stdin_dash);
    g_test_add_func ("/convert/pdf1.4", convert_pdf1_4);
    g_test_add_func ("/convert/pdf1.7", convert_pdf1_7);
    g_test_add_func ("/convert/page_default_png", convert_page_default_png);
    g_test_add_func ("/convert/page_size_png", convert_page_size_png);
    g_test_add_func ("/convert/page_offset_png", convert_page_offset_png);
    g_test_add_func ("/convert/page_default_pdf", convert_page_default_pdf);
    g_test_add_func ("/convert/page_size_pdf", convert_page_size_pdf);
    g_test_add_func ("/convert/page_width_only_fails", convert_page_width_only_fails);
    g_test_add_func ("/convert/page_negative_fails", convert_page_negative_fails);
    g_test_add_func ("/convert/width_bare_100", convert_width_bare_100);
    g_test_add_func ("/convert/width_2in_png", convert_width_2in_png);
    g_test_add_func ("/convert/height_50mm_png", convert_height_50mm_png);
    g_test_add_func ("/convert/width_2in_pdf", convert_width_2in_pdf);
    g_test_add_func ("/convert/height_50mm_pdf", convert_height_50mm_pdf);
    g_test_add_func ("/convert/width_bad_unit_fails", convert_width_bad_unit_fails);
    g_test_add_func ("/convert/accept_language_default_en", convert_accept_language_default_en);
    g_test_add_func ("/convert/accept_language_de", convert_accept_language_de);
    g_test_add_func ("/convert/accept_language_list", convert_accept_language_list);
    g_test_add_func ("/convert/accept_language_invalid_fails", convert_accept_language_invalid_fails);
    g_test_add_func ("/convert/version_license_notice", convert_version_license_notice);
    g_test_add_func ("/convert/help_license_notice", convert_help_license_notice);
    g_test_add_func ("/convert/stdin_pipe_no_tty_warning", convert_stdin_pipe_no_tty_warning);
#endif

    return g_test_run ();
}
