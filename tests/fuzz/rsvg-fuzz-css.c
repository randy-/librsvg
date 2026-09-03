/*
   tests/fuzz/rsvg-fuzz-css.c: CSS/SVG harness (default css/ path).

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
   License along with this program; if not, see LICENCE-LIB.
*/

#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <glib.h>
#include <cairo.h>

#define RSVG_DISABLE_DEPRECATION_WARNINGS
#include "rsvg.h"

/* One case cannot eat the machine. Truncate, do not reject, so AFL
 * still exercises the prefix. */
#define FUZZ_MAX_INPUT ((gsize) 64 * 1024)

#ifndef FUZZ_SEED_DIR
#define FUZZ_SEED_DIR "tests/fuzz/seeds"
#endif

/* Dummy document URL: rsvg_allow_load denies http(s)/ftp; with this
 * base, file: outside the (non-existent) dir is denied; data: is ok. */
static const char dummy_base[] = "file:///nonexistent/rsvg-fuzz-css/x.svg";

static const char builtin_svg[] =
    "<svg xmlns='http://www.w3.org/2000/svg' width='8' height='8'>"
    "<rect class='x' id='y' width='8' height='8' fill='red'/>"
    "</svg>";

static guint64 n_tried;
static guint64 n_loaded;
static guint64 n_rendered;

static gboolean
looks_like_markup (const guint8 *data, gsize len)
{
    gsize i;

    for (i = 0; i < len; i++) {
        guint8 c = data[i];
        if (c == 0xEF && i + 2 < len && data[i + 1] == 0xBB && data[i + 2] == 0xBF) {
            i += 2;
            continue;
        }
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
            continue;
        return c == '<';
    }
    return FALSE;
}

static void
split_svg_css (const guint8 *data, gsize len,
               const guint8 **svg, gsize *svg_len,
               const guint8 **css, gsize *css_len)
{
    gsize i;

    *svg = data;
    *svg_len = len;
    *css = NULL;
    *css_len = 0;

    /* 0xFF 0x00 0xFF separator: SVG then User stylesheet. */
    for (i = 0; i + 3 <= len; i++) {
        if (data[i] == 0xFF && data[i + 1] == 0x00 && data[i + 2] == 0xFF) {
            *svg_len = i;
            *css = data + i + 3;
            *css_len = len - (i + 3);
            return;
        }
    }
}

/* GError / failed load / failed render is success for the fuzzer. */
static void
fuzz_one (const guint8 *data, gsize len)
{
    RsvgHandle *handle;
    GError *error = NULL;
    const guint8 *svg;
    gsize svg_len;
    const guint8 *css;
    gsize css_len;
    cairo_surface_t *surface;
    cairo_t *cr;
    RsvgRectangle vp = { 0, 0, 8, 8 };
    GString *wrapped = NULL;
    const guint8 *feed;
    gsize feed_len;

    n_tried++;

    if (data == NULL)
        return;
    if (len > FUZZ_MAX_INPUT)
        len = FUZZ_MAX_INPUT;

    split_svg_css (data, len, &svg, &svg_len, &css, &css_len);

    feed = svg;
    feed_len = svg_len;
    if (feed_len == 0 || !looks_like_markup (feed, feed_len)) {
        wrapped = g_string_new (
            "<svg xmlns='http://www.w3.org/2000/svg' width='8' height='8'><style>");
        if (feed && feed_len)
            g_string_append_len (wrapped, (const char *) feed, feed_len);
        g_string_append (wrapped,
                         "</style><rect class='x' id='y' width='8' height='8'/></svg>");
        feed = (const guint8 *) wrapped->str;
        feed_len = wrapped->len;
    }

    handle = rsvg_handle_new ();
    if (handle == NULL) {
        if (wrapped)
            g_string_free (wrapped, TRUE);
        return;
    }

    rsvg_handle_set_base_uri (handle, dummy_base);

    if (css && css_len > 0) {
        rsvg_handle_set_stylesheet (handle, css, css_len, &error);
        g_clear_error (&error);
    }

    if (!rsvg_handle_write (handle, feed, feed_len, &error)) {
        g_clear_error (&error);
        rsvg_handle_close (handle, &error);
        g_clear_error (&error);
        g_object_unref (handle);
        if (wrapped)
            g_string_free (wrapped, TRUE);
        return;
    }
    if (!rsvg_handle_close (handle, &error)) {
        g_clear_error (&error);
        g_object_unref (handle);
        if (wrapped)
            g_string_free (wrapped, TRUE);
        return;
    }
    g_clear_error (&error);
    n_loaded++;

    /* Same-blob User sheet: invalid CSS is a GError, not a crash. */
    if (css_len == 0 && svg_len > 0) {
        rsvg_handle_set_stylesheet (handle, svg, svg_len, &error);
        g_clear_error (&error);
    }

    surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 8, 8);
    cr = cairo_create (surface);
    if (rsvg_handle_render_document (handle, cr, &vp, &error))
        n_rendered++;
    g_clear_error (&error);
    cairo_destroy (cr);
    cairo_surface_destroy (surface);
    g_object_unref (handle);
    if (wrapped)
        g_string_free (wrapped, TRUE);
}

int
LLVMFuzzerTestOneInput (const guint8 *data, size_t size)
{
    fuzz_one (data, (gsize) size);
    return 0;
}

/* libFuzzer (-fsanitize=fuzzer) supplies main. AFL++ afl-cc also defines
 * FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION but still needs our main for @@. */
#if !defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION) || defined(RSVG_FUZZ_AFL_MAIN)

static guint8 last_data[FUZZ_MAX_INPUT];
static gsize last_len;
static const char *hang_path = "/tmp/rsvg-fuzz-css-hang.bin";

static void
on_alrm (int sig)
{
    int fd;
    const char msg[] = "fuzz-css: hang (3s), wrote /tmp/rsvg-fuzz-css-hang.bin\n";

    (void) sig;
    (void) write (2, msg, sizeof msg - 1);
    fd = open (hang_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        if (last_len > 0)
            (void) write (fd, last_data, last_len);
        (void) close (fd);
    }
    _exit (124);
}

static void
fuzz_one_guarded (const guint8 *data, gsize len)
{
    gsize n = len;

    if (n > FUZZ_MAX_INPUT)
        n = FUZZ_MAX_INPUT;
    last_len = 0;
    if (data != NULL && n > 0) {
        memcpy (last_data, data, n);
        last_len = n;
    }
    alarm (3);
    fuzz_one (data, len);
    alarm (0);
}

static guint32
xorshift32 (guint32 *s)
{
    guint32 x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x ? x : 0xA5A5A5A5u;
    return *s;
}

static void
drop_warning (const gchar *domain, GLogLevelFlags flags,
              const gchar *message, gpointer user_data)
{
    (void) domain;
    (void) flags;
    (void) message;
    (void) user_data;
}

static void
fuzz_file (const char *path)
{
    gchar *buf = NULL;
    gsize len = 0;
    GError *error = NULL;

    if (!g_file_get_contents (path, &buf, &len, &error)) {
        g_printerr ("fuzz-css: skip %s: %s\n", path, error->message);
        g_clear_error (&error);
        return;
    }
    fuzz_one_guarded ((const guint8 *) buf, len);
    g_free (buf);
}

static void
fuzz_path (const char *path)
{
    GDir *dir;
    const gchar *name;
    gchar *full;

    if (g_file_test (path, G_FILE_TEST_IS_DIR)) {
        dir = g_dir_open (path, 0, NULL);
        if (dir == NULL)
            return;
        while ((name = g_dir_read_name (dir)) != NULL) {
            if (name[0] == '.')
                continue;
            full = g_build_filename (path, name, NULL);
            fuzz_path (full);
            g_free (full);
        }
        g_dir_close (dir);
        return;
    }

    if (g_file_test (path, G_FILE_TEST_IS_REGULAR))
        fuzz_file (path);
}

static void
fuzz_stdin (void)
{
    GString *s = g_string_new (NULL);
    char buf[4096];
    ssize_t n;

    while ((n = read (0, buf, sizeof (buf))) > 0)
        g_string_append_len (s, buf, n);
    fuzz_one_guarded ((const guint8 *) s->str, s->len);
    g_string_free (s, TRUE);
}

static void
mutate_loop (const guint8 **seeds, const gsize *seed_lens, int n_seeds,
             int n_mut, int seconds, guint32 rng)
{
    GTimer *timer = g_timer_new ();
    int i;

    if (n_seeds <= 0)
        return;

    i = 0;
    while (1) {
        const guint8 *src;
        gsize slen;
        guint8 *m;
        gsize mlen;
        int flips, f;
        guint32 r;

        if (n_mut >= 0 && i >= n_mut)
            break;
        if (seconds > 0 && g_timer_elapsed (timer, NULL) >= (gdouble) seconds)
            break;
        if (n_mut < 0 && seconds <= 0)
            break;

        src = seeds[i % n_seeds];
        slen = seed_lens[i % n_seeds];
        if (src == NULL || slen == 0) {
            i++;
            continue;
        }
        r = xorshift32 (&rng);
        mlen = 1 + (r % (slen + 32));
        if (mlen > FUZZ_MAX_INPUT)
            mlen = FUZZ_MAX_INPUT;
        m = g_malloc (mlen);
        memcpy (m, src, MIN (slen, mlen));
        if (mlen > slen)
            memset (m + slen, (guint8) xorshift32 (&rng), mlen - slen);

        flips = 1 + (int) (xorshift32 (&rng) % 12);
        for (f = 0; f < flips; f++) {
            gsize off = xorshift32 (&rng) % mlen;
            m[off] ^= (guint8) (1u << (xorshift32 (&rng) % 8));
        }
        /* Occasional CSS-separator so set_stylesheet sees a second blob. */
        if ((xorshift32 (&rng) % 8) == 0 && mlen >= 8) {
            gsize at = xorshift32 (&rng) % (mlen - 3);
            m[at] = 0xFF;
            m[at + 1] = 0x00;
            m[at + 2] = 0xFF;
        }

        fuzz_one_guarded (m, mlen);
        g_free (m);
        i++;
        if ((i % 1000) == 0)
            g_printerr ("fuzz-css: mutate %d tried=%" G_GUINT64_FORMAT "\n",
                        i, n_tried);
    }
    g_timer_destroy (timer);
}

static void
usage (FILE *out)
{
    fprintf (out,
             "Usage: rsvg-fuzz-css [options] [files-or-dirs|-]\n"
             "  --seeds DIR     seed directory (default: compiled-in %s)\n"
             "  --mutate N      extra bit-flip mutations (default 256)\n"
             "  --seconds N     keep mutating for N seconds\n"
             "  --help\n"
             "\n"
             "Not part of make check. GError / omit is success.\n"
             "AFL++:  make fuzz-afl && afl-fuzz -i tests/fuzz/seeds -o tests/fuzz/out -- ./tests/fuzz/rsvg-fuzz-css-afl @@\n"
             "Dumb loop: ./tests/fuzz/rsvg-fuzz-css --seconds 3600\n"
             "libFuzzer: clang -fsanitize=fuzzer ... tests/fuzz/rsvg-fuzz-css.c -o rsvg-fuzz-css\n",
             FUZZ_SEED_DIR);
}

int
main (int argc, char **argv)
{
    const char *seed_dir = FUZZ_SEED_DIR;
    int n_mut = 256;
    int seconds = 0;
    gboolean mutate_set = FALSE;
    int i;
    GPtrArray *seed_files;
    guint8 **seed_bufs = NULL;
    gsize *seed_lens = NULL;
    int n_seeds = 0;
    gboolean have_inputs = FALSE;

    g_log_set_handler ("librsvg",
                       G_LOG_LEVEL_WARNING | G_LOG_LEVEL_MESSAGE,
                       drop_warning, NULL);
    signal (SIGALRM, on_alrm);

    for (i = 1; i < argc; i++) {
        if (strcmp (argv[i], "--help") == 0 || strcmp (argv[i], "-h") == 0) {
            usage (stdout);
            return 0;
        }
        if (strcmp (argv[i], "--seeds") == 0 && i + 1 < argc) {
            seed_dir = argv[++i];
            continue;
        }
        if (strcmp (argv[i], "--mutate") == 0 && i + 1 < argc) {
            n_mut = atoi (argv[++i]);
            mutate_set = TRUE;
            continue;
        }
        if (strcmp (argv[i], "--seconds") == 0 && i + 1 < argc) {
            seconds = atoi (argv[++i]);
            continue;
        }
        if (strcmp (argv[i], "-") == 0) {
            fuzz_stdin ();
            have_inputs = TRUE;
            continue;
        }
        if (argv[i][0] == '-') {
            g_printerr ("fuzz-css: unknown option %s\n", argv[i]);
            usage (stderr);
            return 2;
        }
        fuzz_path (argv[i]);
        have_inputs = TRUE;
    }

    /* AFL @@ / one-shot files: load each blob once. No --seconds here. */
    if (have_inputs && !mutate_set && seconds <= 0)
        n_mut = 0;
    /* --seconds without --mutate: keep mutating until the timer. */
    if (seconds > 0 && !mutate_set)
        n_mut = -1;

    seed_files = g_ptr_array_new ();
    if (!have_inputs || n_mut > 0 || seconds > 0) {
        GDir *dir = g_dir_open (seed_dir, 0, NULL);
        const gchar *name;

        if (dir) {
            while ((name = g_dir_read_name (dir)) != NULL) {
                gchar *full;
                if (name[0] == '.')
                    continue;
                full = g_build_filename (seed_dir, name, NULL);
                if (g_file_test (full, G_FILE_TEST_IS_REGULAR))
                    g_ptr_array_add (seed_files, full);
                else
                    g_free (full);
            }
            g_dir_close (dir);
        }
    }

    if (!have_inputs) {
        for (i = 0; i < (int) seed_files->len; i++)
            fuzz_file (g_ptr_array_index (seed_files, i));
        if (seed_files->len == 0)
            g_printerr ("fuzz-css: no seeds in %s\n", seed_dir);
    }

    if ((n_mut > 0 || seconds > 0) && seed_files->len > 0) {
        n_seeds = (int) seed_files->len;
        seed_bufs = g_new0 (guint8 *, n_seeds);
        seed_lens = g_new0 (gsize, n_seeds);
        for (i = 0; i < n_seeds; i++) {
            gchar *buf = NULL;
            gsize len = 0;
            if (g_file_get_contents (g_ptr_array_index (seed_files, i),
                                     &buf, &len, NULL)) {
                seed_bufs[i] = (guint8 *) buf;
                seed_lens[i] = len;
            }
        }
        mutate_loop ((const guint8 **) seed_bufs, seed_lens, n_seeds,
                     seconds > 0 ? -1 : n_mut, seconds, 0xC0FFEE01u);
        for (i = 0; i < n_seeds; i++)
            g_free (seed_bufs[i]);
        g_free (seed_bufs);
        g_free (seed_lens);
    }

    for (i = 0; i < (int) seed_files->len; i++)
        g_free (g_ptr_array_index (seed_files, i));
    g_ptr_array_free (seed_files, TRUE);

    /* One-shot file (AFL @@): stay quiet; GError already counted as success. */
    if (!(have_inputs && n_mut == 0 && seconds <= 0))
        g_print ("fuzz-css: tried=%" G_GUINT64_FORMAT " loaded=%" G_GUINT64_FORMAT
                 " rendered=%" G_GUINT64_FORMAT " (GError is success; process stayed up)\n",
                 n_tried, n_loaded, n_rendered);
    return 0;
}

#endif /* !libFuzzer (or RSVG_FUZZ_AFL_MAIN) */
