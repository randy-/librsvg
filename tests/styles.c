/* vim: set ts=4 nowrap ai expandtab sw=4: */
/*
   tests/styles.c: Style and paint tests.

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

#include <glib.h>
#include <string.h>
#include <cairo.h>
#include "rsvg.h"
#include "rsvg-compat.h"
#include "rsvg-private.h"
#include "rsvg-defs.h"
#include "rsvg-styles.h"
#include "rsvg-paint-server.h"
#include "test-utils.h"

union Expected {
    guint color;
    RsvgCssLength length;
};

typedef struct _FixtureData
{
    const gchar *test_name;
    const gchar *bug_id;
    const gchar *file_path;
    const gchar *id;
    const gchar *target_name;
    union Expected expected;
} FixtureData;

static void
assert_equal_color (guint expected, guint actual)
{
    g_assert_cmphex (expected, ==, actual);
}

static void
assert_equal_length (RsvgCssLength *expected, RsvgCssLength *actual)
{
    g_assert_cmpfloat (expected->length, ==, actual->length);
    g_assert_cmpint (expected->factor, ==, actual->factor);
}

static void
assert_equal_value (FixtureData *fixture, RsvgNode *node)
{
    if (g_str_equal (fixture->target_name, "stroke"))
        assert_equal_color (fixture->expected.color, node->state->stroke->core.color->argb);
    else if (g_str_equal (fixture->target_name, "fill"))
        assert_equal_color (fixture->expected.color, node->state->fill->core.color->argb);
    else if (g_str_equal (fixture->target_name, "stroke-width"))
        assert_equal_length (&fixture->expected.length, &node->state->stroke_width);
    else if (g_str_equal (fixture->target_name, "font-weight"))
        g_assert_cmpint (fixture->expected.color, ==, node->state->font_weight);
    else if (g_str_equal (fixture->target_name, "font-size"))
        assert_equal_length (&fixture->expected.length, &node->state->font_size);
    else if (g_str_equal (fixture->target_name, "filter")) {
        if (fixture->bug_id && g_str_equal (fixture->bug_id, "null"))
            g_assert_null (node->state->filter);
        else
            g_assert_nonnull (node->state->filter);
    } else if (g_str_equal (fixture->target_name, "filter-blur")) {
        g_assert_nonnull (node->state->filter);
        g_assert_true (strstr (node->state->filter, "blur") != NULL);
    } else if (g_str_equal (fixture->target_name, "filter-drop")) {
        g_assert_nonnull (node->state->filter);
        g_assert_true (strstr (node->state->filter, "drop-shadow") != NULL);
    } else if (g_str_equal (fixture->target_name, "filter-bright")) {
        g_assert_nonnull (node->state->filter);
        g_assert_true (strstr (node->state->filter, "brightness") != NULL);
    } else if (g_str_equal (fixture->target_name, "filter-urls")) {
        g_assert_nonnull (node->state->filter);
        g_assert_true (strstr (node->state->filter, "url(") != NULL);
        g_assert_true (strstr (node->state->filter, "fblur") != NULL);
    } else if (g_str_equal (fixture->target_name, "white-space"))
        g_assert_cmpint (node->state->white_space, ==, (gint) fixture->expected.color);
    else if (g_str_equal (fixture->target_name, "line-height-normal"))
        g_assert_true (node->state->line_height_normal);
    else if (g_str_equal (fixture->target_name, "line-height"))
        assert_equal_length (&fixture->expected.length, &node->state->line_height);
    else if (g_str_equal (fixture->target_name, "text-orientation"))
        g_assert_cmpint (node->state->text_orientation, ==, (gint) fixture->expected.color);
    else if (g_str_equal (fixture->target_name, "xml-space"))
        g_assert_cmpint (node->state->space_preserve ? 1 : 0, ==, (gint) fixture->expected.color);
    else if (g_str_equal (fixture->target_name, "fr")) {
        RsvgRadialGradient *g = (RsvgRadialGradient *) node;
        g_assert_true (g->hasfr);
        assert_equal_length (&fixture->expected.length, &g->fr);
    } else
        g_assert_not_reached ();
}

static void
test_value (FixtureData *fixture)
{
    RsvgHandle *handle;
    RsvgNode *node;
    gchar *target_file;
    GError *error = NULL;

    if (fixture->bug_id)
        g_test_bug (fixture->bug_id);

    target_file = g_build_filename (test_utils_get_test_data_path (),
                                    fixture->file_path, NULL);
    handle = rsvg_handle_new_from_file (target_file, &error);
    g_free (target_file);

    /* Public RsvgHandle hides priv; the pointer still lives in padding[0]. */
    {
        struct RsvgHandlePrivate *priv = handle->_abi_padding[0];
        node = rsvg_defs_lookup (priv->defs, fixture->id);
    }
    g_assert (node);
    g_assert (node->state);

    assert_equal_value (fixture, node);

    g_object_unref (handle);
}

#define POINTS_PER_INCH (72.0)
#define POINTS_LENGTH(x) ((x) / POINTS_PER_INCH)

static const FixtureData fixtures[] =
{
    {"/styles/selectors/type", NULL, "styles/order.svg", "#black", "fill", .expected.color = 0xff000000},
    {"/styles/selectors/class", NULL, "styles/order.svg", "#blue", "fill", .expected.color = 0xff0000ff},
    {"/styles/selectors/#id", NULL, "styles/order.svg", "#brown", "fill", .expected.color = 0xffa52a2a},
    {"/styles/selectors/style", NULL, "styles/order.svg", "#gray", "fill", .expected.color = 0xff808080},
    {"/styles/selectors/style property prior than class", NULL, "styles/order.svg", "#red", "fill", .expected.color = 0xffff0000},
    {"/styles/selectors/#id prior than class", NULL, "styles/order.svg", "#green", "fill", .expected.color = 0xff008000},
    {"/styles/selectors/type#id prior than class", NULL, "styles/order.svg", "#pink", "fill", .expected.color = 0xffffc0cb},
    {"/styles/selectors/class#id prior than class", NULL, "styles/order.svg", "#yellow", "fill", .expected.color = 0xffffff00},
    {"/styles/selectors/type.class#id prior than class", NULL, "styles/order.svg", "#white", "fill", .expected.color = 0xffffffff},
    {"/styles/selectors/#id prior than type", "418823", "styles/bug418823.svg", "#bla", "fill", .expected.color = 0xff00ff00},
    {"/styles/selectors/comma-separate (fill)", "614643", "styles/bug614643.svg", "#red-rect", "fill", .expected.color = 0xffff0000},
    {"/styles/selectors/comma-separete (stroke)", "614643", "styles/bug614643.svg", "#red-path", "stroke", .expected.color = 0xffff0000},
    {"/styles/override presentation attribute", "614704", "styles/bug614704.svg", "#blue-rect", "fill", .expected.color = 0xff0000ff},
    {"/styles/selectors/2 or more selectors (fill)", "592207", "styles/bug592207.svg", "#target", "fill", .expected.color = 0xffff0000},
    {"/styles/selectors/2 or more selectors (stroke)", "592207", "styles/bug592207.svg", "#target", "stroke", .expected.color = 0xff0000ff},
    {"/styles/svg-element-style", "615701", "styles/svg-class.svg", "#svg", "fill", .expected.color = 0xff0000ff},
    {"/styles/presentation attribute in svg element", "620693", "styles/bug620693.svg", "#svg", "stroke", .expected.color = 0xffff0000},
    {"/styles/!important/stroke", "379629", "styles/bug379629.svg", "#base_shadow", "stroke", .expected.color = 0xffffc0cb /* pink */},
    {"/styles/!important/stroke-width", "379629", "styles/bug379629.svg", "#base_shadow", "stroke-width", .expected.length = {POINTS_LENGTH(5.), 'i'}},
    {"/styles/!important/class", "614606", "styles/bug614606.svg", "#path6306", "fill", .expected.color = 0xffff0000 /* red */ },
    {"/styles/!important/element", "614606", "styles/bug614606.svg", "#path6308", "fill", .expected.color = 0xff000000},
    {"/styles/!important/#id prior than class", NULL, "styles/important.svg", "#red", "fill", .expected.color = 0xffff0000 },
    {"/styles/!important/class prior than type", NULL, "styles/important.svg", "#blue", "fill", .expected.color = 0xff0000ff },
    {"/styles/!important/presentation attribute is invalid", NULL, "styles/important.svg", "#white", "fill", .expected.color = 0xffffffff },
    {"/styles/!important/style prior than class", NULL, "styles/important.svg", "#pink", "fill", .expected.color = 0xffffc0cb },
    {"/styles/color-level4/hex3", NULL, "styles/color-level4.svg", "#hex3", "fill", .expected.color = 0xff00ff00 },
    {"/styles/color-level4/hex4", NULL, "styles/color-level4.svg", "#hex4", "fill", .expected.color = 0x8800ff00 },
    {"/styles/color-level4/hex6", NULL, "styles/color-level4.svg", "#hex6", "fill", .expected.color = 0xff00ff00 },
    {"/styles/color-level4/hex8", NULL, "styles/color-level4.svg", "#hex8", "fill", .expected.color = 0x8000ff00 },
    {"/styles/color-level4/rgb-comma", NULL, "styles/color-level4.svg", "#rgb-comma", "fill", .expected.color = 0xff00ff00 },
    {"/styles/color-level4/rgb-space", NULL, "styles/color-level4.svg", "#rgb-space", "fill", .expected.color = 0xff00ff00 },
    {"/styles/color-level4/rgb-slash", NULL, "styles/color-level4.svg", "#rgb-slash", "fill", .expected.color = 0x8000ff00 },
    {"/styles/color-level4/hsl", NULL, "styles/color-level4.svg", "#hsl", "fill", .expected.color = 0xff00ff00 },
    {"/styles/color-level4/hsla", NULL, "styles/color-level4.svg", "#hsla", "fill", .expected.color = 0x8000ff00 },
    {"/styles/color-level4/hwb", NULL, "styles/color-level4.svg", "#hwb", "fill", .expected.color = 0xff00ff00 },
    {"/styles/color-level4/hwba", NULL, "styles/color-level4.svg", "#hwba", "fill", .expected.color = 0x8000ff00 },
    {"/styles/color-level4/transparent", NULL, "styles/color-level4.svg", "#transparent", "fill", .expected.color = 0x00000000 },
    {"/styles/font-shorthand/fill", NULL, "styles/color-level4.svg", "#font-sh", "fill", .expected.color = 0xff00ff00 },
    {"/styles/font-shorthand/weight", NULL, "styles/color-level4.svg", "#font-sh", "font-weight", .expected.color = 700 },
    {"/styles/font-shorthand/size", NULL, "styles/color-level4.svg", "#font-sh", "font-size", .expected.length = {50., '\0'} },
    {"/styles/ch-unit", NULL, "styles/color-level4.svg", "#ch", "stroke-width", .expected.length = {2., 'c'} },
    {"/styles/filter/blur", NULL, "styles/filter-funcs.svg", "#blur", "filter-blur", .expected.color = 0 },
    {"/styles/filter/drop-shadow", NULL, "styles/filter-funcs.svg", "#drop", "filter-drop", .expected.color = 0 },
    {"/styles/filter/brightness", NULL, "styles/filter-funcs.svg", "#bright", "filter-bright", .expected.color = 0 },
    {"/styles/filter/multi-url", NULL, "styles/filter-funcs.svg", "#urls", "filter-urls", .expected.color = 0 },
    {"/styles/filter/none", "null", "styles/filter-funcs.svg", "#none", "filter", .expected.color = 0 },
    {"/styles/text/white-space-normal", NULL, "styles/text-props.svg", "#ws-normal", "white-space", .expected.color = 0 },
    {"/styles/text/white-space-pre", NULL, "styles/text-props.svg", "#ws-pre", "white-space", .expected.color = 1 },
    {"/styles/text/white-space-nowrap", NULL, "styles/text-props.svg", "#ws-nowrap", "white-space", .expected.color = 2 },
    {"/styles/text/line-height-normal", NULL, "styles/text-props.svg", "#lh-normal", "line-height-normal", .expected.color = 0 },
    {"/styles/text/line-height-number", NULL, "styles/text-props.svg", "#lh-num", "line-height", .expected.length = {1.2, 'N'} },
    {"/styles/text/line-height-percent", NULL, "styles/text-props.svg", "#lh-pct", "line-height", .expected.length = {1.5, 'p'} },
    {"/styles/text/orientation-mixed", NULL, "styles/text-props.svg", "#to-mixed", "text-orientation", .expected.color = 0 },
    {"/styles/text/orientation-upright", NULL, "styles/text-props.svg", "#to-upright", "text-orientation", .expected.color = 1 },
    {"/styles/text/orientation-sideways", NULL, "styles/text-props.svg", "#to-sideways", "text-orientation", .expected.color = 2 },
    {"/styles/text/gov-auto", NULL, "styles/text-props.svg", "#gov-auto", "text-orientation", .expected.color = 0 },
    {"/styles/text/gov-0", NULL, "styles/text-props.svg", "#gov-0", "text-orientation", .expected.color = 1 },
    {"/styles/text/gov-90", NULL, "styles/text-props.svg", "#gov-90", "text-orientation", .expected.color = 2 },
    {"/styles/text/xml-space-preserve", NULL, "styles/text-props.svg", "#xml-preserve", "xml-space", .expected.color = 1 },
    {"/styles/radial-fr", NULL, "styles/radial-fr.svg", "#with-fr", "fr", .expected.length = {0.25, 'p'} },
    /* {"/styles/selectors/descendant", "338160", "styles/bug338160.svg", "#base_shadow", "stroke-width", .expected.length = {2., '\0'}}, */
};
static const gint n_fixtures = G_N_ELEMENTS (fixtures);

static cairo_surface_t *
render_svg_data (const char *svg, int width, int height)
{
    GError *error = NULL;
    RsvgHandle *handle;
    cairo_surface_t *surface;
    cairo_t *cr;
    RsvgRectangle viewport;

    handle = rsvg_handle_new_from_data ((const guint8 *) svg, strlen (svg), &error);
    g_assert_no_error (error);
    g_assert_nonnull (handle);
    rsvg_handle_internal_set_testing (handle, TRUE);

    surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
    g_assert_cmpint (cairo_surface_status (surface), ==, CAIRO_STATUS_SUCCESS);
    cr = cairo_create (surface);
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = width;
    viewport.height = height;
    g_assert_true (rsvg_handle_render_document (handle, cr, &viewport, &error));
    g_assert_no_error (error);
    cairo_destroy (cr);
    g_object_unref (handle);
    cairo_surface_flush (surface);
    return surface;
}

static void
assert_surfaces_equal (cairo_surface_t *a, cairo_surface_t *b)
{
    unsigned char *pa, *pb;
    int wa, ha, sa, wb, hb, sb, x, y;

    wa = cairo_image_surface_get_width (a);
    ha = cairo_image_surface_get_height (a);
    sa = cairo_image_surface_get_stride (a);
    wb = cairo_image_surface_get_width (b);
    hb = cairo_image_surface_get_height (b);
    sb = cairo_image_surface_get_stride (b);
    g_assert_cmpint (wa, ==, wb);
    g_assert_cmpint (ha, ==, hb);
    pa = cairo_image_surface_get_data (a);
    pb = cairo_image_surface_get_data (b);
    for (y = 0; y < ha; y++) {
        for (x = 0; x < wa; x++) {
            guint32 va = ((guint32 *) (pa + y * sa))[x];
            guint32 vb = ((guint32 *) (pb + y * sb))[x];
            g_assert_cmphex (va, ==, vb);
        }
    }
}

static void
compare_svg_pair (const char *left, const char *right, int w, int h)
{
    cairo_surface_t *a = render_svg_data (left, w, h);
    cairo_surface_t *b = render_svg_data (right, w, h);
    assert_surfaces_equal (a, b);
    cairo_surface_destroy (a);
    cairo_surface_destroy (b);
}

static void
test_filter_func_blur (void)
{
    compare_svg_pair (
        "<svg xmlns='http://www.w3.org/2000/svg' width='400' height='400'>"
        "<rect x='100' y='100' width='200' height='200' fill='lime' filter='blur(5)'/>"
        "</svg>",
        "<svg xmlns='http://www.w3.org/2000/svg' width='400' height='400'>"
        "<defs><filter id='filter'><feGaussianBlur stdDeviation='5 5'/></filter></defs>"
        "<rect x='100' y='100' width='200' height='200' fill='lime' filter='url(#filter)'/>"
        "</svg>",
        400, 400);
}

static void
test_filter_func_brightness (void)
{
    compare_svg_pair (
        "<svg xmlns='http://www.w3.org/2000/svg' width='400' height='400'>"
        "<rect x='100' y='100' width='200' height='200' fill='green' filter='brightness(125%)'/>"
        "</svg>",
        "<svg xmlns='http://www.w3.org/2000/svg' width='400' height='400'>"
        "<defs><filter id='filter'><feComponentTransfer>"
        "<feFuncR type='linear' slope='1.25'/>"
        "<feFuncG type='linear' slope='1.25'/>"
        "<feFuncB type='linear' slope='1.25'/>"
        "</feComponentTransfer></filter></defs>"
        "<rect x='100' y='100' width='200' height='200' fill='green' filter='url(#filter)'/>"
        "</svg>",
        400, 400);
}

static void
test_filter_func_contrast (void)
{
    compare_svg_pair (
        "<svg xmlns='http://www.w3.org/2000/svg' width='400' height='400'>"
        "<rect x='100' y='100' width='200' height='200' fill='green' filter='contrast(125%)'/>"
        "</svg>",
        "<svg xmlns='http://www.w3.org/2000/svg' width='400' height='400'>"
        "<defs><filter id='filter'><feComponentTransfer>"
        "<feFuncR type='linear' slope='1.25' intercept='-0.125'/>"
        "<feFuncG type='linear' slope='1.25' intercept='-0.125'/>"
        "<feFuncB type='linear' slope='1.25' intercept='-0.125'/>"
        "</feComponentTransfer></filter></defs>"
        "<rect x='100' y='100' width='200' height='200' fill='green' filter='url(#filter)'/>"
        "</svg>",
        400, 400);
}

static void
test_filter_func_dropshadow (void)
{
    compare_svg_pair (
        "<svg xmlns='http://www.w3.org/2000/svg' width='400' height='400'>"
        "<rect x='100' y='100' width='200' height='200' fill='green' filter='drop-shadow(#ff0000 1px 4px 6px)'/>"
        "</svg>",
        "<svg xmlns='http://www.w3.org/2000/svg' width='400' height='400'>"
        "<defs><filter id='filter'>"
        "<feGaussianBlur in='SourceAlpha' stdDeviation='6'/>"
        "<feOffset dx='1' dy='4' result='offsetblur'/>"
        "<feFlood flood-color='#ff0000'/>"
        "<feComposite in2='offsetblur' operator='in'/>"
        "<feMerge><feMergeNode/><feMergeNode in='SourceGraphic'/></feMerge>"
        "</filter></defs>"
        "<rect x='100' y='100' width='200' height='200' fill='green' filter='url(#filter)'/>"
        "</svg>",
        400, 400);
}

static void
test_filter_func_grayscale (void)
{
    compare_svg_pair (
        "<svg xmlns='http://www.w3.org/2000/svg' width='400' height='400'>"
        "<rect x='100' y='100' width='200' height='200' fill='lime' filter='grayscale(0.75)'/>"
        "</svg>",
        "<svg xmlns='http://www.w3.org/2000/svg' width='400' height='400'>"
        "<defs><filter id='filter'><feColorMatrix type='saturate' values='0.25'/></filter></defs>"
        "<rect x='100' y='100' width='200' height='200' fill='lime' filter='url(#filter)'/>"
        "</svg>",
        400, 400);
}

static void
test_filter_func_huerotate (void)
{
    compare_svg_pair (
        "<svg xmlns='http://www.w3.org/2000/svg' width='400' height='400'>"
        "<rect x='100' y='100' width='200' height='200' fill='green' filter='hue-rotate(128deg)'/>"
        "</svg>",
        "<svg xmlns='http://www.w3.org/2000/svg' width='400' height='400'>"
        "<defs><filter id='filter'><feColorMatrix type='hueRotate' values='128'/></filter></defs>"
        "<rect x='100' y='100' width='200' height='200' fill='green' filter='url(#filter)'/>"
        "</svg>",
        400, 400);
}

static void
test_filter_func_invert (void)
{
    compare_svg_pair (
        "<svg xmlns='http://www.w3.org/2000/svg' width='400' height='400'>"
        "<rect x='100' y='100' width='200' height='200' fill='lime' filter='invert(0.75)'/>"
        "</svg>",
        "<svg xmlns='http://www.w3.org/2000/svg' width='400' height='400'>"
        "<defs><filter id='filter'><feComponentTransfer>"
        "<feFuncR type='table' tableValues='0.75 0.25'/>"
        "<feFuncG type='table' tableValues='0.75 0.25'/>"
        "<feFuncB type='table' tableValues='0.75 0.25'/>"
        "</feComponentTransfer></filter></defs>"
        "<rect x='100' y='100' width='200' height='200' fill='lime' filter='url(#filter)'/>"
        "</svg>",
        400, 400);
}

static void
test_filter_func_opacity (void)
{
    compare_svg_pair (
        "<svg xmlns='http://www.w3.org/2000/svg' width='400' height='400'>"
        "<rect x='100' y='100' width='200' height='200' fill='red'/>"
        "<rect x='100' y='100' width='200' height='200' fill='lime' filter='opacity(0.75)'/>"
        "</svg>",
        "<svg xmlns='http://www.w3.org/2000/svg' width='400' height='400'>"
        "<defs><filter id='filter'><feComponentTransfer>"
        "<feFuncA type='table' tableValues='0 0.75'/>"
        "</feComponentTransfer></filter></defs>"
        "<rect x='100' y='100' width='200' height='200' fill='red'/>"
        "<rect x='100' y='100' width='200' height='200' fill='lime' filter='url(#filter)'/>"
        "</svg>",
        400, 400);
}

static void
test_filter_func_saturate (void)
{
    compare_svg_pair (
        "<svg xmlns='http://www.w3.org/2000/svg' width='400' height='400'>"
        "<rect x='100' y='100' width='200' height='200' fill='lime' filter='saturate(0.75)'/>"
        "</svg>",
        "<svg xmlns='http://www.w3.org/2000/svg' width='400' height='400'>"
        "<defs><filter id='filter'><feColorMatrix type='saturate' values='0.75'/></filter></defs>"
        "<rect x='100' y='100' width='200' height='200' fill='lime' filter='url(#filter)'/>"
        "</svg>",
        400, 400);
}

static void
test_filter_func_sepia (void)
{
    compare_svg_pair (
        "<svg xmlns='http://www.w3.org/2000/svg' width='400' height='400'>"
        "<rect x='100' y='100' width='200' height='200' fill='lime' filter='sepia(0.75)'/>"
        "</svg>",
        "<svg xmlns='http://www.w3.org/2000/svg' width='400' height='400'>"
        "<defs><filter id='filter'><feColorMatrix type='matrix' values='"
        "0.54475 0.57675 0.14175 0 0 "
        "0.26175 0.7645 0.126 0 0 "
        "0.204 0.4005 0.34825 0 0 "
        "0 0 0 1 0'/></filter></defs>"
        "<rect x='100' y='100' width='200' height='200' fill='lime' filter='url(#filter)'/>"
        "</svg>",
        400, 400);
}

static void
test_fe_drop_shadow_element (void)
{
    compare_svg_pair (
        "<svg xmlns='http://www.w3.org/2000/svg' width='200' height='200'>"
        "<filter id='drop-shadow' filterUnits='userSpaceOnUse'>"
        "<feDropShadow dx='5' dy='10' stdDeviation='5 10' flood-color='black' flood-opacity='0.5'/>"
        "</filter>"
        "<rect x='0' y='0' width='100%' height='100%' fill='white'/>"
        "<rect x='50' y='50' width='50' height='50' fill='blue' stroke='magenta' stroke-width='6' filter='url(#drop-shadow)'/>"
        "</svg>",
        "<svg xmlns='http://www.w3.org/2000/svg' width='200' height='200'>"
        "<filter id='drop-shadow' filterUnits='userSpaceOnUse'>"
        "<feGaussianBlur in='SourceAlpha' stdDeviation='5 10'/>"
        "<feOffset dx='5' dy='10' result='offsetblur'/>"
        "<feFlood flood-color='black' flood-opacity='0.5'/>"
        "<feComposite in2='offsetblur' operator='in'/>"
        "<feMerge><feMergeNode/><feMergeNode in='SourceGraphic'/></feMerge>"
        "</filter>"
        "<rect x='0' y='0' width='100%' height='100%' fill='white'/>"
        "<rect x='50' y='50' width='50' height='50' fill='blue' stroke='magenta' stroke-width='6' filter='url(#drop-shadow)'/>"
        "</svg>",
        200, 200);
}

static void
test_multi_url_filter_chain (void)
{
    compare_svg_pair (
        "<svg xmlns='http://www.w3.org/2000/svg' width='400' height='400'>"
        "<defs>"
        "<filter id='f1'><feGaussianBlur stdDeviation='3'/></filter>"
        "<filter id='f2'><feColorMatrix type='saturate' values='0.25'/></filter>"
        "</defs>"
        "<rect x='100' y='100' width='200' height='200' fill='lime' filter='url(#f1) url(#f2)'/>"
        "</svg>",
        "<svg xmlns='http://www.w3.org/2000/svg' width='400' height='400'>"
        "<defs><filter id='f'>"
        "<feGaussianBlur stdDeviation='3'/>"
        "<feColorMatrix type='saturate' values='0.25'/>"
        "</filter></defs>"
        "<rect x='100' y='100' width='200' height='200' fill='lime' filter='url(#f)'/>"
        "</svg>",
        400, 400);
}

static void
test_invalid_filter_reference_cancels_chain (void)
{
    compare_svg_pair (
        "<svg xmlns='http://www.w3.org/2000/svg' width='400' height='400'>"
        "<defs><filter id='filter'><feColorMatrix type='hueRotate' values='240'/></filter></defs>"
        "<rect x='100' y='100' width='200' height='200' fill='lime' filter='url(#filter) url(#nonexistent)'/>"
        "</svg>",
        "<svg xmlns='http://www.w3.org/2000/svg' width='400' height='400'>"
        "<rect x='100' y='100' width='200' height='200' fill='lime'/>"
        "</svg>",
        400, 400);
}

static void
test_xml_space_preserve_newlines (void)
{
    compare_svg_pair (
        "<svg xmlns='http://www.w3.org/2000/svg' width='200' height='40'>"
        "<text x='10' y='25' font-family='sans' font-size='16' xml:space='preserve'>\nA\tB\n</text>"
        "</svg>",
        "<svg xmlns='http://www.w3.org/2000/svg' width='200' height='40'>"
        "<text x='10' y='25' font-family='sans' font-size='16' xml:space='preserve'> A B </text>"
        "</svg>",
        200, 40);
}

static void
test_xml_space_default_collapse (void)
{
    compare_svg_pair (
        "<svg xmlns='http://www.w3.org/2000/svg' width='200' height='40'>"
        "<text x='10' y='25' font-family='sans' font-size='16'>\n  A   B\n</text>"
        "</svg>",
        "<svg xmlns='http://www.w3.org/2000/svg' width='200' height='40'>"
        "<text x='10' y='25' font-family='sans' font-size='16'>A B</text>"
        "</svg>",
        200, 40);
}

static void
test_a_inside_text (void)
{
    compare_svg_pair (
        "<svg xmlns='http://www.w3.org/2000/svg' xmlns:xlink='http://www.w3.org/1999/xlink' width='200' height='40'>"
        "<text x='10' y='25' font-family='sans' font-size='16'>"
        "<a xlink:href='#x' fill='red'>Hello</a></text>"
        "</svg>",
        "<svg xmlns='http://www.w3.org/2000/svg' width='200' height='40'>"
        "<text x='10' y='25' font-family='sans' font-size='16'>"
        "<tspan fill='red'>Hello</tspan></text>"
        "</svg>",
        200, 40);
}

static void
test_tspan_display_none (void)
{
    compare_svg_pair (
        "<svg xmlns='http://www.w3.org/2000/svg' width='200' height='40'>"
        "<text x='10' y='25' font-family='sans' font-size='16'>"
        "foo<tspan display='none'>BAR</tspan>baz</text>"
        "</svg>",
        "<svg xmlns='http://www.w3.org/2000/svg' width='200' height='40'>"
        "<text x='10' y='25' font-family='sans' font-size='16'>foobaz</text>"
        "</svg>",
        200, 40);
}

static void
test_radial_fr_vs_zero (void)
{
    /* fr=50 vs implicit 0 must not match (inner circle of color). */
    cairo_surface_t *a, *b;
    a = render_svg_data (
        "<svg xmlns='http://www.w3.org/2000/svg' width='100' height='100'>"
        "<radialGradient id='g' cx='50' cy='50' r='40' fx='50' fy='50' fr='20'"
        " gradientUnits='userSpaceOnUse'>"
        "<stop offset='0' stop-color='#ff0000'/><stop offset='1' stop-color='#0000ff'/>"
        "</radialGradient>"
        "<rect width='100' height='100' fill='url(#g)'/>"
        "</svg>", 100, 100);
    b = render_svg_data (
        "<svg xmlns='http://www.w3.org/2000/svg' width='100' height='100'>"
        "<radialGradient id='g' cx='50' cy='50' r='40' fx='50' fy='50'"
        " gradientUnits='userSpaceOnUse'>"
        "<stop offset='0' stop-color='#ff0000'/><stop offset='1' stop-color='#0000ff'/>"
        "</radialGradient>"
        "<rect width='100' height='100' fill='url(#g)'/>"
        "</svg>", 100, 100);
    {
        unsigned char *pa = cairo_image_surface_get_data (a);
        unsigned char *pb = cairo_image_surface_get_data (b);
        int sa = cairo_image_surface_get_stride (a);
        int sb = cairo_image_surface_get_stride (b);
        guint32 va = ((guint32 *) (pa + 50 * sa))[50];
        guint32 vb = ((guint32 *) (pb + 50 * sb))[50];
        g_assert_cmphex (va, !=, vb);
    }
    cairo_surface_destroy (a);
    cairo_surface_destroy (b);
}

static void
test_first_xy_only (void)
{
    /* 2.62 uses only the first x/y/dx/dy value. */
    compare_svg_pair (
        "<svg xmlns='http://www.w3.org/2000/svg' width='200' height='80'>"
        "<text y='50' font-family='sans' font-size='24'>"
        "<tspan x='20 80 140'>Foo</tspan></text>"
        "</svg>",
        "<svg xmlns='http://www.w3.org/2000/svg' width='200' height='80'>"
        "<text y='50' font-family='sans' font-size='24'>"
        "<tspan x='20'>Foo</tspan></text>"
        "</svg>",
        200, 80);
}

int
main (int argc, char *argv[])
{
    gint i;
    int result;

    RSVG_G_TYPE_INIT;
    g_test_init (&argc, &argv, NULL);
    g_test_bug_base ("https://bugzilla.gnome.org/show_bug.cgi?id=");

    for (i = 0; i < n_fixtures; i++)
        g_test_add_data_func (fixtures[i].test_name, &fixtures[i], (void*)test_value);

    g_test_add_func ("/filters/func/blur", test_filter_func_blur);
    g_test_add_func ("/filters/func/brightness", test_filter_func_brightness);
    g_test_add_func ("/filters/func/contrast", test_filter_func_contrast);
    g_test_add_func ("/filters/func/drop-shadow", test_filter_func_dropshadow);
    g_test_add_func ("/filters/func/grayscale", test_filter_func_grayscale);
    g_test_add_func ("/filters/func/hue-rotate", test_filter_func_huerotate);
    g_test_add_func ("/filters/func/invert", test_filter_func_invert);
    g_test_add_func ("/filters/func/opacity", test_filter_func_opacity);
    g_test_add_func ("/filters/func/saturate", test_filter_func_saturate);
    g_test_add_func ("/filters/func/sepia", test_filter_func_sepia);
    g_test_add_func ("/filters/feDropShadow", test_fe_drop_shadow_element);
    g_test_add_func ("/filters/invalid-url-cancels-chain", test_invalid_filter_reference_cancels_chain);
    g_test_add_func ("/filters/multi-url-chain", test_multi_url_filter_chain);
    g_test_add_func ("/text/xml-space-preserve", test_xml_space_preserve_newlines);
    g_test_add_func ("/text/xml-space-default", test_xml_space_default_collapse);
    g_test_add_func ("/text/a-inside-text", test_a_inside_text);
    g_test_add_func ("/text/tspan-display-none", test_tspan_display_none);
    g_test_add_func ("/text/first-xy-only", test_first_xy_only);
    g_test_add_func ("/paint/radial-fr-differs-from-zero", test_radial_fr_vs_zero);

    result = g_test_run ();

    rsvg_cleanup ();

    return result;
}
