/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
   rsvg-css-engine.c: CSS cascade and selector matcher.

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

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "rsvg-private.h"
#include "rsvg-styles.h"
#include "rsvg-css.h"
#include "css/rsvg-css-engine.h"

/* See rust/rsvg/src/ua.css — only the rules this engine can parse. */
static const char rsvg_ua_css[] =
    "svg:not(:root), image, marker, pattern, symbol { overflow: hidden; }\n"
    "defs, clipPath, mask, marker, desc, title, metadata,\n"
    "pattern, linearGradient, radialGradient,\n"
    "script, style, symbol { display: none !important; }\n";

gboolean
rsvg_css_engine_enabled (void)
{
    static gsize warned = 0;
    const char *e = g_getenv ("RSVG_CSS_ENGINE");

    /* css/ is the only engine. libcroco is discontinued. */
    if (e != NULL && *e != '\0' &&
        (g_ascii_strcasecmp (e, "croco") == 0 ||
         g_ascii_strcasecmp (e, "old") == 0 ||
         g_ascii_strcasecmp (e, "libcroco") == 0 ||
         strcmp (e, "0") == 0 ||
         g_ascii_strcasecmp (e, "off") == 0)) {
        if (g_once_init_enter (&warned)) {
            g_warning ("RSVG_CSS_ENGINE=%s ignored: libcroco is discontinued; using css/", e);
            g_once_init_leave (&warned, 1);
        }
    }
    return TRUE;
}

/* ---------- selector AST ---------- */

typedef enum {
    COMB_NONE = 0,
    COMB_DESCENDANT,
    COMB_CHILD,
    COMB_NEXT_SIBLING,
    COMB_LATER_SIBLING
} CombKind;

typedef enum {
    ATTR_EXISTS,
    ATTR_EQ,
    ATTR_INCLUDES,
    ATTR_DASH,
    ATTR_PREFIX,
    ATTR_SUFFIX,
    ATTR_SUBSTRING
} AttrOp;

typedef enum {
    PSEUDO_ROOT,
    PSEUDO_EMPTY,
    PSEUDO_LINK,
    PSEUDO_VISITED,
    PSEUDO_LANG,
    PSEUDO_NOT,
    PSEUDO_FIRST_CHILD,
    PSEUDO_LAST_CHILD,
    PSEUDO_ONLY_CHILD,
    PSEUDO_FIRST_OF_TYPE,
    PSEUDO_LAST_OF_TYPE,
    PSEUDO_ONLY_OF_TYPE,
    PSEUDO_NTH_CHILD,
    PSEUDO_NTH_LAST_CHILD,
    PSEUDO_NTH_OF_TYPE,
    PSEUDO_NTH_LAST_OF_TYPE
} PseudoKind;

typedef enum {
    QUAL_CLASS,
    QUAL_ID,
    QUAL_ATTR,
    QUAL_PSEUDO
} QualKind;

typedef struct SimpleSel SimpleSel;

typedef struct {
    QualKind kind;
    char *name;
    char *value;
    AttrOp attr_op;
    gboolean attr_i;
    PseudoKind pseudo;
    int nth_a, nth_b;
    SimpleSel *not_sel;
} Qual;

struct SimpleSel {
    char *type;                 /* NULL = universal */
    Qual *quals;
    int n_quals, quals_alloc;
};

typedef struct {
    SimpleSel simple;
    CombKind comb;              /* combinator that precedes this compound */
} Compound;

typedef struct {
    Compound *parts;
    int n_parts, parts_alloc;
    guint32 spec;
} Selector;

typedef struct {
    char *name;
    char *value;
    gboolean important;
    guint32 order;
} Decl;

typedef struct {
    Selector *sels;
    int n_sels, sels_alloc;
    Decl *decls;
    int n_decls, decls_alloc;
} Rule;

typedef struct {
    RsvgCssOrigin origin;
    Rule *rules;
    int n_rules, rules_alloc;
} Sheet;

struct _RsvgCssEngine {
    Sheet ua;
    Sheet user;
    Sheet author;
    guint32 next_order;
};

/* ---------- helpers ---------- */

static void
skip_ws_and_comments (const char **pp, const char *end)
{
    const char *p = *pp;
    for (;;) {
        while (p < end && g_ascii_isspace (*p))
            p++;
        if (p + 1 < end && p[0] == '/' && p[1] == '*') {
            p += 2;
            while (p + 1 < end && !(p[0] == '*' && p[1] == '/'))
                p++;
            if (p + 1 < end)
                p += 2;
            continue;
        }
        break;
    }
    *pp = p;
}

static char *
parse_ident (const char **pp, const char *end)
{
    const char *p = *pp;
    const char *start;
    if (p >= end)
        return NULL;
    if (*p == '-') {
        if (p + 1 >= end || !(g_ascii_isalpha (p[1]) || p[1] == '_'))
            return NULL;
    } else if (!(g_ascii_isalpha (*p) || *p == '_')) {
        return NULL;
    }
    start = p;
    p++;
    while (p < end && (g_ascii_isalnum (*p) || *p == '_' || *p == '-'))
        p++;
    *pp = p;
    if ((gsize) (p - start) > RSVG_MAX_CSS_IDENT_BYTES)
        return NULL;
    return g_strndup (start, p - start);
}

static char *
parse_string (const char **pp, const char *end)
{
    const char *p = *pp;
    char quote;
    GString *s;
    if (p >= end || (*p != '"' && *p != '\''))
        return NULL;
    quote = *p++;
    s = g_string_new (NULL);
    while (p < end && *p != quote) {
        if (*p == '\\' && p + 1 < end) {
            p++;
            g_string_append_c (s, *p++);
        } else {
            g_string_append_c (s, *p++);
        }
    }
    if (p < end && *p == quote)
        p++;
    *pp = p;
    if (s->len > RSVG_MAX_CSS_ATTR_VALUE_BYTES) {
        g_string_free (s, TRUE);
        return NULL;
    }
    return g_string_free (s, FALSE);
}

static char *
parse_ident_or_string (const char **pp, const char *end)
{
    const char *p = *pp;
    skip_ws_and_comments (&p, end);
    if (p < end && (*p == '"' || *p == '\'')) {
        char *s = parse_string (&p, end);
        *pp = p;
        return s;
    }
    {
        char *s = parse_ident (&p, end);
        *pp = p;
        return s;
    }
}

static void
simple_sel_init (SimpleSel *s)
{
    memset (s, 0, sizeof (*s));
}

static void qual_clear (Qual *q);

static void
simple_sel_add_qual (SimpleSel *s, Qual q)
{
    if (s->n_quals >= RSVG_MAX_CSS_QUALS) {
        qual_clear (&q);
        return;
    }
    if (s->n_quals == s->quals_alloc) {
        s->quals_alloc = s->quals_alloc ? s->quals_alloc * 2 : 4;
        if (s->quals_alloc > RSVG_MAX_CSS_QUALS)
            s->quals_alloc = RSVG_MAX_CSS_QUALS;
        s->quals = g_renew (Qual, s->quals, s->quals_alloc);
    }
    s->quals[s->n_quals++] = q;
}

static void simple_sel_clear (SimpleSel *s);

static void
qual_clear (Qual *q)
{
    g_free (q->name);
    g_free (q->value);
    if (q->not_sel) {
        simple_sel_clear (q->not_sel);
        g_free (q->not_sel);
    }
}

static void
simple_sel_clear (SimpleSel *s)
{
    int i;
    g_free (s->type);
    for (i = 0; i < s->n_quals; i++)
        qual_clear (&s->quals[i]);
    g_free (s->quals);
    memset (s, 0, sizeof (*s));
}

static void
selector_clear (Selector *sel)
{
    int i;
    for (i = 0; i < sel->n_parts; i++)
        simple_sel_clear (&sel->parts[i].simple);
    g_free (sel->parts);
    memset (sel, 0, sizeof (*sel));
}

static guint32
simple_sel_spec (const SimpleSel *s)
{
    guint32 spec = 0;
    int i;
    if (s->type)
        spec += 1;
    for (i = 0; i < s->n_quals; i++) {
        const Qual *q = &s->quals[i];
        if (q->kind == QUAL_ID)
            spec += 0x0100;
        else if (q->kind == QUAL_CLASS || q->kind == QUAL_ATTR)
            spec += 0x0010;
        else if (q->kind == QUAL_PSEUDO) {
            if (q->pseudo == PSEUDO_NOT && q->not_sel)
                spec += simple_sel_spec (q->not_sel);
            else
                spec += 0x0010;
        }
    }
    return spec;
}

static guint32
selector_spec (const Selector *sel)
{
    guint32 spec = 0;
    int i;
    for (i = 0; i < sel->n_parts; i++)
        spec += simple_sel_spec (&sel->parts[i].simple);
    return spec;
}

/* ---------- nth-child ---------- */

static gboolean
parse_css_uint (const char **pp, const char *end, int *out)
{
    char *term = NULL;
    gint64 v;
    const char *p = *pp;

    if (p >= end || !g_ascii_isdigit (*p))
        return FALSE;
    v = g_ascii_strtoll (p, &term, 10);
    if (term == p || v < 0 || v > G_MAXINT)
        return FALSE;
    *out = (int) v;
    *pp = term;
    return TRUE;
}

static gboolean
parse_nth (const char **pp, const char *end, int *a, int *b)
{
    const char *p = *pp;
    char *ident;
    skip_ws_and_comments (&p, end);
    ident = parse_ident (&p, end);
    if (ident) {
        if (g_ascii_strcasecmp (ident, "odd") == 0) {
            *a = 2;
            *b = 1;
            g_free (ident);
            *pp = p;
            return TRUE;
        }
        if (g_ascii_strcasecmp (ident, "even") == 0) {
            *a = 2;
            *b = 0;
            g_free (ident);
            *pp = p;
            return TRUE;
        }
        if (g_ascii_strcasecmp (ident, "n") == 0) {
            *a = 1;
            skip_ws_and_comments (&p, end);
            if (p < end && (*p == '+' || *p == '-')) {
                int sign = (*p == '+') ? 1 : -1;
                p++;
                skip_ws_and_comments (&p, end);
                {
                    int mag = 0;
                    if (!parse_css_uint (&p, end, &mag)) {
                        g_free (ident);
                        return FALSE;
                    }
                    *b = sign * mag;
                }
            } else {
                *b = 0;
            }
            g_free (ident);
            *pp = p;
            return TRUE;
        }
        g_free (ident);
        p = *pp;
        skip_ws_and_comments (&p, end);
    }

    /* [+/-]? digits n?  or  [+/-]? n */
    {
        int sign = 1;
        gboolean saw_num = FALSE;
        long num = 0;
        skip_ws_and_comments (&p, end);
        if (p < end && (*p == '+' || *p == '-')) {
            sign = (*p == '+') ? 1 : -1;
            p++;
            skip_ws_and_comments (&p, end);
        }
        if (p < end && g_ascii_isdigit (*p)) {
            int mag = 0;
            if (!parse_css_uint (&p, end, &mag))
                return FALSE;
            num = mag;
            saw_num = TRUE;
        }
        skip_ws_and_comments (&p, end);
        if (p < end && (*p == 'n' || *p == 'N')) {
            p++;
            *a = saw_num ? (int) (sign * num) : sign;
            skip_ws_and_comments (&p, end);
            if (p < end && (*p == '+' || *p == '-')) {
                int bsign = (*p == '+') ? 1 : -1;
                int mag = 0;
                p++;
                skip_ws_and_comments (&p, end);
                if (!parse_css_uint (&p, end, &mag))
                    return FALSE;
                *b = bsign * mag;
            } else {
                *b = 0;
            }
            *pp = p;
            return TRUE;
        }
        if (saw_num) {
            *a = 0;
            *b = (int) (sign * num);
            *pp = p;
            return TRUE;
        }
    }
    return FALSE;
}

static gboolean
nth_matches (int index1, int a, int b)
{
    int an;
    if (a == 0)
        return index1 == b;
    an = index1 - b;
    if (a > 0)
        return an >= 0 && (an % a) == 0;
    return an <= 0 && (an % a) == 0;
}

/* ---------- parse simple selector / selector ---------- */

static gboolean parse_simple_sel (const char **pp, const char *end, SimpleSel *out, int not_depth);

static gboolean
parse_pseudo (const char **pp, const char *end, Qual *q, int not_depth)
{
    const char *p = *pp;
    char *name;
    memset (q, 0, sizeof (*q));
    q->kind = QUAL_PSEUDO;
    name = parse_ident (&p, end);
    if (!name)
        return FALSE;

    if (g_ascii_strcasecmp (name, "root") == 0)
        q->pseudo = PSEUDO_ROOT;
    else if (g_ascii_strcasecmp (name, "empty") == 0)
        q->pseudo = PSEUDO_EMPTY;
    else if (g_ascii_strcasecmp (name, "link") == 0)
        q->pseudo = PSEUDO_LINK;
    else if (g_ascii_strcasecmp (name, "visited") == 0)
        q->pseudo = PSEUDO_VISITED;
    else if (g_ascii_strcasecmp (name, "first-child") == 0)
        q->pseudo = PSEUDO_FIRST_CHILD;
    else if (g_ascii_strcasecmp (name, "last-child") == 0)
        q->pseudo = PSEUDO_LAST_CHILD;
    else if (g_ascii_strcasecmp (name, "only-child") == 0)
        q->pseudo = PSEUDO_ONLY_CHILD;
    else if (g_ascii_strcasecmp (name, "first-of-type") == 0)
        q->pseudo = PSEUDO_FIRST_OF_TYPE;
    else if (g_ascii_strcasecmp (name, "last-of-type") == 0)
        q->pseudo = PSEUDO_LAST_OF_TYPE;
    else if (g_ascii_strcasecmp (name, "only-of-type") == 0)
        q->pseudo = PSEUDO_ONLY_OF_TYPE;
    else if (g_ascii_strcasecmp (name, "lang") == 0 ||
             g_ascii_strcasecmp (name, "not") == 0 ||
             g_str_has_prefix (name, "nth-")) {
        skip_ws_and_comments (&p, end);
        if (p >= end || *p != '(') {
            g_free (name);
            return FALSE;
        }
        p++;
        if (g_ascii_strcasecmp (name, "lang") == 0) {
            /* :lang(en, cn) — comma-separated language ranges (2.62). */
            GString *langs;
            q->pseudo = PSEUDO_LANG;
            langs = g_string_new (NULL);
            for (;;) {
                char *one = parse_ident_or_string (&p, end);
                if (!one)
                    break;
                if (langs->len)
                    g_string_append_c (langs, ',');
                g_string_append (langs, one);
                g_free (one);
                skip_ws_and_comments (&p, end);
                if (p < end && *p == ',') {
                    p++;
                    skip_ws_and_comments (&p, end);
                    continue;
                }
                break;
            }
            if (langs->len == 0) {
                g_string_free (langs, TRUE);
                g_free (name);
                return FALSE;
            }
            q->value = g_string_free (langs, FALSE);
        } else if (g_ascii_strcasecmp (name, "not") == 0) {
            q->pseudo = PSEUDO_NOT;
            if (not_depth >= RSVG_MAX_CSS_NOT_DEPTH) {
                g_free (name);
                return FALSE;
            }
            q->not_sel = g_new0 (SimpleSel, 1);
            skip_ws_and_comments (&p, end);
            if (!parse_simple_sel (&p, end, q->not_sel, not_depth + 1)) {
                g_free (q->not_sel);
                q->not_sel = NULL;
                g_free (name);
                return FALSE;
            }
        } else {
            if (g_ascii_strcasecmp (name, "nth-child") == 0)
                q->pseudo = PSEUDO_NTH_CHILD;
            else if (g_ascii_strcasecmp (name, "nth-last-child") == 0)
                q->pseudo = PSEUDO_NTH_LAST_CHILD;
            else if (g_ascii_strcasecmp (name, "nth-of-type") == 0)
                q->pseudo = PSEUDO_NTH_OF_TYPE;
            else if (g_ascii_strcasecmp (name, "nth-last-of-type") == 0)
                q->pseudo = PSEUDO_NTH_LAST_OF_TYPE;
            else {
                g_free (name);
                return FALSE;
            }
            if (!parse_nth (&p, end, &q->nth_a, &q->nth_b)) {
                g_free (name);
                return FALSE;
            }
        }
        skip_ws_and_comments (&p, end);
        if (p >= end || *p != ')') {
            g_free (name);
            return FALSE;
        }
        p++;
    } else {
        g_free (name);
        return FALSE;
    }
    g_free (name);
    *pp = p;
    return TRUE;
}

static gboolean
parse_attrib (const char **pp, const char *end, Qual *q)
{
    const char *p = *pp;
    char *ns;
    memset (q, 0, sizeof (*q));
    q->kind = QUAL_ATTR;
    q->attr_op = ATTR_EXISTS;
    skip_ws_and_comments (&p, end);
    ns = parse_ident (&p, end);
    if (!ns)
        return FALSE;
    skip_ws_and_comments (&p, end);
    if (p < end && *p == '|' && !(p + 1 < end && p[1] == '=')) {
        /* ns|attr — keep the local name.  Do not treat "|=" as a namespace. */
        p++;
        g_free (ns);
        skip_ws_and_comments (&p, end);
        ns = parse_ident (&p, end);
        if (!ns)
            return FALSE;
    }
    q->name = ns;
    skip_ws_and_comments (&p, end);
    if (p < end && *p != ']') {
        if (p + 1 < end && p[0] == '~' && p[1] == '=') {
            q->attr_op = ATTR_INCLUDES;
            p += 2;
        } else if (p + 1 < end && p[0] == '|' && p[1] == '=') {
            q->attr_op = ATTR_DASH;
            p += 2;
        } else if (p + 1 < end && p[0] == '^' && p[1] == '=') {
            q->attr_op = ATTR_PREFIX;
            p += 2;
        } else if (p + 1 < end && p[0] == '$' && p[1] == '=') {
            q->attr_op = ATTR_SUFFIX;
            p += 2;
        } else if (p + 1 < end && p[0] == '*' && p[1] == '=') {
            q->attr_op = ATTR_SUBSTRING;
            p += 2;
        } else if (*p == '=') {
            q->attr_op = ATTR_EQ;
            p++;
        } else {
            return FALSE;
        }
        q->value = parse_ident_or_string (&p, end);
        if (!q->value)
            return FALSE;
        skip_ws_and_comments (&p, end);
        if (p < end && (*p == 'i' || *p == 'I')) {
            const char *qch = p + 1;
            if (qch >= end || g_ascii_isspace (*qch) || *qch == ']') {
                q->attr_i = TRUE;
                p++;
                skip_ws_and_comments (&p, end);
            }
        }
    }
    if (p >= end || *p != ']')
        return FALSE;
    p++;
    *pp = p;
    return TRUE;
}

static gboolean
parse_simple_sel (const char **pp, const char *end, SimpleSel *out, int not_depth)
{
    const char *p = *pp;
    gboolean any = FALSE;

    simple_sel_init (out);
    skip_ws_and_comments (&p, end);
    if (p >= end)
        return FALSE;

    if (*p == '*') {
        p++;
        any = TRUE;
    } else if (*p == '|') {
        p++;
        if (p < end && *p == '*')
            p++;
        else {
            out->type = parse_ident (&p, end);
            if (!out->type)
                return FALSE;
        }
        any = TRUE;
    } else if (g_ascii_isalpha (*p) || *p == '_' || *p == '-') {
        char *ident = parse_ident (&p, end);
        if (!ident)
            return FALSE;
        skip_ws_and_comments (&p, end);
        if (p < end && *p == '|') {
            /* ns|type */
            p++;
            g_free (ident);
            if (p < end && *p == '*') {
                p++;
            } else {
                out->type = parse_ident (&p, end);
                if (!out->type)
                    return FALSE;
            }
        } else {
            out->type = ident;
        }
        any = TRUE;
    }

    for (;;) {
        Qual q;
        if (p >= end)
            break;
        if (*p == '.') {
            p++;
            memset (&q, 0, sizeof (q));
            q.kind = QUAL_CLASS;
            q.name = parse_ident (&p, end);
            if (!q.name)
                break;
            simple_sel_add_qual (out, q);
            any = TRUE;
        } else if (*p == '#') {
            p++;
            memset (&q, 0, sizeof (q));
            q.kind = QUAL_ID;
            q.name = parse_ident (&p, end);
            if (!q.name)
                break;
            simple_sel_add_qual (out, q);
            any = TRUE;
        } else if (*p == '[') {
            p++;
            if (!parse_attrib (&p, end, &q)) {
                simple_sel_clear (out);
                return FALSE;
            }
            simple_sel_add_qual (out, q);
            any = TRUE;
        } else if (*p == ':') {
            p++;
            if (p < end && *p == ':')
                p++;            /* skip functional pseudo-element */
            if (!parse_pseudo (&p, end, &q, not_depth)) {
                simple_sel_clear (out);
                return FALSE;
            }
            simple_sel_add_qual (out, q);
            any = TRUE;
        } else {
            break;
        }
    }

    if (!any) {
        simple_sel_clear (out);
        return FALSE;
    }
    *pp = p;
    return TRUE;
}

static gboolean
parse_selector (const char **pp, const char *end, Selector *out)
{
    const char *p = *pp;
    memset (out, 0, sizeof (*out));
    skip_ws_and_comments (&p, end);
    for (;;) {
        Compound c;
        CombKind comb = COMB_NONE;
        const char *before;

        memset (&c, 0, sizeof (c));
        if (out->n_parts > 0) {
            skip_ws_and_comments (&p, end);
            if (p >= end)
                break;
            if (*p == ',')
                break;
            if (*p == '{')
                break;
            if (*p == '>') {
                comb = COMB_CHILD;
                p++;
            } else if (*p == '+') {
                comb = COMB_NEXT_SIBLING;
                p++;
            } else if (*p == '~') {
                comb = COMB_LATER_SIBLING;
                p++;
            } else {
                comb = COMB_DESCENDANT;
            }
            skip_ws_and_comments (&p, end);
        }

        before = p;
        if (!parse_simple_sel (&p, end, &c.simple, 0)) {
            if (out->n_parts == 0)
                return FALSE;
            p = before;
            break;
        }
        c.comb = comb;
        if (out->n_parts >= RSVG_MAX_CSS_SELECTOR_PARTS) {
            simple_sel_clear (&c.simple);
            break;
        }
        if (out->n_parts == out->parts_alloc) {
            out->parts_alloc = out->parts_alloc ? out->parts_alloc * 2 : 4;
            if (out->parts_alloc > RSVG_MAX_CSS_SELECTOR_PARTS)
                out->parts_alloc = RSVG_MAX_CSS_SELECTOR_PARTS;
            out->parts = g_renew (Compound, out->parts, out->parts_alloc);
        }
        out->parts[out->n_parts++] = c;
    }
    if (out->n_parts == 0)
        return FALSE;
    out->spec = selector_spec (out);
    *pp = p;
    return TRUE;
}

/* ---------- match ---------- */

static gboolean
node_is_element (const RsvgNode *n)
{
    return n && n->type != RSVG_NODE_TYPE_CHARS && n->type != RSVG_NODE_TYPE_INVALID;
}

static const char *
node_tag (const RsvgNode *n)
{
    if (!n)
        return NULL;
    if (n->css_tag && n->css_tag[0])
        return n->css_tag;
    return n->name;
}

static gboolean
tags_equal (const char *a, const char *b)
{
    if (!a || !b)
        return FALSE;
    return g_ascii_strcasecmp (a, b) == 0;
}

static RsvgNode *
prev_element (const RsvgNode *n)
{
    RsvgNode *parent;
    guint i;
    if (!n || !n->parent || !n->parent->children)
        return NULL;
    parent = n->parent;
    for (i = 0; i < parent->children->len; i++) {
        if (g_ptr_array_index (parent->children, i) == n) {
            while (i > 0) {
                RsvgNode *c = g_ptr_array_index (parent->children, i - 1);
                i--;
                if (node_is_element (c))
                    return c;
            }
            return NULL;
        }
    }
    return NULL;
}

static int
element_index (const RsvgNode *n, gboolean of_type, gboolean from_end)
{
    RsvgNode *parent;
    int idx = 0, total = 0;
    guint i;
    const char *tag;
    if (!n || !n->parent || !n->parent->children)
        return 1;
    parent = n->parent;
    tag = node_tag (n);
    for (i = 0; i < parent->children->len; i++) {
        RsvgNode *c = g_ptr_array_index (parent->children, i);
        if (!node_is_element (c))
            continue;
        if (of_type && !tags_equal (node_tag (c), tag))
            continue;
        total++;
        if (c == n)
            idx = total;
    }
    if (from_end)
        return total - idx + 1;
    return idx;
}

static int
element_count (const RsvgNode *n, gboolean of_type)
{
    RsvgNode *parent;
    int total = 0;
    guint i;
    const char *tag;
    if (!n || !n->parent || !n->parent->children)
        return 1;
    parent = n->parent;
    tag = node_tag (n);
    for (i = 0; i < parent->children->len; i++) {
        RsvgNode *c = g_ptr_array_index (parent->children, i);
        if (!node_is_element (c))
            continue;
        if (of_type && !tags_equal (node_tag (c), tag))
            continue;
        total++;
    }
    return total;
}

static const char *
node_attr (const RsvgNode *n, const char *name)
{
    const char *v;
    char *colon;
    if (!n)
        return NULL;
    if (g_ascii_strcasecmp (name, "id") == 0 && n->css_id)
        return n->css_id;
    if (g_ascii_strcasecmp (name, "class") == 0 && n->css_class)
        return n->css_class;
    if (!n->atts)
        return NULL;
    v = rsvg_property_bag_lookup (n->atts, name);
    if (v)
        return v;
    /* try local-name match: xlink:href vs href */
    colon = strchr (name, ':');
    if (colon)
        return rsvg_property_bag_lookup (n->atts, colon + 1);
    {
        /* scan not available; try common prefixes */
        char *xh = g_strdup_printf ("xlink:%s", name);
        v = rsvg_property_bag_lookup (n->atts, xh);
        g_free (xh);
        if (v)
            return v;
        xh = g_strdup_printf ("xml:%s", name);
        v = rsvg_property_bag_lookup (n->atts, xh);
        g_free (xh);
        return v;
    }
}

static gboolean
has_class (const RsvgNode *n, const char *cls)
{
    const char *p;
    if (!n || !cls)
        return FALSE;
    p = n->css_class ? n->css_class : node_attr (n, "class");
    if (!p)
        return FALSE;
    while (*p) {
        const char *start;
        while (*p && g_ascii_isspace (*p))
            p++;
        start = p;
        while (*p && !g_ascii_isspace (*p))
            p++;
        if (p > start && (gsize) (p - start) == strlen (cls) &&
            strncmp (start, cls, p - start) == 0)
            return TRUE;
    }
    return FALSE;
}

static const char *
node_lang (const RsvgNode *n)
{
    const RsvgNode *cur;
    for (cur = n; cur; cur = cur->parent) {
        const char *v = node_attr (cur, "xml:lang");
        if (!v)
            v = node_attr (cur, "lang");
        if (v && v[0])
            return v;
        if (cur->state && cur->state->has_lang && cur->state->lang)
            return cur->state->lang;
    }
    return NULL;
}

static gboolean
lang_matches (const char *have, const char *want)
{
    gsize wlen;
    if (!have || !want)
        return FALSE;
    if (g_ascii_strcasecmp (have, want) == 0)
        return TRUE;
    wlen = strlen (want);
    return g_ascii_strncasecmp (have, want, wlen) == 0 && have[wlen] == '-';
}

static gboolean
attr_match (const char *have, const Qual *q)
{
    if (!have)
        return FALSE;
    if (q->attr_op == ATTR_EXISTS)
        return TRUE;
    if (!q->value)
        return FALSE;
    if (q->attr_i) {
        switch (q->attr_op) {
        case ATTR_EQ:
            return g_ascii_strcasecmp (have, q->value) == 0;
        case ATTR_PREFIX:
            return g_ascii_strncasecmp (have, q->value, strlen (q->value)) == 0;
        case ATTR_SUFFIX: {
            gsize hl = strlen (have), vl = strlen (q->value);
            return hl >= vl && g_ascii_strcasecmp (have + hl - vl, q->value) == 0;
        }
        case ATTR_SUBSTRING: {
            gchar *h = g_ascii_strdown (have, -1);
            gchar *v = g_ascii_strdown (q->value, -1);
            gboolean r = strstr (h, v) != NULL;
            g_free (h);
            g_free (v);
            return r;
        }
        case ATTR_INCLUDES:
        case ATTR_DASH:
        default:
            break;
        }
    }
    switch (q->attr_op) {
    case ATTR_EQ:
        return strcmp (have, q->value) == 0;
    case ATTR_PREFIX:
        return g_str_has_prefix (have, q->value);
    case ATTR_SUFFIX:
        return g_str_has_suffix (have, q->value);
    case ATTR_SUBSTRING:
        return strstr (have, q->value) != NULL;
    case ATTR_INCLUDES: {
        const char *p = have;
        gsize vl = strlen (q->value);
        while (*p) {
            const char *s;
            while (*p && g_ascii_isspace (*p))
                p++;
            s = p;
            while (*p && !g_ascii_isspace (*p))
                p++;
            if ((gsize) (p - s) == vl && strncmp (s, q->value, vl) == 0)
                return TRUE;
        }
        return FALSE;
    }
    case ATTR_DASH:
        if (strcmp (have, q->value) == 0)
            return TRUE;
        {
            gsize vl = strlen (q->value);
            return strncmp (have, q->value, vl) == 0 && have[vl] == '-';
        }
    default:
        return FALSE;
    }
}

static gboolean match_simple (const SimpleSel *s, const RsvgNode *n);

static gboolean
match_pseudo (const Qual *q, const RsvgNode *n)
{
    switch (q->pseudo) {
    case PSEUDO_ROOT:
        return n->parent == NULL;
    case PSEUDO_EMPTY: {
        guint i;
        if (!n->children)
            return TRUE;
        for (i = 0; i < n->children->len; i++) {
            RsvgNode *c = g_ptr_array_index (n->children, i);
            if (node_is_element (c))
                return FALSE;
            if (c->type == RSVG_NODE_TYPE_CHARS) {
                RsvgNodeChars *ch = (RsvgNodeChars *) c;
                if (ch->contents && ch->contents->len > 0)
                    return FALSE;
            }
        }
        return TRUE;
    }
    case PSEUDO_LINK: {
        const char *href;
        if (!tags_equal (node_tag (n), "a"))
            return FALSE;
        href = n->atts ? rsvg_property_bag_lookup_href (n->atts) : NULL;
        return href != NULL;
    }
    case PSEUDO_VISITED:
        return FALSE;
    case PSEUDO_LANG: {
        char **langs;
        int i;
        gboolean ok = FALSE;
        if (!q->value)
            return FALSE;
        langs = g_strsplit (q->value, ",", -1);
        for (i = 0; langs[i]; i++) {
            g_strstrip (langs[i]);
            if (lang_matches (node_lang (n), langs[i])) {
                ok = TRUE;
                break;
            }
        }
        g_strfreev (langs);
        return ok;
    }
    case PSEUDO_NOT:
        return q->not_sel && !match_simple (q->not_sel, n);
    case PSEUDO_FIRST_CHILD:
        return element_index (n, FALSE, FALSE) == 1;
    case PSEUDO_LAST_CHILD:
        return element_index (n, FALSE, TRUE) == 1;
    case PSEUDO_ONLY_CHILD:
        return element_count (n, FALSE) == 1;
    case PSEUDO_FIRST_OF_TYPE:
        return element_index (n, TRUE, FALSE) == 1;
    case PSEUDO_LAST_OF_TYPE:
        return element_index (n, TRUE, TRUE) == 1;
    case PSEUDO_ONLY_OF_TYPE:
        return element_count (n, TRUE) == 1;
    case PSEUDO_NTH_CHILD:
        return nth_matches (element_index (n, FALSE, FALSE), q->nth_a, q->nth_b);
    case PSEUDO_NTH_LAST_CHILD:
        return nth_matches (element_index (n, FALSE, TRUE), q->nth_a, q->nth_b);
    case PSEUDO_NTH_OF_TYPE:
        return nth_matches (element_index (n, TRUE, FALSE), q->nth_a, q->nth_b);
    case PSEUDO_NTH_LAST_OF_TYPE:
        return nth_matches (element_index (n, TRUE, TRUE), q->nth_a, q->nth_b);
    default:
        return FALSE;
    }
}

static gboolean
match_simple (const SimpleSel *s, const RsvgNode *n)
{
    int i;
    if (!n || !node_is_element (n))
        return FALSE;
    if (s->type && !tags_equal (node_tag (n), s->type))
        return FALSE;
    for (i = 0; i < s->n_quals; i++) {
        const Qual *q = &s->quals[i];
        switch (q->kind) {
        case QUAL_CLASS:
            if (!has_class (n, q->name))
                return FALSE;
            break;
        case QUAL_ID:
            if (!n->css_id || strcmp (n->css_id, q->name) != 0)
                return FALSE;
            break;
        case QUAL_ATTR:
            if (q->attr_op == ATTR_EXISTS) {
                if (!node_attr (n, q->name))
                    return FALSE;
            } else if (!attr_match (node_attr (n, q->name), q)) {
                return FALSE;
            }
            break;
        case QUAL_PSEUDO:
            if (!match_pseudo (q, n))
                return FALSE;
            break;
        }
    }
    return TRUE;
}

static gboolean
match_from (const Selector *sel, int i, const RsvgNode *n)
{
    const Compound *c;
    if (!n)
        return FALSE;
    c = &sel->parts[i];
    if (!match_simple (&c->simple, n))
        return FALSE;
    if (i == 0)
        return TRUE;
    switch (c->comb) {
    case COMB_CHILD:
        return n->parent && match_from (sel, i - 1, n->parent);
    case COMB_DESCENDANT: {
        RsvgNode *p;
        for (p = n->parent; p; p = p->parent) {
            if (match_from (sel, i - 1, p))
                return TRUE;
        }
        return FALSE;
    }
    case COMB_NEXT_SIBLING: {
        RsvgNode *prev = prev_element (n);
        return prev && match_from (sel, i - 1, prev);
    }
    case COMB_LATER_SIBLING: {
        RsvgNode *prev;
        for (prev = prev_element (n); prev; prev = prev_element (prev)) {
            if (match_from (sel, i - 1, prev))
                return TRUE;
        }
        return FALSE;
    }
    default:
        return FALSE;
    }
}

static gboolean
match_selector (const Selector *sel, const RsvgNode *n)
{
    if (!sel || sel->n_parts == 0)
        return FALSE;
    return match_from (sel, sel->n_parts - 1, n);
}

/* ---------- stylesheet parse ---------- */

static void
sheet_init (Sheet *s, RsvgCssOrigin origin)
{
    memset (s, 0, sizeof (*s));
    s->origin = origin;
}

static void
rule_clear (Rule *r)
{
    int i;
    for (i = 0; i < r->n_sels; i++)
        selector_clear (&r->sels[i]);
    g_free (r->sels);
    for (i = 0; i < r->n_decls; i++) {
        g_free (r->decls[i].name);
        g_free (r->decls[i].value);
    }
    g_free (r->decls);
    memset (r, 0, sizeof (*r));
}

static void
sheet_clear_rules (Sheet *s)
{
    int i;
    for (i = 0; i < s->n_rules; i++)
        rule_clear (&s->rules[i]);
    g_free (s->rules);
    s->rules = NULL;
    s->n_rules = s->rules_alloc = 0;
}

static void
sheet_add_rule (Sheet *s, Rule r)
{
    if (s->n_rules >= RSVG_MAX_CSS_RULES) {
        rule_clear (&r);
        return;
    }
    if (s->n_rules == s->rules_alloc) {
        s->rules_alloc = s->rules_alloc ? s->rules_alloc * 2 : 8;
        if (s->rules_alloc > RSVG_MAX_CSS_RULES)
            s->rules_alloc = RSVG_MAX_CSS_RULES;
        s->rules = g_renew (Rule, s->rules, s->rules_alloc);
    }
    s->rules[s->n_rules++] = r;
}

static char *
parse_decl_value (const char **pp, const char *end, gboolean *important)
{
    const char *p = *pp;
    const char *start;
    const char *val_end;
    GString *s;
    skip_ws_and_comments (&p, end);
    start = p;
    while (p < end && *p != ';' && *p != '}') {
        if (*p == '"' || *p == '\'') {
            char q = *p++;
            while (p < end && *p != q) {
                if (*p == '\\' && p + 1 < end)
                    p++;
                p++;
            }
            if (p < end)
                p++;
            continue;
        }
        if (p + 1 < end && p[0] == '/' && p[1] == '*') {
            p += 2;
            while (p + 1 < end && !(p[0] == '*' && p[1] == '/'))
                p++;
            if (p + 1 < end)
                p += 2;
            continue;
        }
        if (*p == '(') {
            int depth = 1;
            p++;
            while (p < end && depth) {
                if (*p == '(') {
                    depth++;
                    if (depth > RSVG_MAX_CSS_VALUE_PARENS)
                        break;
                } else if (*p == ')')
                    depth--;
                p++;
            }
            continue;
        }
        p++;
    }
    val_end = p;
    while (val_end > start && g_ascii_isspace (val_end[-1]))
        val_end--;
    s = g_string_new_len (start, val_end - start);
    *important = FALSE;
    {
        char *bang = g_strrstr (s->str, "!");
        if (bang) {
            char *w = bang + 1;
            while (*w && g_ascii_isspace (*w))
                w++;
            if (g_ascii_strncasecmp (w, "important", 9) == 0) {
                *important = TRUE;
                while (bang > s->str && g_ascii_isspace (bang[-1]))
                    bang--;
                g_string_truncate (s, bang - s->str);
            }
        }
    }
    *pp = p;
    return g_string_free (s, FALSE);
}

static void parse_stylesheet (RsvgCssEngine *eng, Sheet *sheet,
                              const char *css, gsize len,
                              RsvgHandle *ctx, gboolean allow_import,
                              int import_depth);

static void
handle_import (RsvgCssEngine *eng, Sheet *sheet, const char *uri,
               RsvgHandle *ctx, gboolean allow_import, int import_depth)
{
    char *data;
    gsize data_len;
    char *mime = NULL;

    if (!uri || !ctx)
        return;
    if (import_depth >= RSVG_MAX_CSS_IMPORT_DEPTH)
        return;
    if (!allow_import && g_ascii_strncasecmp (uri, "data:", 5) != 0)
        return;
    data = _rsvg_handle_acquire_data (ctx, uri, &mime, &data_len, NULL);
    if (!data) {
        g_free (mime);
        return;
    }
    if (mime && strcmp (mime, "text/css") != 0) {
        g_free (data);
        g_free (mime);
        return;
    }
    parse_stylesheet (eng, sheet, data, data_len, ctx, allow_import, import_depth + 1);
    g_free (data);
    g_free (mime);
}

static void
parse_stylesheet (RsvgCssEngine *eng, Sheet *sheet,
                  const char *css, gsize len,
                  RsvgHandle *ctx, gboolean allow_import,
                  int import_depth)
{
    const char *p = css;
    const char *end = css + len;

    while (p < end) {
        Rule rule;
        const char *saved;

        saved = p;
        skip_ws_and_comments (&p, end);
        if (p >= end)
            break;
        if (*p == '@') {
            char *at;
            p++;
            at = parse_ident (&p, end);
            skip_ws_and_comments (&p, end);
            if (at && g_ascii_strcasecmp (at, "import") == 0) {
                char *uri = NULL;
                if (p < end && (*p == '"' || *p == '\'')) {
                    uri = parse_string (&p, end);
                } else {
                    char *fn = parse_ident (&p, end);
                    if (fn && g_ascii_strcasecmp (fn, "url") == 0) {
                        skip_ws_and_comments (&p, end);
                        if (p < end && *p == '(') {
                            p++;
                            skip_ws_and_comments (&p, end);
                            if (p < end && (*p == '"' || *p == '\''))
                                uri = parse_string (&p, end);
                            else {
                                const char *s = p;
                                while (p < end && *p != ')' && !g_ascii_isspace (*p))
                                    p++;
                                uri = g_strndup (s, p - s);
                            }
                            skip_ws_and_comments (&p, end);
                            if (p < end && *p == ')')
                                p++;
                        }
                    }
                    g_free (fn);
                }
                skip_ws_and_comments (&p, end);
                if (p < end && *p == ';')
                    p++;
                if (uri && sheet->n_rules < RSVG_MAX_CSS_RULES)
                    handle_import (eng, sheet, uri, ctx, allow_import, import_depth);
                g_free (uri);
            } else {
                /* skip at-rule */
                int depth = 0;
                while (p < end) {
                    if (*p == ';' && depth == 0) {
                        p++;
                        break;
                    }
                    if (*p == '{')
                        depth++;
                    else if (*p == '}') {
                        depth--;
                        p++;
                        if (depth <= 0)
                            break;
                        continue;
                    }
                    p++;
                }
            }
            g_free (at);
            continue;
        }

        if (sheet->n_rules >= RSVG_MAX_CSS_RULES)
            break;

        memset (&rule, 0, sizeof (rule));
        for (;;) {
            Selector sel;
            skip_ws_and_comments (&p, end);
            if (!parse_selector (&p, end, &sel))
                break;
            if (rule.n_sels >= RSVG_MAX_CSS_SELECTORS) {
                selector_clear (&sel);
            } else {
                if (rule.n_sels == rule.sels_alloc) {
                    rule.sels_alloc = rule.sels_alloc ? rule.sels_alloc * 2 : 2;
                    if (rule.sels_alloc > RSVG_MAX_CSS_SELECTORS)
                        rule.sels_alloc = RSVG_MAX_CSS_SELECTORS;
                    rule.sels = g_renew (Selector, rule.sels, rule.sels_alloc);
                }
                rule.sels[rule.n_sels++] = sel;
            }
            skip_ws_and_comments (&p, end);
            if (p < end && *p == ',') {
                p++;
                continue;
            }
            break;
        }
        skip_ws_and_comments (&p, end);
        if (p >= end || *p != '{') {
            rule_clear (&rule);
            /* skip junk until next block. A stray `}` must be consumed
             * or the outer loop stalls (fuzz: "}"). */
            while (p < end && *p != '{' && *p != '}')
                p++;
            if (p < end && *p == '{') {
                int d = 1;
                p++;
                while (p < end && d) {
                    if (*p == '{')
                        d++;
                    else if (*p == '}')
                        d--;
                    p++;
                }
            } else if (p < end && *p == '}') {
                p++;
            }
            if (p == saved)
                p++;
            continue;
        }
        p++;                    /* { */
        for (;;) {
            char *name;
            char *value;
            gboolean imp = FALSE;
            Decl d;
            skip_ws_and_comments (&p, end);
            if (p >= end || *p == '}')
                break;
            name = parse_ident (&p, end);
            skip_ws_and_comments (&p, end);
            if (!name || p >= end || *p != ':') {
                g_free (name);
                while (p < end && *p != ';' && *p != '}')
                    p++;
                if (p < end && *p == ';')
                    p++;
                continue;
            }
            p++;
            value = parse_decl_value (&p, end, &imp);
            if (p < end && *p == ';')
                p++;
            if (name && value) {
                if (rule.n_decls >= RSVG_MAX_CSS_DECLS) {
                    g_free (name);
                    g_free (value);
                } else {
                    d.name = name;
                    d.value = value;
                    d.important = imp;
                    d.order = eng->next_order++;
                    if (rule.n_decls == rule.decls_alloc) {
                        rule.decls_alloc = rule.decls_alloc ? rule.decls_alloc * 2 : 4;
                        if (rule.decls_alloc > RSVG_MAX_CSS_DECLS)
                            rule.decls_alloc = RSVG_MAX_CSS_DECLS;
                        rule.decls = g_renew (Decl, rule.decls, rule.decls_alloc);
                    }
                    rule.decls[rule.n_decls++] = d;
                }
            } else {
                g_free (name);
                g_free (value);
            }
        }
        if (p < end && *p == '}')
            p++;
        if (rule.n_sels > 0 && rule.n_decls > 0)
            sheet_add_rule (sheet, rule);
        else
            rule_clear (&rule);
        /* Do not stall if nothing in this iteration consumed input. */
        if (p == saved)
            p++;
    }
}

/* ---------- engine ---------- */

RsvgCssEngine *
rsvg_css_engine_new (void)
{
    RsvgCssEngine *eng = g_new0 (RsvgCssEngine, 1);
    sheet_init (&eng->ua, RSVG_CSS_ORIGIN_UA);
    sheet_init (&eng->user, RSVG_CSS_ORIGIN_USER);
    sheet_init (&eng->author, RSVG_CSS_ORIGIN_AUTHOR);
    parse_stylesheet (eng, &eng->ua, rsvg_ua_css, strlen (rsvg_ua_css), NULL, FALSE, 0);
    return eng;
}

void
rsvg_css_engine_free (RsvgCssEngine *eng)
{
    if (!eng)
        return;
    sheet_clear_rules (&eng->ua);
    sheet_clear_rules (&eng->user);
    sheet_clear_rules (&eng->author);
    g_free (eng);
}

void
rsvg_css_engine_add_sheet (RsvgCssEngine *eng,
                           RsvgCssOrigin origin,
                           const char *css,
                           gsize len,
                           RsvgHandle *ctx,
                           gboolean allow_external_import)
{
    Sheet *s;
    if (!eng || !css || len == 0)
        return;
    if (len > RSVG_MAX_STYLESHEET_BYTES)
        return;
    if (origin == RSVG_CSS_ORIGIN_UA)
        s = &eng->ua;
    else if (origin == RSVG_CSS_ORIGIN_USER)
        s = &eng->user;
    else
        s = &eng->author;
    parse_stylesheet (eng, s, css, len, ctx, allow_external_import, 0);
}

void
rsvg_css_engine_clear_user (RsvgCssEngine *eng)
{
    if (eng)
        sheet_clear_rules (&eng->user);
}

typedef struct {
    RsvgCssOrigin origin;
    guint32 spec;
    guint32 order;
    gboolean important;
    const char *name;
    const char *value;
} Match;

static int
cmp_match (const void *a, const void *b)
{
    const Match *ma = a, *mb = b;
    if (ma->origin != mb->origin)
        return (int) ma->origin - (int) mb->origin;
    if (ma->spec != mb->spec)
        return ma->spec < mb->spec ? -1 : 1;
    if (ma->order != mb->order)
        return ma->order < mb->order ? -1 : 1;
    return 0;
}

static void
collect_sheet (const Sheet *s, const RsvgNode *n, GArray *acc)
{
    int ri, si, di;
    for (ri = 0; ri < s->n_rules; ri++) {
        const Rule *r = &s->rules[ri];
        for (si = 0; si < r->n_sels; si++) {
            if (!match_selector (&r->sels[si], n))
                continue;
            for (di = 0; di < r->n_decls; di++) {
                Match m;
                m.origin = s->origin;
                m.spec = r->sels[si].spec;
                m.order = r->decls[di].order;
                m.important = r->decls[di].important;
                m.name = r->decls[di].name;
                m.value = r->decls[di].value;
                g_array_append_val (acc, m);
            }
        }
    }
}

static void
style_one_node (RsvgCssEngine *eng, RsvgHandle *handle, RsvgNode *node)
{
    GArray *acc;
    guint i;

    if (!node || !node_is_element (node) || !node->state)
        return;

    rsvg_state_reinit (node->state);

    acc = g_array_new (FALSE, FALSE, sizeof (Match));
    collect_sheet (&eng->ua, node, acc);
    collect_sheet (&eng->user, node, acc);
    collect_sheet (&eng->author, node, acc);
    if (acc->len > 1)
        g_array_sort (acc, cmp_match);

    /* Match 2.62 css.rs cascade(): presentation attributes are the
     * specified base (parse-time). UA fills defaults, then User and
     * Author overwrite, then style="" last. That is how
     * rsvg_handle_set_stylesheet (User origin) beats fill="black". */
    for (i = 0; i < acc->len; i++) {
        Match *m = &g_array_index (acc, Match, i);
        if (m->origin != RSVG_CSS_ORIGIN_UA)
            continue;
        rsvg_parse_style_pair (handle, node->state, m->name, m->value, m->important);
    }

    if (node->atts) {
        const char *tr;
        rsvg_parse_style_pairs (handle, node->state, node->atts);
        tr = rsvg_property_bag_lookup (node->atts, "transform");
        if (tr) {
            cairo_matrix_t affine;
            if (rsvg_parse_transform (&affine, tr)) {
                cairo_matrix_multiply (&node->state->personal_affine, &affine,
                                       &node->state->personal_affine);
                cairo_matrix_multiply (&node->state->affine, &affine, &node->state->affine);
            }
        }
    }

    for (i = 0; i < acc->len; i++) {
        Match *m = &g_array_index (acc, Match, i);
        if (m->origin != RSVG_CSS_ORIGIN_USER)
            continue;
        rsvg_parse_style_pair (handle, node->state, m->name, m->value, m->important);
    }

    for (i = 0; i < acc->len; i++) {
        Match *m = &g_array_index (acc, Match, i);
        if (m->origin != RSVG_CSS_ORIGIN_AUTHOR)
            continue;
        rsvg_parse_style_pair (handle, node->state, m->name, m->value, m->important);
    }
    g_array_free (acc, TRUE);

    if (node->css_style)
        rsvg_parse_style (handle, node->state, node->css_style);
}

static void
cascade_walk (RsvgCssEngine *eng, RsvgHandle *handle, RsvgNode *node, guint depth)
{
    guint i;

    if (!node)
        return;
    if (depth >= RSVG_MAX_CSS_TREE_DEPTH)
        return;

    style_one_node (eng, handle, node);
    if (!node->children)
        return;
    for (i = 0; i < node->children->len; i++)
        cascade_walk (eng, handle, g_ptr_array_index (node->children, i), depth + 1);
}

void
rsvg_css_engine_cascade (RsvgCssEngine *eng, RsvgHandle *handle, RsvgNode *root)
{
    if (!eng || !handle || !root)
        return;
    cascade_walk (eng, handle, root, 0);

    if (root->state && root->state->has_current_color) {
        handle->priv->context_color = root->state->current_color;
        handle->priv->has_context_color = TRUE;
    }
}

void
rsvg_css_engine_cascade_handle (RsvgHandle *handle)
{
    if (!handle || !handle->priv || !handle->priv->css_engine)
        return;
    if (!handle->priv->treebase)
        return;
    rsvg_css_engine_cascade (handle->priv->css_engine, handle, handle->priv->treebase);
}
