/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 sts=4 ts=4 expandtab: */
/* 
   rsvg-css.c: Parse CSS basic data types.
 
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
  
   Authors: Dom Lachowicz <cinamod@hotmail.com> 
   Raph Levien <raph@artofcode.com>
*/

#include "config.h"
#include "rsvg-css.h"
#include "rsvg-private.h"
#include "rsvg-styles.h"

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#include <errno.h>
#include <math.h>

#include <libxml/parser.h>

#define POINTS_PER_INCH (72.0)
#define CM_PER_INCH     (2.54)
#define MM_PER_INCH     (25.4)
#define PICA_PER_INCH   (6.0)

#define SETINHERIT() G_STMT_START {if (inherit != NULL) *inherit = TRUE;} G_STMT_END
#define UNSETINHERIT() G_STMT_START {if (inherit != NULL) *inherit = FALSE;} G_STMT_END

/**
 * rsvg_css_parse_vbox:
 * @vbox: The CSS viewBox
 * @x : The X output
 * @y: The Y output
 * @w: The Width output
 * @h: The Height output
 *
 * Returns: 
 */
RsvgViewBox
rsvg_css_parse_vbox (const char *vbox)
{
    RsvgViewBox vb;
    gdouble *list;
    guint list_len;
    vb.active = FALSE;

    vb.rect.x = vb.rect.y = 0;
    vb.rect.width = vb.rect.height = 0;

    list = rsvg_css_parse_number_list (vbox, &list_len);

    if (!(list && list_len))
        return vb;
    else if (list_len != 4) {
        g_free (list);
        return vb;
    } else {
        vb.rect.x = list[0];
        vb.rect.y = list[1];
        vb.rect.width = list[2];
        vb.rect.height = list[3];
        vb.active = TRUE;

        g_free (list);
        return vb;
    }
}

typedef enum _RelativeSize {
    RELATIVE_SIZE_NORMAL,
    RELATIVE_SIZE_SMALLER,
    RELATIVE_SIZE_LARGER
} RelativeSize;

static double
rsvg_css_parse_raw_length (const char *str, gboolean * in,
                           gboolean * percent, gboolean * em, gboolean * ex, RelativeSize * relative_size)
{
    double length = 0.0;
    char *p = NULL;

    /* 
     *  The supported CSS length unit specifiers are: 
     *  em, ex, px, pt, pc, cm, mm, in, and %
     */
    *percent = FALSE;
    *em = FALSE;
    *ex = FALSE;
    *relative_size = RELATIVE_SIZE_NORMAL;

    length = g_ascii_strtod (str, &p);

    if ((length == -HUGE_VAL || length == HUGE_VAL) && (ERANGE == errno)) {
        /* todo: error condition - figure out how to best represent it */
        return 0.0;
    }

    /* test for either pixels or no unit, which is assumed to be pixels */
    if (p && *p && (strcmp (p, "px") != 0)) {
        if (!strcmp (p, "pt")) {
            length /= POINTS_PER_INCH;
            *in = TRUE;
        } else if (!strcmp (p, "in"))
            *in = TRUE;
        else if (!strcmp (p, "cm")) {
            length /= CM_PER_INCH;
            *in = TRUE;
        } else if (!strcmp (p, "mm")) {
            length /= MM_PER_INCH;
            *in = TRUE;
        } else if (!strcmp (p, "pc")) {
            length /= PICA_PER_INCH;
            *in = TRUE;
        } else if (!strcmp (p, "em"))
            *em = TRUE;
        else if (!strcmp (p, "ex"))
            *ex = TRUE;
        else if (!strcmp (p, "ch"))
            *relative_size = (RelativeSize) 100; /* sentinel: ch, resolved below */
        else if (!strcmp (p, "%")) {
            *percent = TRUE;
            length *= 0.01;
        } else {
            double pow_factor = 0.0;

            if (!g_ascii_strcasecmp (p, "larger")) {
                *relative_size = RELATIVE_SIZE_LARGER;
                return 0.0;
            } else if (!g_ascii_strcasecmp (p, "smaller")) {
                *relative_size = RELATIVE_SIZE_SMALLER;
                return 0.0;
            } else if (!g_ascii_strcasecmp (p, "xx-small")) {
                pow_factor = -3.0;
            } else if (!g_ascii_strcasecmp (p, "x-small")) {
                pow_factor = -2.0;
            } else if (!g_ascii_strcasecmp (p, "small")) {
                pow_factor = -1.0;
            } else if (!g_ascii_strcasecmp (p, "medium")) {
                pow_factor = 0.0;
            } else if (!g_ascii_strcasecmp (p, "large")) {
                pow_factor = 1.0;
            } else if (!g_ascii_strcasecmp (p, "x-large")) {
                pow_factor = 2.0;
            } else if (!g_ascii_strcasecmp (p, "xx-large")) {
                pow_factor = 3.0;
            } else {
                return 0.0;
            }

            length = 12.0 * pow (1.2, pow_factor) / POINTS_PER_INCH;
            *in = TRUE;
        }
    }

    return length;
}

RsvgCssLength
_rsvg_css_parse_length (const char *str)
{
    RsvgCssLength out;
    gboolean percent, em, ex, in;
    RelativeSize relative_size = RELATIVE_SIZE_NORMAL;
    percent = em = ex = in = FALSE;

    out.length = rsvg_css_parse_raw_length (str, &in, &percent, &em, &ex, &relative_size);
    if (percent)
        out.factor = 'p';
    else if (em)
        out.factor = 'm';
    else if (ex)
        out.factor = 'x';
    else if (in)
        out.factor = 'i';
    else if ((int) relative_size == 100)
        out.factor = 'c';       /* ch: 0.5em fallback (CSS Values) */
    else if (relative_size == RELATIVE_SIZE_LARGER)
        out.factor = 'l';
    else if (relative_size == RELATIVE_SIZE_SMALLER)
        out.factor = 's';
    else
        out.factor = '\0';
    return out;
}

/* Recursive evaluation of all parent elements regarding absolute font size */
double
_rsvg_css_normalize_font_size (RsvgState * state, RsvgDrawingCtx * ctx)
{
    RsvgState *parent;

    switch (state->font_size.factor) {
    case 'p':
    case 'm':
    case 'x':
        parent = rsvg_state_parent (state);
        if (parent) {
            double parent_size;
            parent_size = _rsvg_css_normalize_font_size (parent, ctx);
            return state->font_size.length * parent_size;
        }
        break;
    case 's':
    case 'l':
        /* 2.62 font_props.rs: smaller = parent/1.2, larger = parent*1.2 */
        parent = rsvg_state_parent (state);
        if (parent) {
            double parent_size;
            parent_size = _rsvg_css_normalize_font_size (parent, ctx);
            return (state->font_size.factor == 's') ? parent_size / 1.2 : parent_size * 1.2;
        }
        return (state->font_size.factor == 's') ? 12. / 1.2 : 12. * 1.2;
    default:
        return _rsvg_css_normalize_length (&state->font_size, ctx, 'v');
        break;
    }

    return 12.;
}

double
_rsvg_css_normalize_length (const RsvgCssLength * in, RsvgDrawingCtx * ctx, char dir)
{
    if (in->factor == '\0')
        return in->length;
    else if (in->factor == 'p') {
        if (dir == 'h')
            return in->length * ctx->vb.rect.width;
        if (dir == 'v')
            return in->length * ctx->vb.rect.height;
        if (dir == 'o')
            return in->length * rsvg_viewport_percentage (ctx->vb.rect.width,
                                                          ctx->vb.rect.height);
    } else if (in->factor == 'm' || in->factor == 'x' || in->factor == 'c') {
        double font = _rsvg_css_normalize_font_size (rsvg_current_state (ctx), ctx);
        if (in->factor == 'm')
            return in->length * font;
        else
            return in->length * font / 2.; /* ex and ch (horizontal) fallback */
    } else if (in->factor == 'i') {
        if (dir == 'h')
            return in->length * ctx->dpi_x;
        if (dir == 'v')
            return in->length * ctx->dpi_y;
        if (dir == 'o')
            return in->length * rsvg_viewport_percentage (ctx->dpi_x, ctx->dpi_y);
    } else if (in->factor == 'l' || in->factor == 's') {
        /* Same 2.62 relative size as _rsvg_css_normalize_font_size. */
        RsvgState *state = rsvg_current_state (ctx);
        RsvgState *parent = state ? rsvg_state_parent (state) : NULL;
        double parent_size = parent ? _rsvg_css_normalize_font_size (parent, ctx) : 12.;

        return (in->factor == 's') ? parent_size / 1.2 : parent_size * 1.2;
    }

    return 0;
}

/* Recursive evaluation of all parent elements regarding basline-shift */
double
_rsvg_css_accumulate_baseline_shift (RsvgState * state, RsvgDrawingCtx * ctx)
{
    RsvgState *parent;
    double shift = 0.;

    parent = rsvg_state_parent (state);
    if (parent) {
        if (state->has_baseline_shift) {
            double parent_font_size;
            parent_font_size = _rsvg_css_normalize_font_size (parent, ctx); /* font size from here */
            shift = parent_font_size * state->baseline_shift;
        }
        shift += _rsvg_css_accumulate_baseline_shift (parent, ctx); /* baseline-shift for parent element */
    }

    return shift;
}


double
_rsvg_css_hand_normalize_length (const RsvgCssLength * in, gdouble pixels_per_inch,
                                 gdouble width_or_height, gdouble font_size)
{
    if (in->factor == '\0')
        return in->length;
    else if (in->factor == 'p')
        return in->length * width_or_height;
    else if (in->factor == 'm')
        return in->length * font_size;
    else if (in->factor == 'x' || in->factor == 'c')
        return in->length * font_size / 2.;
    else if (in->factor == 'i')
        return in->length * pixels_per_inch;

    return 0;
}

static gint
rsvg_css_unit_to_byte (double value, gboolean percent, double max)
{
    if (percent)
        value = CLAMP (value, 0, 100) / 100.0;
    else
        value = CLAMP (value, 0, max) / max;
    return (gint) floor (value * 255 + 0.5);
}

static gint
rsvg_css_clip_rgb_percent (const char *s, double max)
{
    double value;
    char *end;
    gboolean percent;

    value = g_ascii_strtod (s, &end);
    percent = (*end == '%');
    return rsvg_css_unit_to_byte (value, percent, max);
}

static void
hsl_to_rgb (double h, double s, double l, double *r, double *g, double *b)
{
    double c, x, m, hp, r1, g1, b1;
    int hi;

    h = fmod (h, 360.0);
    if (h < 0)
        h += 360.0;
    s = CLAMP (s, 0.0, 1.0);
    l = CLAMP (l, 0.0, 1.0);
    c = (1.0 - fabs (2.0 * l - 1.0)) * s;
    hp = h / 60.0;
    x = c * (1.0 - fabs (fmod (hp, 2.0) - 1.0));
    hi = (int) floor (hp) % 6;
    switch (hi) {
    case 0: r1 = c; g1 = x; b1 = 0; break;
    case 1: r1 = x; g1 = c; b1 = 0; break;
    case 2: r1 = 0; g1 = c; b1 = x; break;
    case 3: r1 = 0; g1 = x; b1 = c; break;
    case 4: r1 = x; g1 = 0; b1 = c; break;
    default: r1 = c; g1 = 0; b1 = x; break;
    }
    m = l - c / 2.0;
    *r = r1 + m;
    *g = g1 + m;
    *b = b1 + m;
}

static gboolean
parse_color_component (const char **pp, double *out, gboolean *percent)
{
    const char *p = *pp;
    char *end;
    while (*p && g_ascii_isspace (*p))
        p++;
    if (!*p || *p == '/' || *p == ')' || *p == ',')
        return FALSE;
    *out = g_ascii_strtod (p, &end);
    if (end == p)
        return FALSE;
    p = end;
    *percent = FALSE;
    if (*p == '%') {
        *percent = TRUE;
        p++;
    }
    *pp = p;
    return TRUE;
}

/* Parse rgb()/rgba()/hsl()/hsla()/hwb() argument list: comma or space,
 * optional / alpha (CSS Color Level 4). pct[i] is TRUE if that component
 * was a percentage. */
static gboolean
parse_color_func_args (const char *inside, double c[4], gboolean pct[4], int *ncomp)
{
    const char *p = inside;
    int n = 0;

    memset (pct, 0, 4 * sizeof (gboolean));
    while (*p && n < 4) {
        while (*p && g_ascii_isspace (*p))
            p++;
        if (*p == '/') {
            p++;
            while (*p && g_ascii_isspace (*p))
                p++;
        }
        if (*p == ')' || *p == '\0')
            break;
        if (!parse_color_component (&p, &c[n], &pct[n]))
            return FALSE;
        n++;
        while (*p && g_ascii_isspace (*p))
            p++;
        if (*p == ',')
            p++;
    }
    *ncomp = n;
    return n >= 3;
}

/* pack 3 [0,255] ints into one 32 bit one */
#define PACK_RGBA(r,g,b,a) (((a) << 24) | ((r) << 16) | ((g) << 8) | (b))
#define PACK_RGB(r,g,b) PACK_RGBA(r, g, b, 255)

static int
hex_digit (char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;
}

/* CSS2/SVG named colors (same set as the archived libcroco 0.6.15
 * table, minus transparent which is handled above). Sorted for bsearch. */
typedef struct {
    const char *name;
    guint8 r, g, b;
} RsvgCssNamedColor;

static const RsvgCssNamedColor css_named_colors[] = {
    { "aliceblue", 240, 248, 255 },
    { "antiquewhite", 250, 235, 215 },
    { "aqua", 0, 255, 255 },
    { "aquamarine", 127, 255, 212 },
    { "azure", 240, 255, 255 },
    { "beige", 245, 245, 220 },
    { "bisque", 255, 228, 196 },
    { "black", 0, 0, 0 },
    { "blanchedalmond", 255, 235, 205 },
    { "blue", 0, 0, 255 },
    { "blueviolet", 138, 43, 226 },
    { "brown", 165, 42, 42 },
    { "burlywood", 222, 184, 135 },
    { "cadetblue", 95, 158, 160 },
    { "chartreuse", 127, 255, 0 },
    { "chocolate", 210, 105, 30 },
    { "coral", 255, 127, 80 },
    { "cornflowerblue", 100, 149, 237 },
    { "cornsilk", 255, 248, 220 },
    { "crimson", 220, 20, 60 },
    { "cyan", 0, 255, 255 },
    { "darkblue", 0, 0, 139 },
    { "darkcyan", 0, 139, 139 },
    { "darkgoldenrod", 184, 134, 11 },
    { "darkgray", 169, 169, 169 },
    { "darkgreen", 0, 100, 0 },
    { "darkgrey", 169, 169, 169 },
    { "darkkhaki", 189, 183, 107 },
    { "darkmagenta", 139, 0, 139 },
    { "darkolivegreen", 85, 107, 47 },
    { "darkorange", 255, 140, 0 },
    { "darkorchid", 153, 50, 204 },
    { "darkred", 139, 0, 0 },
    { "darksalmon", 233, 150, 122 },
    { "darkseagreen", 143, 188, 143 },
    { "darkslateblue", 72, 61, 139 },
    { "darkslategray", 47, 79, 79 },
    { "darkslategrey", 47, 79, 79 },
    { "darkturquoise", 0, 206, 209 },
    { "darkviolet", 148, 0, 211 },
    { "deeppink", 255, 20, 147 },
    { "deepskyblue", 0, 191, 255 },
    { "dimgray", 105, 105, 105 },
    { "dimgrey", 105, 105, 105 },
    { "dodgerblue", 30, 144, 255 },
    { "firebrick", 178, 34, 34 },
    { "floralwhite", 255, 250, 240 },
    { "forestgreen", 34, 139, 34 },
    { "fuchsia", 255, 0, 255 },
    { "gainsboro", 220, 220, 220 },
    { "ghostwhite", 248, 248, 255 },
    { "gold", 255, 215, 0 },
    { "goldenrod", 218, 165, 32 },
    { "gray", 128, 128, 128 },
    { "green", 0, 128, 0 },
    { "greenyellow", 173, 255, 47 },
    { "grey", 128, 128, 128 },
    { "honeydew", 240, 255, 240 },
    { "hotpink", 255, 105, 180 },
    { "indianred", 205, 92, 92 },
    { "indigo", 75, 0, 130 },
    { "ivory", 255, 255, 240 },
    { "khaki", 240, 230, 140 },
    { "lavender", 230, 230, 250 },
    { "lavenderblush", 255, 240, 245 },
    { "lawngreen", 124, 252, 0 },
    { "lemonchiffon", 255, 250, 205 },
    { "lightblue", 173, 216, 230 },
    { "lightcoral", 240, 128, 128 },
    { "lightcyan", 224, 255, 255 },
    { "lightgoldenrodyellow", 250, 250, 210 },
    { "lightgray", 211, 211, 211 },
    { "lightgreen", 144, 238, 144 },
    { "lightgrey", 211, 211, 211 },
    { "lightpink", 255, 182, 193 },
    { "lightsalmon", 255, 160, 122 },
    { "lightseagreen", 32, 178, 170 },
    { "lightskyblue", 135, 206, 250 },
    { "lightslategray", 119, 136, 153 },
    { "lightslategrey", 119, 136, 153 },
    { "lightsteelblue", 176, 196, 222 },
    { "lightyellow", 255, 255, 224 },
    { "lime", 0, 255, 0 },
    { "limegreen", 50, 205, 50 },
    { "linen", 250, 240, 230 },
    { "magenta", 255, 0, 255 },
    { "maroon", 128, 0, 0 },
    { "mediumaquamarine", 102, 205, 170 },
    { "mediumblue", 0, 0, 205 },
    { "mediumorchid", 186, 85, 211 },
    { "mediumpurple", 147, 112, 219 },
    { "mediumseagreen", 60, 179, 113 },
    { "mediumslateblue", 123, 104, 238 },
    { "mediumspringgreen", 0, 250, 154 },
    { "mediumturquoise", 72, 209, 204 },
    { "mediumvioletred", 199, 21, 133 },
    { "midnightblue", 25, 25, 112 },
    { "mintcream", 245, 255, 250 },
    { "mistyrose", 255, 228, 225 },
    { "moccasin", 255, 228, 181 },
    { "navajowhite", 255, 222, 173 },
    { "navy", 0, 0, 128 },
    { "oldlace", 253, 245, 230 },
    { "olive", 128, 128, 0 },
    { "olivedrab", 107, 142, 35 },
    { "orange", 255, 165, 0 },
    { "orangered", 255, 69, 0 },
    { "orchid", 218, 112, 214 },
    { "palegoldenrod", 238, 232, 170 },
    { "palegreen", 152, 251, 152 },
    { "paleturquoise", 175, 238, 238 },
    { "palevioletred", 219, 112, 147 },
    { "papayawhip", 255, 239, 213 },
    { "peachpuff", 255, 218, 185 },
    { "peru", 205, 133, 63 },
    { "pink", 255, 192, 203 },
    { "plum", 221, 160, 221 },
    { "powderblue", 176, 224, 230 },
    { "purple", 128, 0, 128 },
    { "red", 255, 0, 0 },
    { "rosybrown", 188, 143, 143 },
    { "royalblue", 65, 105, 225 },
    { "saddlebrown", 139, 69, 19 },
    { "salmon", 250, 128, 114 },
    { "sandybrown", 244, 164, 96 },
    { "seagreen", 46, 139, 87 },
    { "seashell", 255, 245, 238 },
    { "sienna", 160, 82, 45 },
    { "silver", 192, 192, 192 },
    { "skyblue", 135, 206, 235 },
    { "slateblue", 106, 90, 205 },
    { "slategray", 112, 128, 144 },
    { "slategrey", 112, 128, 144 },
    { "snow", 255, 250, 250 },
    { "springgreen", 0, 255, 127 },
    { "steelblue", 70, 130, 180 },
    { "tan", 210, 180, 140 },
    { "teal", 0, 128, 128 },
    { "thistle", 216, 191, 216 },
    { "tomato", 255, 99, 71 },
    { "turquoise", 64, 224, 208 },
    { "violet", 238, 130, 238 },
    { "wheat", 245, 222, 179 },
    { "white", 255, 255, 255 },
    { "whitesmoke", 245, 245, 245 },
    { "yellow", 255, 255, 0 },
    { "yellowgreen", 154, 205, 50 }
};

static int
css_named_color_cmp (const void *key, const void *member)
{
    const RsvgCssNamedColor *c = member;
    return g_ascii_strcasecmp ((const char *) key, c->name);
}

static gboolean
css_lookup_named_color (const char *str, guint8 *r, guint8 *g, guint8 *b)
{
    const RsvgCssNamedColor *hit;

    hit = bsearch (str, css_named_colors, G_N_ELEMENTS (css_named_colors),
                   sizeof (css_named_colors[0]), css_named_color_cmp);
    if (hit == NULL)
        return FALSE;
    *r = hit->r;
    *g = hit->g;
    *b = hit->b;
    return TRUE;
}

/**
 * rsvg_css_parse_color:
 * @str: string to parse
 * @inherit: whether the value is specified (TRUE) or inherit (FALSE)
 *
 * Parse a CSS Color Level 4 color: #rgb/#rgba/#rrggbb/#rrggbbaa,
 * rgb()/rgba()/hsl()/hsla()/hwb() (comma or space + optional / alpha),
 * named colors, transparent, inherit.
 */
guint32
rsvg_css_parse_color (const char *str, gboolean * inherit)
{
    gint val = 0;

    SETINHERIT ();

    while (str && g_ascii_isspace (*str))
        str++;
    if (!str || !*str) {
        UNSETINHERIT ();
        return PACK_RGB (0, 0, 0);
    }

    if (str[0] == '#') {
        int digits[8], n = 0, i;
        for (i = 1; str[i] && n < 8; i++) {
            int d = hex_digit (str[i]);
            if (d < 0)
                break;
            digits[n++] = d;
        }
        if (n == 3 || n == 4) {
            int r = digits[0] * 17, g = digits[1] * 17, b = digits[2] * 17;
            int a = (n == 4) ? digits[3] * 17 : 255;
            val = PACK_RGBA (r, g, b, a);
        } else if (n == 6 || n == 8) {
            int r = (digits[0] << 4) | digits[1];
            int g = (digits[2] << 4) | digits[3];
            int b = (digits[4] << 4) | digits[5];
            int a = (n == 8) ? ((digits[6] << 4) | digits[7]) : 255;
            val = PACK_RGBA (r, g, b, a);
        } else {
            UNSETINHERIT ();
            val = PACK_RGB (0, 0, 0);
        }
    } else if (g_ascii_strncasecmp (str, "rgb", 3) == 0 ||
               g_ascii_strncasecmp (str, "hsl", 3) == 0 ||
               g_ascii_strncasecmp (str, "hwb", 3) == 0) {
        const char *paren = strchr (str, '(');
        double c[4] = { 0, 0, 0, 1 };
        gboolean pct[4] = { FALSE, FALSE, FALSE, FALSE };
        int ncomp = 0;
        gboolean is_hsl = g_ascii_strncasecmp (str, "hsl", 3) == 0;
        gboolean is_hwb = g_ascii_strncasecmp (str, "hwb", 3) == 0;

        if (!paren || !parse_color_func_args (paren + 1, c, pct, &ncomp)) {
            UNSETINHERIT ();
            return PACK_RGB (0, 0, 0);
        }

        if (is_hsl || is_hwb) {
            double r, g, b, h, s_or_w, l_or_b, a;
            h = c[0];
            s_or_w = pct[1] ? c[1] / 100.0 : c[1];
            l_or_b = pct[2] ? c[2] / 100.0 : c[2];
            a = 1.0;
            if (ncomp >= 4)
                a = pct[3] ? c[3] / 100.0 : c[3];
            a = CLAMP (a, 0, 1);
            if (is_hsl) {
                hsl_to_rgb (h, s_or_w, l_or_b, &r, &g, &b);
            } else {
                double w = CLAMP (s_or_w, 0, 1), blk = CLAMP (l_or_b, 0, 1);
                if (w + blk >= 1.0) {
                    double gray = (w + blk > 0) ? w / (w + blk) : 0;
                    r = g = b = gray;
                } else {
                    hsl_to_rgb (h, 1.0, 0.5, &r, &g, &b);
                    r = r * (1.0 - w - blk) + w;
                    g = g * (1.0 - w - blk) + w;
                    b = b * (1.0 - w - blk) + w;
                }
            }
            val = PACK_RGBA ((int) floor (CLAMP (r, 0, 1) * 255 + 0.5),
                             (int) floor (CLAMP (g, 0, 1) * 255 + 0.5),
                             (int) floor (CLAMP (b, 0, 1) * 255 + 0.5),
                             (int) floor (a * 255 + 0.5));
        } else {
            int r, g, b, a;
            r = pct[0] ? rsvg_css_unit_to_byte (c[0], TRUE, 255) : (int) floor (CLAMP (c[0], 0, 255) + 0.5);
            g = pct[1] ? rsvg_css_unit_to_byte (c[1], TRUE, 255) : (int) floor (CLAMP (c[1], 0, 255) + 0.5);
            b = pct[2] ? rsvg_css_unit_to_byte (c[2], TRUE, 255) : (int) floor (CLAMP (c[2], 0, 255) + 0.5);
            if (ncomp >= 4) {
                double aa = pct[3] ? c[3] / 100.0 : c[3];
                a = (int) floor (CLAMP (aa, 0, 1) * 255 + 0.5);
            } else {
                a = 255;
            }
            val = PACK_RGBA (r, g, b, a);
        }
    } else if (!strcmp (str, "inherit")) {
        UNSETINHERIT ();
    } else if (!strcmp (str, "transparent")) {
        val = PACK_RGBA (0, 0, 0, 0);
    } else {
        guint8 r, g, b;

        if (css_lookup_named_color (str, &r, &g, &b)) {
            val = PACK_RGB (r, g, b);
        } else {
            UNSETINHERIT ();
            val = PACK_RGB (0, 0, 0);
        }
    }

    return val;
}

#undef PACK_RGB
#undef PACK_RGBA

guint
rsvg_css_parse_opacity (const char *str)
{
    char *end_ptr = NULL;
    double opacity;

    opacity = g_ascii_strtod (str, &end_ptr);

    if (((opacity == -HUGE_VAL || opacity == HUGE_VAL) && (ERANGE == errno)) ||
        *end_ptr != '\0')
        opacity = 1.;

    opacity = CLAMP (opacity, 0., 1.);

    return (guint) floor (opacity * 255. + 0.5);
}

/*
  <angle>: An angle value is a <number>  optionally followed immediately with 
  an angle unit identifier. Angle unit identifiers are:

    * deg: degrees
    * grad: grads
    * rad: radians

    For properties defined in [CSS2], an angle unit identifier must be provided.
    For SVG-specific attributes and properties, the angle unit identifier is 
    optional. If not provided, the angle value is assumed to be in degrees.
*/
double
rsvg_css_parse_angle (const char *str)
{
    double degrees;
    char *end_ptr;

    degrees = g_ascii_strtod (str, &end_ptr);

    /* todo: error condition - figure out how to best represent it */
    if ((degrees == -HUGE_VAL || degrees == HUGE_VAL) && (ERANGE == errno))
        return 0.0;

    if (end_ptr) {
        if (!strcmp (end_ptr, "rad"))
            return degrees * 180. / G_PI;
        else if (!strcmp (end_ptr, "grad"))
            return degrees * 360. / 400.;
    }

    return degrees;
}

/*
  <frequency>: Frequency values are used with aural properties. The normative 
  definition of frequency values can be found in [CSS2-AURAL]. A frequency 
  value is a <number> immediately followed by a frequency unit identifier. 
  Frequency unit identifiers are:

    * Hz: Hertz
    * kHz: kilo Hertz

    Frequency values may not be negative.
*/
double
rsvg_css_parse_frequency (const char *str)
{
    double f_hz;
    char *end_ptr;

    f_hz = g_ascii_strtod (str, &end_ptr);

    /* todo: error condition - figure out how to best represent it */
    if ((f_hz == -HUGE_VAL || f_hz == HUGE_VAL) && (ERANGE == errno))
        return 0.0;

    if (end_ptr && !strcmp (end_ptr, "kHz"))
        return f_hz * 1000.;

    return f_hz;
}

/*
  <time>: A time value is a <number> immediately followed by a time unit 
  identifier. Time unit identifiers are:
  
  * ms: milliseconds
  * s: seconds
  
  Time values are used in CSS properties and may not be negative.
*/
double
rsvg_css_parse_time (const char *str)
{
    double ms;
    char *end_ptr;

    ms = g_ascii_strtod (str, &end_ptr);

    /* todo: error condition - figure out how to best represent it */
    if ((ms == -HUGE_VAL || ms == HUGE_VAL) && (ERANGE == errno))
        return 0.0;

    if (end_ptr && !strcmp (end_ptr, "s"))
        return ms * 1000.;

    return ms;
}

PangoStyle
rsvg_css_parse_font_style (const char *str, gboolean * inherit)
{
    SETINHERIT ();

    if (str) {
        if (!strcmp (str, "oblique"))
            return PANGO_STYLE_OBLIQUE;
        if (!strcmp (str, "italic"))
            return PANGO_STYLE_ITALIC;
        if (!strcmp (str, "normal"))
            return PANGO_STYLE_NORMAL;
        if (!strcmp (str, "inherit")) {
            UNSETINHERIT ();
            return PANGO_STYLE_NORMAL;
        }
    }
    UNSETINHERIT ();
    return PANGO_STYLE_NORMAL;
}

PangoVariant
rsvg_css_parse_font_variant (const char *str, gboolean * inherit)
{
    SETINHERIT ();

    if (str) {
        if (!strcmp (str, "small-caps"))
            return PANGO_VARIANT_SMALL_CAPS;
        else if (!strcmp (str, "inherit")) {
            UNSETINHERIT ();
            return PANGO_VARIANT_NORMAL;
        }
    }
    UNSETINHERIT ();
    return PANGO_VARIANT_NORMAL;
}

PangoWeight
rsvg_css_parse_font_weight (const char *str, gboolean * inherit)
{
    SETINHERIT ();
    if (str) {
        if (!strcmp (str, "lighter"))
            return PANGO_WEIGHT_LIGHT;
        else if (!strcmp (str, "bold"))
            return PANGO_WEIGHT_BOLD;
        else if (!strcmp (str, "bolder"))
            return PANGO_WEIGHT_ULTRABOLD;
        else if (!strcmp (str, "100"))
            return (PangoWeight) 100;
        else if (!strcmp (str, "200"))
            return (PangoWeight) 200;
        else if (!strcmp (str, "300"))
            return (PangoWeight) 300;
        else if (!strcmp (str, "400"))
            return (PangoWeight) 400;
        else if (!strcmp (str, "500"))
            return (PangoWeight) 500;
        else if (!strcmp (str, "600"))
            return (PangoWeight) 600;
        else if (!strcmp (str, "700"))
            return (PangoWeight) 700;
        else if (!strcmp (str, "800"))
            return (PangoWeight) 800;
        else if (!strcmp (str, "900"))
            return (PangoWeight) 900;
        else if (!strcmp (str, "inherit")) {
            UNSETINHERIT ();
            return PANGO_WEIGHT_NORMAL;
        }
    }

    UNSETINHERIT ();
    return PANGO_WEIGHT_NORMAL;
}

PangoStretch
rsvg_css_parse_font_stretch (const char *str, gboolean * inherit)
{
    SETINHERIT ();

    if (str) {
        if (!strcmp (str, "ultra-condensed"))
            return PANGO_STRETCH_ULTRA_CONDENSED;
        else if (!strcmp (str, "extra-condensed"))
            return PANGO_STRETCH_EXTRA_CONDENSED;
        else if (!strcmp (str, "condensed") || !strcmp (str, "narrower"))       /* narrower not quite correct */
            return PANGO_STRETCH_CONDENSED;
        else if (!strcmp (str, "semi-condensed"))
            return PANGO_STRETCH_SEMI_CONDENSED;
        else if (!strcmp (str, "semi-expanded"))
            return PANGO_STRETCH_SEMI_EXPANDED;
        else if (!strcmp (str, "expanded") || !strcmp (str, "wider"))   /* wider not quite correct */
            return PANGO_STRETCH_EXPANDED;
        else if (!strcmp (str, "extra-expanded"))
            return PANGO_STRETCH_EXTRA_EXPANDED;
        else if (!strcmp (str, "ultra-expanded"))
            return PANGO_STRETCH_ULTRA_EXPANDED;
        else if (!strcmp (str, "inherit")) {
            UNSETINHERIT ();
            return PANGO_STRETCH_NORMAL;
        }
    }
    UNSETINHERIT ();
    return PANGO_STRETCH_NORMAL;
}

const char *
rsvg_css_parse_font_family (const char *str, gboolean * inherit)
{
    SETINHERIT ();

    if (!str)
        return NULL;
    else if (!strcmp (str, "inherit")) {
        UNSETINHERIT ();
        return NULL;
    } else
        return str;
}

#if !defined(HAVE_STRTOK_R)

static char *
strtok_r (char *s, const char *delim, char **last)
{
    char *p;

    if (s == NULL)
        s = *last;

    if (s == NULL)
        return NULL;

    while (*s && strchr (delim, *s))
        s++;

    if (*s == '\0') {
        *last = NULL;
        return NULL;
    }

    p = s;
    while (*p && !strchr (delim, *p))
        p++;

    if (*p == '\0')
        *last = NULL;
    else {
        *p = '\0';
        p++;
        *last = p;
    }

    return s;
}

#endif                          /* !HAVE_STRTOK_R */

gchar **
rsvg_css_parse_list (const char *in_str, guint * out_list_len)
{
    char *ptr, *tok;
    char *str;

    guint n = 0;
    GSList *string_list = NULL;
    gchar **string_array = NULL;

    str = g_strdup (in_str);
    tok = strtok_r (str, ", \t", &ptr);
    if (tok != NULL) {
        if (strcmp (tok, " ") != 0) {
            string_list = g_slist_prepend (string_list, g_strdup (tok));
            n++;
        }

        while ((tok = strtok_r (NULL, ", \t", &ptr)) != NULL) {
            if (strcmp (tok, " ") != 0) {
                if (n >= RSVG_MAX_CSS_LIST_TOKENS) {
                    g_slist_foreach (string_list, (GFunc) g_free, NULL);
                    g_slist_free (string_list);
                    g_free (str);
                    if (out_list_len)
                        *out_list_len = 0;
                    return NULL;
                }
                string_list = g_slist_prepend (string_list, g_strdup (tok));
                n++;
            }
        }
    }
    g_free (str);

    if (out_list_len)
        *out_list_len = n;

    if (string_list) {
        GSList *slist;

        string_array = g_new (gchar *, n + 1);

        string_array[n--] = NULL;
        for (slist = string_list; slist; slist = slist->next)
            string_array[n--] = (gchar *) slist->data;

        g_slist_free (string_list);
    }

    return string_array;
}

gdouble *
rsvg_css_parse_number_list (const char *in_str, guint * out_list_len)
{
    gchar **string_array;
    gdouble *output;
    guint len, i;

    if (out_list_len)
        *out_list_len = 0;

    string_array = rsvg_css_parse_list (in_str, &len);

    if (!(string_array && len))
        return NULL;

    output = g_new (gdouble, len);

    /* TODO: some error checking */
    for (i = 0; i < len; i++)
        output[i] = g_ascii_strtod (string_array[i], NULL);

    g_strfreev (string_array);

    if (out_list_len != NULL)
        *out_list_len = len;

    return output;
}

void
rsvg_css_parse_number_optional_number (const char *str, double *x, double *y)
{
    char *endptr;

    /* TODO: some error checking */

    *x = g_ascii_strtod (str, &endptr);

    if (endptr && *endptr != '\0')
        while (g_ascii_isspace (*endptr) && *endptr)
            endptr++;

    if (endptr && *endptr)
        *y = g_ascii_strtod (endptr, NULL);
    else
        *y = *x;
}

int
rsvg_css_parse_aspect_ratio (const char *str)
{
    char **elems;
    guint nb_elems;

    int ratio = RSVG_ASPECT_RATIO_NONE;

    elems = rsvg_css_parse_list (str, &nb_elems);

    if (elems && nb_elems) {
        guint i;

        for (i = 0; i < nb_elems; i++) {
            if (!strcmp (elems[i], "xMinYMin"))
                ratio = RSVG_ASPECT_RATIO_XMIN_YMIN;
            else if (!strcmp (elems[i], "xMidYMin"))
                ratio = RSVG_ASPECT_RATIO_XMID_YMIN;
            else if (!strcmp (elems[i], "xMaxYMin"))
                ratio = RSVG_ASPECT_RATIO_XMAX_YMIN;
            else if (!strcmp (elems[i], "xMinYMid"))
                ratio = RSVG_ASPECT_RATIO_XMIN_YMID;
            else if (!strcmp (elems[i], "xMidYMid"))
                ratio = RSVG_ASPECT_RATIO_XMID_YMID;
            else if (!strcmp (elems[i], "xMaxYMid"))
                ratio = RSVG_ASPECT_RATIO_XMAX_YMID;
            else if (!strcmp (elems[i], "xMinYMax"))
                ratio = RSVG_ASPECT_RATIO_XMIN_YMAX;
            else if (!strcmp (elems[i], "xMidYMax"))
                ratio = RSVG_ASPECT_RATIO_XMID_YMAX;
            else if (!strcmp (elems[i], "xMaxYMax"))
                ratio = RSVG_ASPECT_RATIO_XMAX_YMAX;
            else if (!strcmp (elems[i], "slice"))
                ratio |= RSVG_ASPECT_RATIO_SLICE;
        }

        g_strfreev (elems);
    }

    return ratio;
}

gboolean
rsvg_css_parse_overflow (const char *str, gboolean * inherit)
{
    SETINHERIT ();
    if (!strcmp (str, "visible") || !strcmp (str, "auto"))
        return 1;
    if (!strcmp (str, "hidden") || !strcmp (str, "scroll"))
        return 0;
    UNSETINHERIT ();
    return 0;
}

/* This is quite hacky and not entirely correct, but apparently
 * libxml2 has NO support for parsing pseudo attributes as defined
 * by the xml-stylesheet spec.
 *
 * Wrap the PI data in a dummy element and parse with xmlReadMemory.
 * The old push-parser path passed strlen(tag)+1 (including the NUL)
 * to xmlCreatePushParserCtxt; libxml2 2.15 treats that extra byte as
 * document content and xmlParseDocument() returns -1, so xml-stylesheet
 * href/type were dropped (struct-use-04-b fills stayed black).
 * xmlReadMemory(buffer, strlen, …) is valid on older libxml2 and 2.15+.
 */
char **
rsvg_css_parse_xml_attribute_string (const char *attribute_string)
{
    xmlDocPtr doc;
    xmlNodePtr node;
    xmlAttrPtr attr;
    char *tag;
    GPtrArray *attributes;
    char **retval = NULL;

    tag = g_strdup_printf ("<rsvg-hack %s />", attribute_string);
    doc = xmlReadMemory (tag, (int) strlen (tag), "rsvg-hack.xml", NULL,
                         XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
    g_free (tag);
    if (doc == NULL)
        return NULL;

    node = xmlDocGetRootElement (doc);
    if (node == NULL ||
        strcmp ((const char *) node->name, "rsvg-hack") != 0 ||
        node->properties == NULL)
        goto done;

    attributes = g_ptr_array_new ();
    for (attr = node->properties; attr; attr = attr->next) {
        xmlNodePtr content = attr->children;

        g_ptr_array_add (attributes, g_strdup ((char *) attr->name));
        if (content && content->content)
          g_ptr_array_add (attributes, g_strdup ((char *) content->content));
        else
          g_ptr_array_add (attributes, g_strdup (""));
    }

    g_ptr_array_add (attributes, NULL);
    retval = (char **) g_ptr_array_free (attributes, FALSE);

  done:
    xmlFreeDoc (doc);
    return retval;
}
