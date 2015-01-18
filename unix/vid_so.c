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
// Main windowed and fullscreen graphics interface module. This module
// is used for both the software and OpenGL rendering versions of the
// Quake refresh engine.

#include <dlfcn.h> // ELF dl loader
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include "../client/client.h"

// Console variables that we need to access from this module
cvar_t		*vid_gamma;
cvar_t		*vid_ref;			// Name of Refresh DLL loaded
cvar_t		*vid_xpos;			// X coordinate of window position
cvar_t		*vid_ypos;			// Y coordinate of window position
cvar_t		*vid_fullscreen;
cvar_t		*r_customwidth;
cvar_t		*r_customheight;

// Global variables used internally by this module
viddef_t	viddef;				// global video state; used by other modules
qboolean	kmgl_active = 0;

#define VID_NUM_MODES ( sizeof( vid_modes ) / sizeof( vid_modes[0] ) )

cvar_t 		*scanforcd; // Knightmare- just here to enable command line option

int     vidref_val;

extern void	VID_MenuShutdown (void);


/*
==========================================================================

DLL GLUE

==========================================================================
*/

#define	MAXPRINTMSG	8192 // was 4096
void VID_Printf (int print_level, char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];
//	static qboolean	inupdate;

	va_start (argptr, fmt);
//	vsprintf (msg, fmt, argptr);
	Q_vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);

	if (print_level == PRINT_ALL)
		Com_Printf ("%s", msg);
	else
		Com_DPrintf ("%s", msg);
}

void VID_Error (int err_level, char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];
//	static qboolean	inupdate;
	
	va_start (argptr, fmt);
//	vsprintf (msg, fmt, argptr);
	Q_vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);

	Com_Error (err_level,"%s", msg);
}

//==========================================================================

/*
============
VID_Restart_f

Console command to re-start the video mode and refresh DLL. We do this
simply by setting the modified flag for the vid_ref variable, which will
cause the entire video mode and refresh DLL to be reset on the next frame.
============
*/
void VID_Restart_f (void)
{
	vid_ref->modified = true;
}

/*
** VID_GetModeInfo
*/
typedef struct vidmode_s
{
	const char *description;
	int         width, height;
	int         mode;
} vidmode_t;

// Knightmare- added 1280x1024, 1400x1050, 856x480, 1024x480 modes
vidmode_t vid_modes[] =
{
#include "../qcommon/vid_modes.h"
};

qboolean VID_GetModeInfo( int *width, int *height, int mode )
{
	if ( mode < 0 || mode >= VID_NUM_MODES )
		return false;

	*width  = vid_modes[mode].width;
	*height = vid_modes[mode].height;

	return true;
}

/*
** VID_NewWindow
*/
void VID_NewWindow ( int width, int height)
{
	viddef.width  = width;
	viddef.height = height;
	cl.force_refdef = true;		// can't use a paused refdef
}

void VID_FreeReflib (void)
{
	kmgl_active  = false;
}

extern	decalpolys_t	*active_decals;
static qboolean reclip_decals = false;
qboolean	vid_reloading; // Knightmare- flag to not unnecessarily drop console

/*
==============
UpdateVideoRef
==============
*/
void UpdateVideoRef (void)
{
	char	reason[128];

	if ( vid_ref->modified )
	{
		cl.force_refdef = true;		// can't use a paused refdef
		S_StopAllSounds();

		// unclip decals
		if (active_decals) {
			CL_UnclipDecals();
			reclip_decals = true;
		}
	}

	vid_reloading = false;

	while (vid_ref->modified)
	{	// refresh has changed
		vid_ref->modified = false;
		vid_fullscreen->modified = true;
		cl.refresh_prepped = false;
		if (cl.cinematictime > 0) // Knightmare added
			cls.disable_screen = false;
		else
			cls.disable_screen = true;
		vid_reloading = true;
		// end Knightmare

		//==========================
		// compacted code from VID_LoadRefresh
		//==========================
		if ( kmgl_active )
		{
			R_Shutdown();
			VID_FreeReflib ();
		}

		Com_Printf( "\n--------- Renderer Initialization ---------\n");

		if ( !R_Init( 0, 0, reason ) == -1 )
		{
			R_Shutdown();
			VID_FreeReflib ();
			Com_Error (ERR_FATAL, "Couldn't initialize OpenGL renderer!\n%s", reason);
		}

		Com_Printf( "------------------------------------\n");

		kmgl_active = true;
		//==========================
	}
	/* prefer to fall back on X if active */
	//if (getenv("DISPLAY"))
		Cvar_Set( "vid_ref", "kmglx" );
	/*else
		Cvar_Set( "vid_ref", "kmsdlgl" );*/

	// added to close loading screen
//	if (cl.refresh_prepped && vid_reloading)
//		cls.disable_screen = false;

	// re-clip decals
	if (cl.refresh_prepped && reclip_decals) {
		CL_ReclipDecals();
		reclip_decals = false;
	}

 	vid_reloading = false;
}

/*
============
VID_CheckChanges

This function gets called once just before drawing each frame, and it's sole purpose in life
is to check to see if any of the video mode parameters have changed, and if they have to 
update the rendering DLL and/or video mode to match.
============
*/
void VID_CheckChanges (void)
{

	//update changed vid_ref
	UpdateVideoRef ();

	// update our window position
	if ( vid_xpos->modified || vid_ypos->modified )
	{
		if (!vid_fullscreen->value)
		vid_fullscreen->modified = false;

		vid_xpos->modified = false;
		vid_ypos->modified = false;
	}

}

/*
============
VID_Init
============
*/
void VID_Init (void)
{
	/* Create the video variables so we know how to start the graphics drivers */
	// if DISPLAY is defined, try X
	//if (getenv("DISPLAY"))
		vid_ref = Cvar_Get ("vid_ref", "kmglx", CVAR_ARCHIVE);
	/*else
		vid_ref = Cvar_Get ("vid_ref", "kmsdlgl", CVAR_ARCHIVE);*/
		
	vid_xpos = Cvar_Get ("vid_xpos", "3", CVAR_ARCHIVE);
	vid_ypos = Cvar_Get ("vid_ypos", "22", CVAR_ARCHIVE);
	vid_fullscreen = Cvar_Get ("vid_fullscreen", "0", CVAR_ARCHIVE);
	vid_gamma = Cvar_Get( "vid_gamma", "0.8", CVAR_ARCHIVE );
	r_customwidth = Cvar_Get( "r_customwidth", "1600", CVAR_ARCHIVE );
	r_customheight = Cvar_Get( "r_customheight", "1024", CVAR_ARCHIVE );
	// Knightmare- just here to enable command line option without error
	scanforcd = Cvar_Get( "scanforcd", "0", 0 );

	Cvar_Set( "vid_ref", "kmglx" );
	vidref_val = VIDREF_GL;	// this is always in GL mode
	
	/* Add some console commands that we want to handle */
	Cmd_AddCommand ("vid_restart", VID_Restart_f);
		
	/* Start the graphics mode and load refresh DLL */
	VID_CheckChanges();
}

/*
============
VID_Shutdown
============
*/
void VID_Shutdown (void)
{
	if ( kmgl_active )
	{
		R_Shutdown ();
		VID_FreeReflib ();
	}
}

