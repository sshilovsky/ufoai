/**
 * @file m_windows.h
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

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

#ifndef CLIENT_MENU_M_WINDOWS_H
#define CLIENT_MENU_M_WINDOWS_H

#include "../../common/common.h"

/* prototype */
struct uiNode_s;

/* module initialization */
void UI_InitWindows(void);

/* window stack */
int UI_GetLastFullScreenWindow(void);
struct uiNode_s* UI_PushWindow(const char *name, const char *parentName);
void UI_InitStack(const char* activeMenu, const char* mainMenu, qboolean popAll, qboolean pushActive);
void UI_PopWindow(qboolean all);
void UI_PopWindowWithEscKey(void);
void UI_CloseWindow(const char* name);
struct uiNode_s* UI_GetActiveWindow(void);
int UI_CompleteWithWindow(const char *partial, const char **match);
qboolean UI_IsWindowOnStack(const char* name);
qboolean UI_IsPointOnWindow(void);
void UI_InvalidateStack(void);
void UI_InsertWindow(struct uiNode_s* window);
void UI_MoveWindowOnTop (struct uiNode_s * window);

/* deprecated */
const char* UI_GetActiveWindowName(void);
void UI_GetActiveRenderRect(int *x, int *y, int *width, int *height);

/** @todo move it on m_nodes, its a common getter/setter */
void UI_SetNewWindowPos(struct uiNode_s* menu, int x, int y);
struct uiNode_s *UI_GetWindow(const char *name);

#endif
