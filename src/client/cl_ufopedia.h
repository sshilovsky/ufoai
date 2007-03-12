/**
 * @file cl_ufopedia.h
 * @brief Header file for Ufopedia script interpreter.
 */

/*
Copyright (C) 2002-2006 UFO: Alien Invasion team.

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

#ifndef CLIENT_CL_UFOPEDIA_H
#define CLIENT_CL_UFOPEDIA_H

#define MAX_PEDIACHAPTERS	16

typedef struct pediaChapter_s
{
	int idx;				/* self-link */
	char	id[MAX_VAR];
	char	name[MAX_VAR];
	int first;
	int last;
} pediaChapter_t;

void UP_ResetUfopedia(void);
void UP_ParseUpChapters(char *title, char **text);
void UP_OpenWith(char *name);
void UP_OpenCopyWith(char *name);
void CL_ItemDescription(int item);
int UP_GetUnreadMails(void);

#endif /* CLIENT_CL_UFOPEDIA_H */
