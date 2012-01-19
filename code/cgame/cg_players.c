/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
//
// cg_players.c -- handle the media and animation for player entities
#include "cg_local.h"

char	*cg_customSoundNames[MAX_CUSTOM_SOUNDS] = {
	"*death1.wav",
	"*death2.wav",
	"*death3.wav",
	"*jump1.wav",
	"*pain25_1.wav",
	"*pain50_1.wav",
	"*pain75_1.wav",
	"*pain100_1.wav",
	"*falling1.wav",
	"*gasp.wav",
	"*drown.wav",
	"*fall1.wav",
	"*taunt.wav"
};


/*
================
CG_CustomSound

================
*/
sfxHandle_t	CG_CustomSound( int clientNum, const char *soundName ) {
	clientInfo_t *ci;
	int			i;

	if ( soundName[0] != '*' ) {
		return trap_S_RegisterSound( soundName, qfalse );
	}

	if ( clientNum < 0 || clientNum >= MAX_CLIENTS ) {
		clientNum = 0;
	}
	ci = &cgs.clientinfo[ clientNum ];

	for ( i = 0 ; i < MAX_CUSTOM_SOUNDS && cg_customSoundNames[i] ; i++ ) {
		if ( !strcmp( soundName, cg_customSoundNames[i] ) ) {
			return ci->sounds[i];
		}
	}

	CG_Error( "Unknown custom sound: %s", soundName );
	return 0;
}



/*
=============================================================================

CLIENT INFO

=============================================================================
*/

/*
==========================
CG_FileExists
==========================
*/
static qboolean	CG_FileExists(const char *filename) {
	int len;

	len = trap_FS_FOpenFile( filename, NULL, FS_READ );
	if (len>0) {
		return qtrue;
	}
	return qfalse;
}


/*
==========================
CG_RegisterClientModelname
==========================
*/
static qboolean CG_RegisterClientModelname( clientInfo_t *ci, const char *modelName, const char *skinName, const char *headModelName, const char *headSkinName, const char *teamName ) {
	
	return qtrue;
}

/*
====================
CG_ColorFromString
====================
*/
static void CG_ColorFromString( const char *v, vec3_t color ) {
	int val;

	VectorClear( color );

	val = atoi( v );

	if ( val < 1 || val > 7 ) {
		VectorSet( color, 1, 1, 1 );
		return;
	}

	if ( val & 1 ) {
		color[2] = 1.0f;
	}
	if ( val & 2 ) {
		color[1] = 1.0f;
	}
	if ( val & 4 ) {
		color[0] = 1.0f;
	}
}

/*
===================
CG_LoadClientInfo

Load it now, taking the disk hits.
This will usually be deferred to a safe time
===================
*/
static void CG_LoadClientInfo( int clientNum, clientInfo_t *ci ) {
	int i;
	ci->deferred = qfalse;

	// reset any existing players and bodies, because they might be in bad
	// frames for this new model
	for ( i = 0 ; i < MAX_GENTITIES ; i++ ) {
		if ( cg_entities[i].currentState.clientNum == clientNum
			&& cg_entities[i].currentState.eType == ET_PLAYER ) {
			CG_ResetPlayerEntity( &cg_entities[i] );
		}
	}
}

/*
======================
CG_CopyClientInfoModel
======================
*/
static void CG_CopyClientInfoModel( clientInfo_t *from, clientInfo_t *to ) {
	VectorCopy( from->headOffset, to->headOffset );
	to->footsteps = from->footsteps;
	to->gender = from->gender;

	to->legsModel = from->legsModel;
	to->legsSkin = from->legsSkin;
	to->torsoModel = from->torsoModel;
	to->torsoSkin = from->torsoSkin;
	to->headModel = from->headModel;
	to->headSkin = from->headSkin;
	to->modelIcon = from->modelIcon;

	to->newAnims = from->newAnims;

	memcpy( to->sounds, from->sounds, sizeof( to->sounds ) );
}

/*
======================
CG_ScanForExistingClientInfo
======================
*/
static qboolean CG_ScanForExistingClientInfo( clientInfo_t *ci ) {
	int		i;
	clientInfo_t	*match;

	for ( i = 0 ; i < cgs.maxclients ; i++ ) {
		match = &cgs.clientinfo[ i ];
		if ( !match->infoValid ) {
			continue;
		}
		if ( match->deferred ) {
			continue;
		}
		if ( !Q_stricmp( ci->modelName, match->modelName )
			&& !Q_stricmp( ci->skinName, match->skinName )
			&& !Q_stricmp( ci->headModelName, match->headModelName )
			&& !Q_stricmp( ci->headSkinName, match->headSkinName )
			&& !Q_stricmp( ci->blueTeam, match->blueTeam )
			&& !Q_stricmp( ci->redTeam, match->redTeam )
			&& (cgs.gametype < GT_TEAM || ci->team == match->team) ) {
			// this clientinfo is identical, so use it's handles

			ci->deferred = qfalse;

			CG_CopyClientInfoModel( match, ci );

			return qtrue;
		}
	}

	// nothing matches, so defer the load
	return qfalse;
}

/*
======================
CG_SetDeferredClientInfo

We aren't going to load it now, so grab some other
client's info to use until we have some spare time.
======================
*/
static void CG_SetDeferredClientInfo( int clientNum, clientInfo_t *ci ) {
	int		i;
	clientInfo_t	*match;

	// if someone else is already the same models and skins we
	// can just load the client info
	for ( i = 0 ; i < cgs.maxclients ; i++ ) {
		match = &cgs.clientinfo[ i ];
		if ( !match->infoValid || match->deferred ) {
			continue;
		}
		if ( Q_stricmp( ci->skinName, match->skinName ) ||
			 Q_stricmp( ci->modelName, match->modelName ) ||
//			 Q_stricmp( ci->headModelName, match->headModelName ) ||
//			 Q_stricmp( ci->headSkinName, match->headSkinName ) ||
			 (cgs.gametype >= GT_TEAM && ci->team != match->team) ) {
			continue;
		}
		// just load the real info cause it uses the same models and skins
		CG_LoadClientInfo( clientNum, ci );
		return;
	}

	// if we are in teamplay, only grab a model if the skin is correct
	if ( cgs.gametype >= GT_TEAM ) {
		for ( i = 0 ; i < cgs.maxclients ; i++ ) {
			match = &cgs.clientinfo[ i ];
			if ( !match->infoValid || match->deferred ) {
				continue;
			}
			if ( Q_stricmp( ci->skinName, match->skinName ) ||
				(cgs.gametype >= GT_TEAM && ci->team != match->team) ) {
				continue;
			}
			ci->deferred = qtrue;
			CG_CopyClientInfoModel( match, ci );
			return;
		}
		// load the full model, because we don't ever want to show
		// an improper team skin.  This will cause a hitch for the first
		// player, when the second enters.  Combat shouldn't be going on
		// yet, so it shouldn't matter
		CG_LoadClientInfo( clientNum, ci );
		return;
	}

	// find the first valid clientinfo and grab its stuff
	for ( i = 0 ; i < cgs.maxclients ; i++ ) {
		match = &cgs.clientinfo[ i ];
		if ( !match->infoValid ) {
			continue;
		}

		ci->deferred = qtrue;
		CG_CopyClientInfoModel( match, ci );
		return;
	}

	// we should never get here...
	CG_Printf( "CG_SetDeferredClientInfo: no valid clients!\n" );

	CG_LoadClientInfo( clientNum, ci );
}


/*
======================
CG_NewClientInfo
======================
*/
void CG_NewClientInfo( int clientNum ) {
	clientInfo_t *ci;
	clientInfo_t newInfo;
	const char	*configstring;
	const char	*v;
	char		*slash;

	ci = &cgs.clientinfo[clientNum];

	configstring = CG_ConfigString( clientNum + CS_PLAYERS );
	if ( !configstring[0] ) {
		memset( ci, 0, sizeof( *ci ) );
		return;		// player just left
	}

	// build into a temp buffer so the defer checks can use
	// the old value
	memset( &newInfo, 0, sizeof( newInfo ) );

	// isolate the player's name
	v = Info_ValueForKey(configstring, "n");
	Q_strncpyz( newInfo.name, v, sizeof( newInfo.name ) );

	// colors
	v = Info_ValueForKey( configstring, "c1" );
	CG_ColorFromString( v, newInfo.color1 );

	v = Info_ValueForKey( configstring, "c2" );
	CG_ColorFromString( v, newInfo.color2 );

	// bot skill
	v = Info_ValueForKey( configstring, "skill" );
	newInfo.botSkill = atoi( v );

	// handicap
	v = Info_ValueForKey( configstring, "hc" );
	newInfo.handicap = atoi( v );

	// wins
	v = Info_ValueForKey( configstring, "w" );
	newInfo.wins = atoi( v );

	// losses
	v = Info_ValueForKey( configstring, "l" );
	newInfo.losses = atoi( v );

	// team
	v = Info_ValueForKey( configstring, "t" );
	newInfo.team = atoi( v );

	// team task
	v = Info_ValueForKey( configstring, "tt" );
	newInfo.teamTask = atoi(v);

	// team leader
	v = Info_ValueForKey( configstring, "tl" );
	newInfo.teamLeader = atoi(v);

	v = Info_ValueForKey( configstring, "g_redteam" );
	Q_strncpyz(newInfo.redTeam, v, MAX_TEAMNAME);

	v = Info_ValueForKey( configstring, "g_blueteam" );
	Q_strncpyz(newInfo.blueTeam, v, MAX_TEAMNAME);

	// model
	v = Info_ValueForKey( configstring, "model" );
	if ( cg_forceModel.integer ) {
		// forcemodel makes everyone use a single model
		// to prevent load hitches
		char modelStr[MAX_QPATH];
		char *skin;

		if( cgs.gametype >= GT_TEAM ) {
			Q_strncpyz( newInfo.modelName, DEFAULT_TEAM_MODEL, sizeof( newInfo.modelName ) );
			Q_strncpyz( newInfo.skinName, "default", sizeof( newInfo.skinName ) );
		} else {
			trap_Cvar_VariableStringBuffer( "model", modelStr, sizeof( modelStr ) );
			if ( ( skin = strchr( modelStr, '/' ) ) == NULL) {
				skin = "default";
			} else {
				*skin++ = 0;
			}

			Q_strncpyz( newInfo.skinName, skin, sizeof( newInfo.skinName ) );
			Q_strncpyz( newInfo.modelName, modelStr, sizeof( newInfo.modelName ) );
		}

		if ( cgs.gametype >= GT_TEAM ) {
			// keep skin name
			slash = strchr( v, '/' );
			if ( slash ) {
				Q_strncpyz( newInfo.skinName, slash + 1, sizeof( newInfo.skinName ) );
			}
		}
	} else {
		Q_strncpyz( newInfo.modelName, v, sizeof( newInfo.modelName ) );

		slash = strchr( newInfo.modelName, '/' );
		if ( !slash ) {
			// modelName didn not include a skin name
			Q_strncpyz( newInfo.skinName, "default", sizeof( newInfo.skinName ) );
		} else {
			Q_strncpyz( newInfo.skinName, slash + 1, sizeof( newInfo.skinName ) );
			// truncate modelName
			*slash = 0;
		}
	}

	// head model
	v = Info_ValueForKey( configstring, "hmodel" );
	if ( cg_forceModel.integer ) {
		// forcemodel makes everyone use a single model
		// to prevent load hitches
		char modelStr[MAX_QPATH];
		char *skin;

		if( cgs.gametype >= GT_TEAM ) {
			Q_strncpyz( newInfo.headModelName, DEFAULT_TEAM_MODEL, sizeof( newInfo.headModelName ) );
			Q_strncpyz( newInfo.headSkinName, "default", sizeof( newInfo.headSkinName ) );
		} else {
			trap_Cvar_VariableStringBuffer( "headmodel", modelStr, sizeof( modelStr ) );
			if ( ( skin = strchr( modelStr, '/' ) ) == NULL) {
				skin = "default";
			} else {
				*skin++ = 0;
			}

			Q_strncpyz( newInfo.headSkinName, skin, sizeof( newInfo.headSkinName ) );
			Q_strncpyz( newInfo.headModelName, modelStr, sizeof( newInfo.headModelName ) );
		}

		if ( cgs.gametype >= GT_TEAM ) {
			// keep skin name
			slash = strchr( v, '/' );
			if ( slash ) {
				Q_strncpyz( newInfo.headSkinName, slash + 1, sizeof( newInfo.headSkinName ) );
			}
		}
	} else {
		Q_strncpyz( newInfo.headModelName, v, sizeof( newInfo.headModelName ) );

		slash = strchr( newInfo.headModelName, '/' );
		if ( !slash ) {
			// modelName didn not include a skin name
			Q_strncpyz( newInfo.headSkinName, "default", sizeof( newInfo.headSkinName ) );
		} else {
			Q_strncpyz( newInfo.headSkinName, slash + 1, sizeof( newInfo.headSkinName ) );
			// truncate modelName
			*slash = 0;
		}
	}

	// scan for an existing clientinfo that matches this modelname
	// so we can avoid loading checks if possible
	if ( !CG_ScanForExistingClientInfo( &newInfo ) ) {
		qboolean	forceDefer;

		forceDefer = trap_MemoryRemaining() < 4000000;

		// if we are defering loads, just have it pick the first valid
		if ( forceDefer || (cg_deferPlayers.integer && !cg_buildScript.integer && !cg.loading ) ) {
			// keep whatever they had if it won't violate team skins
			CG_SetDeferredClientInfo( clientNum, &newInfo );
			// if we are low on memory, leave them with this model
			if ( forceDefer ) {
				CG_Printf( "Memory is low.  Using deferred model.\n" );
				newInfo.deferred = qfalse;
			}
		} else {
			CG_LoadClientInfo( clientNum, &newInfo );
		}
	}

	// replace whatever was there with the new one
	newInfo.infoValid = qtrue;
	*ci = newInfo;
}



/*
======================
CG_LoadDeferredPlayers

Called each frame when a player is dead
and the scoreboard is up
so deferred players can be loaded
======================
*/
void CG_LoadDeferredPlayers( void ) {
	int		i;
	clientInfo_t	*ci;

	// scan for a deferred player to load
	for ( i = 0, ci = cgs.clientinfo ; i < cgs.maxclients ; i++, ci++ ) {
		if ( ci->infoValid && ci->deferred ) {
			// if we are low on memory, leave it deferred
			if ( trap_MemoryRemaining() < 4000000 ) {
				CG_Printf( "Memory is low.  Using deferred model.\n" );
				ci->deferred = qfalse;
				continue;
			}
			CG_LoadClientInfo( i, ci );
//			break;
		}
	}
}

/*
=============================================================================

PLAYER ANGLES

=============================================================================
*/

/*
==================
CG_SwingAngles
==================
*/
static void CG_SwingAngles( float destination, float swingTolerance, float clampTolerance,
					float speed, float *angle, qboolean *swinging ) {
	float	swing;
	float	move;
	float	scale;

	if ( !*swinging ) {
		// see if a swing should be started
		swing = AngleSubtract( *angle, destination );
		if ( swing > swingTolerance || swing < -swingTolerance ) {
			*swinging = qtrue;
		}
	}

	if ( !*swinging ) {
		return;
	}

	// modify the speed depending on the delta
	// so it doesn't seem so linear
	swing = AngleSubtract( destination, *angle );
	scale = fabs( swing );
	if ( scale < swingTolerance * 0.5 ) {
		scale = 0.5;
	} else if ( scale < swingTolerance ) {
		scale = 1.0;
	} else {
		scale = 2.0;
	}

	// swing towards the destination angle
	if ( swing >= 0 ) {
		move = cg.frametime * scale * speed;
		if ( move >= swing ) {
			move = swing;
			*swinging = qfalse;
		}
		*angle = AngleMod( *angle + move );
	} else if ( swing < 0 ) {
		move = cg.frametime * scale * -speed;
		if ( move <= swing ) {
			move = swing;
			*swinging = qfalse;
		}
		*angle = AngleMod( *angle + move );
	}

	// clamp to no more than tolerance
	swing = AngleSubtract( destination, *angle );
	if ( swing > clampTolerance ) {
		*angle = AngleMod( destination - (clampTolerance - 1) );
	} else if ( swing < -clampTolerance ) {
		*angle = AngleMod( destination + (clampTolerance - 1) );
	}
}

/*
=================
CG_AddPainTwitch
=================
*/
static void CG_AddPainTwitch( centity_t *cent, vec3_t torsoAngles ) {
	int		t;
	float	f;

	t = cg.time - cent->pe.painTime;
	if ( t >= PAIN_TWITCH_TIME ) {
		return;
	}

	f = 1.0 - (float)t / PAIN_TWITCH_TIME;

	if ( cent->pe.painDirection ) {
		torsoAngles[ROLL] += 20 * f;
	} else {
		torsoAngles[ROLL] -= 20 * f;
	}
}


/*
===============
CG_PlayerAngles

Handles seperate torso motion

  legs pivot based on direction of movement

  head always looks exactly at cent->lerpAngles

  if motion < 20 degrees, show in head only
  if < 45 degrees, also show in torso
===============
*/
static void CG_PlayerAngles( centity_t *cent, vec3_t legs[3], vec3_t torso[3], vec3_t head[3] ) {

}


//==========================================================================

/*
===============
CG_TrailItem
===============
*/
static void CG_TrailItem( centity_t *cent, qhandle_t hModel ) {
	refEntity_t		ent;
	vec3_t			angles;
	vec3_t			axis[3];

	VectorCopy( cent->lerpAngles, angles );
	angles[PITCH] = 0;
	angles[ROLL] = 0;
	AnglesToAxis( angles, axis );

	memset( &ent, 0, sizeof( ent ) );
	VectorMA( cent->lerpOrigin, -16, axis[0], ent.origin );
	ent.origin[2] += 16;
	angles[YAW] += 90;
	AnglesToAxis( angles, ent.axis );

	ent.hModel = hModel;
	trap_R_AddRefEntityToScene( &ent );
}



/*
===============
CG_PlayerFloatSprite

Float a sprite over the player's head
===============
*/
static void CG_PlayerFloatSprite( centity_t *cent, qhandle_t shader ) {
	int				rf;
	refEntity_t		ent;

	if ( cent->currentState.number == cg.snap->ps.clientNum && !cg.renderingThirdPerson ) {
		rf = RF_THIRD_PERSON;		// only show in mirrors
	} else {
		rf = 0;
	}

	memset( &ent, 0, sizeof( ent ) );
	VectorCopy( cent->lerpOrigin, ent.origin );
	ent.origin[2] += 48;
	ent.reType = RT_SPRITE;
	ent.customShader = shader;
	ent.radius = 10;
	ent.renderfx = rf;
	ent.shaderRGBA[0] = 255;
	ent.shaderRGBA[1] = 255;
	ent.shaderRGBA[2] = 255;
	ent.shaderRGBA[3] = 255;
	trap_R_AddRefEntityToScene( &ent );
}



/*
===============
CG_PlayerSprites

Float sprites over the player's head
===============
*/
static void CG_PlayerSprites( centity_t *cent ) {
	int		team;

	if ( cent->currentState.eFlags & EF_CONNECTION ) {
		CG_PlayerFloatSprite( cent, cgs.media.connectionShader );
		return;
	}

	if ( cent->currentState.eFlags & EF_TALK ) {
		CG_PlayerFloatSprite( cent, cgs.media.balloonShader );
		return;
	}

	if ( cent->currentState.eFlags & EF_AWARD_IMPRESSIVE ) {
		CG_PlayerFloatSprite( cent, cgs.media.medalImpressive );
		return;
	}

	if ( cent->currentState.eFlags & EF_AWARD_EXCELLENT ) {
		CG_PlayerFloatSprite( cent, cgs.media.medalExcellent );
		return;
	}

	if ( cent->currentState.eFlags & EF_AWARD_GAUNTLET ) {
		CG_PlayerFloatSprite( cent, cgs.media.medalGauntlet );
		return;
	}

	if ( cent->currentState.eFlags & EF_AWARD_DEFEND ) {
		CG_PlayerFloatSprite( cent, cgs.media.medalDefend );
		return;
	}

	if ( cent->currentState.eFlags & EF_AWARD_ASSIST ) {
		CG_PlayerFloatSprite( cent, cgs.media.medalAssist );
		return;
	}

	if ( cent->currentState.eFlags & EF_AWARD_CAP ) {
		CG_PlayerFloatSprite( cent, cgs.media.medalCapture );
		return;
	}

	team = cgs.clientinfo[ cent->currentState.clientNum ].team;
	if ( !(cent->currentState.eFlags & EF_DEAD) &&
		cg.snap->ps.stats[STAT_TEAM] == team &&
		cgs.gametype >= GT_TEAM) {
		if (cg_drawFriend.integer) {
			CG_PlayerFloatSprite( cent, cgs.media.friendShader );
		}
		return;
	}
}

/*
===============
CG_PlayerShadow

Returns the Z component of the surface being shadowed

  should it return a full plane instead of a Z?
===============
*/
#define	SHADOW_DISTANCE		128
static qboolean CG_PlayerShadow( centity_t *cent, float *shadowPlane ) {
	vec3_t		end, mins = {-15, -15, 0}, maxs = {15, 15, 2};
	trace_t		trace;
	float		alpha;

	*shadowPlane = 0;

	if ( cg_shadows.integer == 0 ) {
		return qfalse;
	}


	// send a trace down from the player to the ground
	VectorCopy( cent->lerpOrigin, end );
	end[2] -= SHADOW_DISTANCE;

	trap_CM_BoxTrace( &trace, cent->lerpOrigin, end, mins, maxs, 0, MASK_PLAYERSOLID );

	// no shadow if too high
	if ( trace.fraction == 1.0 || trace.startsolid || trace.allsolid ) {
		return qfalse;
	}

	*shadowPlane = trace.endpos[2] + 1;

	if ( cg_shadows.integer != 1 ) {	// no mark for stencil or projection shadows
		return qtrue;
	}

	// fade the shadow out with height
	alpha = 1.0 - trace.fraction;

	// hack / FPE - bogus planes?
	//assert( DotProduct( trace.plane.normal, trace.plane.normal ) != 0.0f )

	// add the mark as a temporary, so it goes directly to the renderer
	// without taking a spot in the cg_marks array
	CG_ImpactMark( cgs.media.shadowMarkShader, trace.endpos, trace.plane.normal,
		/*cent->pe.legs.yawAngle*/ 0 , alpha,alpha,alpha,1, qfalse, 24, qtrue );

	return qtrue;
}


/*
===============
CG_PlayerSplash

Draw a mark at the water surface
===============
*/
static void CG_PlayerSplash( centity_t *cent ) {
	vec3_t		start, end;
	trace_t		trace;
	int			contents;
	polyVert_t	verts[4];

	if ( !cg_shadows.integer ) {
		return;
	}

	VectorCopy( cent->lerpOrigin, end );
	end[2] -= 24;

	// if the feet aren't in liquid, don't make a mark
	// this won't handle moving water brushes, but they wouldn't draw right anyway...
	contents = trap_CM_PointContents( end, 0 );
	if ( !( contents & ( CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA ) ) ) {
		return;
	}

	VectorCopy( cent->lerpOrigin, start );
	start[2] += 32;

	// if the head isn't out of liquid, don't make a mark
	contents = trap_CM_PointContents( start, 0 );
	if ( contents & ( CONTENTS_SOLID | CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA ) ) {
		return;
	}

	// trace down to find the surface
	trap_CM_BoxTrace( &trace, start, end, NULL, NULL, 0, ( CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA ) );

	if ( trace.fraction == 1.0 ) {
		return;
	}

	// create a mark polygon
	VectorCopy( trace.endpos, verts[0].xyz );
	verts[0].xyz[0] -= 32;
	verts[0].xyz[1] -= 32;
	verts[0].st[0] = 0;
	verts[0].st[1] = 0;
	verts[0].modulate[0] = 255;
	verts[0].modulate[1] = 255;
	verts[0].modulate[2] = 255;
	verts[0].modulate[3] = 255;

	VectorCopy( trace.endpos, verts[1].xyz );
	verts[1].xyz[0] -= 32;
	verts[1].xyz[1] += 32;
	verts[1].st[0] = 0;
	verts[1].st[1] = 1;
	verts[1].modulate[0] = 255;
	verts[1].modulate[1] = 255;
	verts[1].modulate[2] = 255;
	verts[1].modulate[3] = 255;

	VectorCopy( trace.endpos, verts[2].xyz );
	verts[2].xyz[0] += 32;
	verts[2].xyz[1] += 32;
	verts[2].st[0] = 1;
	verts[2].st[1] = 1;
	verts[2].modulate[0] = 255;
	verts[2].modulate[1] = 255;
	verts[2].modulate[2] = 255;
	verts[2].modulate[3] = 255;

	VectorCopy( trace.endpos, verts[3].xyz );
	verts[3].xyz[0] += 32;
	verts[3].xyz[1] -= 32;
	verts[3].st[0] = 1;
	verts[3].st[1] = 0;
	verts[3].modulate[0] = 255;
	verts[3].modulate[1] = 255;
	verts[3].modulate[2] = 255;
	verts[3].modulate[3] = 255;

	trap_R_AddPolyToScene( cgs.media.wakeMarkShader, 4, verts );
}

/*
=================
CG_LightVerts
=================
*/
int CG_LightVerts( vec3_t normal, int numVerts, polyVert_t *verts )
{
	int				i, j;
	float			incoming;
	vec3_t			ambientLight;
	vec3_t			lightDir;
	vec3_t			directedLight;

	trap_R_LightForPoint( verts[0].xyz, ambientLight, directedLight, lightDir );

	for (i = 0; i < numVerts; i++) {
		incoming = DotProduct (normal, lightDir);
		if ( incoming <= 0 ) {
			verts[i].modulate[0] = ambientLight[0];
			verts[i].modulate[1] = ambientLight[1];
			verts[i].modulate[2] = ambientLight[2];
			verts[i].modulate[3] = 255;
			continue;
		}
		j = ( ambientLight[0] + incoming * directedLight[0] );
		if ( j > 255 ) {
			j = 255;
		}
		verts[i].modulate[0] = j;

		j = ( ambientLight[1] + incoming * directedLight[1] );
		if ( j > 255 ) {
			j = 255;
		}
		verts[i].modulate[1] = j;

		j = ( ambientLight[2] + incoming * directedLight[2] );
		if ( j > 255 ) {
			j = 255;
		}
		verts[i].modulate[2] = j;

		verts[i].modulate[3] = 255;
	}
	return qtrue;
}

/*
===============
CG_Player
===============
*/
void CG_Player( centity_t *cent ) {

}


//=====================================================================

/*
===============
CG_ResetPlayerEntity

A player just came into view or teleported, so reset all animation info
===============
*/
void CG_ResetPlayerEntity( centity_t *cent ) {
	cent->errorTime = -99999;		// guarantee no error decay added
	cent->extrapolated = qfalse;

	//BG_EvaluateTrajectory( &cent->currentState.pos, cg.time, cent->lerpOrigin );
	//BG_EvaluateTrajectory( &cent->currentState.apos, cg.time, cent->lerpAngles );

	VectorCopy( cent->currentState.origin, cent->lerpOrigin );
	VectorCopy( cent->currentState.angles, cent->lerpAngles );

	VectorCopy( cent->lerpOrigin, cent->rawOrigin );
	VectorCopy( cent->lerpAngles, cent->rawAngles );
}

