/**
 * @file cht_shared.h
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

#ifndef GAME_CHR_SHARED_H
#define GAME_CHR_SHARED_H

#include "q_shared.h"
#include "inv_shared.h"

typedef enum {
	KILLED_ENEMIES,		/**< Killed enemies */
	KILLED_CIVILIANS,	/**< Civilians, animals */
	KILLED_TEAM,		/**< Friendly fire, own team, partner-teams. */

	KILLED_NUM_TYPES
} killtypes_t;

/** @note Changing order/entries also changes network-transmission and savegames! */
typedef enum {
	ABILITY_POWER,
	ABILITY_SPEED,
	ABILITY_ACCURACY,
	ABILITY_MIND,

	SKILL_CLOSE,
	SKILL_HEAVY,
	SKILL_ASSAULT,
	SKILL_SNIPER,
	SKILL_EXPLOSIVE,
	SKILL_NUM_TYPES
} abilityskills_t;

#define ABILITY_NUM_TYPES SKILL_CLOSE

/**
 * @brief Structure of all stats collected in a mission.
 * @note More general Info: http://ufoai.ninex.info/wiki/index.php/Proposals/Attribute_Increase
 * @note Mostly collected in g_client.c and not used anywhere else (at least that's the plan ;)).
 * The result is parsed into chrScoreGlobal_t which is stored in savegames.
 * @note BTAxis about "hit" count:
 * "But yeah, what we want is a counter per skill. This counter should start at 0
 * every battle, and then be increased by 1 everytime:
 * - a direct fire weapon hits (or deals damage, same thing) the actor the weapon
 *   was fired at. If it wasn't fired at an actor, nothing should happen.
 * - a splash weapon deals damage to any enemy actor. If multiple actors are hit,
 *   increase the counter multiple times."
 */
typedef struct chrScoreMission_s {

	/* Movement counts. */
	int movedNormal;
	int movedCrouched;

	/* Kills & stuns */
	/** @todo use existing code */
	int kills[KILLED_NUM_TYPES];	/**< Count of kills (aliens, civilians, teammates) */
	int stuns[KILLED_NUM_TYPES];	/**< Count of stuns(aliens, civilians, teammates) */

	/* Hits/Misses */
	int fired[SKILL_NUM_TYPES];				/**< Count of fired "firemodes" (i.e. the count of how many times the soldier started shooting) */
	int firedTUs[SKILL_NUM_TYPES];				/**< Count of TUs used for the fired "firemodes". (direct hits only)*/
	qboolean firedHit[KILLED_NUM_TYPES];	/** Temporarily used for shot-stats calculations and status-tracking. Not used in stats.*/
	int hits[SKILL_NUM_TYPES][KILLED_NUM_TYPES];	/**< Count of hits (aliens, civilians or, teammates) per skill.
													 * It is a sub-count of "fired".
													 * It's planned to be increased by 1 for each series of shots that dealt _some_ damage. */
	int firedSplash[SKILL_NUM_TYPES];	/**< Count of fired splash "firemodes". */
	int firedSplashTUs[SKILL_NUM_TYPES];				/**< Count of TUs used for the fired "firemodes" (splash damage only). */
	qboolean firedSplashHit[KILLED_NUM_TYPES];	/** Same as firedHit but for Splash damage. */
	int hitsSplash[SKILL_NUM_TYPES][KILLED_NUM_TYPES];	/**< Count of splash hits. */
	int hitsSplashDamage[SKILL_NUM_TYPES][KILLED_NUM_TYPES];	/**< Count of dealt splash damage (aliens, civilians or, teammates).
														 		 * This is counted in overall damage (healthpoint).*/
	/** @todo Check HEALING of others. */
	int skillKills[SKILL_NUM_TYPES];	/**< Number of kills related to each skill. */

	int heal;	/**< How many hitpoints has this soldier received trough healing in battlescape. */
} chrScoreMission_t;

/**
 * @brief Structure of all stats collected for an actor over time.
 * @note More general Info: http://ufoai.ninex.info/wiki/index.php/Proposals/Attribute_Increase
 * @note This information is stored in savegames (in contract to chrScoreMission_t).
 * @note WARNING: if you change something here you'll have to make sure all the network and savegame stuff is updated as well!
 * Additionally you have to check the size of the network-transfer in G_SendCharacterData and GAME_CP_Results
 */
typedef struct chrScoreGlobal_s {
	int experience[SKILL_NUM_TYPES + 1]; /**< Array of experience values for all skills, and health. @todo What are the mins and maxs for these values */

	int skills[SKILL_NUM_TYPES];		/**< Array of skills and abilities. This is the total value. */
	int initialSkills[SKILL_NUM_TYPES + 1];		/**< Array of initial skills and abilities. This is the value generated at character generation time. */

	/* Kills & Stuns */
	int kills[KILLED_NUM_TYPES];	/**< Count of kills (aliens, civilians, teammates) */
	int stuns[KILLED_NUM_TYPES];	/**< Count of stuns(aliens, civilians, teammates) */

	int assignedMissions;		/**< Number of missions this soldier was assigned to. */

	int rank;					/**< Index of rank (in ccs.ranks). */
} chrScoreGlobal_t;

typedef struct chrFiremodeSettings_s {
	actorHands_t hand;	/**< Stores the used hand */
	fireDefIndex_t fmIdx;	/**< Stores the used firemode index. Max. number is MAX_FIREDEFS_PER_WEAPON -1=undef*/
	const objDef_t *weapon;
} chrFiremodeSettings_t;

/**
 * @brief How many TUs (and of what type) did a player reserve for a unit?
 * @sa CL_ActorUsableTUs
 * @sa CL_ActorReservedTUs
 * @sa CL_ActorReserveTUs
 */
typedef struct chrReservations_s {
	/* Reaction fire reservation (for current round and next enemy round) */
	int reaction;	/**< Did the player activate RF with a usable firemode?
					 * (And at the same time storing the TU-costs of this firemode) */

	/* Crouch reservation (for current round)	*/
	int crouch;	/**< Did the player reserve TUs for crouching (or standing up)? Depends exclusively on TU_CROUCH. */

	/* Shot reservation (for current round) */
	int shot;	/**< If non-zero we reserved a shot in this turn. */
	chrFiremodeSettings_t shotSettings;	/**< Stores what type of firemode & weapon
										 * (and hand) was used for "shot" reservation. */
} chrReservations_t;

typedef enum {
	RES_REACTION,
	RES_CROUCH,
	RES_SHOT,
	RES_ALL,
	RES_ALL_ACTIVE,
	RES_TYPES /**< Max. */
} reservation_types_t;

/** @brief Describes a character with all its attributes
 * @todo doesn't belong here */
typedef struct character_s {
	int ucn;					/**< unique character number */
	char name[MAX_VAR];			/**< Character name (as in: soldier name). */
	char path[MAX_VAR];
	char body[MAX_VAR];
	char head[MAX_VAR];
	int skin;					/**< Index of skin. */

	int HP;						/**< Health points (current ones). */
	int minHP;					/**< Minimum hp during combat */
	int maxHP;					/**< Maximum health points (as in: 100% == fully healed). */
	int STUN;
	int morale;

	int state;					/**< a character can request some initial states when the team is spawned (like reaction fire) */

	chrScoreGlobal_t score;		/**< Array of scores/stats the soldier/unit collected over time. */
	chrScoreMission_t *scoreMission;		/**< Array of scores/stats the soldier/unit collected in a mission - only used in battlescape (server side). Otherwise it's NULL. */

	/** @sa memcpy in Grid_CheckForbidden */
	actorSizeEnum_t fieldSize;				/**< @sa ACTOR_SIZE_**** */

	inventory_t i;			/**< Inventory definition. */

	teamDef_t *teamDef;			/**< Pointer to team definition. */
	int gender;				/**< Gender index. */
	chrReservations_t reservedTus;	/** < Stores the reserved TUs for actions. @sa See chrReserveSettings_t for more. */
	chrFiremodeSettings_t RFmode;	/** < Stores the firemode to be used for reaction fire (if the fireDef allows that) See also reaction_firemode_type_t */
} character_t;

/* ================================ */
/*  CHARACTER GENERATING FUNCTIONS  */
/* ================================ */

void CHRSH_CharGenAbilitySkills(character_t * chr, qboolean multiplayer) __attribute__((nonnull));
const char *CHRSH_CharGetBody(const character_t* const chr) __attribute__((nonnull));
const char *CHRSH_CharGetHead(const character_t* const chr) __attribute__((nonnull));
qboolean CHRSH_IsTeamDefAlien(const teamDef_t* const td) __attribute__((nonnull));
qboolean CHRSH_IsTeamDefRobot(const teamDef_t* const td) __attribute__((nonnull));

#endif
