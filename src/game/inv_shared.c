/**
 * @file inv_shared.c
 * @brief Common object-, inventory-, container- and firemode-related functions.
 * @note Shared inventory management functions prefix: INVSH_
 * @note Shared firemode management functions prefix: FIRESH_
 * @note Shared character generating functions prefix: CHRSH_
 */

/*
Copyright (C) 2002-2009 UFO: Alien Invasion.

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

#include "inv_shared.h"
#include "../shared/shared.h"

static csi_t *CSI;

/**
 * @brief Initializes client server shared data pointer. This works because the client and the server are both
 * using exactly the same pointer.
 * @param[in] import The client server interface pointer
 * @sa G_Init
 * @sa Com_ParseScripts
 */
void INVSH_InitCSI (csi_t * import)
{
	CSI = import;
}

/**
 * @brief Checks whether a given inventory definition is of special type
 * @param invDef The inventory definition to check
 * @return @c true if the given inventory definition is of type floor
 */
qboolean INV_IsFloorDef (const invDef_t* invDef)
{
	return invDef->id == CSI->idFloor;
}

/**
 * @brief Checks whether a given inventory definition is of special type
 * @param invDef The inventory definition to check
 * @return @c true if the given inventory definition is of type right
 */
qboolean INV_IsRightDef (const invDef_t* invDef)
{
	return invDef->id == CSI->idRight;
}

/**
 * @brief Checks whether a given inventory definition is of special type
 * @param invDef The inventory definition to check
 * @return @c true if the given inventory definition is of type left
 */
qboolean INV_IsLeftDef (const invDef_t* invDef)
{
	return invDef->id == CSI->idLeft;
}

/**
 * @brief Checks whether a given inventory definition is of special type
 * @param invDef The inventory definition to check
 * @return @c true if the given inventory definition is of type equip
 */
qboolean INV_IsEquipDef (const invDef_t* invDef)
{
	return invDef->id == CSI->idEquip;
}

/**
 * @brief Checks whether a given inventory definition is of special type
 * @param invDef The inventory definition to check
 * @return @c true if the given inventory definition is of type armour
 */
qboolean INV_IsArmourDef (const invDef_t* invDef)
{
	return invDef->id == CSI->idArmour;
}

static int cacheCheckToInventory = INV_DOES_NOT_FIT;

/**
 * @brief Will check if the item-shape is colliding with something else in the container-shape at position x/y.
 * @note The function expects an already rotated shape for itemShape. Use INVSH_ShapeRotate if needed.
 * @param[in] shape Pointer to 'uint32_t shape[SHAPE_BIG_MAX_HEIGHT]'
 * @param[in] itemShape
 * @param[in] x
 * @param[in] y
 */
static qboolean INVSH_CheckShapeCollision (const uint32_t *shape, const uint32_t itemShape, const int x, const int y)
{
	int i;

	/* Negative positions not allowed (all items are supposed to have at least one bit set in the first row and column) */
	if (x < 0 || y < 0) {
		Com_DPrintf(DEBUG_SHARED, "INVSH_CheckShapeCollision: x or y value negative: x=%i y=%i!\n", x, y);
		return qtrue;
	}

	for (i = 0; i < SHAPE_SMALL_MAX_HEIGHT; i++) {
		/* 0xFF is the length of one row in a "small shape" i.e. SHAPE_SMALL_MAX_WIDTH */
		const int itemRow = (itemShape >> (i * SHAPE_SMALL_MAX_WIDTH)) & 0xFF;
		/* Result has to be limited to 32bit (SHAPE_BIG_MAX_WIDTH) */
		const uint32_t itemRowShifted = itemRow << x;

		/* Check x maximum. */
		if (itemRowShifted >> x != itemRow)
			/* Out of bounds (32bit; a few bits of this row in itemShape were truncated) */
			return qtrue;

		/* Check y maximum. */
		if (y + i >= SHAPE_BIG_MAX_HEIGHT && itemRow)
			/* This row (row "i" in itemShape) is outside of the max. bound and has bits in it. */
			return qtrue;

		/* Check for collisions of the item with the big mask. */
		if (itemRowShifted & shape[y + i])
			return qtrue;
	}

	return qfalse;
}

/**
 * @brief Checks if an item-shape can be put into a container at a certain position... ignores any 'special' types of containers.
 * @param[in] i
 * @param[in] container The container (index) to look into.
 * @param[in] itemShape The shape info of an item to fit into the container.
 * @param[in] x The x value in the container (1 << x in the shape bitmask)
 * @param[in] y The x value in the container (SHAPE_BIG_MAX_HEIGHT is the max)
 * @param[in] ignoredItem You can ignore one item in the container (most often the currently dragged one). Use NULL if you want to check against all items in the container.
 * @sa INVSH_CheckToInventory
 * @return qfalse if the item does not fit, qtrue if it fits.
 */
static qboolean INVSH_CheckToInventory_shape (const inventory_t * const i, const invDef_t * container, const uint32_t itemShape, const int x, const int y, const invList_t *ignoredItem)
{
	int j;
	invList_t *ic;
	static uint32_t mask[SHAPE_BIG_MAX_HEIGHT];

	assert(container);

	if (container->scroll)
		Sys_Error("INVSH_CheckToInventory_shape: No scrollable container will ever use this. This type does not support grid-packing!");

	/* check bounds */
	if (x < 0 || y < 0 || x >= SHAPE_BIG_MAX_WIDTH || y >= SHAPE_BIG_MAX_HEIGHT)
		return qfalse;

	if (!cacheCheckToInventory) {
		/* extract shape info */
		for (j = 0; j < SHAPE_BIG_MAX_HEIGHT; j++)
			mask[j] = ~container->shape[j];

		/* Add other items to mask. (i.e. merge their shapes at their location into the generated mask) */
		for (ic = i->c[container->id]; ic; ic = ic->next) {
			if (ignoredItem == ic)
				continue;

			if (ic->item.rotated)
				INVSH_MergeShapes(mask, INVSH_ShapeRotate(ic->item.t->shape), ic->x, ic->y);
			else
				INVSH_MergeShapes(mask, ic->item.t->shape, ic->x, ic->y);
		}
	}

	/* Test for collisions with newly generated mask. */
	if (INVSH_CheckShapeCollision(mask, itemShape, x, y))
		return qfalse;

	/* Everything ok. */
	return qtrue;
}

/**
 * @param[in] i The inventory to check the item in.
 * @param[in] od The item to check in the inventory.
 * @param[in] container The index of the container in the inventory to check the item in.
 * @param[in] x The x value in the container (1 << x in the shape bitmask)
 * @param[in] y The y value in the container (SHAPE_BIG_MAX_HEIGHT is the max)
 * @param[in] ignoredItem You can ignore one item in the container (most often the currently dragged one). Use NULL if you want to check against all items in the container.
 * @return INV_DOES_NOT_FIT if the item does not fit
 * @return INV_FITS if it fits and
 * @return INV_FITS_ONLY_ROTATED if it fits only when rotated 90 degree (to the left).
 * @return INV_FITS_BOTH if it fits either normally or when rotated 90 degree (to the left).
 */
int INVSH_CheckToInventory (const inventory_t * const i, const objDef_t *od, const invDef_t * container, const int x, const int y, const invList_t *ignoredItem)
{
	int fits;
	assert(i);
	assert(container);
	assert(od);

	/* armour vs item */
	if (INV_IsArmour(od)) {
		if (!container->armour && !container->all) {
			return INV_DOES_NOT_FIT;
		}
	} else if (!od->extension && container->extension) {
		return INV_DOES_NOT_FIT;
	} else if (!od->headgear && container->headgear) {
		return INV_DOES_NOT_FIT;
	} else if (container->armour) {
		return INV_DOES_NOT_FIT;
	}

	/* twohanded item */
	if (od->holdTwoHanded) {
		if ((INV_IsRightDef(container) && i->c[CSI->idLeft]) || INV_IsLeftDef(container))
			return INV_DOES_NOT_FIT;
	}

	/* left hand is busy if right wields twohanded */
	if (INV_IsLeftDef(container)) {
		if (i->c[CSI->idRight] && i->c[CSI->idRight]->item.t->holdTwoHanded)
			return INV_DOES_NOT_FIT;

		/* can't put an item that is 'fireTwoHanded' into the left hand */
		if (od->fireTwoHanded)
			return INV_DOES_NOT_FIT;
	}

	/* Single item containers, e.g. hands, extension or headgear. */
	if (container->single) {
		if (i->c[container->id]) {
			/* There is already an item. */
			return INV_DOES_NOT_FIT;
		} else {
			fits = INV_DOES_NOT_FIT; /* equals 0 */

			if (INVSH_CheckToInventory_shape(i, container, od->shape, x, y, ignoredItem))
				fits |= INV_FITS;
			if (INVSH_CheckToInventory_shape(i, container, INVSH_ShapeRotate(od->shape), x, y, ignoredItem))
				fits |= INV_FITS_ONLY_ROTATED;

			if (fits != INV_DOES_NOT_FIT)
				return fits;	/**< Return INV_FITS_BOTH if both if statements where true above. */

			Com_DPrintf(DEBUG_SHARED, "INVSH_CheckToInventory: INFO: Moving to 'single' container but item would not fit normally.\n");
			return INV_FITS; /**< We are returning with status qtrue (1) if the item does not fit at all - unlikely but not impossible. */
		}
	}

	/* Scrolling container have endless room, the item always fits. */
	if (container->scroll)
		return INV_FITS;

	/* Check 'grid' containers. */
	fits = INV_DOES_NOT_FIT; /* equals 0 */
	if (INVSH_CheckToInventory_shape(i, container, od->shape, x, y, ignoredItem))
		fits |= INV_FITS;
	/** @todo aren't both (equip and floor) temp container? */
	if (!INV_IsEquipDef(container) && !INV_IsFloorDef(container)
	&& INVSH_CheckToInventory_shape(i, container, INVSH_ShapeRotate(od->shape), x, y, ignoredItem))
		fits |= INV_FITS_ONLY_ROTATED;

	return fits;	/**< Return INV_FITS_BOTH if both if statements where true above. */
}

/**
 * @brief Check if the (physical) information of 2 items is exactly the same.
 * @param[in] item1 First item to compare.
 * @param[in] item2 Second item to compare.
 * @return qtrue if they are identical or qfalse otherwise.
 */
qboolean INVSH_CompareItem (item_t *item1, item_t *item2)
{
	if (item1->t == item2->t && item1->m == item2->m && item1->a == item2->a)
		return qtrue;

	return qfalse;
}

/**
 * @brief Check if a position in a container is used by an item (i.e. collides with the shape).
 * @param[in] ic A pointer to an invList_t struct. The position is checked against its contained item.
 * @param[in] x The x location in the container.
 * @param[in] y The y location in the container.
 */
static qboolean INVSH_ShapeCheckPosition (const invList_t *ic, const int x, const int y)
{
	assert(ic);

 	/* Check if the position is inside the shape (depending on rotation value) of the item. */
	if (ic->item.rotated) {
 		if (((INVSH_ShapeRotate(ic->item.t->shape) >> (x - ic->x) >> (y - ic->y) * SHAPE_SMALL_MAX_WIDTH)) & 1)
 			return qtrue;
	} else {
 		if (((ic->item.t->shape >> (x - ic->x) >> (y - ic->y) * SHAPE_SMALL_MAX_WIDTH)) & 1)
 			return qtrue;
 	}

	/* Position is out of bounds or position not inside item-shape. */
	return qfalse;
}

/**
 * @brief Calculates the first "true" bit in the shape and returns its position in the container.
 * @note Use this to get the first "grab-able" grid-location (in the container) of an item.
 * @param[in] ic A pointer to an invList_t struct.
 * @param[out] x The x location inside the item.
 * @param[out] y The x location inside the item.
 * @sa INVSH_CheckToInventory
 */
void INVSH_GetFirstShapePosition (const invList_t *ic, int* const x, int* const y)
{
	int tempX, tempY;

	assert(ic);

	for (tempX = 0; tempX < SHAPE_SMALL_MAX_HEIGHT; tempX++)
		for (tempY = 0; tempY < SHAPE_SMALL_MAX_HEIGHT; tempY++)
			if (INVSH_ShapeCheckPosition(ic, ic->x + tempX, ic->y + tempY)) {
				*x = tempX;
				*y = tempY;
				return;
			}

	*x = *y = NONE;
}

/**
 * @brief Searches if there is a specific item already in the inventory&container.
 * @param[in] inv Pointer to the inventory where we will search.
 * @param[in] container Container in the inventory.
 * @param[in] item The item to search for.
 * @return qtrue if there already is at least one item of this type, otherwise qfalse.
 */
qboolean INVSH_ExistsInInventory (const inventory_t* const inv, const invDef_t * container, item_t item)
{
	invList_t *ic;

	for (ic = inv->c[container->id]; ic; ic = ic->next)
		if (INVSH_CompareItem(&ic->item, &item)) {
			return qtrue;
		}

	return qfalse;
}

/**
 * @brief Checks whether a given item is an aircraftitem item
 * @note This is done by checking whether it's a craftitem and not
 * marked as a dummy item - the combination of both means, that it's a
 * basedefence item.
 * @param[in] obj pointer to item definition to check whether it's an aircraftiem item
 * @return true if the given item is an aircraftitem item
 * @sa INV_IsBaseDefenceItem
 */
qboolean INV_IsCraftItem (const objDef_t *obj)
{
	return obj->craftitem.type != MAX_ACITEMS && !obj->isDummy;
}

/**
 * @brief Checks whether a given item is a basedefence item
 * @note This is done by checking whether it's a craftitem and whether it's
 * marked as a dummy item - the combination of both means, that it's a
 * basedefence item.
 * @param[in] obj pointer to item definition to check whether it's a basedefence item
 * @return true if the given item is a basedefence item
 * @sa INV_IsCraftItem
 */
qboolean INV_IsBaseDefenceItem (const objDef_t *obj)
{
	return obj->craftitem.type != MAX_ACITEMS && obj->isDummy;
}

/**
 * @brief Searches if there is an item at location (x,y) in a container.
 * @param[in] i Pointer to the inventory where we will search.
 * @param[in] container Container in the inventory.
 * @param[in] x/y Position in the container that you want to check.
 * @return invList_t Pointer to the invList_t/item that is located at x/y.
 * @sa INV_SearchInScrollableContainer
 */
invList_t *INVSH_SearchInInventory (const inventory_t* const i, const invDef_t * container, const int x, const int y)
{
	invList_t *ic;

	assert(container);

	/* Only a single item. */
	if (container->single)
		return i->c[container->id];

	if (container->scroll)
		Sys_Error("INVSH_SearchInInventory: Scrollable containers (%i:%s) are not supported by this function.\nUse INV_SearchInScrollableContainer instead!",
				container->id, container->name);

	/* More than one item - search for the item that is located at location x/y in this container. */
	for (ic = i->c[container->id]; ic; ic = ic->next)
		if (INVSH_ShapeCheckPosition(ic, x, y))
			return ic;

	/* Found nothing. */
	return NULL;
}

qboolean INVSH_UseableForTeam (const objDef_t *od, const int team)
{
	const qboolean isArmour = INV_IsArmour(od);
	if (isArmour && od->useable != team)
		return qfalse;

	return qtrue;
}

/**
 * @brief Finds space for item in inv at container
 * @param[in] inv The inventory to search space in
 * @param[in] item The item to check the space for
 * @param[in] container The container to search in
 * @param[out] px The x position in the container
 * @param[out] py The y position in the container
 * @param[in] ignoredItem You can ignore one item in the container (most often the currently dragged one). Use NULL if you want to check against all items in the container.
 * @sa INVSH_CheckToInventory
 */
void INVSH_FindSpace (const inventory_t* const inv, const item_t *item, const invDef_t * container, int* const px, int* const py, const invList_t *ignoredItem)
{
	int x, y;

	assert(inv);
	assert(container);
	assert(!cacheCheckToInventory);

	/* Scrollable container always have room. We return a dummy location. */
	if (container->scroll) {
		*px = *py = 0;
		return;
	}

	/** @todo optimize for single containers */

	for (y = 0; y < SHAPE_BIG_MAX_HEIGHT; y++) {
		for (x = 0; x < SHAPE_BIG_MAX_WIDTH; x++) {
			const int checkedTo = INVSH_CheckToInventory(inv, item->t, container, x, y, ignoredItem);
			if (checkedTo) {
				cacheCheckToInventory = INV_DOES_NOT_FIT;
				*px = x;
				*py = y;
				return;
			} else {
				cacheCheckToInventory = INV_FITS;
			}
		}
	}
	cacheCheckToInventory = INV_DOES_NOT_FIT;

#ifdef PARANOID
	Com_DPrintf(DEBUG_SHARED, "INVSH_FindSpace: no space for %s: %s in %s\n",
		item->t->type, item->t->id, container->name);
#endif
	*px = *py = NONE;
}

/*
================================
CHARACTER GENERATING FUNCTIONS
================================
*/

/**
 * @brief Check if a team definition is alien.
 * @param[in] td Pointer to the team definition to check.
 */
qboolean CHRSH_IsTeamDefAlien (const teamDef_t* const td)
{
	return td->race == RACE_TAMAN || td->race == RACE_ORTNOK
		|| td->race == RACE_BLOODSPIDER || td->race == RACE_SHEVAAR;
}

/**
 * @brief Templates for the different unit types. Each element of the array is a tuple that
 * indicates the minimum and the maximum value for the relevant ability or skill.
 */
static const int commonSoldier[][2] =
	{{15, 25}, /* Strength */
	 {15, 25}, /* Speed */
	 {20, 30}, /* Accuracy */
	 {20, 35}, /* Mind */
	 {15, 25}, /* Close */
	 {15, 25}, /* Heavy */
	 {15, 25}, /* Assault */
	 {15, 25}, /* Sniper */
	 {15, 25}, /* Explosives */
	 {80, 110}}; /* Health */

static const int closeSoldier[][2] =
	{{15, 25}, /* Strength */
	 {15, 25}, /* Speed */
	 {20, 30}, /* Accuracy */
	 {20, 35}, /* Mind */
	 {25, 40}, /* Close */
	 {13, 23}, /* Heavy */
	 {13, 23}, /* Assault */
	 {13, 23}, /* Sniper */
	 {13, 23}, /* Explosives */
	 {80, 110}}; /* Health */

static const int heavySoldier[][2] =
	{{15, 25}, /* Strength */
	 {15, 25}, /* Speed */
	 {20, 30}, /* Accuracy */
	 {20, 35}, /* Mind */
	 {13, 23}, /* Close */
	 {25, 40}, /* Heavy */
	 {13, 23}, /* Assault */
	 {13, 23}, /* Sniper */
	 {13, 23}, /* Explosives */
	 {80, 110}}; /* Health */

static const int assaultSoldier[][2] =
	{{15, 25}, /* Strength */
	 {15, 25}, /* Speed */
	 {20, 30}, /* Accuracy */
	 {20, 35}, /* Mind */
	 {13, 23}, /* Close */
	 {13, 23}, /* Heavy */
	 {25, 40}, /* Assault */
	 {13, 23}, /* Sniper */
	 {13, 23}, /* Explosives */
	 {80, 110}}; /* Health */

static const int sniperSoldier[][2] =
	{{15, 25}, /* Strength */
	 {15, 25}, /* Speed */
	 {20, 30}, /* Accuracy */
	 {20, 35}, /* Mind */
	 {13, 23}, /* Close */
	 {13, 23}, /* Heavy */
	 {13, 23}, /* Assault */
	 {25, 40}, /* Sniper */
	 {13, 23}, /* Explosives */
	 {80, 110}}; /* Health */

static const int blastSoldier[][2] =
	{{15, 25}, /* Strength */
	 {15, 25}, /* Speed */
	 {20, 30}, /* Accuracy */
	 {20, 35}, /* Mind */
	 {13, 23}, /* Close */
	 {13, 23}, /* Heavy */
	 {13, 23}, /* Assault */
	 {13, 23}, /* Sniper */
	 {25, 40}, /* Explosives */
	 {80, 110}}; /* Health */

static const int eliteSoldier[][2] =
	{{25, 35}, /* Strength */
	 {25, 35}, /* Speed */
	 {30, 40}, /* Accuracy */
	 {30, 45}, /* Mind */
	 {25, 40}, /* Close */
	 {25, 40}, /* Heavy */
	 {25, 40}, /* Assault */
	 {25, 40}, /* Sniper */
	 {25, 40}, /* Explosives */
	 {100, 130}}; /* Health */

static const int civilSoldier[][2] =
	{{5, 10}, /* Strength */
	 {5, 10}, /* Speed */
	 {10, 15}, /* Accuracy */
	 {10, 15}, /* Mind */
	 {5, 10}, /* Close */
	 {5, 10}, /* Heavy */
	 {5, 10}, /* Assault */
	 {5, 10}, /* Sniper */
	 {5, 10}, /* Explosives */
	 {5, 10}}; /* Health */

static const int tamanSoldier[][2] =
	{{25, 35}, /* Strength */
	 {25, 35}, /* Speed */
	 {40, 50}, /* Accuracy */
	 {50, 85}, /* Mind */
	 {50, 90}, /* Close */
	 {50, 90}, /* Heavy */
	 {50, 90}, /* Assault */
	 {50, 90}, /* Sniper */
	 {50, 90}, /* Explosives */
	 {100, 130}}; /* Health */

static const int ortnokSoldier[][2] =
	{{45, 65}, /* Strength */
	 {20, 30}, /* Speed */
	 {30, 45}, /* Accuracy */
	 {20, 40}, /* Mind */
	 {50, 90}, /* Close */
	 {50, 90}, /* Heavy */
	 {50, 90}, /* Assault */
	 {50, 90}, /* Sniper */
	 {50, 90}, /* Explosives */
	 {150, 190}}; /* Health */

static const int shevaarSoldier[][2] =
	{{30, 40}, /* Strength */
	 {30, 40}, /* Speed */
	 {40, 70}, /* Accuracy */
	 {30, 60}, /* Mind */
	 {50, 90}, /* Close */
	 {50, 90}, /* Heavy */
	 {50, 90}, /* Assault */
	 {50, 90}, /* Sniper */
	 {50, 90}, /* Explosives */
	 {120, 160}}; /* Health */

static const int bloodSoldier[][2] =
	{{55, 55}, /* Strength */
	 {50, 50}, /* Speed */
	 {50, 50}, /* Accuracy */
	 {0, 0}, /* Mind */
	 {50, 50}, /* Close */
	 {50, 50}, /* Heavy */
	 {50, 50}, /* Assault */
	 {50, 50}, /* Sniper */
	 {50, 50}, /* Explosives */
	 {150, 150}}; /* Health */

static const int robotSoldier[][2] =
	{{55, 55}, /* Strength */
	 {40, 40}, /* Speed */
	 {50, 50}, /* Accuracy */
	 {0, 0}, /* Mind */
	 {50, 50}, /* Close */
	 {50, 50}, /* Heavy */
	 {50, 50}, /* Assault */
	 {50, 50}, /* Sniper */
	 {50, 50}, /* Explosives */
	 {200, 200}}; /* Health */

/** @brief For multiplayer characters ONLY! */
static const int MPSoldier[][2] =
	{{25, 75}, /* Strength */
	 {25, 35}, /* Speed */
	 {20, 75}, /* Accuracy */
	 {30, 75}, /* Mind */
	 {20, 75}, /* Close */
	 {20, 75}, /* Heavy */
	 {20, 75}, /* Assault */
	 {20, 75}, /* Sniper */
	 {20, 75}, /* Explosives */
	 {80, 130}}; /* Health */

/**
 * @brief Generates a skill and ability set for any character.
 * @param[in] chr Pointer to the character, for which we generate stats.
 * @param[in] multiplayer If this is true we use the skill values from @c MPSoldier
 * mulitplayer is a special case here
 */
void CHRSH_CharGenAbilitySkills (character_t * chr, qboolean multiplayer)
{
	int i;
	const int (*chrTemplate)[2];

	/* Add modifiers for difficulty setting here! */
	switch (chr->teamDef->race) {
	case RACE_TAMAN:
		chrTemplate = tamanSoldier;
		break;
	case RACE_ORTNOK:
		chrTemplate = ortnokSoldier;
		break;
	case RACE_BLOODSPIDER:
		chrTemplate = bloodSoldier;
		break;
	case RACE_SHEVAAR:
		chrTemplate = shevaarSoldier;
		break;
	case RACE_CIVILIAN:
		chrTemplate = civilSoldier;
		break;
	case RACE_PHALANX_HUMAN: {
		if (multiplayer) {
			chrTemplate = MPSoldier;
		} else {
			/* Determine which soldier template to use.
			 * 25% of the soldiers will be specialists (5% chance each).
			 * 1% of the soldiers will be elite.
			 * 74% of the soldiers will be common. */
			const float soldierRoll = frand();
			if (soldierRoll <= 0.01f)
				chrTemplate = eliteSoldier;
			else if (soldierRoll <= 0.06)
				chrTemplate = closeSoldier;
			else if (soldierRoll <= 0.11)
				chrTemplate = heavySoldier;
			else if (soldierRoll <= 0.16)
				chrTemplate = assaultSoldier;
			else if (soldierRoll <= 0.22)
				chrTemplate = sniperSoldier;
			else if (soldierRoll <= 0.26)
				chrTemplate = blastSoldier;
			else
				chrTemplate = commonSoldier;
		}
		break;
	}
	case RACE_ROBOT:
		chrTemplate = robotSoldier;
		break;
	default:
		Sys_Error("CHRSH_CharGenAbilitySkills: unexpected race '%i'", chr->teamDef->race);
	}

	assert(chrTemplate);

	/* Abilities and skills -- random within the range */
	for (i = 0; i < SKILL_NUM_TYPES; i++) {
		const int abilityWindow = chrTemplate[i][1] - chrTemplate[i][0];
		/* Reminder: In case if abilityWindow==0 the ability will be set to the lower limit. */
		const int temp = (frand() * abilityWindow) + chrTemplate[i][0];
		chr->score.skills[i] = temp;
		chr->score.initialSkills[i] = temp;
	}

	{
		/* Health. */
		const int abilityWindow = chrTemplate[i][1] - chrTemplate[i][0];
		const int temp = (frand() * abilityWindow) + chrTemplate[i][0];
		chr->score.initialSkills[SKILL_NUM_TYPES] = temp;
		chr->maxHP = temp;
		chr->HP = temp;
	}

	/* Morale */
	chr->morale = GET_MORALE(chr->score.skills[ABILITY_MIND]);
	if (chr->morale >= MAX_SKILL)
		chr->morale = MAX_SKILL;

	/* Initialize the experience values */
	for (i = 0; i <= SKILL_NUM_TYPES; i++) {
		chr->score.experience[i] = 0;
	}
}

/**
 * @brief Returns the body model for the soldiers for armoured and non armoured soldiers
 * @param[in] chr Pointer to character struct
 * @sa CHRSH_CharGetBody
 * @return the character body model (from a static buffer)
 */
const char *CHRSH_CharGetBody (const character_t * const chr)
{
	static char returnModel[MAX_VAR];

	/* models of UGVs don't change - because they are already armoured */
	if (chr->i.c[CSI->idArmour] && chr->teamDef->race != RACE_ROBOT) {
		const objDef_t *od = chr->i.c[CSI->idArmour]->item.t;
		const char *id = od->armourPath;
		if (!INV_IsArmour(od))
			Sys_Error("CHRSH_CharGetBody: Item is no armour");

		Com_sprintf(returnModel, sizeof(returnModel), "%s%s/%s", chr->path, id, chr->body);
	} else
		Com_sprintf(returnModel, sizeof(returnModel), "%s/%s", chr->path, chr->body);
	return returnModel;
}

/**
 * @brief Returns the head model for the soldiers for armoured and non armoured soldiers
 * @param[in] chr Pointer to character struct
 * @sa CHRSH_CharGetBody
 */
const char *CHRSH_CharGetHead (const character_t * const chr)
{
	static char returnModel[MAX_VAR];

	/* models of UGVs don't change - because they are already armoured */
	if (chr->i.c[CSI->idArmour] && chr->teamDef->race != RACE_ROBOT) {
		const objDef_t *od = chr->i.c[CSI->idArmour]->item.t;
		const char *id = od->armourPath;
		if (!INV_IsArmour(od))
			Sys_Error("CHRSH_CharGetBody: Item is no armour");

		Com_sprintf(returnModel, sizeof(returnModel), "%s%s/%s", chr->path, id, chr->head);
	} else
		Com_sprintf(returnModel, sizeof(returnModel), "%s/%s", chr->path, chr->head);
	return returnModel;
}

/**
 * @brief Returns the index of this item in the inventory.
 * @note check that id is a none empty string
 * @note previously known as RS_GetItem
 * @param[in] id the item id in our object definition array (csi.ods)
 * @sa INVSH_GetItemByID
 */
objDef_t *INVSH_GetItemByIDSilent (const char *id)
{
	int i;

	if (!id)
		return NULL;
	for (i = 0; i < CSI->numODs; i++) {	/* i = item index */
		objDef_t *item = &CSI->ods[i];
		if (!strcmp(id, item->id)) {
			return item;
		}
	}
	return NULL;
}

/**
 * @brief Returns the index of this item in the inventory.
 */
objDef_t *INVSH_GetItemByIDX (int index)
{
	if (index == NONE)
		return NULL;

	if (index < 0 || index >= CSI->numODs)
		Sys_Error("Invalid object index given: %i", index);

	return &CSI->ods[index];
}

/**
 * @brief Returns the item that belongs to the given id or @c NULL if it wasn't found.
 * @param[in] id the item id in our object definition array (csi.ods)
 * @sa INVSH_GetItemByIDSilent
 */
objDef_t *INVSH_GetItemByID (const char *id)
{
	objDef_t *od = INVSH_GetItemByIDSilent(id);
	if (!od)
		Com_Printf("INVSH_GetItemByID: Item \"%s\" not found.\n", id);

	return od;
}

/**
 * Searched an inventory container by a given container id
 * @param[in] id ID or name of the inventory container to search for
 * @return @c NULL if not found
 */
invDef_t *INVSH_GetInventoryDefinitionByID (const char *id)
{
	containerIndex_t i;
	invDef_t *container;

 	for (i = 0, container = CSI->ids; i < CSI->numIDs; container++, i++)
 		if (!strcmp(id, container->name))
 			return container;

 	return NULL;
}

/**
 * @brief Checks if an item can be used to reload a weapon.
 * @param[in] od The object definition of the ammo.
 * @param[in] weapon The weapon (in the inventory) to check the item with.
 * @return qboolean Returns qtrue if the item can be used in the given weapon, otherwise qfalse.
 */
qboolean INVSH_LoadableInWeapon (const objDef_t *od, const objDef_t *weapon)
{
	int i;
	qboolean usable = qfalse;

#ifdef DEBUG
	if (!od) {
		Com_DPrintf(DEBUG_SHARED, "INVSH_LoadableInWeapon: No pointer given for 'od'.\n");
		return qfalse;
	}
	if (!weapon) {
		Com_DPrintf(DEBUG_SHARED, "INVSH_LoadableInWeapon: No weapon pointer given.\n");
		return qfalse;
	}
#endif

	if (od && od->numWeapons == 1 && od->weapons[0] && od->weapons[0] == od) {
		/* The weapon is only linked to itself. */
		return qfalse;
	}

	for (i = 0; i < od->numWeapons; i++) {
#ifdef DEBUG
		if (!od->weapons[i]) {
			Com_DPrintf(DEBUG_SHARED, "INVSH_LoadableInWeapon: No weapon pointer set for the %i. entry found in item '%s'.\n", i, od->id);
			break;
		}
#endif
		if (weapon == od->weapons[i]) {
			usable = qtrue;
			break;
		}
	}

	return usable;
}

/*
===============================
FIREMODE MANAGEMENT FUNCTIONS
===============================
*/

/**
 * @brief Get the fire definitions for a given object
 * @param[in] obj The object to get the firedef for
 * @param[in] weapFdsIdx the weapon index in the fire definition array
 * @param[in] fdIdx the fire definition index for the weapon (given by @c weapFdsIdx)
 * @return Will never return NULL
 * @sa FIRESH_FiredefForWeapon
 */
const fireDef_t* FIRESH_GetFiredef (const objDef_t *obj, const int weapFdsIdx, const int fdIdx)
{
	if (weapFdsIdx < 0 || weapFdsIdx >= MAX_WEAPONS_PER_OBJDEF)
		Sys_Error("FIRESH_GetFiredef: weapFdsIdx out of bounds [%i] for item '%s'", weapFdsIdx, obj->id);
	if (fdIdx < 0 || fdIdx >= MAX_FIREDEFS_PER_WEAPON)
		Sys_Error("FIRESH_GetFiredef: fdIdx out of bounds [%i] for item '%s'", fdIdx, obj->id);
	return &obj->fd[weapFdsIdx & (MAX_WEAPONS_PER_OBJDEF - 1)][fdIdx & (MAX_FIREDEFS_PER_WEAPON - 1)];
}

/**
 * @brief Returns the firedefinitions for a given weapon/ammo
 * @return The array (one-dimensional) of the firedefs of the ammo for a given weapon, or @c NULL if the ammo
 * doesn't support the given weapon
 * @sa FIRESH_GetFiredef
 */
const fireDef_t *FIRESH_FiredefForWeapon (const item_t *item)
{
	int i;
	const objDef_t *ammo = item->m;
	const objDef_t *weapon = item->t;

	/* this weapon does not use ammo, check for
	 * existing firedefs in the weapon. */
	if (weapon->numWeapons > 0)
		ammo = item->t;

	if (!ammo)
		return NULL;

	for (i = 0; i < ammo->numWeapons; i++) {
		if (weapon == ammo->weapons[i])
			return &ammo->fd[i][0];
	}

	return NULL;
}

/**
 * @brief Checks whether the given list contains a reaction fire enabled weapon.
 * @return @c NULL if no reaction fire enabled weapon was found in the given list, the
 * reaction fire enabled object otherwise.
 */
const objDef_t* INVSH_HasReactionFireEnabledWeapon (const invList_t *invList)
{
	if (!invList)
		return NULL;

	while (invList) {
		if (invList->item.t) {
			const fireDef_t *fd = FIRESH_FiredefForWeapon(&invList->item);
			if (fd && fd->reaction)
				return invList->item.t;
		}
		invList = invList->next;
	}

	return NULL;
}

/**
 * @brief Will merge the second shape (=itemShape) into the first one (=big container shape) on the position x/y.
 * @note The function expects an already rotated shape for itemShape. Use INVSH_ShapeRotate if needed.
 * @param[in] shape Pointer to 'uint32_t shape[SHAPE_BIG_MAX_HEIGHT]'
 * @param[in] itemShape
 * @param[in] x
 * @param[in] y
 */
void INVSH_MergeShapes (uint32_t *shape, const uint32_t itemShape, const int x, const int y)
{
	int i;

	for (i = 0; (i < SHAPE_SMALL_MAX_HEIGHT) && (y + i < SHAPE_BIG_MAX_HEIGHT); i++)
		shape[y + i] |= ((itemShape >> i * SHAPE_SMALL_MAX_WIDTH) & 0xFF) << x;
}

/**
 * @brief Checks the shape if there is a 1-bit on the position x/y.
 * @param[in] shape Pointer to 'uint32_t shape[SHAPE_BIG_MAX_HEIGHT]'
 * @param[in] x
 * @param[in] y
 */
qboolean INVSH_CheckShape (const uint32_t *shape, const int x, const int y)
{
	const uint32_t row = shape[y];
	int position = pow(2, x);

	if (y >= SHAPE_BIG_MAX_HEIGHT || x >= SHAPE_BIG_MAX_WIDTH || x < 0 || y < 0) {
		Com_Printf("INVSH_CheckShape: Bad x or y value: (x=%i, y=%i)\n", x, y);
		return qfalse;
	}

	if ((row & position) == 0)
		return qfalse;
	else
		return qtrue;
}

/**
 * @brief Checks the shape if there is a 1-bit on the position x/y.
 * @param[in] shape The shape to check in. (8x4)
 * @param[in] x
 * @param[in] y
 */
static qboolean INVSH_CheckShapeSmall (const uint32_t shape, const int x, const int y)
{
	if (y >= SHAPE_BIG_MAX_HEIGHT || x >= SHAPE_BIG_MAX_WIDTH || x < 0 || y < 0) {
		Com_Printf("INVSH_CheckShapeSmall: Bad x or y value: (x=%i, y=%i)\n", x, y);
		return qfalse;
	}

	return shape & (0x01 << (y * SHAPE_SMALL_MAX_WIDTH + x));
}

/**
 * @brief Counts the used bits in a shape (item shape).
 * @param[in] shape The shape to count the bits in.
 * @return Number of bits.
 * @note Used to calculate the real field usage in the inventory
 */
int INVSH_ShapeSize (const uint32_t shape)
{
 	int bitCounter = 0;
	int i;

	for (i = 0; i < SHAPE_SMALL_MAX_HEIGHT * SHAPE_SMALL_MAX_WIDTH; i++)
		if (shape & (1 << i))
			bitCounter++;

	return bitCounter;
}

/**
 * @brief Sets one bit in a shape to true/1
 * @note Only works for V_SHAPE_SMALL!
 * If the bit is already set the shape is not changed.
 * @param[in] shape The shape to modify. (8x4)
 * @param[in] x The x (width) position of the bit to set.
 * @param[in] y The y (height) position of the bit to set.
 * @return The new shape.
 */
static uint32_t INVSH_ShapeSetBit (uint32_t shape, const int x, const int y)
{
	if (x >= SHAPE_SMALL_MAX_WIDTH || y >= SHAPE_SMALL_MAX_HEIGHT || x < 0 || y < 0) {
		Com_Printf("INVSH_ShapeSetBit: Bad x or y value: (x=%i, y=%i)\n", x,y);
		return shape;
	}

	shape |= 0x01 << (y * SHAPE_SMALL_MAX_WIDTH + x);
	return shape;
}


/**
 * @brief Rotates a shape definition 90 degree to the left (CCW)
 * @note Only works for V_SHAPE_SMALL!
 * @param[in] shape The shape to rotate.
 * @return The new shape.
 */
uint32_t INVSH_ShapeRotate (const uint32_t shape)
{
	int h, w;
	uint32_t shapeNew = 0;
	int maxWidth = -1;

	for (w = SHAPE_SMALL_MAX_WIDTH - 1; w >= 0; w--) {
		for (h = 0; h < SHAPE_SMALL_MAX_HEIGHT; h++) {
			if (INVSH_CheckShapeSmall(shape, w, h)) {
				if (w >= SHAPE_SMALL_MAX_HEIGHT) {
					/* Object can't be rotated (code-wise), it is longer than SHAPE_SMALL_MAX_HEIGHT allows. */
					return shape;
				}

				if (maxWidth < 0)
					maxWidth = w;

				shapeNew = INVSH_ShapeSetBit(shapeNew, h, maxWidth - w);
			}
		}
	}

	return shapeNew;
}
