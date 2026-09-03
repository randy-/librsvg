/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 sts=4 ts=4 expandtab: */
/*
 * License: Public Domain.
 * Author: Robert Staudinger <robsta@gnome.org>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <rsvg.h>
#include "rsvg-compat.h"

static void
show_help (GOptionContext *context)
{
    char *help;
    help = g_option_context_get_help (context, TRUE, NULL);
    perror (help);
    g_free (help), help = NULL;
}

int
main (int	  argc,
      char	**argv)
{
    GOptionContext      *context;
    char const          *fragment;
    char const         **filenames;
    char const          *file;
    RsvgHandle          *handle;
    GError              *error;
    int                  exit_code;
    int                  i;

    GOptionEntry options[] = {
        { "fragment", 'f', 0, G_OPTION_ARG_STRING, &fragment, "The SVG fragment to address.", "<string>" },
        { G_OPTION_REMAINING, 0, G_OPTION_FLAG_FILENAME, G_OPTION_ARG_FILENAME_ARRAY, &filenames, NULL, "[FILE...]" },
        { NULL }
    };

    RSVG_G_TYPE_INIT;

    context = NULL;
    fragment = NULL;
    filenames = NULL;
    handle = NULL;
    error = NULL;

    context = g_option_context_new ("- SVG measuring tool.");
    g_option_context_add_main_entries (context, options, NULL);

    /* No args? */
    if (argc < 2) {
        show_help (context);
        exit_code = EXIT_SUCCESS;
        goto bail;
    }

    error = NULL;
    g_option_context_parse (context, &argc, &argv, &error);
    if (error) {
        show_help (context);
        g_warning ("%s", error->message);
        exit_code = EXIT_FAILURE;
        goto bail;
    }

    /* Invalid / missing args? */
    if (filenames == NULL) {
        show_help (context);
        exit_code = EXIT_FAILURE;
        goto bail;
    }

    g_option_context_free (context), context = NULL;

    for (i = 0; NULL != (file = filenames[i]); i++) {

        error = NULL;
        handle = rsvg_handle_new_from_file (file, &error);
        if (error) {
            g_warning ("%s", error->message);
            exit_code = EXIT_FAILURE;
            goto bail;
        }

        if (fragment && handle) {
            RsvgRectangle viewport, ink;
            double dw = 0, dh = 0;
            char *id = NULL;

            if (!rsvg_handle_get_intrinsic_size_in_pixels (handle, &dw, &dh)
                || dw <= 0 || dh <= 0) {
                dw = 1;
                dh = 1;
            }
            viewport.x = viewport.y = 0;
            viewport.width = dw;
            viewport.height = dh;
            if (fragment[0] == '#')
                id = g_strdup (fragment);
            else
                id = g_strdup_printf ("#%s", fragment);
            if (!rsvg_handle_get_geometry_for_layer (handle, id, &viewport,
                                                     &ink, NULL, &error)) {
                g_warning ("%s: fragment `'%s' not found.",
                        file, fragment);
                g_free (id);
                exit_code = EXIT_FAILURE;
                goto bail;
            }
            g_free (id);

            printf ("%s, fragment `%s': x=%d, y=%d, %dx%d, em=%f, ex=%f\n",
                    file, fragment,
                    (int) (ink.x + 0.5), (int) (ink.y + 0.5),
                    (int) (ink.width + 0.5), (int) (ink.height + 0.5),
                    ink.width, ink.height);

        } else if (handle) {
            double w = 0, h = 0;

            if (!rsvg_handle_get_intrinsic_size_in_pixels (handle, &w, &h)
                || w <= 0 || h <= 0) {
                RsvgRectangle ink = { 0, 0, 0, 0 };

                rsvg_handle_get_geometry_for_element (handle, NULL, &ink, NULL, NULL);
                w = ink.width;
                h = ink.height;
            }
            if (w <= 0)
                w = 1;
            if (h <= 0)
                h = 1;
            printf ("%s: %dx%d, em=%f, ex=%f\n", file,
                    (int) (w + 0.5), (int) (h + 0.5),
                    w, h);
        } else {
            g_warning ("Could not open file `%s'", file);
            exit_code = EXIT_FAILURE;
            goto bail;
        }

        g_object_unref (handle), handle = NULL;
    }

    exit_code = EXIT_SUCCESS;

bail:
    if (handle)
        g_object_unref (handle), handle = NULL;
    if (context)
        g_option_context_free (context), context = NULL;
    if (error)
        g_error_free (error), error = NULL;

    return exit_code;
}
