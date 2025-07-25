/*
   Copyright (C) 1999-2007 id Software, Inc. and contributors.
   For a list of contributors, see the accompanying CONTRIBUTORS file.

   This file is part of GtkRadiant.

   GtkRadiant is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   GtkRadiant is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GtkRadiant; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */


typedef struct
{
	int numpoints;
	vec3_t p[4];
} winding_t;

#define MAX_POINTS_ON_WINDING   512

// you can define on_epsilon in the makefile as tighter
#ifndef ON_EPSILON
#define ON_EPSILON  0.1
#endif

winding_t   *AllocWinding( int points );
vec_t   WindingArea( winding_t *w );
void    WindingCenter( winding_t *w, vec3_t center );
void    ClipWindingEpsilon( winding_t *in, vec3_t normal, vec_t dist,
							vec_t epsilon, winding_t **front, winding_t **back );
void    ClipWindingEpsilonStrict( winding_t *in, vec3_t normal, vec_t dist,
								  vec_t epsilon, winding_t **front, winding_t **back );
winding_t   *ChopWinding( winding_t *in, vec3_t normal, vec_t dist );
winding_t   *CopyWinding( winding_t *w );
winding_t   *ReverseWinding( winding_t *w );
winding_t   *BaseWindingForPlane( vec3_t normal, vec_t dist );
void    CheckWinding( winding_t *w );
void    WindingPlane( winding_t *w, vec3_t normal, vec_t *dist );
void    RemoveColinearPoints( winding_t *w );
int     WindingOnPlaneSide( winding_t *w, vec3_t normal, vec_t dist );
void    FreeWinding( winding_t *w );
void    WindingBounds( winding_t *w, vec3_t mins, vec3_t maxs );

void    AddWindingToConvexHull( winding_t *w, winding_t **hull, vec3_t normal );

void    ChopWindingInPlace( winding_t **w, vec3_t normal, vec_t dist, vec_t epsilon );
// frees the original if clipped

void pw( winding_t *w );


///////////////////////////////////////////////////////////////////////////////////////
// Below is double-precision stuff.  This was initially needed by the base winding code
// in q3map2 brush processing.
///////////////////////////////////////////////////////////////////////////////////////

typedef struct
{
	int numpoints;
	vec3_accu_t p[4];
} winding_accu_t;

winding_accu_t  *BaseWindingForPlaneAccu( vec3_t normal, vec_t dist );
void    ChopWindingInPlaceAccu( winding_accu_t **w, vec3_t normal, vec_t dist, vec_t epsilon );
winding_t   *CopyWindingAccuToRegular( winding_accu_t *w );
void    FreeWindingAccu( winding_accu_t *w );
