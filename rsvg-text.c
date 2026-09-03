/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 sts=4 ts=4 expandtab: */
/*
   rsvg-text.c: Text handling routines for RSVG

   Copyright (C) 2000 Eazel, Inc.
   Copyright (C) 2002 Dom Lachowicz <cinamod@hotmail.com>
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

   Author: Raph Levien <raph@artofcode.com>
*/

#include <string.h>

#include "rsvg-private.h"
#include "rsvg-styles.h"
#include "rsvg-text.h"
#include "rsvg-css.h"

#include "rsvg-shapes.h"

/* what we use for text rendering depends on what cairo has to offer */
#include <pango/pangocairo.h>

typedef struct _RsvgNodeText RsvgNodeText;

struct _RsvgNodeText {
    RsvgNode super;
    RsvgCssLength x, y, dx, dy;
};

typedef struct _RsvgNodeTref RsvgNodeTref;

struct _RsvgNodeTref {
    RsvgNode super;
    char *link;
};
char *
rsvg_make_valid_utf8 (const char *str, int len)
{
    GString *string;
    const char *remainder, *invalid;
    int remaining_bytes, valid_bytes;

    string = NULL;
    remainder = str;

    if (len < 0)
        remaining_bytes = strlen (str);
    else
        remaining_bytes = len;

    while (remaining_bytes != 0) {
        if (g_utf8_validate (remainder, remaining_bytes, &invalid))
            break;
        valid_bytes = invalid - remainder;

        if (string == NULL)
            string = g_string_sized_new (remaining_bytes);

        g_string_append_len (string, remainder, valid_bytes);
        g_string_append_c (string, '?');

        remaining_bytes -= valid_bytes + 1;
        remainder = invalid + 1;
    }

    if (string == NULL)
        return len < 0 ? g_strndup (str, len) : g_strdup (str);

    g_string_append (string, remainder);

    return g_string_free (string, FALSE);
}

static gboolean
_rsvg_node_has_prev_sibling (RsvgNode *node)
{
    RsvgNode *parent = node->parent;
    guint i;

    if (!parent || !parent->children)
        return FALSE;
    for (i = 0; i < parent->children->len; i++) {
        if (g_ptr_array_index (parent->children, i) == node)
            return i > 0;
    }
    return FALSE;
}

static gboolean
_rsvg_node_has_next_sibling (RsvgNode *node)
{
    RsvgNode *parent = node->parent;
    guint i;

    if (!parent || !parent->children)
        return FALSE;
    for (i = 0; i < parent->children->len; i++) {
        if (g_ptr_array_index (parent->children, i) == node)
            return (i + 1) < parent->children->len;
    }
    return FALSE;
}

/* SVG1.1 xml:space, matching rust/rsvg/src/space.rs (2.62.3).
 * CSS white-space is parsed but not applied on <text>, same as 2.62. */
static GString *
_rsvg_text_chomp (RsvgState *state, RsvgNode *chars_node, gboolean * lastwasspace)
{
    const char *s = ((RsvgNodeChars *) chars_node)->contents->str;
    GString *out = g_string_new (NULL);
    const char *p;

    (void) lastwasspace;

    if (state->space_preserve) {
        for (p = s; *p; p++)
            g_string_append_c (out, (*p == '\n' || *p == '\t') ? ' ' : *p);
        return out;
    }

    {
        GString *tmp = g_string_new (NULL);
        const char *start, *end;
        gboolean in_space = FALSE;
        gboolean before = _rsvg_node_has_prev_sibling (chars_node);
        gboolean after = _rsvg_node_has_next_sibling (chars_node);

        for (p = s; *p; p++) {
            if (*p == '\n')
                continue;
            g_string_append_c (tmp, (*p == '\t') ? ' ' : *p);
        }

        start = tmp->str;
        end = tmp->str + tmp->len;
        if (!before) {
            while (start < end && *start == ' ')
                start++;
        }
        if (!after) {
            while (end > start && end[-1] == ' ')
                end--;
        }

        for (p = start; p < end; p++) {
            if (*p == ' ') {
                if (!in_space)
                    g_string_append_c (out, ' ');
                in_space = TRUE;
            } else {
                g_string_append_c (out, *p);
                in_space = FALSE;
            }
        }
        g_string_free (tmp, TRUE);
    }

    return out;
}

static gboolean
_rsvg_text_node_is_link (RsvgNode *node)
{
    return RSVG_NODE_TYPE (node) == RSVG_NODE_TYPE_GROUP
        && node->css_tag && strcmp (node->css_tag, "a") == 0;
}

/* 2.62 accepts a list in x/y/dx/dy but only uses the first value. */
static RsvgCssLength
_rsvg_text_parse_first_length (const char *value)
{
    char *copy, *p;
    RsvgCssLength len;

    copy = g_strdup (value);
    for (p = copy; *p; p++) {
        if (g_ascii_isspace (*p) || *p == ',') {
            *p = '\0';
            break;
        }
    }
    len = _rsvg_css_parse_length (copy);
    g_free (copy);
    return len;
}


static void
_rsvg_node_text_set_atts (RsvgNode * self, RsvgHandle * ctx, RsvgPropertyBag * atts)
{
    const char *klazz = NULL, *id = NULL, *value;
    RsvgNodeText *text = (RsvgNodeText *) self;

    if (rsvg_property_bag_size (atts)) {
        if ((value = rsvg_property_bag_lookup (atts, "x")))
            text->x = _rsvg_text_parse_first_length (value);
        if ((value = rsvg_property_bag_lookup (atts, "y")))
            text->y = _rsvg_text_parse_first_length (value);
        if ((value = rsvg_property_bag_lookup (atts, "dx")))
            text->dx = _rsvg_text_parse_first_length (value);
        if ((value = rsvg_property_bag_lookup (atts, "dy")))
            text->dy = _rsvg_text_parse_first_length (value);
        if ((value = rsvg_property_bag_lookup (atts, "class")))
            klazz = value;
        if ((value = rsvg_property_bag_lookup (atts, "id"))) {
            id = value;
            rsvg_defs_register_name (ctx->priv->defs, value, self);
        }

        rsvg_parse_style_attrs (ctx, self->state, "text", klazz, id, atts);
    }
}

static void
 rsvg_text_render_text (RsvgDrawingCtx * ctx, const char *text, gdouble * x, gdouble * y);


static void
 _rsvg_node_text_type_tspan (RsvgNodeText * self, RsvgDrawingCtx * ctx,
                             gdouble * x, gdouble * y, gboolean * lastwasspace,
                             gboolean usetextonly);

static void
 _rsvg_node_text_type_tref (RsvgNodeTref * self, RsvgDrawingCtx * ctx,
                            gdouble * x, gdouble * y, gboolean * lastwasspace,
                            gboolean usetextonly);

/* This function is responsible of selecting render for a text element including its children and giving it the drawing context */
static void
_rsvg_node_text_type_children (RsvgNode * self, RsvgDrawingCtx * ctx,
                               gdouble * x, gdouble * y, gboolean * lastwasspace,
                               gboolean usetextonly)
{
    guint i;

    if (!ctx->text_measure_only)
        rsvg_push_discrete_layer (ctx);
    for (i = 0; i < self->children->len; i++) {
        RsvgNode *node = g_ptr_array_index (self->children, i);
        RsvgNodeType type = RSVG_NODE_TYPE (node);

        if (type == RSVG_NODE_TYPE_CHARS) {
            GString *str = _rsvg_text_chomp (rsvg_current_state (ctx), node, lastwasspace);
            rsvg_text_render_text (ctx, str->str, x, y);
            g_string_free (str, TRUE);
        } else {
            if (usetextonly) {
                _rsvg_node_text_type_children (node, ctx, x, y, lastwasspace,
                                               usetextonly);
            } else {
                if (type == RSVG_NODE_TYPE_TSPAN) {
                    RsvgNodeText *tspan = (RsvgNodeText *) node;
                    rsvg_state_push (ctx);
                    rsvg_state_reinherit_top (ctx, node->state, 0);
                    if (!rsvg_current_state (ctx)->display_none)
                        _rsvg_node_text_type_tspan (tspan, ctx, x, y, lastwasspace,
                                                    usetextonly);
                    rsvg_state_pop (ctx);
                } else if (type == RSVG_NODE_TYPE_TREF) {
                    RsvgNodeTref *tref = (RsvgNodeTref *) node;
                    _rsvg_node_text_type_tref (tref, ctx, x, y, lastwasspace,
                                               usetextonly);
                } else if (_rsvg_text_node_is_link (node)) {
                    /* 2.62 treats <a> inside <text> as a zero-offset tspan. */
                    rsvg_state_push (ctx);
                    rsvg_state_reinherit_top (ctx, node->state, 0);
                    if (!rsvg_current_state (ctx)->display_none)
                        _rsvg_node_text_type_children (node, ctx, x, y, lastwasspace,
                                                       usetextonly);
                    rsvg_state_pop (ctx);
                }
            }
        }
    }
    if (!ctx->text_measure_only)
        rsvg_pop_discrete_layer (ctx);
}

static int
 _rsvg_node_text_length_tref (RsvgNodeTref * self, RsvgDrawingCtx * ctx,
                              gdouble * x, gboolean * lastwasspace,
                              gboolean usetextonly);

static int
 _rsvg_node_text_length_tspan (RsvgNodeText * self, RsvgDrawingCtx * ctx,
                               gdouble * x, gboolean * lastwasspace,
                               gboolean usetextonly);

static gdouble rsvg_text_length_text_as_string (RsvgDrawingCtx * ctx, const char *text);

static int
_rsvg_node_text_length_children (RsvgNode * self, RsvgDrawingCtx * ctx,
                                 gdouble * length, gboolean * lastwasspace,
                                 gboolean usetextonly)
{
    guint i;
    int out = FALSE;
    for (i = 0; i < self->children->len; i++) {
        RsvgNode *node = g_ptr_array_index (self->children, i);
        RsvgNodeType type = RSVG_NODE_TYPE (node);

        rsvg_state_push (ctx);
        rsvg_state_reinherit_top (ctx, node->state, 0);
        if (rsvg_current_state (ctx)->display_none
            && (type == RSVG_NODE_TYPE_TSPAN || _rsvg_text_node_is_link (node))) {
            rsvg_state_pop (ctx);
            continue;
        }
        if (type == RSVG_NODE_TYPE_CHARS) {
            GString *str = _rsvg_text_chomp (rsvg_current_state (ctx), node, lastwasspace);
            *length += rsvg_text_length_text_as_string (ctx, str->str);
            g_string_free (str, TRUE);
        } else {
            if (usetextonly) {
                out = _rsvg_node_text_length_children(node, ctx, length,
                                                      lastwasspace,
                                                      usetextonly);
            } else {
                if (type == RSVG_NODE_TYPE_TSPAN) {
                    RsvgNodeText *tspan = (RsvgNodeText *) node;
                    out = _rsvg_node_text_length_tspan (tspan, ctx, length,
                                                        lastwasspace,
                                                        usetextonly);
                } else if (type == RSVG_NODE_TYPE_TREF) {
                    RsvgNodeTref *tref = (RsvgNodeTref *) node;
                    out = _rsvg_node_text_length_tref (tref, ctx, length,
                                                       lastwasspace,
                                                       usetextonly);
                } else if (_rsvg_text_node_is_link (node)) {
                    out = _rsvg_node_text_length_children (node, ctx, length,
                                                           lastwasspace,
                                                           usetextonly);
                }
            }
        }
        rsvg_state_pop (ctx);
        if (out)
            break;
    }
    return out;
}


static void
_rsvg_node_text_draw (RsvgNode * self, RsvgDrawingCtx * ctx, int dominate)
{
    double x, y, dx, dy, length = 0;
    gboolean lastwasspace = TRUE;
    RsvgNodeText *text = (RsvgNodeText *) self;
    rsvg_state_reinherit_top (ctx, self->state, dominate);

    x = _rsvg_css_normalize_length (&text->x, ctx, 'h');
    y = _rsvg_css_normalize_length (&text->y, ctx, 'v');
    dx = _rsvg_css_normalize_length (&text->dx, ctx, 'h');
    dy = _rsvg_css_normalize_length (&text->dy, ctx, 'v');

    if (rsvg_current_state (ctx)->text_anchor != TEXT_ANCHOR_START) {
        _rsvg_node_text_length_children (self, ctx, &length, &lastwasspace, FALSE);
        if (rsvg_current_state (ctx)->text_anchor == TEXT_ANCHOR_MIDDLE)
            length /= 2;
    }
    if (PANGO_GRAVITY_IS_VERTICAL (rsvg_current_state (ctx)->text_gravity)) {
        y -= length;
        if (rsvg_current_state (ctx)->text_anchor == TEXT_ANCHOR_MIDDLE)
            dy /= 2;
        if (rsvg_current_state (ctx)->text_anchor == TEXT_ANCHOR_END)
            dy = 0;
    } else {
        x -= length;
        if (rsvg_current_state (ctx)->text_anchor == TEXT_ANCHOR_MIDDLE)
            dx /= 2;
        if (rsvg_current_state (ctx)->text_anchor == TEXT_ANCHOR_END)
            dx = 0;
    }
    x += dx;
    y += dy;

    /* rust/rsvg/src/text.rs unions every span’s ink box, then
     * to_user_space() uses that for objectBoundingBox paint. */
    ctx->text_obb_valid = FALSE;
    ctx->text_measure_only = TRUE;
    {
        double mx = x, my = y;
        gboolean mlws = TRUE;

        _rsvg_node_text_type_children (self, ctx, &mx, &my, &mlws, FALSE);
    }
    ctx->text_measure_only = FALSE;

    lastwasspace = TRUE;
    _rsvg_node_text_type_children (self, ctx, &x, &y, &lastwasspace, FALSE);
    ctx->text_obb_valid = FALSE;
}

RsvgNode *
rsvg_new_text (void)
{
    RsvgNodeText *text;
    text = g_new (RsvgNodeText, 1);
    _rsvg_node_init (&text->super, RSVG_NODE_TYPE_TEXT);
    text->super.draw = _rsvg_node_text_draw;
    text->super.set_atts = _rsvg_node_text_set_atts;
    text->x = text->y = text->dx = text->dy = _rsvg_css_parse_length ("0");
    return &text->super;
}

static void
_rsvg_node_text_type_tspan (RsvgNodeText * self, RsvgDrawingCtx * ctx,
                            gdouble * x, gdouble * y, gboolean * lastwasspace,
                            gboolean usetextonly)
{
    double dx, dy, length = 0;
    rsvg_state_reinherit_top (ctx, self->super.state, 0);

    dx = _rsvg_css_normalize_length (&self->dx, ctx, 'h');
    dy = _rsvg_css_normalize_length (&self->dy, ctx, 'v');

    if (rsvg_current_state (ctx)->text_anchor != TEXT_ANCHOR_START) {
        gboolean lws = *lastwasspace;
        _rsvg_node_text_length_children (&self->super, ctx, &length, &lws,
                                         usetextonly);
        if (rsvg_current_state (ctx)->text_anchor == TEXT_ANCHOR_MIDDLE)
            length /= 2;
    }

    if (self->x.factor != 'n') {
        *x = _rsvg_css_normalize_length (&self->x, ctx, 'h');
        if (!PANGO_GRAVITY_IS_VERTICAL (rsvg_current_state (ctx)->text_gravity)) {
            *x -= length;
            if (rsvg_current_state (ctx)->text_anchor == TEXT_ANCHOR_MIDDLE)
                dx /= 2;
            if (rsvg_current_state (ctx)->text_anchor == TEXT_ANCHOR_END)
                dx = 0;
        }
    }
    *x += dx;

    if (self->y.factor != 'n') {
        *y = _rsvg_css_normalize_length (&self->y, ctx, 'v');
        if (PANGO_GRAVITY_IS_VERTICAL (rsvg_current_state (ctx)->text_gravity)) {
            *y -= length;
            if (rsvg_current_state (ctx)->text_anchor == TEXT_ANCHOR_MIDDLE)
                dy /= 2;
            if (rsvg_current_state (ctx)->text_anchor == TEXT_ANCHOR_END)
                dy = 0;
        }
    }
    *y += dy;
    _rsvg_node_text_type_children (&self->super, ctx, x, y, lastwasspace,
                                   usetextonly);
}

static int
_rsvg_node_text_length_tspan (RsvgNodeText * self,
                              RsvgDrawingCtx * ctx, gdouble * length,
                              gboolean * lastwasspace, gboolean usetextonly)
{
    if (self->x.factor != 'n' || self->y.factor != 'n')
        return TRUE;

    if (PANGO_GRAVITY_IS_VERTICAL (rsvg_current_state (ctx)->text_gravity))
        *length += _rsvg_css_normalize_length (&self->dy, ctx, 'v');
    else
        *length += _rsvg_css_normalize_length (&self->dx, ctx, 'h');

    return _rsvg_node_text_length_children (&self->super, ctx, length,
                                             lastwasspace, usetextonly);
}

static void
_rsvg_node_tspan_set_atts (RsvgNode * self, RsvgHandle * ctx, RsvgPropertyBag * atts)
{
    const char *klazz = NULL, *id = NULL, *value;
    RsvgNodeText *text = (RsvgNodeText *) self;

    if (rsvg_property_bag_size (atts)) {
        if ((value = rsvg_property_bag_lookup (atts, "x")))
            text->x = _rsvg_text_parse_first_length (value);
        if ((value = rsvg_property_bag_lookup (atts, "y")))
            text->y = _rsvg_text_parse_first_length (value);
        if ((value = rsvg_property_bag_lookup (atts, "dx")))
            text->dx = _rsvg_text_parse_first_length (value);
        if ((value = rsvg_property_bag_lookup (atts, "dy")))
            text->dy = _rsvg_text_parse_first_length (value);
        if ((value = rsvg_property_bag_lookup (atts, "class")))
            klazz = value;
        if ((value = rsvg_property_bag_lookup (atts, "id"))) {
            id = value;
            rsvg_defs_register_name (ctx->priv->defs, value, self);
        }

        rsvg_parse_style_attrs (ctx, self->state, "tspan", klazz, id, atts);
    }
}

RsvgNode *
rsvg_new_tspan (void)
{
    RsvgNodeText *text;
    text = g_new (RsvgNodeText, 1);
    _rsvg_node_init (&text->super, RSVG_NODE_TYPE_TSPAN);
    text->super.set_atts = _rsvg_node_tspan_set_atts;
    text->x.factor = text->y.factor = 'n';
    text->dx = text->dy = _rsvg_css_parse_length ("0");
    return &text->super;
}

static void
_rsvg_node_text_type_tref (RsvgNodeTref * self, RsvgDrawingCtx * ctx,
                           gdouble * x, gdouble * y, gboolean * lastwasspace,
                           gboolean usetextonly)
{
    RsvgNode *link;

    if (self->link == NULL)
      return;
    link = rsvg_acquire_node (ctx, self->link);
    if (link == NULL)
      return;

    _rsvg_node_text_type_children (link, ctx, x, y, lastwasspace,
                                                    TRUE);

    rsvg_release_node (ctx, link);
}

static int
_rsvg_node_text_length_tref (RsvgNodeTref * self, RsvgDrawingCtx * ctx, gdouble * x,
                             gboolean * lastwasspace, gboolean usetextonly)
{
    gboolean result;
    RsvgNode *link;

    if (self->link == NULL)
      return FALSE;
    link = rsvg_acquire_node (ctx, self->link);
    if (link == NULL)
      return FALSE;

    result = _rsvg_node_text_length_children (link, ctx, x, lastwasspace, TRUE);

    rsvg_release_node (ctx, link);

    return result;
}

static void
rsvg_node_tref_free (RsvgNode * node)
{
    RsvgNodeTref *self = (RsvgNodeTref *) node;
    g_free (self->link);
    _rsvg_node_free (node);
}

static void
_rsvg_node_tref_set_atts (RsvgNode * self, RsvgHandle * ctx, RsvgPropertyBag * atts)
{
    const char *value;
    RsvgNodeTref *text = (RsvgNodeTref *) self;

    if (rsvg_property_bag_size (atts)) {
        if ((value = rsvg_property_bag_lookup_href (atts))) {
            g_free (text->link);
            text->link = g_strdup (value);
        }
        if ((value = rsvg_property_bag_lookup (atts, "id")))
            rsvg_defs_register_name (ctx->priv->defs, value, self);
    }
}

RsvgNode *
rsvg_new_tref (void)
{
    RsvgNodeTref *text;
    text = g_new (RsvgNodeTref, 1);
    _rsvg_node_init (&text->super, RSVG_NODE_TYPE_TREF);
    text->super.free = rsvg_node_tref_free;
    text->super.set_atts = _rsvg_node_tref_set_atts;
    text->link = NULL;
    return &text->super;
}

typedef struct _RsvgTextLayout RsvgTextLayout;

struct _RsvgTextLayout {
    PangoLayout *layout;
    RsvgDrawingCtx *ctx;
    TextAnchor anchor;
    gdouble x, y;
};

static void
rsvg_text_layout_free (RsvgTextLayout * layout)
{
    g_object_unref (layout->layout);
    g_free (layout);
}

static PangoLayout *
rsvg_text_create_layout (RsvgDrawingCtx * ctx,
                         RsvgState * state, const char *text, PangoContext * context)
{
    PangoFontDescription *font_desc;
    PangoLayout *layout;
    PangoAttrList *attr_list;
    PangoAttribute *attribute;

    if (state->lang)
        pango_context_set_language (context, pango_language_from_string (state->lang));

    if (state->unicode_bidi == UNICODE_BIDI_OVERRIDE || state->unicode_bidi == UNICODE_BIDI_EMBED)
        pango_context_set_base_dir (context, state->text_dir);

    if (PANGO_GRAVITY_IS_VERTICAL (state->text_gravity))
        pango_context_set_base_gravity (context, state->text_gravity);

    font_desc = pango_font_description_copy (pango_context_get_font_description (context));

    if (state->font_family)
        pango_font_description_set_family_static (font_desc, state->font_family);

    pango_font_description_set_style (font_desc, state->font_style);
    pango_font_description_set_variant (font_desc, state->font_variant);
    pango_font_description_set_weight (font_desc, state->font_weight);
    pango_font_description_set_stretch (font_desc, state->font_stretch);
    pango_font_description_set_size (font_desc,
                                     _rsvg_css_normalize_font_size (state, ctx) *
                                     PANGO_SCALE / ctx->dpi_y * 72);

    layout = pango_layout_new (context);
    pango_layout_set_font_description (layout, font_desc);
    pango_font_description_free (font_desc);

    attr_list = pango_attr_list_new ();
    attribute = pango_attr_letter_spacing_new (_rsvg_css_normalize_length (&state->letter_spacing,
                                                                           ctx, 'h') * PANGO_SCALE);
    attribute->start_index = 0;
    attribute->end_index = G_MAXINT;
    pango_attr_list_insert (attr_list, attribute); 

    if (state->has_font_decor && text) {
        if (state->font_decor & TEXT_UNDERLINE) {
            attribute = pango_attr_underline_new (PANGO_UNDERLINE_SINGLE);
            attribute->start_index = 0;
            attribute->end_index = -1;
            pango_attr_list_insert (attr_list, attribute);
        }
	if (state->font_decor & TEXT_STRIKE) {
            attribute = pango_attr_strikethrough_new (TRUE);
            attribute->start_index = 0;
            attribute->end_index = -1;
            pango_attr_list_insert (attr_list, attribute);
	}
    }

    pango_layout_set_attributes (layout, attr_list);
    pango_attr_list_unref (attr_list);

    if (text)
        pango_layout_set_text (layout, text, -1);
    else
        pango_layout_set_text (layout, NULL, 0);

    pango_layout_set_alignment (layout, (state->text_dir == PANGO_DIRECTION_LTR) ?
                                PANGO_ALIGN_LEFT : PANGO_ALIGN_RIGHT);

    return layout;
}


static RsvgTextLayout *
rsvg_text_layout_new (RsvgDrawingCtx * ctx, RsvgState * state, const char *text)
{
    RsvgTextLayout *layout;

    if (ctx->pango_context == NULL)
        ctx->pango_context = ctx->render->create_pango_context (ctx);

    layout = g_new0 (RsvgTextLayout, 1);

    layout->layout = rsvg_text_create_layout (ctx, state, text, ctx->pango_context);
    layout->ctx = ctx;

    layout->anchor = state->text_anchor;

    return layout;
}

/* rust/rsvg/src/text.rs compute_text_box: pango ink in user space. */
static gboolean
rsvg_text_layout_ink_rect (PangoLayout *layout, double x, double y,
                           cairo_rectangle_t *rect)
{
    PangoRectangle ink;
    PangoGravity gravity;

    pango_layout_get_extents (layout, &ink, NULL);
    if (ink.width == 0 || ink.height == 0)
        return FALSE;

    gravity = pango_context_get_gravity (pango_layout_get_context (layout));
    if (PANGO_GRAVITY_IS_VERTICAL (gravity)) {
        rect->x = x + (ink.x - ink.height) / (double) PANGO_SCALE;
        rect->y = y + ink.y / (double) PANGO_SCALE;
        rect->width = ink.height / (double) PANGO_SCALE;
        rect->height = ink.width / (double) PANGO_SCALE;
    } else {
        rect->x = x + ink.x / (double) PANGO_SCALE;
        rect->y = y + ink.y / (double) PANGO_SCALE;
        rect->width = ink.width / (double) PANGO_SCALE;
        rect->height = ink.height / (double) PANGO_SCALE;
    }
    return TRUE;
}

static void
rsvg_text_obb_union (RsvgDrawingCtx *ctx, const cairo_rectangle_t *rect)
{
    double x2, y2;

    if (rect->width <= 0 || rect->height <= 0)
        return;
    if (!ctx->text_obb_valid) {
        ctx->text_obb = *rect;
        ctx->text_obb_valid = TRUE;
        return;
    }
    x2 = MAX (ctx->text_obb.x + ctx->text_obb.width, rect->x + rect->width);
    y2 = MAX (ctx->text_obb.y + ctx->text_obb.height, rect->y + rect->height);
    ctx->text_obb.x = MIN (ctx->text_obb.x, rect->x);
    ctx->text_obb.y = MIN (ctx->text_obb.y, rect->y);
    ctx->text_obb.width = x2 - ctx->text_obb.x;
    ctx->text_obb.height = y2 - ctx->text_obb.y;
}

void
rsvg_text_render_text (RsvgDrawingCtx * ctx, const char *text, gdouble * x, gdouble * y)
{
    PangoContext *context;
    PangoLayout *layout;
    PangoLayoutIter *iter;
    RsvgState *state;
    gint w, h;
    double offset_x, offset_y, offset;

    state = rsvg_current_state (ctx);

    /* Do not render the text if the font size is zero. See bug #581491.
     * smaller/larger store length 0 and factor 's'/'l'; 2.62 resolves
     * them to parent/1.2 and parent*1.2 (font_props.rs). */
    if (state->font_size.factor != 's' && state->font_size.factor != 'l'
        && state->font_size.length == 0)
        return;

    context = ctx->render->create_pango_context (ctx);
    layout = rsvg_text_create_layout (ctx, state, text, context);
    pango_layout_get_size (layout, &w, &h);
    iter = pango_layout_get_iter (layout);
    offset = pango_layout_iter_get_baseline (iter) / (double) PANGO_SCALE;
    offset += _rsvg_css_accumulate_baseline_shift (state, ctx);
    if (PANGO_GRAVITY_IS_VERTICAL (state->text_gravity)) {
        offset_x = -offset;
        offset_y = 0;
    } else {
        offset_x = 0;
        offset_y = offset;
    }
    pango_layout_iter_free (iter);
    /* visibility:hidden still consumes advance (2.62 is_visible).
     * rust still unions hidden spans into text_extents. */
    if (ctx->text_measure_only) {
        cairo_rectangle_t ink_rect;

        if (rsvg_text_layout_ink_rect (layout, *x - offset_x, *y - offset_y, &ink_rect))
            rsvg_text_obb_union (ctx, &ink_rect);
    } else if (state->visible)
        ctx->render->render_pango_layout (ctx, layout, *x - offset_x, *y - offset_y);
    if (PANGO_GRAVITY_IS_VERTICAL (state->text_gravity))
        *y += w / (double)PANGO_SCALE;
    else
        *x += w / (double)PANGO_SCALE;

    g_object_unref (layout);
    g_object_unref (context);
}

static gdouble
rsvg_text_layout_width (RsvgTextLayout * layout)
{
    gint width;

    pango_layout_get_size (layout->layout, &width, NULL);

    return width / (double)PANGO_SCALE;
}

static gdouble
rsvg_text_length_text_as_string (RsvgDrawingCtx * ctx, const char *text)
{
    RsvgTextLayout *layout;
    gdouble x;

    layout = rsvg_text_layout_new (ctx, rsvg_current_state (ctx), text);
    layout->x = layout->y = 0;

    x = rsvg_text_layout_width (layout);

    rsvg_text_layout_free (layout);
    return x;
}
