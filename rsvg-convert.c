/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 sts=4 ts=4 expandtab: */
/*

   rsvg-convert.c: Command line utility for exercising rsvg with cairo.
 
   Copyright (C) 2005 Red Hat, Inc.
   Copyright (C) 2005 Dom Lachowicz <cinamod@hotmail.com>
   Copyright (C) 2005 Caleb Moore <c.moore@student.unsw.edu.au>
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
  
   Authors: Carl Worth <cworth@cworth.org>, 
            Caleb Moore <c.moore@student.unsw.edu.au>,
            Dom Lachowicz <cinamod@hotmail.com>
*/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <locale.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#ifdef G_OS_UNIX
#include <unistd.h>
#include <gio/gunixinputstream.h>
#endif
#ifdef G_OS_WIN32
#include <io.h>
#endif

#ifdef G_OS_WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <gio/gwin32inputstream.h>
#endif

#include "rsvg-css.h"
#include "rsvg.h"
#include "rsvg-compat.h"
#include "rsvg-size-callback.h"

#ifdef CAIRO_HAS_PS_SURFACE
#include <cairo-ps.h>
#endif

#ifdef CAIRO_HAS_PDF_SURFACE
#include <cairo-pdf.h>
#endif

#ifdef CAIRO_HAS_SVG_SURFACE
#include <cairo-svg.h>
#endif

#ifdef CAIRO_HAS_XML_SURFACE
#include <cairo-xml.h>
#endif

static void
display_error (GError * err)
{
    if (err) {
        g_printerr ("%s\n", err->message);
        g_error_free (err);
    }
}

/* Short notice only. Full text is LICENCE (GPL-2.0-only per COPYING). */
static void
convert_print_license_notice (FILE *out)
{
    fprintf (out,
             "rsvg-convert %s\n"
             "Copyright (C) 1999-2026 The librsvg C library authors\n"
             "License: GNU GPL-2.0-only\n"
             "This is free software; you are welcome to change and redistribute it.\n"
             "There is NO WARRANTY, to the extent permitted by law.\n",
             VERSION);
}

static gboolean
convert_stdin_is_tty (void)
{
#ifdef G_OS_WIN32
    return _isatty (_fileno (stdin));
#else
    return isatty (STDIN_FILENO);
#endif
}

static void
convert_print_stdin_tty_notice (void)
{
    convert_print_license_notice (stderr);
    fputc ('\n', stderr);
    fputs ("Reading from standard input.\n", stderr);
    fputs ("Type Control-C to exit if this is not what you expected.\n", stderr);
}

static cairo_status_t
rsvg_cairo_write_func (void *closure, const unsigned char *data, unsigned int length)
{
    if (fwrite (data, 1, length, (FILE *) closure) == length)
        return CAIRO_STATUS_SUCCESS;
    return CAIRO_STATUS_WRITE_ERROR;
}

#ifdef CAIRO_HAS_PDF_SURFACE
static gboolean
convert_is_pdf_format (const char *format)
{
    return format != NULL
        && (g_str_equal (format, "pdf")
            || g_str_equal (format, "pdf1.4")
            || g_str_equal (format, "pdf1.5")
            || g_str_equal (format, "pdf1.6")
            || g_str_equal (format, "pdf1.7"));
}

static gboolean
convert_pdf_restrict_version (cairo_surface_t *surface,
                              const char *format,
                              GError **error)
{
    cairo_pdf_version_t want;
    const cairo_pdf_version_t *vers = NULL;
    int n_vers = 0, i;
    cairo_status_t st;

    if (format == NULL || g_str_equal (format, "pdf"))
        return TRUE;

    if (g_str_equal (format, "pdf1.4"))
        want = CAIRO_PDF_VERSION_1_4;
    else if (g_str_equal (format, "pdf1.5"))
        want = CAIRO_PDF_VERSION_1_5;
#if CAIRO_VERSION >= CAIRO_VERSION_ENCODE (1, 18, 0)
    else if (g_str_equal (format, "pdf1.6"))
        want = CAIRO_PDF_VERSION_1_6;
    else if (g_str_equal (format, "pdf1.7"))
        want = CAIRO_PDF_VERSION_1_7;
#endif
    else {
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                     _("PDF version “%s” is not supported by this Cairo"), format);
        return FALSE;
    }

    cairo_pdf_get_versions (&vers, &n_vers);
    for (i = 0; i < n_vers; i++) {
        if (vers[i] == want)
            break;
    }
    if (i >= n_vers) {
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                     _("PDF version “%s” is not supported by this Cairo"), format);
        return FALSE;
    }

    cairo_pdf_surface_restrict_to_version (surface, want);
    st = cairo_surface_status (surface);
    if (st != CAIRO_STATUS_SUCCESS) {
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                     "%s", cairo_status_to_string (st));
        return FALSE;
    }
    return TRUE;
}
#else
static gboolean
convert_is_pdf_format (const char *format)
{
    return format != NULL && g_str_equal (format, "pdf");
}
#endif

static char *
get_lookup_id_from_command_line (const char *lookup_id)
{
    char *export_lookup_id;

    if (lookup_id == NULL)
        export_lookup_id = NULL;
    else {
        /* rsvg_handle_has_sub() and rsvg_defs_lookup() expect ids to have a
         * '#' prepended to them, so they can lookup ids in externs like
         * "subfile.svg#subid".  For the user's convenience, we include this
         * '#' automatically; we only support specifying ids from the
         * toplevel, and don't expect users to lookup things in externs.
         */
        export_lookup_id = g_strdup_printf ("#%s", lookup_id);
    }

    return export_lookup_id;
}

static gboolean
convert_get_size (RsvgHandle *rsvg, const char *id,
                  double *out_w, double *out_h, GError **error)
{
    RsvgRectangle ink;

    if (id == NULL) {
        if (rsvg_handle_get_intrinsic_size_in_pixels (rsvg, out_w, out_h)
            && *out_w > 0 && *out_h > 0)
            return TRUE;
    }

    if (!rsvg_handle_get_geometry_for_element (rsvg, id, &ink, NULL, error))
        return FALSE;

    *out_w = ink.width;
    *out_h = ink.height;
    return TRUE;
}

/* Convert-local CSS length (absolute units rust accepts on -w/-h).
 * Not a new library symbol. Bare number = CONVERT_UNIT_NONE. */
typedef enum {
    CONVERT_UNIT_NONE = 0,
    CONVERT_UNIT_PX,
    CONVERT_UNIT_PT,
    CONVERT_UNIT_PC,
    CONVERT_UNIT_IN,
    CONVERT_UNIT_CM,
    CONVERT_UNIT_MM
} ConvertLengthUnit;

#define CONVERT_POINTS_PER_INCH 72.0
#define CONVERT_CM_PER_INCH     2.54
#define CONVERT_MM_PER_INCH     25.4
#define CONVERT_PICA_PER_INCH   6.0

static gboolean
convert_parse_length (const char *str,
                      const char *opt,
                      gboolean allow_negative,
                      gboolean allow_physical,
                      double *out_value,
                      ConvertLengthUnit *out_unit,
                      GError **error)
{
    const char *p;
    char *end = NULL;
    double v;
    ConvertLengthUnit unit = CONVERT_UNIT_NONE;

    if (str == NULL) {
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     _("Invalid %s: missing value"), opt);
        return FALSE;
    }

    p = str;
    while (*p == ' ' || *p == '\t')
        p++;

    v = g_ascii_strtod (p, &end);
    if (end == p) {
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     _("Invalid %s: “%s” cannot be parsed as a length"), opt, str);
        return FALSE;
    }

    while (*end == ' ' || *end == '\t')
        end++;

    if (*end == '\0') {
        unit = CONVERT_UNIT_NONE;
    } else if (g_ascii_strncasecmp (end, "px", 2) == 0) {
        unit = CONVERT_UNIT_PX;
        end += 2;
    } else if (g_ascii_strncasecmp (end, "pt", 2) == 0) {
        unit = CONVERT_UNIT_PT;
        end += 2;
    } else if (g_ascii_strncasecmp (end, "pc", 2) == 0) {
        unit = CONVERT_UNIT_PC;
        end += 2;
    } else if (g_ascii_strncasecmp (end, "in", 2) == 0) {
        unit = CONVERT_UNIT_IN;
        end += 2;
    } else if (g_ascii_strncasecmp (end, "cm", 2) == 0) {
        unit = CONVERT_UNIT_CM;
        end += 2;
    } else if (g_ascii_strncasecmp (end, "mm", 2) == 0) {
        unit = CONVERT_UNIT_MM;
        end += 2;
    } else {
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     _("Invalid %s “%s”: supported units are px, in, cm, mm, pt, pc"),
                     opt, str);
        return FALSE;
    }

    while (*end == ' ' || *end == '\t')
        end++;
    if (*end != '\0') {
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     _("Invalid %s: “%s” cannot be parsed as a length"), opt, str);
        return FALSE;
    }

    if (!allow_physical && unit != CONVERT_UNIT_NONE && unit != CONVERT_UNIT_PX) {
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     _("Invalid %s: CSS length units are not supported; use a pixel number (got “%s”)"),
                     opt, str);
        return FALSE;
    }

    if (!isfinite (v)) {
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     _("Invalid %s: “%s” is not a finite number"), opt, str);
        return FALSE;
    }

    if (!allow_negative && v < 0.0) {
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     _("Invalid %s: value must be non-negative"), opt);
        return FALSE;
    }

    *out_value = v;
    *out_unit = unit;
    return TRUE;
}

/* Page flags: pixel numbers only (optional px). */
static gboolean
convert_parse_px_number (const char *str,
                         const char *opt,
                         gboolean allow_negative,
                         double *out,
                         GError **error)
{
    ConvertLengthUnit unit;

    if (!convert_parse_length (str, opt, allow_negative, FALSE, out, &unit, error))
        return FALSE;
    return TRUE;
}

/* PNG/SVG: CSS px at DPI. Print: rust to_points (bare stays cairo user units). */
static double
convert_resolve_length (double v, ConvertLengthUnit unit, double dpi, gboolean to_points)
{
    if (!to_points) {
        switch (unit) {
        case CONVERT_UNIT_NONE:
        case CONVERT_UNIT_PX:
            return v;
        case CONVERT_UNIT_IN:
            return v * dpi;
        case CONVERT_UNIT_CM:
            return v * dpi / CONVERT_CM_PER_INCH;
        case CONVERT_UNIT_MM:
            return v * dpi / CONVERT_MM_PER_INCH;
        case CONVERT_UNIT_PT:
            return v * dpi / CONVERT_POINTS_PER_INCH;
        case CONVERT_UNIT_PC:
            return v * dpi / CONVERT_PICA_PER_INCH;
        }
    }

    switch (unit) {
    case CONVERT_UNIT_NONE:
        return v;
    case CONVERT_UNIT_PX:
        return v / dpi * CONVERT_POINTS_PER_INCH;
    case CONVERT_UNIT_IN:
        return v * CONVERT_POINTS_PER_INCH;
    case CONVERT_UNIT_CM:
        return v / CONVERT_CM_PER_INCH * CONVERT_POINTS_PER_INCH;
    case CONVERT_UNIT_MM:
        return v / CONVERT_MM_PER_INCH * CONVERT_POINTS_PER_INCH;
    case CONVERT_UNIT_PT:
        return v;
    case CONVERT_UNIT_PC:
        return v / CONVERT_PICA_PER_INCH * CONVERT_POINTS_PER_INCH;
    }

    return v;
}

static int
convert_iround (double v)
{
    return (int) floor (v + 0.5);
}

/* Double copy of _rsvg_size_callback for CSS-length -w/-h. */
static void
convert_apply_size_double (double natural_w, double natural_h,
                           gboolean have_w, gboolean have_h,
                           double req_w, double req_h,
                           double x_zoom, double y_zoom,
                           gboolean keep_aspect,
                           double *out_w, double *out_h)
{
    double w = natural_w;
    double h = natural_h;
    double in_w = natural_w;
    double in_h = natural_h;

    if (!have_w && !have_h) {
        w = x_zoom * natural_w;
        h = y_zoom * natural_h;
    } else if (x_zoom == 1.0 && y_zoom == 1.0) {
        if (!have_w || !have_h) {
            double zoom;

            if (!have_w)
                zoom = req_h / natural_h;
            else if (!have_h)
                zoom = req_w / natural_w;
            else
                zoom = MIN (req_w / natural_w, req_h / natural_h);
            w = zoom * natural_w;
            h = zoom * natural_h;
        } else {
            w = req_w;
            h = req_h;
        }
    } else {
        double zoomx, zoomy, zoom;

        w = x_zoom * natural_w;
        h = y_zoom * natural_h;
        if ((have_w && w > req_w) || (have_h && h > req_h)) {
            zoomx = have_w ? req_w / w : G_MAXDOUBLE;
            zoomy = have_h ? req_h / h : G_MAXDOUBLE;
            zoom = MIN (zoomx, zoomy);
            w *= zoom;
            h *= zoom;
        }
    }

    if (keep_aspect) {
        double out_min = MIN (w, h);

        if (out_min == w)
            h = in_h * (w / in_w);
        else
            w = in_w * (h / in_h);
    }

    *out_w = w;
    *out_h = h;
}

#define RSVG_CONVERT_ACCEPT_LANGUAGE_ENV "RSVG_CONVERT_ACCEPT_LANGUAGE"

static gboolean
convert_is_bcp47_tag (const char *tag)
{
    const char *p = tag;
    int n;

    if (p == NULL || *p == '\0')
        return FALSE;

    n = 0;
    while (*p != '\0' && *p != '-') {
        if (!g_ascii_isalpha (*p))
            return FALSE;
        n++;
        p++;
        if (n > 8)
            return FALSE;
    }
    if (n < 1)
        return FALSE;

    while (*p == '-') {
        p++;
        n = 0;
        while (*p != '\0' && *p != '-') {
            if (!g_ascii_isalnum (*p))
                return FALSE;
            n++;
            p++;
            if (n > 8)
                return FALSE;
        }
        if (n < 1)
            return FALSE;
    }

    return *p == '\0';
}

static gboolean
convert_valid_qweight (const char *q)
{
    gsize ndigits;

    if (q[0] != '0' && q[0] != '1')
        return FALSE;
    if (q[1] == '\0')
        return TRUE;
    if (q[1] != '.')
        return FALSE;
    ndigits = strlen (q + 2);
    if (ndigits > 3)
        return FALSE;
    if (q[0] == '0') {
        guint i;
        for (i = 0; i < ndigits; i++) {
            if (!g_ascii_isdigit (q[2 + i]))
                return FALSE;
        }
        return TRUE;
    }
    {
        guint i;
        for (i = 0; i < ndigits; i++) {
            if (q[2 + i] != '0')
                return FALSE;
        }
    }
    return TRUE;
}

/* HTTP Accept-Language (RFC 7231). Rust fails on invalid tags; we do too. */
static gboolean
convert_parse_accept_language (const char *str, char **out_joined, GError **error)
{
    gchar **parts;
    GPtrArray *tags;
    guint i;

    if (str == NULL || !g_str_is_ascii (str)) {
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     _("invalid characters in language list"));
        return FALSE;
    }

    parts = g_strsplit (str, ",", -1);
    tags = g_ptr_array_new_with_free_func (g_free);

    for (i = 0; parts[i] != NULL; i++) {
        gchar *item = g_strstrip (parts[i]);
        gchar *semi;
        gchar *tag;
        gchar *quality;

        if (item[0] == '\0')
            continue;

        semi = strchr (item, ';');
        if (semi != NULL) {
            *semi = '\0';
            tag = g_strstrip (item);
            quality = g_strstrip (semi + 1);
            if (!g_str_has_prefix (quality, "q=")) {
                g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                             _("invalid q= weight"));
                g_ptr_array_unref (tags);
                g_strfreev (parts);
                return FALSE;
            }
            if (!convert_valid_qweight (quality + 2)) {
                g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                             _("invalid q= weight"));
                g_ptr_array_unref (tags);
                g_strfreev (parts);
                return FALSE;
            }
        } else {
            tag = item;
        }

        if (!convert_is_bcp47_tag (tag)) {
            g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                         _("invalid language tag: %s"), tag);
            g_ptr_array_unref (tags);
            g_strfreev (parts);
            return FALSE;
        }
        g_ptr_array_add (tags, g_strdup (tag));
    }

    g_strfreev (parts);

    if (tags->len == 0) {
        g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
                     _("no language tags in list"));
        g_ptr_array_unref (tags);
        return FALSE;
    }

    g_ptr_array_add (tags, NULL);
    *out_joined = g_strjoinv (",", (gchar **) tags->pdata);
    g_ptr_array_free (tags, TRUE);
    return TRUE;
}

int
main (int argc, char **argv)
{
    GOptionContext *g_option_context;
    double x_zoom = 1.0;
    double y_zoom = 1.0;
    double zoom = 1.0;
    double dpi_x = 96.0;
    double dpi_y = 96.0;
    char *width_str = NULL;
    char *height_str = NULL;
    gboolean have_width = FALSE;
    gboolean have_height = FALSE;
    double width_value = 0.0;
    double height_value = 0.0;
    ConvertLengthUnit width_unit = CONVERT_UNIT_NONE;
    ConvertLengthUnit height_unit = CONVERT_UNIT_NONE;
    int width = -1;
    int height = -1;
    double render_width = 0.0;
    double render_height = 0.0;
    char *page_width_str = NULL;
    char *page_height_str = NULL;
    char *left_str = NULL;
    char *top_str = NULL;
    double page_width = 0.0;
    double page_height = 0.0;
    double page_left = 0.0;
    double page_top = 0.0;
    gboolean have_page_size = FALSE;
    double surface_width = 0.0;
    double surface_height = 0.0;
    int bVersion = 0;
    char *format = NULL;
    char *output = NULL;
    char *export_id = NULL;
    char *stylesheet = NULL;
    char *accept_language_str = NULL;
    char *accept_language_joined = NULL;
    guint8 *css_data = NULL;
    gsize css_len = 0;
    int keep_aspect_ratio = FALSE;
    guint32 background_color = 0;
    char *background_color_str = NULL;
    gboolean using_stdin = FALSE;
    gboolean stdin_tty_notice = FALSE;
    gboolean unlimited = FALSE;
    gboolean keep_image_data = FALSE;
    gboolean no_keep_image_data = FALSE;
    GError *error = NULL;

    int i;
    char **args = NULL;
    gint n_args = 0;
    RsvgHandle *rsvg = NULL;
    cairo_surface_t *surface = NULL;
    cairo_t *cr = NULL;
    RsvgHandleFlags flags = RSVG_HANDLE_FLAGS_NONE;
    FILE *output_file = stdout;
    char *export_lookup_id;
    double unscaled_width, unscaled_height;
    int scaled_width, scaled_height;

#ifdef G_OS_WIN32
    HANDLE handle;
#endif

    GOptionEntry options_table[] = {
        {"dpi-x", 'd', 0, G_OPTION_ARG_DOUBLE, &dpi_x,
         N_("pixels per inch [optional; defaults to 96dpi]"), N_("<float>")},
        {"dpi-y", 'p', 0, G_OPTION_ARG_DOUBLE, &dpi_y,
         N_("pixels per inch [optional; defaults to 96dpi]"), N_("<float>")},
        {"x-zoom", 'x', 0, G_OPTION_ARG_DOUBLE, &x_zoom,
         N_("x zoom factor [optional; defaults to 1.0]"), N_("<float>")},
        {"y-zoom", 'y', 0, G_OPTION_ARG_DOUBLE, &y_zoom,
         N_("y zoom factor [optional; defaults to 1.0]"), N_("<float>")},
        {"zoom", 'z', 0, G_OPTION_ARG_DOUBLE, &zoom, N_("zoom factor [optional; defaults to 1.0]"),
         N_("<float>")},
        {"width", 'w', 0, G_OPTION_ARG_STRING, &width_str,
         N_("width [optional; defaults to the SVG's width]"), N_("<length>")},
        {"height", 'h', 0, G_OPTION_ARG_STRING, &height_str,
         N_("height [optional; defaults to the SVG's height]"), N_("<length>")},
        {"top", 0, 0, G_OPTION_ARG_STRING, &top_str,
         N_("distance between top of page and image [optional; defaults to 0]"), N_("<float>")},
        {"left", 0, 0, G_OPTION_ARG_STRING, &left_str,
         N_("distance between left of page and image [optional; defaults to 0]"), N_("<float>")},
        {"page-width", 0, 0, G_OPTION_ARG_STRING, &page_width_str,
         N_("width of output media [optional; defaults to the image width]"), N_("<float>")},
        {"page-height", 0, 0, G_OPTION_ARG_STRING, &page_height_str,
         N_("height of output media [optional; defaults to the image height]"), N_("<float>")},
        {"format", 'f', 0, G_OPTION_ARG_STRING, &format,
         N_("save format [optional; defaults to 'png']"),
         N_("[png, pdf, pdf1.4, pdf1.5, pdf1.6, pdf1.7, ps, eps, svg, xml, recording]")},
        {"output", 'o', 0, G_OPTION_ARG_STRING, &output,
         N_("output filename [optional; defaults to stdout]"), NULL},
        {"export-id", 'i', 0, G_OPTION_ARG_STRING, &export_id,
         N_("SVG id of object to export [optional; defaults to exporting all objects]"), N_("<object id>")},
        {"stylesheet", 's', 0, G_OPTION_ARG_FILENAME, &stylesheet,
         N_("CSS stylesheet to apply [optional]"), N_("<filename.css>")},
        {"accept-language", 'l', 0, G_OPTION_ARG_STRING, &accept_language_str,
         N_("languages for systemLanguage, e.g. es-MX,de,en [optional; default is LANG]"),
         N_("<language-tags>")},
        {"keep-aspect-ratio", 'a', 0, G_OPTION_ARG_NONE, &keep_aspect_ratio,
         N_("whether to preserve the aspect ratio [optional; defaults to FALSE]"), NULL},
        {"background-color", 'b', 0, G_OPTION_ARG_STRING, &background_color_str,
         N_("set the background color [optional; defaults to None]"), N_("[black, white, #abccee, #aaa...]")},
        {"unlimited", 'u', 0, G_OPTION_ARG_NONE, &unlimited, N_("Allow huge SVG files"), NULL},
        {"keep-image-data", 0, 0, G_OPTION_ARG_NONE, &keep_image_data, N_("Keep image data"), NULL},
        {"no-keep-image-data", 0, 0, G_OPTION_ARG_NONE, &no_keep_image_data, N_("Don't keep image data"), NULL},
        {"version", 'v', 0, G_OPTION_ARG_NONE, &bVersion, N_("show version information"), NULL},
        {G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &args, NULL, N_("[FILE...]")},
        {NULL}
    };

    /* Set the locale so that UTF-8 filenames work */
    setlocale(LC_ALL, "");

    RSVG_G_TYPE_INIT;

    g_option_context = g_option_context_new (_("- SVG Converter"));
    g_option_context_add_main_entries (g_option_context, options_table, NULL);
    g_option_context_set_help_enabled (g_option_context, TRUE);
    {
        char *notice = g_strdup_printf (
            "rsvg-convert %s\n"
            "Copyright (C) 1999-2026 The librsvg C library authors\n"
            "License: GNU GPL-2.0-only\n"
            "This is free software; you are welcome to change and redistribute it.\n"
            "There is NO WARRANTY, to the extent permitted by law.",
            VERSION);
        g_option_context_set_description (g_option_context, notice);
        g_free (notice);
    }
    if (!g_option_context_parse (g_option_context, &argc, &argv, &error)) {
        g_option_context_free (g_option_context);
        display_error (error);
        exit (1);
    }

    g_option_context_free (g_option_context);

    if (width_str != NULL) {
        if (!convert_parse_length (width_str, "--width", FALSE, TRUE,
                                   &width_value, &width_unit, &error)) {
            display_error (error);
            g_free (width_str);
            g_free (height_str);
            exit (1);
        }
        if (width_value <= 0.0) {
            g_printerr (_("Width must be greater than 0.\n"));
            g_free (width_str);
            g_free (height_str);
            exit (1);
        }
        have_width = TRUE;
    }

    if (height_str != NULL) {
        if (!convert_parse_length (height_str, "--height", FALSE, TRUE,
                                   &height_value, &height_unit, &error)) {
            display_error (error);
            g_free (width_str);
            g_free (height_str);
            exit (1);
        }
        if (height_value <= 0.0) {
            g_printerr (_("Height must be greater than 0.\n"));
            g_free (width_str);
            g_free (height_str);
            exit (1);
        }
        have_height = TRUE;
    }

    if ((page_width_str != NULL) != (page_height_str != NULL)) {
        g_printerr (_("Please specify both the --page-width and --page-height options together.\n"));
        g_free (page_width_str);
        g_free (page_height_str);
        g_free (left_str);
        g_free (top_str);
        g_free (width_str);
        g_free (height_str);
        exit (1);
    }

    if (page_width_str != NULL) {
        if (!convert_parse_px_number (page_width_str, "--page-width", FALSE,
                                      &page_width, &error)
            || !convert_parse_px_number (page_height_str, "--page-height", FALSE,
                                         &page_height, &error)) {
            display_error (error);
            g_free (page_width_str);
            g_free (page_height_str);
            g_free (left_str);
            g_free (top_str);
            g_free (width_str);
            g_free (height_str);
            exit (1);
        }
        if (page_width <= 0.0 || page_height <= 0.0) {
            g_printerr (_("Page width and height must be greater than 0.\n"));
            g_free (page_width_str);
            g_free (page_height_str);
            g_free (left_str);
            g_free (top_str);
            g_free (width_str);
            g_free (height_str);
            exit (1);
        }
        have_page_size = TRUE;
    }

    if (left_str != NULL
        && !convert_parse_px_number (left_str, "--left", TRUE, &page_left, &error)) {
        display_error (error);
        g_free (page_width_str);
        g_free (page_height_str);
        g_free (left_str);
        g_free (top_str);
        g_free (width_str);
        g_free (height_str);
        exit (1);
    }

    if (top_str != NULL
        && !convert_parse_px_number (top_str, "--top", TRUE, &page_top, &error)) {
        display_error (error);
        g_free (page_width_str);
        g_free (page_height_str);
        g_free (left_str);
        g_free (top_str);
        g_free (width_str);
        g_free (height_str);
        exit (1);
    }

    if (accept_language_str != NULL) {
        if (!convert_parse_accept_language (accept_language_str, &accept_language_joined, &error)) {
            display_error (error);
            g_free (accept_language_str);
            exit (1);
        }
    }

    if (bVersion != 0) {
        convert_print_license_notice (stdout);
        return 0;
    }

    if (output != NULL) {
        output_file = fopen (output, "wb");
        if (!output_file) {
            g_printerr (_("Error saving to file: %s\n"), output);
            g_free (output);
            exit (1);
        }

        g_free (output);
    }

    if (args)
        while (args[n_args] != NULL)
            n_args++;

    if (n_args == 0) {
        n_args = 1;
        using_stdin = TRUE;
    } else if (n_args == 1 && args && args[0] && strcmp (args[0], "-") == 0) {
        using_stdin = TRUE;
    } else if (n_args > 1 && (!format || !(!strcmp (format, "ps") || !strcmp (format, "eps") || convert_is_pdf_format (format)))) {
        g_printerr (_("Multiple SVG files are only allowed for PDF and (E)PS output.\n"));
        exit (1);
    }

    if (format != NULL &&
        (g_str_equal (format, "ps") || g_str_equal (format, "eps") || convert_is_pdf_format (format)) &&
        !no_keep_image_data)
        keep_image_data = TRUE;

    if (zoom != 1.0)
        x_zoom = y_zoom = zoom;

    if (unlimited)
        flags |= RSVG_HANDLE_FLAG_UNLIMITED;

    if (keep_image_data)
        flags |= RSVG_HANDLE_FLAG_KEEP_IMAGE_DATA;

    if (stylesheet != NULL) {
        gchar *css_text = NULL;

        if (!g_file_get_contents (stylesheet, &css_text, &css_len, &error)) {
            g_printerr (_("Error reading stylesheet:"));
            display_error (error);
            g_free (stylesheet);
            exit (1);
        }
        css_data = (guint8 *) css_text;
    }

    /* Override systemLanguage matching for this process only. Do not
     * change LANG. Unset when -l is omitted so a parent env cannot
     * change default convert behavior. */
    if (accept_language_joined != NULL)
        g_setenv (RSVG_CONVERT_ACCEPT_LANGUAGE_ENV, accept_language_joined, TRUE);
    else
        g_unsetenv (RSVG_CONVERT_ACCEPT_LANGUAGE_ENV);

    for (i = 0; i < n_args; i++) {
        GFile *file;
        GInputStream *stream;

        if (using_stdin || (args && args[i] && strcmp (args[i], "-") == 0)) {
            if (!stdin_tty_notice && convert_stdin_is_tty ()) {
                convert_print_stdin_tty_notice ();
                stdin_tty_notice = TRUE;
            }

            file = NULL;
#ifdef _WIN32
            handle = GetStdHandle (STD_INPUT_HANDLE);

            if (handle == INVALID_HANDLE_VALUE) {
              gchar *emsg = g_win32_error_message (GetLastError());
              g_printerr ( _("Unable to acquire HANDLE for STDIN: %s\n"), emsg);
              g_free (emsg);
              exit (1);
            }
            stream = g_win32_input_stream_new (handle, FALSE);
#else
            stream = g_unix_input_stream_new (STDIN_FILENO, FALSE);
#endif
        } else {
            file = g_file_new_for_commandline_arg (args[i]);
            stream = (GInputStream *) g_file_read (file, NULL, &error);

            if (stream == NULL)
                goto done;
        }

        rsvg = rsvg_handle_new_from_stream_sync (stream, file, flags, NULL, &error);

        if (rsvg != NULL)
            rsvg_handle_set_dpi_x_y (rsvg, dpi_x, dpi_y);

        if (rsvg != NULL && css_data != NULL) {
            if (!rsvg_handle_set_stylesheet (rsvg, css_data, css_len, &error)) {
                g_printerr (_("Error applying stylesheet:"));
                display_error (error);
                g_free (css_data);
                g_free (stylesheet);
                exit (1);
            }
        }

        /* P20: same font map as rsvg-test when RSVG_PARITY=1 (test
         * fonts + rust hint options). Not a public convert flag. */
        if (rsvg != NULL
            && g_getenv ("RSVG_PARITY") != NULL
            && g_strcmp0 (g_getenv ("RSVG_PARITY"), "0") != 0)
            rsvg_handle_internal_set_testing (rsvg, TRUE);

    done:
        g_clear_object (&stream);
        g_clear_object (&file);

        if (error != NULL) {
            g_printerr (_("Error reading SVG:"));
            display_error (error);
            g_printerr ("\n");
            exit (1);
        }

        export_lookup_id = get_lookup_id_from_command_line (export_id);
        if (export_lookup_id != NULL
            && !rsvg_handle_has_sub (rsvg, export_lookup_id)) {
            g_printerr (_("File %s does not have an object with id \"%s\"\n"), args[i], export_id);
            exit (1);
        }

        if (i == 0) {
            struct RsvgSizeCallbackData size_data;
            GError *geo_error = NULL;

            if (!convert_get_size (rsvg, export_lookup_id, &unscaled_width,
                                   &unscaled_height, &geo_error)) {
                g_printerr ("Could not get dimensions for file %s\n", args[i]);
                display_error (geo_error);
                exit (1);
            }
            if (unscaled_width <= 0)
                unscaled_width = 1;
            if (unscaled_height <= 0)
                unscaled_height = 1;

            {
                gboolean print_units;
                gboolean width_physical, height_physical;

                print_units = format != NULL
                    && (convert_is_pdf_format (format)
                        || g_str_equal (format, "ps")
                        || g_str_equal (format, "eps"));
                width_physical = have_width && width_unit != CONVERT_UNIT_NONE;
                height_physical = have_height && height_unit != CONVERT_UNIT_NONE;

                if (width_physical || height_physical) {
                    double req_w = 0.0, req_h = 0.0;

                    if (have_width)
                        req_w = convert_resolve_length (width_value, width_unit,
                                                        dpi_x, print_units);
                    if (have_height)
                        req_h = convert_resolve_length (height_value, height_unit,
                                                        dpi_y, print_units);
                    convert_apply_size_double (unscaled_width, unscaled_height,
                                               have_width, have_height,
                                               req_w, req_h, x_zoom, y_zoom,
                                               keep_aspect_ratio,
                                               &render_width, &render_height);
                    scaled_width = convert_iround (ceil (render_width));
                    scaled_height = convert_iround (ceil (render_height));
                    if (scaled_width < 1)
                        scaled_width = 1;
                    if (scaled_height < 1)
                        scaled_height = 1;
                } else {
                    if (have_width)
                        width = convert_iround (width_value);
                    if (have_height)
                        height = convert_iround (height_value);

                    /* if both are unspecified, assume user wants to zoom the image in at least 1 dimension */
                    if (width == -1 && height == -1) {
                        size_data.type = RSVG_SIZE_ZOOM;
                        size_data.x_zoom = x_zoom;
                        size_data.y_zoom = y_zoom;
                        size_data.keep_aspect_ratio = keep_aspect_ratio;
                    } else if (x_zoom == 1.0 && y_zoom == 1.0) {
                        /* if one parameter is unspecified, assume user wants to keep the aspect ratio */
                        if (width == -1 || height == -1) {
                            size_data.type = RSVG_SIZE_WH_MAX;
                            size_data.width = width;
                            size_data.height = height;
                            size_data.keep_aspect_ratio = keep_aspect_ratio;
                        } else {
                            size_data.type = RSVG_SIZE_WH;
                            size_data.width = width;
                            size_data.height = height;
                            size_data.keep_aspect_ratio = keep_aspect_ratio;
                        }
                    } else {
                        /* assume the user wants to zoom the image, but cap the maximum size */
                        size_data.type = RSVG_SIZE_ZOOM_MAX;
                        size_data.x_zoom = x_zoom;
                        size_data.y_zoom = y_zoom;
                        size_data.width = width;
                        size_data.height = height;
                        size_data.keep_aspect_ratio = keep_aspect_ratio;
                    }

                    scaled_width = (int) (unscaled_width + 0.5);
                    scaled_height = (int) (unscaled_height + 0.5);
                    if (scaled_width < 1)
                        scaled_width = 1;
                    if (scaled_height < 1)
                        scaled_height = 1;
                    _rsvg_size_callback (&scaled_width, &scaled_height, &size_data);
                    render_width = (double) scaled_width;
                    render_height = (double) scaled_height;
                }
            }

            /* Page size defaults to the rendered image. Overflow of
             * image+offset past the page is clipped (cairo); the page
             * does not grow. Same rule as rust 2.62. */
            if (have_page_size) {
                surface_width = page_width;
                surface_height = page_height;
            } else {
                surface_width = render_width;
                surface_height = render_height;
            }

            if (!format || !strcmp (format, "png")) {
                int png_w, png_h;

                if (have_page_size) {
                    if (surface_width > (double) G_MAXINT
                        || surface_height > (double) G_MAXINT) {
                        g_printerr (_("Page size is too large.\n"));
                        exit (1);
                    }
                    png_w = (int) ceil (surface_width);
                    png_h = (int) ceil (surface_height);
                    if (png_w < 1)
                        png_w = 1;
                    if (png_h < 1)
                        png_h = 1;
                } else {
                    png_w = scaled_width;
                    png_h = scaled_height;
                }
                surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                                      png_w, png_h);
            }
#ifdef CAIRO_HAS_PDF_SURFACE
            else if (convert_is_pdf_format (format)) {
                GError *pdf_error = NULL;

                surface = cairo_pdf_surface_create_for_stream (rsvg_cairo_write_func, output_file,
                                                               surface_width, surface_height);
                if (!convert_pdf_restrict_version (surface, format, &pdf_error)) {
                    g_printerr ("%s\n", pdf_error ? pdf_error->message : _("PDF version not supported"));
                    g_clear_error (&pdf_error);
                    cairo_surface_destroy (surface);
                    exit (1);
                }
            }
#endif
#ifdef CAIRO_HAS_PS_SURFACE
            else if (!strcmp (format, "ps") || !strcmp (format, "eps")){
                surface = cairo_ps_surface_create_for_stream (rsvg_cairo_write_func, output_file,
                                                              surface_width, surface_height);
                if(!strcmp (format, "eps"))
                    cairo_ps_surface_set_eps(surface, TRUE);
            }
#endif
#ifdef CAIRO_HAS_SVG_SURFACE
            else if (!strcmp (format, "svg"))
                surface = cairo_svg_surface_create_for_stream (rsvg_cairo_write_func, output_file,
                                                               surface_width, surface_height);
#endif
#ifdef CAIRO_HAS_XML_SURFACE
            else if (!strcmp (format, "xml")) {
                cairo_device_t *device = cairo_xml_create_for_stream (rsvg_cairo_write_func, output_file);
                surface = cairo_xml_surface_create (device, CAIRO_CONTENT_COLOR_ALPHA,
                                                    surface_width, surface_height);
                cairo_device_destroy (device);
            }
#if CAIRO_VERSION >= CAIRO_VERSION_ENCODE (1, 10, 0)
            else if (!strcmp (format, "recording"))
                surface = cairo_recording_surface_create (CAIRO_CONTENT_COLOR_ALPHA, NULL);
#endif
#endif
            else {
                g_printerr (_("Unknown output format."));
                exit (1);
            }

            if (cairo_surface_status (surface) != CAIRO_STATUS_SUCCESS) {
                g_printerr (_("Error creating output surface: %s\n"),
                            cairo_status_to_string (cairo_surface_status (surface)));
                cairo_surface_destroy (surface);
                exit (1);
            }

            cr = cairo_create (surface);
        }

        // Set background color (page, not just the image)
        if (background_color_str && g_ascii_strcasecmp(background_color_str, "none") != 0) {
            background_color = rsvg_css_parse_color(background_color_str, FALSE);

            cairo_set_source_rgb (
                cr, 
                ((background_color >> 16) & 0xff) / 255.0, 
                ((background_color >> 8) & 0xff) / 255.0, 
                ((background_color >> 0) & 0xff) / 255.0);
            cairo_rectangle (cr, 0, 0, surface_width, surface_height);
            cairo_fill (cr);
        }

        {
            RsvgRectangle viewport = { 0, 0, render_width, render_height };
            GError *render_error = NULL;
            gboolean ok;

            cairo_save (cr);
            cairo_translate (cr, page_left, page_top);

            if (export_lookup_id)
                ok = rsvg_handle_render_element (rsvg, cr, export_lookup_id,
                                                 &viewport, &render_error);
            else
                ok = rsvg_handle_render_document (rsvg, cr, &viewport,
                                                  &render_error);
            cairo_restore (cr);
            if (!ok) {
                g_printerr (_("Error rendering SVG:"));
                display_error (render_error);
                g_printerr ("\n");
                exit (1);
            }
        }

        g_free (export_lookup_id);

        if (!format || !strcmp (format, "png"))
            cairo_surface_write_to_png_stream (surface, rsvg_cairo_write_func, output_file);
#if CAIRO_HAS_XML_SURFACE && CAIRO_VERSION >= CAIRO_VERSION_ENCODE (1, 10, 0)
        else if (!strcmp (format, "recording")) {
            cairo_device_t *device = cairo_xml_create_for_stream (rsvg_cairo_write_func, output_file);
            cairo_xml_for_recording_surface (device, surface);
            cairo_device_destroy (device);
        }
#endif
        else if (!strcmp (format, "xml"))
          ;
        else if (!strcmp (format, "svg") || convert_is_pdf_format (format)
                 || !strcmp (format, "ps") || !strcmp (format, "eps"))
            cairo_show_page (cr);
        else
          g_assert_not_reached ();

        g_object_unref (rsvg);
    }

    cairo_destroy (cr);

    cairo_surface_destroy (surface);

    fclose (output_file);

    g_strfreev (args);
    g_free (css_data);
    g_free (stylesheet);
    g_free (page_width_str);
    g_free (page_height_str);
    g_free (left_str);
    g_free (top_str);
    g_free (width_str);
    g_free (height_str);
    g_free (accept_language_str);
    g_free (accept_language_joined);

    return 0;
}
