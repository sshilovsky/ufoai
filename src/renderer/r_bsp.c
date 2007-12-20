/**
 * @file r_bsp.c
 * @brief BSP model code
 */

/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "r_local.h"
#include "r_lightmap.h"

static vec3_t modelorg;			/* relative to viewpoint */
static model_t *currentmodel;

/*
=============================================================
BRUSH MODELS
=============================================================
*/

#define BACKFACE_EPSILON 0.01

/**
 * @brief
 */
static void R_DrawInlineBrushModel (entity_t *e)
{
	int i;
	cBspPlane_t *plane;
	float dot;
	mBspSurface_t *surf;
	mBspSurface_t *opaque_surfaces, *opaque_warp_surfaces;
	mBspSurface_t *alpha_surfaces, *alpha_warp_surfaces;
	mBspSurface_t *material_surfaces;

	opaque_surfaces = opaque_warp_surfaces = NULL;
	alpha_surfaces = alpha_warp_surfaces = NULL;

	material_surfaces = NULL;

	surf = &e->model->bsp.surfaces[e->model->bsp.firstmodelsurface];

	for (i = 0; i < e->model->bsp.nummodelsurfaces; i++, surf++) {
		/* find which side of the node we are on */
		plane = surf->plane;

		dot = DotProduct(modelorg, plane->normal) - plane->dist;

		if (((surf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
			(!(surf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON))) {

			/* add to appropriate surface chain */
			if (surf->texinfo->flags & (SURF_TRANS33 | SURF_TRANS66)) {
				if (surf->texinfo->flags & SURF_WARP) {
					surf->next = alpha_warp_surfaces;
					alpha_warp_surfaces = surf;
				} else {
					surf->next = alpha_surfaces;
					alpha_surfaces = surf;
				}
			} else {
				if (surf->texinfo->flags & SURF_WARP) {
					surf->next = opaque_warp_surfaces;
					opaque_warp_surfaces = surf;
				} else {
					surf->next = opaque_surfaces;
					opaque_surfaces = surf;
				}
			}

			/* add to the material chain if appropriate */
			if (surf->texinfo->image->material.flags & STAGE_RENDER) {
				surf->materialchain = material_surfaces;
				material_surfaces = surf;
			}
		}
	}

	R_DrawOpaqueSurfaces(opaque_surfaces);

	R_DrawOpaqueWarpSurfaces(opaque_warp_surfaces);

	R_EnableBlend(qtrue);

	R_DrawAlphaSurfaces(alpha_surfaces);

	R_DrawAlphaWarpSurfaces(alpha_warp_surfaces);

	R_DrawMaterialSurfaces(material_surfaces);

	R_EnableBlend(qfalse);
}


/**
 * @brief Draws a brush model
 * @note E.g. a func_breakable or func_door
 */
void R_DrawBrushModel (entity_t * e)
{
	vec3_t mins, maxs;
	int i;
	qboolean rotated;

	if (currentmodel->bsp.nummodelsurfaces == 0)
		return;

	if (e->angles[0] || e->angles[1] || e->angles[2]) {
		rotated = qtrue;
		for (i = 0; i < 3; i++) {
			mins[i] = e->origin[i] - currentmodel->radius;
			maxs[i] = e->origin[i] + currentmodel->radius;
		}
	} else {
		rotated = qfalse;
		VectorAdd(e->origin, currentmodel->mins, mins);
		VectorAdd(e->origin, currentmodel->maxs, maxs);
	}

	if (R_CullBox(mins, maxs))
		return;

	VectorSubtract(refdef.vieworg, e->origin, modelorg);
	if (rotated) {
		vec3_t temp;
		vec3_t forward, right, up;

		VectorCopy(modelorg, temp);
		AngleVectors(e->angles, forward, right, up);
		modelorg[0] = DotProduct(temp, forward);
		modelorg[1] = -DotProduct(temp, right);
		modelorg[2] = DotProduct(temp, up);
	}

	qglPushMatrix();
	e->angles[0] = -e->angles[0];	/* stupid quake bug */
	e->angles[2] = -e->angles[2];	/* stupid quake bug */
	R_RotateForEntity(e);
	e->angles[0] = -e->angles[0];	/* stupid quake bug */
	e->angles[2] = -e->angles[2];	/* stupid quake bug */

	R_DrawInlineBrushModel(e);

	qglPopMatrix();
}


/*
=============================================================
WORLD MODEL
=============================================================
*/

/**
 * @sa R_DrawWorld
 * @sa R_RecurseWorld
 */
static void R_RecursiveWorldNode (mBspNode_t * node)
{
	int c, side, sidebit;
	cBspPlane_t *plane;
	mBspSurface_t *surf;
	float dot;

	if (node->contents == CONTENTS_SOLID)
		return;					/* solid */

	if (R_CullBox(node->minmaxs, node->minmaxs + 3))
		return;

	/* if a leaf node, draw stuff */
	if (node->contents != LEAFNODE)
		return;

	/* node is just a decision point, so go down the apropriate sides
	 * find which side of the node we are on */
	plane = node->plane;

	if (r_isometric->integer) {
		dot = -DotProduct(vpn, plane->normal);
	} else if (plane->type >= 3) {
		dot = DotProduct(modelorg, plane->normal) - plane->dist;
	} else {
		dot = modelorg[plane->type] - plane->dist;
	}

	if (dot >= 0) {
		side = 0;
		sidebit = 0;
	} else {
		side = 1;
		sidebit = SURF_PLANEBACK;
	}

	/* recurse down the children, front side first */
	R_RecursiveWorldNode(node->children[side]);

	/* draw stuff */
	for (c = node->numsurfaces, surf = currentmodel->bsp.surfaces + node->firstsurface; c; c--, surf++) {
		if ((surf->flags & SURF_PLANEBACK) != sidebit)
			continue;			/* wrong side */

		/* add to appropriate surface chain */
		if (surf->texinfo->flags & (SURF_TRANS33 | SURF_TRANS66)) {
			if (surf->texinfo->flags & SURF_WARP) {
				surf->next = r_alpha_warp_surfaces;
				r_alpha_warp_surfaces = surf;
			} else {
				surf->next = r_alpha_surfaces;
				r_alpha_surfaces = surf;
			}
		} else {
			if (surf->texinfo->flags & SURF_WARP) {
				surf->next = r_opaque_warp_surfaces;
				r_opaque_warp_surfaces = surf;
			} else {
				surf->next = r_opaque_surfaces;
				r_opaque_surfaces = surf;
			}
		}

		/* add to the material chain if appropriate */
		if (surf->texinfo->image->material.flags & STAGE_RENDER) {
			surf->materialchain = r_material_surfaces;
			r_material_surfaces = surf;
		}
	}

	/* recurse down the back side */
	R_RecursiveWorldNode(node->children[!side]);
}

/**
 * @sa R_GetLevelSurfaceChains
 */
static void R_RecurseWorld (mBspNode_t * node)
{
	if (!node->plane) {
		R_RecurseWorld(node->children[0]);
		R_RecurseWorld(node->children[1]);
	} else {
		VectorCopy(refdef.vieworg, modelorg);

		R_Color(NULL);
		memset(gl_lms.lightmap_surfaces, 0, sizeof(gl_lms.lightmap_surfaces));

		R_RecursiveWorldNode(node);
	}
}


/**
 * @brief Fills the surface chains for the current worldlevel and hide other levels
 * @sa cvar cl_worldlevel
 */
void R_GetLevelSurfaceChains (void)
{
	int i, tile, mask;

	if (refdef.rdflags & RDF_NOWORLDMODEL)
		return;

	if (!r_drawworld->integer)
		return;

	mask = 1 << refdef.worldlevel;

	/* reset surface chains and regenerate them */
	r_opaque_surfaces = r_opaque_warp_surfaces = NULL;
	r_alpha_surfaces = r_alpha_warp_surfaces = NULL;

	r_material_surfaces = NULL;

	for (tile = 0; tile < rNumTiles; tile++) {
		currentmodel = rTiles[tile];

		/* don't draw weapon-, actorclip and stepon */
		/* @note Change this to 258 to see the actorclip brushes in-game */
		for (i = 0; i <= LEVEL_LASTVISIBLE; i++) {
			/* check the worldlevel flags */
			if (i && !(i & mask))
				continue;

			if (!currentmodel->bsp.submodels[i].numfaces)
				continue;

			R_RecurseWorld(currentmodel->bsp.nodes + currentmodel->bsp.submodels[i].headnode);
		}
	}
}
