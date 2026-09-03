/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 sts=4 ts=4 expandtab: */
/*
   rsvg-cairo-render.c: The cairo backend plugin

   Copyright (C) 2005 Dom Lachowicz <cinamod@hotmail.com>
   Caleb Moore <c.moore@student.unsw.edu.au>
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
   Caleb Moore <c.moore@student.unsw.edu.au>
*/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <math.h>
#include <string.h>

#include "rsvg.h"
#include "rsvg-private.h"
#include "rsvg-cairo.h"
#include "rsvg-cairo-draw.h"
#include "rsvg-cairo-render.h"
#include "rsvg-styles.h"
#include "rsvg-structure.h"

static void
rsvg_cairo_render_free (RsvgRender * self)
{
    RsvgCairoRender *me = RSVG_CAIRO_RENDER (self);

    /* TODO */

#ifdef HAVE_PANGOFT2
    if (me->font_config_for_testing) {
        FcConfigDestroy (me->font_config_for_testing);
        me->font_config_for_testing = NULL;
    }

    if (me->font_map_for_testing) {
        g_object_unref (me->font_map_for_testing);
        me->font_map_for_testing = NULL;
    }
#endif

    g_free (me);
}

RsvgCairoRender *
rsvg_cairo_render_new (cairo_t * cr, double width, double height)
{
    RsvgCairoRender *cairo_render = g_new0 (RsvgCairoRender, 1);
    cairo_matrix_t matrix;

    cairo_render->super.type = RSVG_RENDER_TYPE_CAIRO;
    cairo_render->super.free = rsvg_cairo_render_free;
    cairo_render->super.create_pango_context = rsvg_cairo_create_pango_context;
    cairo_render->super.render_pango_layout = rsvg_cairo_render_pango_layout;
    cairo_render->super.render_surface = rsvg_cairo_render_surface;
    cairo_render->super.render_path = rsvg_cairo_render_path;
    cairo_render->super.pop_discrete_layer = rsvg_cairo_pop_discrete_layer;
    cairo_render->super.push_discrete_layer = rsvg_cairo_push_discrete_layer;
    cairo_render->super.add_clipping_rect = rsvg_cairo_add_clipping_rect;
    cairo_render->super.get_surface_of_node = rsvg_cairo_get_surface_of_node;
    cairo_render->width = width;
    cairo_render->height = height;
    cairo_render->offset_x = 0;
    cairo_render->offset_y = 0;
    cairo_render->initial_cr = cr;
    cairo_render->cr = cr;
    cairo_render->cr_stack = NULL;
    cairo_render->bb_stack = NULL;
    cairo_render->surfaces_stack = NULL;

#ifdef HAVE_PANGOFT2
    cairo_render->font_config_for_testing = NULL;
    cairo_render->font_map_for_testing = NULL;
#endif

    cairo_matrix_init_identity (&matrix);
    rsvg_bbox_init (&cairo_render->bbox, &matrix);
    rsvg_bbox_init (&cairo_render->logical_bbox, &matrix);
    cairo_render->logical_bb_stack = NULL;

    return cairo_render;
}

static void rsvg_cairo_transformed_image_bounding_box (
    cairo_matrix_t * transform,
    double width, double height,
    double *x0, double *y0, double *x1, double *y1)
{
    double x00 = 0, x01 = 0, x10 = width, x11 = width;
    double y00 = 0, y01 = height, y10 = 0, y11 = height;
    double t;

    /* transform the four corners of the image */
    cairo_matrix_transform_point (transform, &x00, &y00);
    cairo_matrix_transform_point (transform, &x01, &y01);
    cairo_matrix_transform_point (transform, &x10, &y10);
    cairo_matrix_transform_point (transform, &x11, &y11);

    /* find minimum and maximum coordinates */
    t = x00  < x01 ? x00  : x01;
    t = t < x10 ? t : x10;
    *x0 = floor (t < x11 ? t : x11);

    t = y00  < y01 ? y00  : y01;
    t = t < y10 ? t : y10;
    *y0 = floor (t < y11 ? t : y11);

    t = x00  > x01 ? x00  : x01;
    t = t > x10 ? t : x10;
    *x1 = ceil (t > x11 ? t : x11);

    t = y00  > y01 ? y00  : y01;
    t = t > y10 ? t : y10;
    *y1 = ceil (t > y11 ? t : y11);
}

RsvgDrawingCtx *
rsvg_cairo_new_drawing_ctx (cairo_t * cr, RsvgHandle * handle)
{
    RsvgDimensionData data;
    RsvgDrawingCtx *draw;
    RsvgCairoRender *render;
    RsvgState *state;
    cairo_matrix_t affine;
    double bbx0, bby0, bbx1, bby1;

    rsvg_handle_get_document_size (handle, &data);
    if (data.width == 0 || data.height == 0)
        return NULL;

    draw = g_new (RsvgDrawingCtx, 1);

    cairo_get_matrix (cr, &affine);

    /* find bounding box of image as transformed by the current cairo context
     * The size of this bounding box determines the size of the intermediate
     * surfaces allocated during drawing. */
    rsvg_cairo_transformed_image_bounding_box (&affine,
                                               data.width, data.height,
                                               &bbx0, &bby0, &bbx1, &bby1);

    render = rsvg_cairo_render_new (cr, bbx1 - bbx0, bby1 - bby0);

    if (!render)
        return NULL;

    draw->render = (RsvgRender *) render;
    render->offset_x = bbx0;
    render->offset_y = bby0;

    draw->state = NULL;

    draw->defs = handle->priv->defs;
    draw->dpi_x = handle->priv->dpi_x;
    draw->dpi_y = handle->priv->dpi_y;
    draw->vb.rect.width = data.em;
    draw->vb.rect.height = data.ex;
    draw->num_elements_acquired = 0;
    draw->pango_context = NULL;
    draw->drawsub_stack = NULL;
    draw->acquired_nodes = NULL;
    draw->is_testing = handle->priv->is_testing;
    draw->context_fill = NULL;
    draw->context_stroke = NULL;
    draw->context_color = handle->priv->context_color;
    draw->has_context_color = handle->priv->has_context_color;
    draw->cancellable = handle->priv->render_cancellable;
    draw->cancelled = FALSE;
    draw->suppress_markers = FALSE;
    draw->text_obb_valid = FALSE;
    draw->text_measure_only = FALSE;
    memset (&draw->text_obb, 0, sizeof (draw->text_obb));

    rsvg_state_push (draw);
    state = rsvg_current_state (draw);

    /* apply cairo transformation to our affine transform */
    cairo_matrix_multiply (&state->affine, &affine, &state->affine);

    /* scale according to size set by size_func callback */
    cairo_matrix_init_scale (&affine, data.width / data.em, data.height / data.ex);
    cairo_matrix_multiply (&state->affine, &affine, &state->affine);

    /* adjust transform so that the corner of the bounding box above is
     * at (0,0) - we compensate for this in _set_rsvg_affine() in
     * rsvg-cairo-render.c and a few other places */
    state->affine.x0 -= render->offset_x;
    state->affine.y0 -= render->offset_y;

    rsvg_bbox_init (&((RsvgCairoRender *) draw->render)->bbox, &state->affine);
    rsvg_bbox_init (&((RsvgCairoRender *) draw->render)->logical_bbox, &state->affine);

    return draw;
}

/**
 * rsvg_handle_render_cairo_sub:
 * @handle: A #RsvgHandle
 * @cr: A Cairo renderer
 * @id: (nullable): An element's id within the SVG, or %NULL to render
 *   the whole SVG. For example, if you have a layer called "layer1"
 *   that you wish to render, pass "##layer1" as the id.
 *
 * Draws a subset of a SVG to a Cairo surface
 *
 * Returns: %TRUE if drawing succeeded.
 *
 * Since: 2.14
 */
gboolean
rsvg_handle_render_cairo_sub (RsvgHandle * handle, cairo_t * cr, const char *id)
{
    RsvgDrawingCtx *draw;
    RsvgNode *drawsub = NULL;
    gboolean retval = FALSE;

    g_return_val_if_fail (handle != NULL, FALSE);

    if (handle->priv->state != RSVG_HANDLE_STATE_CLOSED_OK)
        return FALSE;

    if (id && *id)
        drawsub = rsvg_defs_lookup (handle->priv->defs, id);

    if (drawsub == NULL && id != NULL) {
        /* todo: there's no way to signal that @id doesn't exist */
        return FALSE;
    }

    draw = rsvg_cairo_new_drawing_ctx (cr, handle);
    if (!draw)
        return FALSE;

    while (drawsub != NULL) {
        draw->drawsub_stack = g_slist_prepend (draw->drawsub_stack, drawsub);
        drawsub = drawsub->parent;
    }

    rsvg_state_push (draw);
    cairo_save (cr);

    rsvg_node_draw ((RsvgNode *) handle->priv->treebase, draw, 0);

    if (rsvg_drawing_ctx_check_cancelled (draw) ||
        rsvg_drawing_ctx_limits_exceeded (draw)) {
        retval = FALSE;
    } else {
        retval = TRUE;
    }

    cairo_restore (cr);
    rsvg_state_pop (draw);
    rsvg_drawing_ctx_free (draw);

    return retval;
}

/**
 * rsvg_handle_render_cairo:
 * @handle: A #RsvgHandle
 * @cr: A Cairo renderer
 *
 * Draws a SVG to a Cairo surface
 *
 * Returns: %TRUE if drawing succeeded.
 * Since: 2.14
 */
gboolean
rsvg_handle_render_cairo (RsvgHandle * handle, cairo_t * cr)
{
    return rsvg_handle_render_cairo_sub (handle, cr, NULL);
}

static gboolean
rsvg_handle_check_ready (RsvgHandle *handle, GError **error)
{
    if (handle == NULL) {
        g_set_error (error, RSVG_ERROR, RSVG_ERROR_FAILED, "NULL handle");
        return FALSE;
    }

    if (handle->priv == NULL || handle->priv->state != RSVG_HANDLE_STATE_CLOSED_OK) {
        g_set_error (error, RSVG_ERROR, RSVG_ERROR_FAILED,
                     "handle must be fully loaded before rendering");
        return FALSE;
    }

    if (handle->priv->treebase == NULL) {
        g_set_error (error, RSVG_ERROR, RSVG_ERROR_FAILED, "SVG document is empty");
        return FALSE;
    }

    return TRUE;
}

static RsvgNode *
rsvg_handle_lookup_node (RsvgHandle *handle, const char *id, GError **error)
{
    RsvgNode *node;

    if (id == NULL || id[0] == '\0')
        return handle->priv->treebase;

    node = rsvg_defs_lookup (handle->priv->defs, id);
    if (node == NULL) {
        g_set_error (error, RSVG_ERROR, RSVG_ERROR_FAILED,
                     "element %s not found", id);
        return NULL;
    }

    return node;
}

static void
rsvg_rectangle_from_bbox (RsvgRectangle *rect, const RsvgBbox *bbox)
{
    if (bbox->virgin) {
        rect->x = rect->y = rect->width = rect->height = 0;
        return;
    }

    rect->x = bbox->rect.x;
    rect->y = bbox->rect.y;
    rect->width = bbox->rect.width;
    rect->height = bbox->rect.height;
}

static void
rsvg_rectangle_apply_viewport (RsvgRectangle *rect,
                               const RsvgRectangle *viewport,
                               double doc_width,
                               double doc_height)
{
    double sx, sy;

    if (doc_width <= 0 || doc_height <= 0)
        return;

    sx = viewport->width / doc_width;
    sy = viewport->height / doc_height;
    rect->x = viewport->x + rect->x * sx;
    rect->y = viewport->y + rect->y * sy;
    rect->width *= sx;
    rect->height *= sy;
}

/* Draw @node with inherited paint/style from ancestors, but identity
 * parent transforms (the element's own transform is kept). */
static void
rsvg_draw_node_isolated (RsvgDrawingCtx *draw, RsvgNode *node)
{
    GSList *chain = NULL;
    GSList *l;
    RsvgNode *walk;
    cairo_matrix_t initial;
    int pushes = 0;

    initial = rsvg_current_state (draw)->affine;

    for (walk = node; walk != NULL; walk = walk->parent)
        chain = g_slist_prepend (chain, walk);

    for (l = chain; l != NULL; l = l->next) {
        RsvgNode *cur = l->data;
        RsvgState *st;

        rsvg_state_push (draw);
        pushes++;
        rsvg_state_reinherit_top (draw, cur->state, 0);
        st = rsvg_current_state (draw);

        if (cur == node)
            cairo_matrix_multiply (&st->affine, &cur->state->personal_affine, &initial);
        else
            st->affine = initial;
    }

    g_slist_free (chain);

    node->draw (node, draw, 3);

    while (pushes-- > 0)
        rsvg_state_pop (draw);
}

static gboolean
rsvg_handle_measure (RsvgHandle *handle,
                     RsvgNode *node,
                     gboolean from_root,
                     RsvgRectangle *ink,
                     RsvgRectangle *logical,
                     GError **error)
{
    cairo_surface_t *target;
    cairo_t *cr;
    RsvgDrawingCtx *draw;
    RsvgCairoRender *render;
    gboolean ok = FALSE;

    target = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 1, 1);
    cr = cairo_create (target);
    draw = rsvg_cairo_new_drawing_ctx (cr, handle);
    if (!draw) {
        g_set_error (error, RSVG_ERROR, RSVG_ERROR_FAILED, "could not create drawing context");
        cairo_destroy (cr);
        cairo_surface_destroy (target);
        return FALSE;
    }

    if (from_root) {
        RsvgNode *walk = node;

        while (walk != NULL) {
            draw->drawsub_stack = g_slist_prepend (draw->drawsub_stack, walk);
            walk = walk->parent;
        }

        rsvg_state_push (draw);
        rsvg_node_draw (handle->priv->treebase, draw, 0);
        rsvg_state_pop (draw);
    } else {
        rsvg_draw_node_isolated (draw, node);
    }

    if (rsvg_drawing_ctx_limits_exceeded (draw)) {
        g_set_error (error, RSVG_ERROR, RSVG_ERROR_FAILED, "drawing limits exceeded");
        goto out;
    }

    render = RSVG_CAIRO_RENDER (draw->render);
    if (ink)
        rsvg_rectangle_from_bbox (ink, &render->bbox);
    if (logical) {
        if (render->logical_bbox.virgin && ink)
            *logical = *ink;
        else
            rsvg_rectangle_from_bbox (logical, &render->logical_bbox);
    }

    ok = TRUE;

out:
    rsvg_drawing_ctx_free (draw);
    cairo_destroy (cr);
    cairo_surface_destroy (target);
    return ok;
}

static gboolean
rsvg_handle_draw_node (RsvgHandle *handle,
                       cairo_t *cr,
                       RsvgNode *node,
                       gboolean from_root,
                       GError **error)
{
    RsvgDrawingCtx *draw;
    gboolean ok = FALSE;

    draw = rsvg_cairo_new_drawing_ctx (cr, handle);
    if (!draw) {
        g_set_error (error, RSVG_ERROR, RSVG_ERROR_FAILED, "could not create drawing context");
        return FALSE;
    }

    if (from_root) {
        RsvgNode *walk = node;

        while (walk != NULL) {
            draw->drawsub_stack = g_slist_prepend (draw->drawsub_stack, walk);
            walk = walk->parent;
        }

        rsvg_state_push (draw);
        cairo_save (cr);
        rsvg_node_draw (handle->priv->treebase, draw, 0);
        cairo_restore (cr);
        rsvg_state_pop (draw);
    } else {
        cairo_save (cr);
        rsvg_draw_node_isolated (draw, node);
        cairo_restore (cr);
    }

    if (rsvg_drawing_ctx_check_cancelled (draw)) {
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED, "rendering cancelled");
    } else if (rsvg_drawing_ctx_limits_exceeded (draw)) {
        g_set_error (error, RSVG_ERROR, RSVG_ERROR_FAILED, "drawing limits exceeded");
    } else if (cairo_status (cr) != CAIRO_STATUS_SUCCESS) {
        g_set_error (error, RSVG_ERROR, RSVG_ERROR_FAILED,
                     "cairo error: %s", cairo_status_to_string (cairo_status (cr)));
    } else {
        ok = TRUE;
    }

    rsvg_drawing_ctx_free (draw);
    return ok;
}

gboolean
rsvg_handle_render_document (RsvgHandle          *handle,
                             cairo_t             *cr,
                             const RsvgRectangle *viewport,
                             GError             **error)
{
    RsvgDimensionData dim;
    double sx, sy;

    if (!rsvg_handle_check_ready (handle, error))
        return FALSE;
    if (cr == NULL || viewport == NULL) {
        g_set_error (error, RSVG_ERROR, RSVG_ERROR_FAILED, "NULL argument");
        return FALSE;
    }
    if (cairo_status (cr) != CAIRO_STATUS_SUCCESS) {
        g_set_error (error, RSVG_ERROR, RSVG_ERROR_FAILED,
                     "cairo context is in an error state");
        return FALSE;
    }

    rsvg_handle_get_document_size (handle, &dim);
    if (dim.width <= 0 || dim.height <= 0) {
        g_set_error (error, RSVG_ERROR, RSVG_ERROR_FAILED, "document has no size");
        return FALSE;
    }

    sx = viewport->width / (double) dim.width;
    sy = viewport->height / (double) dim.height;

    cairo_save (cr);
    cairo_translate (cr, viewport->x, viewport->y);
    cairo_scale (cr, sx, sy);
    if (!rsvg_handle_draw_node (handle, cr, handle->priv->treebase, TRUE, error)) {
        cairo_restore (cr);
        return FALSE;
    }
    cairo_restore (cr);
    return TRUE;
}

gboolean
rsvg_handle_render_layer (RsvgHandle          *handle,
                          cairo_t             *cr,
                          const char          *id,
                          const RsvgRectangle *viewport,
                          GError             **error)
{
    RsvgNode *node;
    RsvgDimensionData dim;
    double sx, sy;

    if (!rsvg_handle_check_ready (handle, error))
        return FALSE;
    if (cr == NULL || viewport == NULL) {
        g_set_error (error, RSVG_ERROR, RSVG_ERROR_FAILED, "NULL argument");
        return FALSE;
    }
    if (cairo_status (cr) != CAIRO_STATUS_SUCCESS) {
        g_set_error (error, RSVG_ERROR, RSVG_ERROR_FAILED,
                     "cairo context is in an error state");
        return FALSE;
    }

    node = rsvg_handle_lookup_node (handle, id, error);
    if (node == NULL)
        return FALSE;

    rsvg_handle_get_document_size (handle, &dim);
    if (dim.width <= 0 || dim.height <= 0) {
        g_set_error (error, RSVG_ERROR, RSVG_ERROR_FAILED, "document has no size");
        return FALSE;
    }

    sx = viewport->width / (double) dim.width;
    sy = viewport->height / (double) dim.height;

    cairo_save (cr);
    cairo_translate (cr, viewport->x, viewport->y);
    cairo_scale (cr, sx, sy);
    if (!rsvg_handle_draw_node (handle, cr, node, TRUE, error)) {
        cairo_restore (cr);
        return FALSE;
    }
    cairo_restore (cr);
    return TRUE;
}

gboolean
rsvg_handle_render_element (RsvgHandle          *handle,
                            cairo_t             *cr,
                            const char          *id,
                            const RsvgRectangle *element_viewport,
                            GError             **error)
{
    RsvgNode *node;
    RsvgRectangle ink;
    double scale;

    if (!rsvg_handle_check_ready (handle, error))
        return FALSE;
    if (cr == NULL || element_viewport == NULL) {
        g_set_error (error, RSVG_ERROR, RSVG_ERROR_FAILED, "NULL argument");
        return FALSE;
    }
    if (cairo_status (cr) != CAIRO_STATUS_SUCCESS) {
        g_set_error (error, RSVG_ERROR, RSVG_ERROR_FAILED,
                     "cairo context is in an error state");
        return FALSE;
    }

    node = rsvg_handle_lookup_node (handle, id, error);
    if (node == NULL)
        return FALSE;

    if (!rsvg_handle_measure (handle, node, FALSE, &ink, NULL, error))
        return FALSE;

    if (ink.width <= 0 || ink.height <= 0) {
        g_set_error (error, RSVG_ERROR, RSVG_ERROR_FAILED, "element has no size");
        return FALSE;
    }

    scale = MIN (element_viewport->width / ink.width,
                 element_viewport->height / ink.height);

    cairo_save (cr);
    cairo_translate (cr, element_viewport->x, element_viewport->y);
    cairo_scale (cr, scale, scale);
    cairo_translate (cr, -ink.x, -ink.y);
    if (!rsvg_handle_draw_node (handle, cr, node, FALSE, error)) {
        cairo_restore (cr);
        return FALSE;
    }
    cairo_restore (cr);
    return TRUE;
}

gboolean
rsvg_handle_get_geometry_for_layer (RsvgHandle          *handle,
                                    const char          *id,
                                    const RsvgRectangle *viewport,
                                    RsvgRectangle       *out_ink_rect,
                                    RsvgRectangle       *out_logical_rect,
                                    GError             **error)
{
    RsvgNode *node;
    RsvgDimensionData dim;
    RsvgRectangle ink, logical;

    if (!rsvg_handle_check_ready (handle, error))
        return FALSE;
    if (viewport == NULL) {
        g_set_error (error, RSVG_ERROR, RSVG_ERROR_FAILED, "NULL viewport");
        return FALSE;
    }

    node = rsvg_handle_lookup_node (handle, id, error);
    if (node == NULL)
        return FALSE;

    if (!rsvg_handle_measure (handle, node, TRUE, &ink, &logical, error))
        return FALSE;

    rsvg_handle_get_document_size (handle, &dim);
    if (dim.width > 0 && dim.height > 0) {
        rsvg_rectangle_apply_viewport (&ink, viewport, dim.width, dim.height);
        rsvg_rectangle_apply_viewport (&logical, viewport, dim.width, dim.height);
    }

    if (out_ink_rect)
        *out_ink_rect = ink;
    if (out_logical_rect)
        *out_logical_rect = logical;
    return TRUE;
}

gboolean
rsvg_handle_get_geometry_for_element (RsvgHandle    *handle,
                                      const char    *id,
                                      RsvgRectangle *out_ink_rect,
                                      RsvgRectangle *out_logical_rect,
                                      GError       **error)
{
    RsvgNode *node;
    RsvgRectangle ink, logical;

    if (!rsvg_handle_check_ready (handle, error))
        return FALSE;

    node = rsvg_handle_lookup_node (handle, id, error);
    if (node == NULL)
        return FALSE;

    if (!rsvg_handle_measure (handle, node, FALSE, &ink, &logical, error))
        return FALSE;

    logical.x -= ink.x;
    logical.y -= ink.y;
    ink.width = ink.width;
    ink.height = ink.height;
    ink.x = 0;
    ink.y = 0;

    if (out_ink_rect)
        *out_ink_rect = ink;
    if (out_logical_rect)
        *out_logical_rect = logical;
    return TRUE;
}
