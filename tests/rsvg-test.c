/* vim: set sw=4 sts=4: -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 8 -*-
 *
 * rsvg-test - Regression test utility for librsvg
 *
 * Copyright © 2004 Richard D. Worth
 * Copyright © 2006 Red Hat, Inc.
 * Copyright © 2007 Emmanuel Pacaud
 * Copyright (C) 2026 Randy Butler
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of the authors
 * not be used in advertising or publicity pertaining to distribution
 * of the software without specific, written prior permission.
 * The authors make no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 * THE AUTHORS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors: Emmanuel Pacaud <emmanuel.pacaud@lapp.in2p3.fr>
 *	    Richard D. Worth <richard@theworths.org>
 *	    Carl Worth <cworth@cworth.org>
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rsvg.h"
#include "rsvg-compat.h"

#include "test-utils.h"

typedef struct _buffer_diff_result {
    unsigned int pixels_changed;
    unsigned int max_diff;
} buffer_diff_result_t;

static cairo_surface_t *read_png (const char *test_name);
static cairo_surface_t *read_png_gfile (GFile *file);
static GFile *parity_ref_png_file (GFile *svg_file);
static void rsvg_parity_check (gconstpointer data);
static void save_image (cairo_surface_t *surface, const char *test_name, const char *extension);
static void compare_surfaces (cairo_surface_t *surface_a,
                              cairo_surface_t *surface_b,
                              cairo_surface_t *surface_diff,
                              buffer_diff_result_t *result,
                              gboolean announce);

/* Compare two buffers, returning the number of pixels that are
 * different and the maximum difference of any single color channel in
 * result_ret.
 *
 * This function should be rewritten to compare all formats supported by
 * cairo_format_t instead of taking a mask as a parameter.
 */
static void
buffer_diff_core (unsigned char *_buf_a,
		  unsigned char *_buf_b,
		  unsigned char *_buf_diff,
		  int		width,
		  int		height,
		  int		stride,
		  guint32       mask,
		  buffer_diff_result_t *result_ret)
{
    int x, y;
    guint32 *row_a, *row_b, *row;
    buffer_diff_result_t result = {0, 0};
    guint32 *buf_a = (guint32 *) _buf_a;
    guint32 *buf_b = (guint32 *) _buf_b;
    guint32 *buf_diff = (guint32 *) _buf_diff;

    stride /= sizeof(guint32);
    for (y = 0; y < height; y++)
    {
	row_a = buf_a + y * stride;
	row_b = buf_b + y * stride;
	row = buf_diff + y * stride;
	for (x = 0; x < width; x++)
	{
	    /* check if the pixels are the same */
	    if ((row_a[x] & mask) != (row_b[x] & mask)) {
		int channel;
		guint32 diff_pixel = 0;

		/* calculate a difference value for all 4 channels */
		for (channel = 0; channel < 4; channel++) {
		    int value_a = (row_a[x] >> (channel*8)) & 0xff;
		    int value_b = (row_b[x] >> (channel*8)) & 0xff;
		    unsigned int diff;
		    diff = abs (value_a - value_b);
		    if (diff > result.max_diff)
			result.max_diff = diff;
		    diff *= 4;  /* emphasize */
		    if (diff)
		        diff += 128; /* make sure it's visible */
		    if (diff > 255)
		        diff = 255;
		    diff_pixel |= diff << (channel*8);
		}

		result.pixels_changed++;
		if ((diff_pixel & 0x00ffffff) == 0) {
		    /* alpha only difference, convert to luminance */
		    guint8 alpha = diff_pixel >> 24;
		    diff_pixel = alpha * 0x010101;
		}
		row[x] = diff_pixel;
	    } else {
		row[x] = 0;
	    }
	    row[x] |= 0xff000000; /* Set ALPHA to 100% (opaque) */
	}
    }

    *result_ret = result;
}

static void
compare_surfaces (cairo_surface_t	*surface_a,
		  cairo_surface_t	*surface_b,
		  cairo_surface_t	*surface_diff,
		  buffer_diff_result_t	*result,
		  gboolean		 announce)
{
    /* Here, we run cairo's old buffer_diff algorithm which looks for
     * pixel-perfect images.
     */
    buffer_diff_core (cairo_image_surface_get_data (surface_a),
		      cairo_image_surface_get_data (surface_b),
		      cairo_image_surface_get_data (surface_diff),
		      cairo_image_surface_get_width (surface_a),
		      cairo_image_surface_get_height (surface_a),
		      cairo_image_surface_get_stride (surface_a),
		      0xffffffff,
		      result);
    if (result->pixels_changed == 0)
	return;

    if (announce)
        g_test_message ("%d pixels differ (with maximum difference of %d) from reference image\n",
                        result->pixels_changed, result->max_diff);
}

static char *
get_output_file (const char *test_file,
                 const char *extension)
{
  const char *output_dir = g_get_tmp_dir ();
  char *result, *base;

  base = g_path_get_basename (test_file);

  if (g_str_has_suffix (base, ".svg"))
    base[strlen (base) - strlen (".svg")] = '\0';

  result = g_strconcat (output_dir, G_DIR_SEPARATOR_S, base, extension, NULL);
  g_free (base);

  return result;
}

static void
save_image (cairo_surface_t *surface,
            const char      *test_name,
            const char      *extension)
{
  char *filename = get_output_file (test_name, extension);

  g_test_message ("Storing test result image at %s", filename);
  g_assert (cairo_surface_write_to_png (surface, filename) == CAIRO_STATUS_SUCCESS);

  g_free (filename);
}

static gboolean
is_svg_or_subdir (GFile *file)
{
    char *basename;
    gboolean ignore;
    gboolean result;

    result = FALSE;

    basename = g_file_get_basename (file);
    ignore = g_str_has_prefix (basename, "ignore") || strcmp (basename, "resources") == 0;

    if (ignore)
	goto out;

    if (g_file_query_file_type (file, 0, NULL) == G_FILE_TYPE_DIRECTORY) {
	result = TRUE;
	goto out;
    }

    result = g_str_has_suffix (basename, ".svg");

out:
    g_free (basename);

    return result;
}

/* ---- 2.62.3 parity corpus (tests/fixtures/parity/) ---- */

static gboolean parity_enabled = FALSE;
static gboolean parity_local_enabled = FALSE;
static char *parity_local_dir = NULL;
static GHashTable *parity_xfail = NULL;
static GPtrArray *parity_xfail_generated = NULL;
static char *parity_xfail_out_path = NULL;
static int parity_n_pass = 0;
static int parity_n_fail = 0;
static int parity_n_xfail = 0;
static int parity_n_skip = 0;
static int parity_n_upass = 0;

typedef struct {
    char rclass[4];
    unsigned norm_px;
    unsigned norm_max_diff;
} RsvgRNorm;

static GHashTable *parity_rnorms = NULL;
static GHashTable *check_rnorms = NULL;

static void
load_rnorms_into (GHashTable **table, const char *path)
{
    gchar *contents = NULL;
    gchar **lines;
    gsize i;

    if (!path || !g_file_get_contents (path, &contents, NULL, NULL))
        return;

    if (*table)
        g_hash_table_destroy (*table);
    *table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
    lines = g_strsplit (contents, "\n", -1);
    for (i = 0; lines[i]; i++) {
        char *line = lines[i];
        char **tok;
        RsvgRNorm *rn;

        g_strstrip (line);
        if (line[0] == '\0' || line[0] == '#')
            continue;
        tok = g_strsplit_set (line, " \t", 4);
        if (!tok[0] || !tok[1] || !tok[2] || !tok[3]
            || (strcmp (tok[0], "TFM") != 0
                && strcmp (tok[0], "TAA") != 0
                && strcmp (tok[0], "TCR") != 0)) {
            g_strfreev (tok);
            continue;
        }
        rn = g_new0 (RsvgRNorm, 1);
        memcpy (rn->rclass, tok[0], MIN (strlen (tok[0]), sizeof (rn->rclass) - 1));
        rn->norm_px = (unsigned) g_ascii_strtoull (tok[2], NULL, 10);
        rn->norm_max_diff = (unsigned) g_ascii_strtoull (tok[3], NULL, 10);
        g_hash_table_replace (*table, g_strdup (tok[1]), rn);
        g_strfreev (tok);
    }
    g_strfreev (lines);
    g_free (contents);
}

static void
load_parity_rnorms (const char *path)
{
    load_rnorms_into (&parity_rnorms, path);
}

static void
load_check_rnorms (const char *path)
{
    load_rnorms_into (&check_rnorms, path);
}

static const RsvgRNorm *
lookup_rnorm_in (GHashTable *table, const char *rel)
{
    if (!table || !rel)
        return NULL;
    return g_hash_table_lookup (table, rel);
}

static const RsvgRNorm *
lookup_rnorm (const char *rel)
{
    return lookup_rnorm_in (parity_rnorms, rel);
}

static const RsvgRNorm *
lookup_check_rnorm (const char *rel)
{
    return lookup_rnorm_in (check_rnorms, rel);
}

static gboolean
parity_use_rband (void)
{
    return parity_rnorms != NULL && !parity_local_enabled;
}

static unsigned
rband_px_limit (unsigned norm_px)
{
    unsigned twice = norm_px * 2u;
    unsigned plus = norm_px + 50u;

    return twice > plus ? twice : plus;
}

static unsigned
rband_diff_limit (unsigned norm_max_diff)
{
    unsigned twice = norm_max_diff * 2u;
    unsigned plus = norm_max_diff + 2u;

    return twice > plus ? twice : plus;
}

static char *
rnorm_label (const char *rel)
{
    if (rel && g_str_has_suffix (rel, ".svg"))
        return g_strndup (rel, strlen (rel) - 4);
    return g_strdup (rel ? rel : "?");
}

/* ARGB32 native-endian: alpha is the high byte. */
static gboolean
surfaces_have_opaque_vs_empty (cairo_surface_t *a, cairo_surface_t *b)
{
    int w, h, stride, x, y;
    const guint32 *pa, *pb;

    cairo_surface_flush (a);
    cairo_surface_flush (b);
    w = cairo_image_surface_get_width (a);
    h = cairo_image_surface_get_height (a);
    stride = cairo_image_surface_get_stride (a) / (int) sizeof (guint32);
    pa = (const guint32 *) cairo_image_surface_get_data (a);
    pb = (const guint32 *) cairo_image_surface_get_data (b);
    if (!pa || !pb)
        return FALSE;

    for (y = 0; y < h; y++) {
        const guint32 *ra = pa + y * stride;
        const guint32 *rb = pb + y * stride;

        for (x = 0; x < w; x++) {
            guint8 aa = (guint8) (ra[x] >> 24);
            guint8 ab = (guint8) (rb[x] >> 24);

            if ((aa == 255 && ab == 0) || (ab == 255 && aa == 0))
                return TRUE;
        }
    }
    return FALSE;
}

static char *
file_path (GFile *file)
{
    return g_file_get_path (file);
}

static gboolean
is_parity_file (GFile *file)
{
    char *path = file_path (file);
    gboolean r = path && strstr (path, G_DIR_SEPARATOR_S "parity" G_DIR_SEPARATOR_S) != NULL;
    g_free (path);
    return r;
}

static char *
parity_relpath (GFile *file)
{
    const char *data = test_utils_get_test_data_path ();
    char *base;
    char *path;
    char *rel = NULL;

    base = g_build_filename (data, "parity", NULL);
    path = file_path (file);
    if (path && g_str_has_prefix (path, base)) {
        const char *rest = path + strlen (base);
        while (*rest == G_DIR_SEPARATOR)
            rest++;
        rel = g_strdup (rest);
    }
    g_free (base);
    g_free (path);
    return rel;
}

static char *
reftest_relpath (GFile *file)
{
    const char *data = test_utils_get_test_data_path ();
    char *base;
    char *path;
    char *rel = NULL;

    base = g_build_filename (data, "reftests", NULL);
    path = file_path (file);
    if (path && g_str_has_prefix (path, base)) {
        const char *rest = path + strlen (base);

        while (*rest == G_DIR_SEPARATOR)
            rest++;
        rel = g_strdup (rest);
    }
    g_free (base);
    g_free (path);
    return rel;
}

static GFile *
parity_ref_png_file (GFile *svg_file)
{
    char *rel;
    char *refname;
    char *path;
    GFile *ref;
    char *uri;
    char *ref_uri;

    if (parity_local_enabled) {
        if (!parity_local_dir)
            return NULL;
        rel = parity_relpath (svg_file);
        if (!rel || !g_str_has_suffix (rel, ".svg")) {
            g_free (rel);
            return NULL;
        }
        rel[strlen (rel) - 4] = '\0';
        refname = g_strconcat (rel, "-ref.png", NULL);
        path = g_build_filename (parity_local_dir, refname, NULL);
        ref = g_file_new_for_path (path);
        g_free (rel);
        g_free (refname);
        g_free (path);
        return ref;
    }

    uri = g_file_get_uri (svg_file);
    if (!uri || !g_str_has_suffix (uri, ".svg")) {
        g_free (uri);
        return NULL;
    }
    uri[strlen (uri) - 4] = '\0';
    ref_uri = g_strconcat (uri, "-ref.png", NULL);
    ref = g_file_new_for_uri (ref_uri);
    g_free (ref_uri);
    g_free (uri);
    return ref;
}

static gboolean
has_ref_png (GFile *svg_file)
{
    GFile *ref;
    gboolean exists;

    ref = parity_ref_png_file (svg_file);
    if (!ref)
        return FALSE;
    exists = g_file_query_exists (ref, NULL);
    g_object_unref (ref);
    return exists;
}

static gboolean
is_parity_svg_or_subdir (GFile *file)
{
    char *basename;
    gboolean result = FALSE;

    basename = g_file_get_basename (file);

    if (g_str_has_prefix (basename, "ignore")
        || strcmp (basename, "resources") == 0
        || strcmp (basename, "images") == 0)
        goto out;

    if (g_file_query_file_type (file, 0, NULL) == G_FILE_TYPE_DIRECTORY) {
        result = TRUE;
        goto out;
    }

    if (!g_str_has_suffix (basename, ".svg"))
        goto out;

    /* Reference SVGs and helpers are not PNG reftests. */
    if (g_str_has_suffix (basename, "-ref.svg"))
        goto out;

    result = TRUE;

out:
    g_free (basename);
    return result;
}

static void
load_parity_xfail (const char *path)
{
    gchar *contents = NULL;
    gchar **lines;
    gsize i;

    if (!path || !g_file_get_contents (path, &contents, NULL, NULL))
        return;

    parity_xfail = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
    lines = g_strsplit (contents, "\n", -1);
    for (i = 0; lines[i]; i++) {
        char *line = lines[i];
        char *hash;
        char **tok;

        g_strstrip (line);
        if (line[0] == '\0' || line[0] == '#')
            continue;

        /* Accept "path", "fail path", or "skip path". */
        tok = g_strsplit_set (line, " \t", 3);
        if (!tok[0]) {
            g_strfreev (tok);
            continue;
        }
        if (strcmp (tok[0], "fail") == 0 || strcmp (tok[0], "skip") == 0
            || strcmp (tok[0], "crash") == 0) {
            if (tok[1] && tok[1][0])
                g_hash_table_add (parity_xfail, g_strdup (tok[1]));
        } else {
            hash = strchr (tok[0], '#');
            if (hash)
                *hash = '\0';
            g_strstrip (tok[0]);
            if (tok[0][0])
                g_hash_table_add (parity_xfail, g_strdup (tok[0]));
        }
        g_strfreev (tok);
    }
    g_strfreev (lines);
    g_free (contents);
}

static gboolean
is_xfail_path (const char *rel)
{
    /* Same-renderer local refs are not the rust-oracle xfail list. */
    if (parity_local_enabled)
        return FALSE;
    return rel && parity_xfail && g_hash_table_contains (parity_xfail, rel);
}

static void
add_parity_from_list (const char *list_path)
{
    gchar *contents = NULL;
    gchar **lines;
    gsize i;
    char *parity_root_path;
    GFile *parity_root;

    if (!list_path || !g_file_get_contents (list_path, &contents, NULL, NULL))
        return;

    parity_root_path = g_build_filename (test_utils_get_test_data_path (),
                                         "parity", NULL);
    parity_root = g_file_new_for_path (parity_root_path);

    lines = g_strsplit (contents, "\n", -1);
    for (i = 0; lines[i]; i++) {
        gchar *line = lines[i];
        GFile *svg;

        g_strstrip (line);
        if (!line[0] || line[0] == '#')
            continue;
        svg = g_file_resolve_relative_path (parity_root, line);
        if (svg) {
            test_utils_add_test_for_all_files ("/rsvg/parity-local", parity_root,
                                               svg, rsvg_parity_check,
                                               is_parity_svg_or_subdir);
            g_object_unref (svg);
        }
    }
    g_strfreev (lines);
    g_object_unref (parity_root);
    g_free (parity_root_path);
    g_free (contents);
}

static void
record_generated_xfail (const char *rel, const char *reason)
{
    if (!parity_xfail_generated || !rel)
        return;
    g_ptr_array_add (parity_xfail_generated,
                     g_strdup_printf ("fail\t%s\t%s", rel, reason ? reason : "mismatch"));
}

static int
cmp_strings (gconstpointer a, gconstpointer b)
{
    return strcmp (*(char * const *) a, *(char * const *) b);
}

static void
parity_write_generated_xfail (void)
{
    FILE *fp;
    guint i;

    if (!parity_xfail_out_path || !parity_xfail_generated)
        return;

    g_ptr_array_sort (parity_xfail_generated, cmp_strings);
    fp = fopen (parity_xfail_out_path, "w");
    if (!fp)
        return;

    fprintf (fp,
             "# Living xfail list: librsvg vs 2.62.3 reftests\n"
             "# Corpus: tests/fixtures/parity/ (copy of rust/rsvg/tests/fixtures/reftests)\n"
             "# Do not implement features to shrink this list in the P0 step.\n"
             "# Format: fail<TAB>relative-path<TAB>reason\n"
             "#\n"
             "# Summary from last generate run: pass=%d fail=%d skip=%d\n"
             "#\n",
             parity_n_pass, parity_n_fail, parity_n_skip);

    for (i = 0; i < parity_xfail_generated->len; i++)
        fprintf (fp, "%s\n", (char *) parity_xfail_generated->pdata[i]);

    fclose (fp);
}

static void
parity_print_summary (void)
{
    if (parity_n_xfail || parity_n_upass)
        g_printerr ("parity summary: pass=%d fail=%d xfail=%d skip=%d unexpected_pass=%d\n",
                    parity_n_pass, parity_n_fail, parity_n_xfail, parity_n_skip, parity_n_upass);
    else
        g_printerr ("parity summary: pass=%d fail=%d skip=%d\n",
                    parity_n_pass, parity_n_fail, parity_n_skip);
    parity_write_generated_xfail ();
}

static void
parity_outcome_fail (const char *rel, const char *reason)
{
    /* Tolerance band (TFM/TAA/TCR). In-band residuals are
     * silent passes; do not TAP-skip them as xfails. */
    if (!parity_use_rband () && is_xfail_path (rel)) {
        parity_n_xfail++;
        g_test_skip (reason ? reason : "xfail");
        return;
    }
    parity_n_fail++;
    record_generated_xfail (rel, reason);
    g_test_message ("%s", reason ? reason : "failed");
    g_test_fail ();
}

static void
parity_outcome_pass (const char *rel)
{
    if (!parity_use_rband () && is_xfail_path (rel)) {
        parity_n_upass++;
        g_test_message ("UNEXPECTED PASS: remove %s from tests/parity-xfail.txt",
                        rel ? rel : "?");
        g_test_fail ();
        return;
    }
    parity_n_pass++;
}

static void
rsvg_parity_check (gconstpointer data)
{
    GFile *test_file = G_FILE (data);
    RsvgHandle *rsvg;
    RsvgDimensionData dimensions;
    cairo_t *cr = NULL;
    cairo_surface_t *surface_a = NULL, *surface_b = NULL, *surface_diff = NULL;
    buffer_diff_result_t result;
    char *test_file_base;
    char *rel;
    unsigned int width_a, height_a, stride_a;
    unsigned int width_b, height_b, stride_b;
    GError *error = NULL;
    char *reason = NULL;
    gchar *saved_lang = NULL;
    gboolean had_lang = FALSE;
    gboolean pushed_de_lang = FALSE;

    rel = parity_relpath (test_file);
    test_file_base = g_file_get_uri (test_file);
    if (g_str_has_suffix (test_file_base, ".svg"))
        test_file_base[strlen (test_file_base) - strlen (".svg")] = '\0';

    if (!has_ref_png (test_file)) {
        parity_n_skip++;
        g_test_skip (parity_local_enabled
                     ? "no local-ref.png (same-renderer subset)"
                     : "no -ref.png (svg-to-svg, resource, or not a PNG reftest)");
        goto out_free;
    }

    /* rust reftests set LANGUAGE=de:en_US:en: so both de and en match.
     * C reads LANG at parse time. Isolate de to this fixture only. */
    if (rel && strcmp (rel, "system-language-de.svg") == 0) {
        saved_lang = g_strdup (g_getenv ("LANG"));
        had_lang = g_getenv ("LANG") != NULL;
        g_setenv ("LANG", "de", TRUE);
        pushed_de_lang = TRUE;
    }

    rsvg = rsvg_handle_new_from_gfile_sync (test_file, 0, NULL, &error);
    if (error || rsvg == NULL) {
        reason = g_strdup_printf ("load-error: %s",
                                  error ? error->message : "NULL handle");
        g_clear_error (&error);
        parity_outcome_fail (rel, reason);
        g_free (reason);
        goto out_free;
    }

    rsvg_handle_internal_set_testing (rsvg, TRUE);
    rsvg_handle_get_dimensions (rsvg, &dimensions);
    if (dimensions.width <= 0 || dimensions.height <= 0) {
        reason = g_strdup_printf ("bad-dimensions: %dx%d",
                                  dimensions.width, dimensions.height);
        parity_outcome_fail (rel, reason);
        g_free (reason);
        g_object_unref (rsvg);
        goto out_free;
    }

    surface_a = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                            dimensions.width, dimensions.height);
    if (cairo_surface_status (surface_a) != CAIRO_STATUS_SUCCESS) {
        reason = g_strdup_printf ("cairo-create: %s",
                                  cairo_status_to_string (cairo_surface_status (surface_a)));
        parity_outcome_fail (rel, reason);
        g_free (reason);
        cairo_surface_destroy (surface_a);
        g_object_unref (rsvg);
        goto out_free;
    }

    cr = cairo_create (surface_a);
    rsvg_handle_render_cairo (rsvg, cr);

    if (parity_local_enabled) {
        GFile *ref_file = parity_ref_png_file (test_file);
        surface_b = ref_file ? read_png_gfile (ref_file) : NULL;
        if (ref_file)
            g_object_unref (ref_file);
    } else {
        surface_b = read_png (test_file_base);
    }
    if (!surface_b || cairo_surface_status (surface_b) != CAIRO_STATUS_SUCCESS) {
        reason = g_strdup ("ref-png-unreadable");
        parity_outcome_fail (rel, reason);
        g_free (reason);
        goto out_surfaces;
    }

    width_a = cairo_image_surface_get_width (surface_a);
    height_a = cairo_image_surface_get_height (surface_a);
    stride_a = cairo_image_surface_get_stride (surface_a);
    width_b = cairo_image_surface_get_width (surface_b);
    height_b = cairo_image_surface_get_height (surface_b);
    stride_b = cairo_image_surface_get_stride (surface_b);

    if (width_a != width_b || height_a != height_b || stride_a != stride_b) {
        reason = g_strdup_printf ("size-mismatch: %dx%d != %dx%d",
                                  width_a, height_a, width_b, height_b);
        parity_outcome_fail (rel, reason);
        g_free (reason);
        goto out_surfaces;
    }

    surface_diff = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                               dimensions.width, dimensions.height);
    compare_surfaces (surface_a, surface_b, surface_diff, &result, FALSE);

    if (!(result.pixels_changed && result.max_diff > 1)) {
        parity_outcome_pass (rel);
    } else if (parity_use_rband () && lookup_rnorm (rel)) {
        const RsvgRNorm *rn = lookup_rnorm (rel);
        char *label = rnorm_label (rel);
        gboolean hole = FALSE;

        if (strcmp (rn->rclass, "TAA") == 0)
            hole = surfaces_have_opaque_vs_empty (surface_a, surface_b);

        if (hole) {
            reason = g_strdup_printf ("%s %s opaque-vs-empty: %u/%u (norm %u/%u)",
                                      rn->rclass, label,
                                      result.pixels_changed, result.max_diff,
                                      rn->norm_px, rn->norm_max_diff);
            save_image (surface_diff, test_file_base, "-diff.png");
            parity_outcome_fail (rel, reason);
            g_free (reason);
        } else if (result.pixels_changed <= rband_px_limit (rn->norm_px)
                   && result.max_diff <= rband_diff_limit (rn->norm_max_diff)) {
            parity_outcome_pass (rel);
        } else {
            reason = g_strdup_printf ("%s %s out of spec: %u/%u (norm %u/%u)",
                                      rn->rclass, label,
                                      result.pixels_changed, result.max_diff,
                                      rn->norm_px, rn->norm_max_diff);
            save_image (surface_diff, test_file_base, "-diff.png");
            parity_outcome_fail (rel, reason);
            g_free (reason);
        }
        g_free (label);
    } else {
        reason = g_strdup_printf ("pixels=%u max_diff=%u",
                                  result.pixels_changed, result.max_diff);
        save_image (surface_diff, test_file_base, "-diff.png");
        parity_outcome_fail (rel, reason);
        g_free (reason);
    }

    cairo_surface_destroy (surface_diff);

out_surfaces:
    cairo_surface_destroy (surface_a);
    if (surface_b)
        cairo_surface_destroy (surface_b);
    if (cr)
        cairo_destroy (cr);
    g_object_unref (rsvg);

out_free:
    if (pushed_de_lang) {
        if (had_lang)
            g_setenv ("LANG", saved_lang ? saved_lang : "", TRUE);
        else
            g_unsetenv ("LANG");
    }
    g_free (saved_lang);
    g_free (test_file_base);
    g_free (rel);
}

static cairo_status_t
read_from_stream (void          *stream,
                  unsigned char *data,
                  unsigned int   length)

{
  gssize result;
  GError *error = NULL;

  result = g_input_stream_read (stream, data, length, NULL, &error);
  g_assert_no_error (error);
  g_assert (result == length);

  return CAIRO_STATUS_SUCCESS;
}

static cairo_surface_t *
read_png_gfile (GFile *file)
{
  GFileInputStream *stream;
  GError *error = NULL;
  cairo_surface_t *surface;

  stream = g_file_read (file, NULL, &error);
  g_assert_no_error (error);
  g_assert (stream);

  surface = cairo_image_surface_create_from_png_stream (read_from_stream, stream);

  g_object_unref (stream);

  return surface;
}

static cairo_surface_t *
read_png (const char *test_name)
{
  char *reference_uri;
  GFile *file;
  cairo_surface_t *surface;

  reference_uri = g_strconcat (test_name, "-ref.png", NULL);
  file = g_file_new_for_uri (reference_uri);
  g_free (reference_uri);

  surface = read_png_gfile (file);
  g_object_unref (file);

  return surface;
}

static void
rsvg_cairo_check (gconstpointer data)
{
    GFile *test_file = G_FILE (data);
    RsvgHandle *rsvg;
    RsvgDimensionData dimensions;
    cairo_t *cr;
    cairo_surface_t *surface_a, *surface_b, *surface_diff;
    buffer_diff_result_t result;
    char *test_file_base;
    char *rel;
    unsigned int width_a, height_a, stride_a;
    unsigned int width_b, height_b, stride_b;
    GError *error = NULL;

    rel = reftest_relpath (test_file);
    test_file_base = g_file_get_uri (test_file);
    if (g_str_has_suffix (test_file_base, ".svg"))
      test_file_base[strlen (test_file_base) - strlen (".svg")] = '\0';

    rsvg = rsvg_handle_new_from_gfile_sync (test_file, 0, NULL, &error);
    g_assert_no_error (error);
    g_assert (rsvg != NULL);

    rsvg_handle_internal_set_testing (rsvg, TRUE);

    rsvg_handle_get_dimensions (rsvg, &dimensions);
    g_assert (dimensions.width > 0);
    g_assert (dimensions.height > 0);
    surface_a = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
					    dimensions.width, dimensions.height);
    cr = cairo_create (surface_a);
    rsvg_handle_render_cairo (rsvg, cr);
    save_image (surface_a, test_file_base, "-out.png");

    surface_b = read_png (test_file_base);
    width_a = cairo_image_surface_get_width (surface_a);
    height_a = cairo_image_surface_get_height (surface_a);
    stride_a = cairo_image_surface_get_stride (surface_a);
    width_b = cairo_image_surface_get_width (surface_b);
    height_b = cairo_image_surface_get_height (surface_b);
    stride_b = cairo_image_surface_get_stride (surface_b);

    if (width_a  != width_b  ||
	height_a != height_b ||
	stride_a != stride_b) {
        g_test_fail ();
        g_test_message ("Image size mismatch (%dx%d != %dx%d)\n",
                        width_a, height_a, width_b, height_b); 
    }
    else {
	const RsvgRNorm *rn;
	surface_diff = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
						   dimensions.width, dimensions.height);

	compare_surfaces (surface_a, surface_b, surface_diff, &result, FALSE);

	if (!(result.pixels_changed && result.max_diff > 1)) {
            /* perfect match — silent */
	} else if ((rn = lookup_check_rnorm (rel))) {
            char *label = rnorm_label (rel);
            gboolean hole = FALSE;

            if (strcmp (rn->rclass, "TAA") == 0)
                hole = surfaces_have_opaque_vs_empty (surface_a, surface_b);

            if (hole) {
                g_test_message ("%s %s opaque-vs-empty: %u/%u (norm %u/%u)",
                                rn->rclass, label,
                                result.pixels_changed, result.max_diff,
                                rn->norm_px, rn->norm_max_diff);
                save_image (surface_diff, test_file_base, "-diff.png");
                g_test_fail ();
            } else if (result.pixels_changed <= rband_px_limit (rn->norm_px)
                       && result.max_diff <= rband_diff_limit (rn->norm_max_diff)) {
                /* in-band residual vs the 2.40 C PNG — silent */
            } else {
                g_test_message ("%s %s out of spec: %u/%u (norm %u/%u)",
                                rn->rclass, label,
                                result.pixels_changed, result.max_diff,
                                rn->norm_px, rn->norm_max_diff);
                save_image (surface_diff, test_file_base, "-diff.png");
                g_test_fail ();
            }
            g_free (label);
	} else {
            g_test_message ("%d pixels differ (with maximum difference of %d) from reference image\n",
                            result.pixels_changed, result.max_diff);
            g_test_fail ();
            save_image (surface_diff, test_file_base, "-diff.png");
	}

	cairo_surface_destroy (surface_diff);
    }

    cairo_surface_destroy (surface_a);
    cairo_surface_destroy (surface_b);
    cairo_destroy (cr);

    g_object_unref (rsvg);
    g_free (test_file_base);
    g_free (rel);
}

int
main (int argc, char **argv)
{
    int result;
    const char *xfail_in;

    RSVG_G_TYPE_INIT;
    g_test_init (&argc, &argv, NULL);

    rsvg_set_default_dpi_x_y (72, 72);

    /* RSVG_PARITY=1 runs tests/fixtures/parity (2.62.3 corpus).
     * Default (unset) keeps the 2.52.0 reftest subset so make check stays green.
     */
    parity_enabled = g_getenv ("RSVG_PARITY") != NULL
        && g_strcmp0 (g_getenv ("RSVG_PARITY"), "0") != 0;

    parity_local_enabled = g_getenv ("RSVG_PARITY_LOCAL") != NULL
        && g_strcmp0 (g_getenv ("RSVG_PARITY_LOCAL"), "0") != 0;
    if (parity_local_enabled) {
        const char *dir = g_getenv ("RSVG_PARITY_LOCAL_DIR");
        if (dir && *dir)
            parity_local_dir = g_strdup (dir);
        else
            parity_local_dir = g_build_filename (test_utils_get_test_data_path (),
                                                 "parity-local", NULL);
        parity_enabled = TRUE;
    }

    xfail_in = g_getenv ("RSVG_PARITY_XFAIL");
    if (xfail_in && !parity_local_enabled)
        load_parity_xfail (xfail_in);

    if (parity_enabled && !parity_local_enabled) {
        const char *rn_in = g_getenv ("RSVG_PARITY_TNORMS");
        char *rn_default = NULL;

        if (rn_in && *rn_in) {
            load_parity_rnorms (rn_in);
        } else {
            rn_default = g_build_filename (g_test_get_dir (G_TEST_DIST),
                                           "parity-t-numbers.txt", NULL);
            load_parity_rnorms (rn_default);
            g_free (rn_default);
        }
    } else if (!parity_local_enabled) {
        const char *rn_in = g_getenv ("RSVG_CHECK_TNORMS");
        char *rn_default = NULL;

        if (rn_in && *rn_in) {
            load_check_rnorms (rn_in);
        } else {
            rn_default = g_build_filename (g_test_get_dir (G_TEST_DIST),
                                           "make-check-t-numbers.txt", NULL);
            load_check_rnorms (rn_default);
            g_free (rn_default);
        }
    }

    parity_xfail_out_path = g_strdup (g_getenv ("RSVG_PARITY_XFAIL_OUT"));
    if (parity_xfail_out_path)
        parity_xfail_generated = g_ptr_array_new_with_free_func (g_free);

    if (argc < 2) {
        GFile *base, *tests;

        base = g_file_new_for_path (test_utils_get_test_data_path ());
        tests = NULL;
        if (parity_local_enabled && g_getenv ("RSVG_PARITY_LOCAL_LIST")) {
            add_parity_from_list (g_getenv ("RSVG_PARITY_LOCAL_LIST"));
        } else if (parity_enabled) {
            tests = g_file_get_child (base, "parity");
            test_utils_add_test_for_all_files (parity_local_enabled
                                               ? "/rsvg/parity-local" : "/rsvg/parity",
                                               tests, tests,
                                               rsvg_parity_check, is_parity_svg_or_subdir);
        } else {
            tests = g_file_get_child (base, "reftests");
            test_utils_add_test_for_all_files ("/rsvg/reftest", tests, tests,
                                               rsvg_cairo_check, is_svg_or_subdir);
        }
        if (tests)
            g_object_unref (tests);
        g_object_unref (base);
    } else {
        guint i;

        for (i = 1; i < argc; i++) {
            GFile *file = g_file_new_for_commandline_arg (argv[i]);

            if (parity_enabled || is_parity_file (file)) {
                parity_enabled = TRUE;
                test_utils_add_test_for_all_files ("/rsvg/parity", NULL, file,
                                                   rsvg_parity_check, is_parity_svg_or_subdir);
            } else {
                test_utils_add_test_for_all_files ("/rsvg/reftest", NULL, file,
                                                   rsvg_cairo_check, is_svg_or_subdir);
            }

            g_object_unref (file);
        }
    }

    /* g_test_init makes g_warning fatal. 2.62 fixtures exercise CSS that
     * libcroco rejects with "CSS parsing error"; those must be recorded as
     * xfails, not abort the suite. Baseline make check is unchanged.
     */
    if (parity_enabled)
        g_log_set_always_fatal (G_LOG_FATAL_MASK);

    result = g_test_run ();

    if (parity_enabled)
        parity_print_summary ();

    rsvg_cleanup ();

    return result;
}

