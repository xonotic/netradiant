/* -------------------------------------------------------------------------------

   This code is based on source provided under the terms of the Id Software
   LIMITED USE SOFTWARE LICENSE AGREEMENT, a copy of which is included with the
   GtkRadiant sources (see LICENSE_ID). If you did not receive a copy of
   LICENSE_ID, please contact Id Software immediately at info@idsoftware.com.

   All changes and additions to the original source which have been developed by
   other contributors (see CONTRIBUTORS) are provided under the terms of the
   license the contributors choose (see LICENSE), to the extent permitted by the
   LICENSE_ID. If you did not receive a copy of the contributor license,
   please contact the GtkRadiant maintainers at info@gtkradiant.com immediately.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
   DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
   ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   ----------------------------------------------------------------------------------

   This code has been altered significantly from its original form, to support
   several games based on the Quake III Arena engine, in the form of "Q3Map2."

   ------------------------------------------------------------------------------- */



/* marker */
#ifndef GAME_WARSOW_H
#define GAME_WARSOW_H



/* -------------------------------------------------------------------------------

   content and surface flags

   ------------------------------------------------------------------------------- */

// reuses defines from game_qfusion.h



/* -------------------------------------------------------------------------------

   game_t struct

   ------------------------------------------------------------------------------- */

{
	"warsow",           /* -game x */
	"basewsw",          /* default base game data dir */
	".warsow",          /* unix home sub-dir */
	"warsow",           /* magic path word */
	"scripts",          /* shader directory */
	65535,              /* max lightmapped surface verts */
	65535,              /* max surface verts */
	393210,             /* max surface indexes */
	qfalse,             /* enable per shader prefix surface flags and .tex file */
	qfalse,             /* flares */
	"flareshader",      /* default flare shader */
	qfalse,             /* wolf lighting model? */
	512,                /* lightmap width/height */
	1.0f,               /* lightmap gamma */
	qtrue,              /* lightmap sRGB */
	qtrue,              /* texture sRGB */
	qtrue,              /* color sRGB */
	0.0f,               /* lightmap exposure */
	1.0f,               /* lightmap compensate */
	1.0f,               /* lightgrid scale */
	1.0f,               /* lightgrid ambient scale */
	qtrue,              /* light angle attenuation uses half-lambert curve */
	qtrue,              /* disable shader lightstyles hack */
	qtrue,              /* keep light entities on bsp */
	4,                  /* default patchMeta subdivisions tolerance */
	qtrue,              /* patch casting enabled */
	qtrue,              /* compile deluxemaps */
	0,                  /* deluxemaps default mode */
	512,                /* minimap size */
	1.0f,               /* minimap sharpener */
	1.0f / 66.0f,       /* minimap border */
	qtrue,              /* minimap keep aspect */
	MINIMAP_MODE_GRAY,  /* minimap mode */
	"../minimaps/%s.tga", /* minimap name format */
	MINIMAP_SIDECAR_NONE, /* minimap sidecar format */
	"FBSP",             /* bsp file prefix */
	1,                  /* bsp file version */
	qfalse,             /* cod-style lump len/ofs order */
	LoadRBSPFile,       /* bsp load function */
	WriteRBSPFile,      /* bsp write function */

	{
		/* name				contentFlags				contentFlagsClear			surfaceFlags				surfaceFlagsClear			compileFlags				compileFlagsClear */

		/* default */
		{ "default",        F_CONT_SOLID,               -1,                         0,                          -1,                         C_SOLID,                    -1 },


		/* ydnar */
		{ "lightgrid",      0,                          0,                          0,                          0,                          C_LIGHTGRID,                0 },
		{ "antiportal",     0,                          0,                          0,                          0,                          C_ANTIPORTAL,               0 },
		{ "skip",           0,                          0,                          0,                          0,                          C_SKIP,                     0 },


		/* compiler */
		{ "origin",         F_CONT_ORIGIN,              F_CONT_SOLID,               0,                          0,                          C_ORIGIN | C_TRANSLUCENT,   C_SOLID },
		{ "areaportal",     F_CONT_AREAPORTAL,          F_CONT_SOLID,               0,                          0,                          C_AREAPORTAL | C_TRANSLUCENT,   C_SOLID },
		{ "trans",          F_CONT_TRANSLUCENT,         0,                          0,                          0,                          C_TRANSLUCENT,              0 },
		{ "detail",         F_CONT_DETAIL,              0,                          0,                          0,                          C_DETAIL,                   0 },
		{ "structural",     F_CONT_STRUCTURAL,          0,                          0,                          0,                          C_STRUCTURAL,               0 },
		{ "hint",           0,                          0,                          F_SURF_HINT,                0,                          C_HINT,                     0 },
		{ "nodraw",         0,                          0,                          F_SURF_NODRAW,              0,                          C_NODRAW,                   0 },

		{ "alphashadow",    0,                          0,                          F_SURF_ALPHASHADOW,         0,                          C_ALPHASHADOW | C_TRANSLUCENT,  0 },
		{ "lightfilter",    0,                          0,                          F_SURF_LIGHTFILTER,         0,                          C_LIGHTFILTER | C_TRANSLUCENT,  0 },
		{ "nolightmap",     0,                          0,                          F_SURF_VERTEXLIT,           0,                          C_VERTEXLIT,                0 },
		{ "pointlight",     0,                          0,                          F_SURF_VERTEXLIT,           0,                          C_VERTEXLIT,                0 },


		/* game */
		{ "nonsolid",       0,                          F_CONT_SOLID,               F_SURF_NONSOLID,            0,                          0,                          C_SOLID },

		{ "trigger",        F_CONT_TRIGGER,             F_CONT_SOLID,               0,                          0,                          C_TRANSLUCENT,              C_SOLID },

		{ "water",          F_CONT_WATER,               F_CONT_SOLID,               0,                          0,                          C_LIQUID | C_TRANSLUCENT,   C_SOLID },
		{ "slime",          F_CONT_SLIME,               F_CONT_SOLID,               0,                          0,                          C_LIQUID | C_TRANSLUCENT,   C_SOLID },
		{ "lava",           F_CONT_LAVA,                F_CONT_SOLID,               0,                          0,                          C_LIQUID | C_TRANSLUCENT,   C_SOLID },

		{ "playerclip",     F_CONT_PLAYERCLIP,          F_CONT_SOLID,               0,                          0,                          C_DETAIL | C_TRANSLUCENT,   C_SOLID },
		{ "monsterclip",    F_CONT_MONSTERCLIP,         F_CONT_SOLID,               0,                          0,                          C_DETAIL | C_TRANSLUCENT,   C_SOLID },
		{ "nodrop",         F_CONT_NODROP,              F_CONT_SOLID,               0,                          0,                          C_TRANSLUCENT,              C_SOLID },

		{ "clusterportal",  F_CONT_CLUSTERPORTAL,       F_CONT_SOLID,               0,                          0,                          C_TRANSLUCENT,              C_SOLID },
		{ "donotenter",     F_CONT_DONOTENTER,          F_CONT_SOLID,               0,                          0,                          C_TRANSLUCENT,              C_SOLID },
		{ "botclip",        F_CONT_BOTCLIP,             F_CONT_SOLID,               0,                          0,                          C_TRANSLUCENT,              C_SOLID },

		{ "fog",            F_CONT_FOG,                 F_CONT_SOLID,               0,                          0,                          C_FOG,                      C_SOLID },
		{ "sky",            0,                          0,                          F_SURF_SKY,                 0,                          C_SKY,                      0 },

		{ "slick",          0,                          0,                          F_SURF_SLICK,               0,                          0,                          0 },

		{ "noimpact",       0,                          0,                          F_SURF_NOIMPACT,            0,                          0,                          0 },
		{ "nomarks",        0,                          0,                          F_SURF_NOMARKS,             0,                          C_NOMARKS,                  0 },
		{ "ladder",         0,                          0,                          F_SURF_LADDER,              0,                          0,                          0 },
		{ "nodamage",       0,                          0,                          F_SURF_NODAMAGE,            0,                          0,                          0 },
		{ "metalsteps",     0,                          0,                          F_SURF_METALSTEPS,          0,                          0,                          0 },
		{ "flesh",          0,                          0,                          F_SURF_FLESH,               0,                          0,                          0 },
		{ "nosteps",        0,                          0,                          F_SURF_NOSTEPS,             0,                          0,                          0 },
		{ "nodlight",       0,                          0,                          F_SURF_NODLIGHT,            0,                          0,                          0 },
		{ "dust",           0,                          0,                          F_SURF_DUST,                0,                          0,                          0 },


		/* null */
		{ NULL, 0, 0, 0, 0, 0, 0 }
	}
}



/* end marker */
#endif