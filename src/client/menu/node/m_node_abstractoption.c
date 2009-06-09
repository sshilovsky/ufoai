/**
 * @file m_node_selectbox.c
 * @todo manage disabled option
 */

/*
Copyright (C) 1997-2008 UFO:AI Team

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

#include "../m_main.h"
#include "../m_internal.h"
#include "../m_parse.h"
#include "../m_draw.h"
#include "../m_data.h"
#include "m_node_abstractoption.h"
#include "m_node_abstractnode.h"

#define SELECTBOX_SIDE_WIDTH 7.0f
#define SELECTBOX_RIGHT_WIDTH 20.0f

#define SELECTBOX_SPACER 2.0f
#define SELECTBOX_BOTTOM_HEIGHT 4.0f

/**
 * @brief Sort options by alphabet
 */
void MN_OptionNodeSortOptions (menuNode_t *node)
{
	assert(MN_NodeInstanceOf(node, "abstractoption"));
	MN_SortOptions(&node->u.option.first);
}

/**
 * @brief Adds a new selectbox option to a selectbox node
 * @return NULL if menuSelectBoxes is 'full' - otherwise pointer to the selectBoxOption
 * @param[in] node The abstractoption where you want to append the option
 * @param[in] option The option to add
 * @note You have to add the values manually to the option pointer
 */
menuOption_t* MN_NodeAppendOption (menuNode_t *node, menuOption_t *newOption)
{
	assert(node);
	assert(newOption);
	assert(MN_NodeInstanceOf(node, "abstractoption"));

	if (!node->u.option.first)
		node->u.option.first = newOption;
	else {
		/* link it in */
		menuOption_t *option = node->u.option.first;
		while (option->next)
			option = option->next;
		option->next = newOption;
	}
	node->u.option.count++;
	return newOption;
}

static void MN_UpdateOption_f (void)
{
	menuNode_t *node;
	menuOption_t *option;

	if (Cmd_Argc() != 4) {
		Com_Printf("Usage: %s <nodepath> <optionname> <hide|display|enable|disable>\n", Cmd_Argv(0));
		return;
	}

	node = MN_GetNodeByPath(Cmd_Argv(1));
	if (node == NULL) {
		Com_Printf("MN_UpdateOption_f: '%s' node not found.\n", Cmd_Argv(1));
		return;
	}

	if (!MN_NodeInstanceOf(node, "abstractoption")) {
		Com_Printf("MN_UpdateOption_f: '%s' node is not an 'abstractoption'.\n", Cmd_Argv(1));
		return;
	}

	option = node->u.option.first;
	while (option) {
		if (!strcmp(option->id, Cmd_Argv(2)))
			break;
		option = option->next;
	}
	if (option == NULL) {
		Com_Printf("MN_UpdateOption_f: option '%s' from '%s' node not found.\n", Cmd_Argv(2), Cmd_Argv(1));
		return;
	}

	if (!strcmp("disable", Cmd_Argv(3))) {
		option->disabled = qtrue;
	} else if (!strcmp("enable", Cmd_Argv(3))) {
		option->disabled = qfalse;
	} else if (!strcmp("hide", Cmd_Argv(3))) {
		option->invis = qtrue;
	} else if (!strcmp("display", Cmd_Argv(3))) {
		option->invis = qfalse;
	} else {
		Com_Printf("MN_AddListener_f: '%s' command do not exists.\n", Cmd_Argv(3));
		return;
	}
}

static const value_t properties[] = {
	{"dataid", V_UI_DATAID, offsetof(menuNode_t, u.option.dataId), MEMBER_SIZEOF(menuNode_t, u.option.dataId)},
	{"option", V_UI_OPTIONNODE, offsetof(menuNode_t, u.option.first), 0},
	{"viewpos", V_INT, offsetof(menuNode_t, u.option.pos),  MEMBER_SIZEOF(menuNode_t, u.option.pos)},
	{"count", V_INT, offsetof(menuNode_t, u.option.count),  MEMBER_SIZEOF(menuNode_t, u.option.count)},
	{"onviewchange", V_UI_ACTION, offsetof(menuNode_t, u.option.onViewChange), MEMBER_SIZEOF(menuNode_t, u.option.onViewChange)},

	{NULL, V_NULL, 0, 0}
};

void MN_RegisterAbstractOptionNode (nodeBehaviour_t *behaviour)
{
	behaviour->name = "abstractoption";
	behaviour->isAbstract = qtrue;
	behaviour->properties = properties;
	Cmd_AddCommand("mn_updateoption", MN_UpdateOption_f, "Update some option status");
}
