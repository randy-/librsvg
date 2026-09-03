/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
   rsvg-css-engine.h: CSS cascade and selector matcher.

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

#ifndef RSVG_CSS_ENGINE_H
#define RSVG_CSS_ENGINE_H

#include <glib.h>

G_BEGIN_DECLS

typedef struct _RsvgHandle RsvgHandle;
typedef struct _RsvgNode RsvgNode;
typedef struct _RsvgCssEngine RsvgCssEngine;

typedef enum {
    RSVG_CSS_ORIGIN_UA = 0,
    RSVG_CSS_ORIGIN_USER = 1,
    RSVG_CSS_ORIGIN_AUTHOR = 2
} RsvgCssOrigin;

/* Always TRUE. RSVG_CSS_ENGINE=croco|old|libcroco|0|off is ignored
 * (libcroco is discontinued; one g_warning). */
gboolean         rsvg_css_engine_enabled (void);

RsvgCssEngine   *rsvg_css_engine_new (void);
void             rsvg_css_engine_free (RsvgCssEngine *eng);

void             rsvg_css_engine_add_sheet (RsvgCssEngine *eng,
                                            RsvgCssOrigin origin,
                                            const char *css,
                                            gsize len,
                                            RsvgHandle *ctx,
                                            gboolean allow_external_import);

void             rsvg_css_engine_clear_user (RsvgCssEngine *eng);

/* Re-init each element's state, apply presentation attrs, cascade, style="". */
void             rsvg_css_engine_cascade (RsvgCssEngine *eng,
                                          RsvgHandle *handle,
                                          RsvgNode *root);

void             rsvg_css_engine_cascade_handle (RsvgHandle *handle);

G_END_DECLS

#endif /* RSVG_CSS_ENGINE_H */
