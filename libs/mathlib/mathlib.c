/*
   Copyright (C) 1999-2006 Id Software, Inc. and contributors.
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

// mathlib.c -- math primitives
#include "mathlib.h"
// we use memcpy and memset
#include <memory.h>

const vec3_t vec3_origin = {0.0f,0.0f,0.0f};

const vec3_t g_vec3_axis_x = { 1, 0, 0, };
const vec3_t g_vec3_axis_y = { 0, 1, 0, };
const vec3_t g_vec3_axis_z = { 0, 0, 1, };

/*
   ================
   VectorIsOnAxis
   ================
 */
qboolean VectorIsOnAxis( vec3_t v ){
	int i, zeroComponentCount;

	zeroComponentCount = 0;
	for ( i = 0; i < 3; i++ )
	{
		if ( v[i] == 0.0 ) {
			zeroComponentCount++;
		}
	}

	if ( zeroComponentCount > 1 ) {
		// The zero vector will be on axis.
		return qtrue;
	}

	return qfalse;
}

/*
   ================
   VectorIsOnAxialPlane
   ================
 */
qboolean VectorIsOnAxialPlane( vec3_t v ){
	int i;

	for ( i = 0; i < 3; i++ )
	{
		if ( v[i] == 0.0 ) {
			// The zero vector will be on axial plane.
			return qtrue;
		}
	}

	return qfalse;
}

// Ported from NRC
static void vector3_cross_C( const vec3_t v1, const vec3_t v2, vec3_t out ){
	out[0] = v1[1] * v2[2] - v1[2] * v2[1];
	out[1] = v1[2] * v2[0] - v1[0] * v2[2];
	out[2] = v1[0] * v2[1] - v1[1] * v2[0];
}

// Ported from NRC
static size_t vector3_min_abs_component_index_C( vec3_t v ){
	const size_t mini = ( fabs( v[1] ) < fabs( v[0] ) )? 1 : 0;
	return ( fabs( v[2] ) < fabs( v[mini] ) )? 2 : mini;
}

const vec3_t g_vector3_axis_x_C = { 1, 0, 0 };
const vec3_t g_vector3_axis_y_C = { 0, 1, 0 };
const vec3_t g_vector3_axis_z_C = { 0, 0, 1 };

const vec3_t *g_vector3_axes_C[3] = { &g_vector3_axis_x_C, &g_vector3_axis_y_C, &g_vector3_axis_z_C };

/*
   ================
   MakeNormalVectors

   Given a normalized forward vector, create two
   other perpendicular vectors
   ================
 */
void MakeNormalVectors( vec3_t forward, vec3_t right, vec3_t up ){
#if 0
	/* This is known to be buggy and to fuse some brushes,
	see: https://gitlab.com/xonotic/netradiant/-/issues/195 */

	float d;

	// this rotate and negate guarantees a vector
	// not colinear with the original
	//! fails with forward( -0.577350259 -0.577350259 0.577350259 )
	//! colinear right( 0.577350259 0.577350259 -0.577350259 )
	right[1] = -forward[0];
	right[2] = forward[1];
	right[0] = forward[2];

	d = DotProduct( right, forward );
	VectorMA( right, -d, forward, right );
	VectorNormalize( right, right );
	CrossProduct( right, forward, up );
#else
	const vec3_t *index = g_vector3_axes_C[ vector3_min_abs_component_index_C( forward ) ];
	vector3_cross_C( *index, forward, right );
	VectorNormalize( right, right );
	CrossProduct( right, forward, up );
#endif
}

vec_t VectorLength( const vec3_t v ){
	int i;
	float length;

	length = 0.0f;
	for ( i = 0 ; i < 3 ; i++ )
		length += v[i] * v[i];
	length = (float)sqrt( length );

	return length;
}

qboolean VectorCompare( const vec3_t v1, const vec3_t v2 ){
	int i;

	for ( i = 0 ; i < 3 ; i++ )
		if ( fabs( v1[i] - v2[i] ) > EQUAL_EPSILON ) {
			return qfalse;
		}

	return qtrue;
}

void VectorMA( const vec3_t va, vec_t scale, const vec3_t vb, vec3_t vc ){
	vc[0] = va[0] + scale * vb[0];
	vc[1] = va[1] + scale * vb[1];
	vc[2] = va[2] + scale * vb[2];
}

void _CrossProduct( vec3_t v1, vec3_t v2, vec3_t cross ){
	cross[0] = v1[1] * v2[2] - v1[2] * v2[1];
	cross[1] = v1[2] * v2[0] - v1[0] * v2[2];
	cross[2] = v1[0] * v2[1] - v1[1] * v2[0];
}

vec_t _DotProduct( vec3_t v1, vec3_t v2 ){
	return v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2];
}

void _VectorSubtract( vec3_t va, vec3_t vb, vec3_t out ){
	out[0] = va[0] - vb[0];
	out[1] = va[1] - vb[1];
	out[2] = va[2] - vb[2];
}

void _VectorAdd( vec3_t va, vec3_t vb, vec3_t out ){
	out[0] = va[0] + vb[0];
	out[1] = va[1] + vb[1];
	out[2] = va[2] + vb[2];
}

void _VectorCopy( vec3_t in, vec3_t out ){
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
}

vec_t VectorAccurateNormalize( const vec3_t in, vec3_t out ) {

	// The sqrt() function takes double as an input and returns double as an
	// output according the the man pages on Debian and on FreeBSD.  Therefore,
	// I don't see a reason why using a double outright (instead of using the
	// vec_accu_t alias for example) could possibly be frowned upon.

	double x, y, z, length;

	x = (double) in[0];
	y = (double) in[1];
	z = (double) in[2];

	length = sqrt( ( x * x ) + ( y * y ) + ( z * z ) );
	if ( length == 0 ) {
		VectorClear( out );
		return 0;
	}

	out[0] = (vec_t) ( x / length );
	out[1] = (vec_t) ( y / length );
	out[2] = (vec_t) ( z / length );

	return (vec_t) length;
}

vec_t VectorFastNormalize_( const vec3_t in, vec3_t out ) {

	// SmileTheory: This is ioquake3's VectorNormalize2
	//              for when accuracy matters less than speed
	float length, ilength;

	length = in[0] * in[0] + in[1] * in[1] + in[2] * in[2];

	if ( length ) {
		/* writing it this way allows gcc to recognize that rsqrt can be used */
		ilength = 1 / (float)sqrt( length );
		/* sqrt(length) = length * (1 / sqrt(length)) */
		length *= ilength;
		out[0] = in[0] * ilength;
		out[1] = in[1] * ilength;
		out[2] = in[2] * ilength;
	}
	else {
		VectorClear( out );
	}

	return length;
}

vec_t ColorNormalize( const vec3_t in, vec3_t out ) {
	float max, scale;

	max = in[0];
	if ( in[1] > max ) {
		max = in[1];
	}
	if ( in[2] > max ) {
		max = in[2];
	}

	if ( max == 0 ) {
		out[0] = out[1] = out[2] = 1.0;
		return 0;
	}

	scale = 1.0f / max;

	VectorScale( in, scale, out );

	return max;
}

void VectorInverse( vec3_t v ){
	v[0] = -v[0];
	v[1] = -v[1];
	v[2] = -v[2];
}

/*
   void VectorScale (vec3_t v, vec_t scale, vec3_t out)
   {
    out[0] = v[0] * scale;
    out[1] = v[1] * scale;
    out[2] = v[2] * scale;
   }
 */

void VectorRotate( vec3_t vIn, vec3_t vRotation, vec3_t out ){
	vec3_t vWork, va;
	int nIndex[3][2];
	int i;

	VectorCopy( vIn, va );
	VectorCopy( va, vWork );
	nIndex[0][0] = 1; nIndex[0][1] = 2;
	nIndex[1][0] = 2; nIndex[1][1] = 0;
	nIndex[2][0] = 0; nIndex[2][1] = 1;

	for ( i = 0; i < 3; i++ )
	{
		if ( vRotation[i] != 0 ) {
			float dAngle = vRotation[i] * Q_PI / 180.0f;
			float c = (vec_t)cos( dAngle );
			float s = (vec_t)sin( dAngle );
			vWork[nIndex[i][0]] = va[nIndex[i][0]] * c - va[nIndex[i][1]] * s;
			vWork[nIndex[i][1]] = va[nIndex[i][0]] * s + va[nIndex[i][1]] * c;
		}
		VectorCopy( vWork, va );
	}
	VectorCopy( vWork, out );
}

void VectorRotateOrigin( vec3_t vIn, vec3_t vRotation, vec3_t vOrigin, vec3_t out ){
	vec3_t vTemp, vTemp2;

	VectorSubtract( vIn, vOrigin, vTemp );
	VectorRotate( vTemp, vRotation, vTemp2 );
	VectorAdd( vTemp2, vOrigin, out );
}

void VectorPolar( vec3_t v, float radius, float theta, float phi ){
	v[0] = (float)( radius * cos( theta ) * cos( phi ) );
	v[1] = (float)( radius * sin( theta ) * cos( phi ) );
	v[2] = (float)( radius * sin( phi ) );
}

void VectorSnap( vec3_t v ){
	int i;
	for ( i = 0; i < 3; i++ )
	{
		v[i] = (vec_t)FLOAT_TO_INTEGER( v[i] );
	}
}

void VectorISnap( vec3_t point, int snap ){
	int i;
	for ( i = 0 ; i < 3 ; i++ )
	{
		point[i] = (vec_t)FLOAT_SNAP( point[i], snap );
	}
}

void VectorFSnap( vec3_t point, float snap ){
	int i;
	for ( i = 0 ; i < 3 ; i++ )
	{
		point[i] = (vec_t)FLOAT_SNAP( point[i], snap );
	}
}

void _Vector5Add( vec5_t va, vec5_t vb, vec5_t out ){
	out[0] = va[0] + vb[0];
	out[1] = va[1] + vb[1];
	out[2] = va[2] + vb[2];
	out[3] = va[3] + vb[3];
	out[4] = va[4] + vb[4];
}

void _Vector5Scale( vec5_t v, vec_t scale, vec5_t out ){
	out[0] = v[0] * scale;
	out[1] = v[1] * scale;
	out[2] = v[2] * scale;
	out[3] = v[3] * scale;
	out[4] = v[4] * scale;
}

void _Vector53Copy( vec5_t in, vec3_t out ){
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
}

// NOTE: added these from Ritual's Q3Radiant
const int INVALID_BOUNDS = 99999;
void ClearBounds( vec3_t mins, vec3_t maxs ){
	mins[0] = mins[1] = mins[2] = +INVALID_BOUNDS;
	maxs[0] = maxs[1] = maxs[2] = -INVALID_BOUNDS;
}

void AddPointToBounds( vec3_t v, vec3_t mins, vec3_t maxs ){
	int i;
	vec_t val;

	if ( mins[0] == +INVALID_BOUNDS ) {
		if ( maxs[0] == -INVALID_BOUNDS ) {
			VectorCopy( v, mins );
			VectorCopy( v, maxs );
		}
	}

	for ( i = 0 ; i < 3 ; i++ )
	{
		val = v[i];
		if ( val < mins[i] ) {
			mins[i] = val;
		}
		if ( val > maxs[i] ) {
			maxs[i] = val;
		}
	}
}

void AngleVectors( vec3_t angles, vec3_t forward, vec3_t right, vec3_t up ){
	float angle;
	static float sr, sp, sy, cr, cp, cy;
	// static to help MS compiler fp bugs

	angle = angles[YAW] * ( Q_PI * 2.0f / 360.0f );
	sy = (vec_t)sin( angle );
	cy = (vec_t)cos( angle );
	angle = angles[PITCH] * ( Q_PI * 2.0f / 360.0f );
	sp = (vec_t)sin( angle );
	cp = (vec_t)cos( angle );
	angle = angles[ROLL] * ( Q_PI * 2.0f / 360.0f );
	sr = (vec_t)sin( angle );
	cr = (vec_t)cos( angle );

	if ( forward ) {
		forward[0] = cp * cy;
		forward[1] = cp * sy;
		forward[2] = -sp;
	}
	if ( right ) {
		right[0] = -sr * sp * cy + cr * sy;
		right[1] = -sr * sp * sy - cr * cy;
		right[2] = -sr * cp;
	}
	if ( up ) {
		up[0] = cr * sp * cy + sr * sy;
		up[1] = cr * sp * sy - sr * cy;
		up[2] = cr * cp;
	}
}

void VectorToAngles( vec3_t vec, vec3_t angles ){
	float forward;
	float yaw, pitch;

	if ( ( vec[ 0 ] == 0 ) && ( vec[ 1 ] == 0 ) ) {
		yaw = 0;
		if ( vec[ 2 ] > 0 ) {
			pitch = 90;
		}
		else
		{
			pitch = 270;
		}
	}
	else
	{
		yaw = (vec_t)atan2( vec[ 1 ], vec[ 0 ] ) * 180 / Q_PI;
		if ( yaw < 0 ) {
			yaw += 360;
		}

		forward = ( float )sqrt( vec[ 0 ] * vec[ 0 ] + vec[ 1 ] * vec[ 1 ] );
		pitch = (vec_t)atan2( vec[ 2 ], forward ) * 180 / Q_PI;
		if ( pitch < 0 ) {
			pitch += 360;
		}
	}

	angles[ 0 ] = pitch;
	angles[ 1 ] = yaw;
	angles[ 2 ] = 0;
}

/*
   =====================
   PlaneFromPoints

   Returns false if the triangle is degenrate.
   The normal will point out of the clock for clockwise ordered points
   =====================
 */
qboolean PlaneFromPoints( vec4_t plane, const vec3_t a, const vec3_t b, const vec3_t c ) {
	vec3_t d1, d2;

	VectorSubtract( b, a, d1 );
	VectorSubtract( c, a, d2 );
	CrossProduct( d2, d1, plane );
	if ( VectorNormalize( plane, plane ) == 0 ) {
		return qfalse;
	}

	plane[3] = DotProduct( a, plane );
	return qtrue;
}

/*
** NormalToLatLong
**
** We use two byte encoded normals in some space critical applications.
** Lat = 0 at (1,0,0) to 360 (-1,0,0), encoded in 8-bit sine table format
** Lng = 0 at (0,0,1) to 180 (0,0,-1), encoded in 8-bit sine table format
**
*/
void NormalToLatLong( const vec3_t normal, byte bytes[2] ) {
	// check for singularities
	if ( normal[0] == 0 && normal[1] == 0 ) {
		if ( normal[2] > 0 ) {
			bytes[0] = 0;
			bytes[1] = 0;       // lat = 0, long = 0
		}
		else {
			bytes[0] = 128;
			bytes[1] = 0;       // lat = 0, long = 128
		}
	}
	else {
		int a, b;

		a = (int)( RAD2DEG( atan2( normal[1], normal[0] ) ) * ( 255.0f / 360.0f ) );
		a &= 0xff;

		b = (int)( RAD2DEG( acos( normal[2] ) ) * ( 255.0f / 360.0f ) );
		b &= 0xff;

		bytes[0] = b;   // longitude
		bytes[1] = a;   // lattitude
	}
}

/*
   =================
   PlaneTypeForNormal
   =================
 */
int PlaneTypeForNormal( vec3_t normal ) {
	if ( normal[0] == 1.0 || normal[0] == -1.0 ) {
		return PLANE_X;
	}
	if ( normal[1] == 1.0 || normal[1] == -1.0 ) {
		return PLANE_Y;
	}
	if ( normal[2] == 1.0 || normal[2] == -1.0 ) {
		return PLANE_Z;
	}

	return PLANE_NON_AXIAL;
}

/*
   ================
   MatrixMultiply
   ================
 */
void MatrixMultiply( float in1[3][3], float in2[3][3], float out[3][3] ) {
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] +
				in1[0][2] * in2[2][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] +
				in1[0][2] * in2[2][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] +
				in1[0][2] * in2[2][2];
	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] +
				in1[1][2] * in2[2][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] +
				in1[1][2] * in2[2][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] +
				in1[1][2] * in2[2][2];
	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] +
				in1[2][2] * in2[2][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] +
				in1[2][2] * in2[2][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] +
				in1[2][2] * in2[2][2];
}

void ProjectPointOnPlane( vec3_t dst, const vec3_t p, const vec3_t normal ){
	float d;
	vec3_t n;
	float inv_denom;

	inv_denom = 1.0F / DotProduct( normal, normal );

	d = DotProduct( normal, p ) * inv_denom;

	n[0] = normal[0] * inv_denom;
	n[1] = normal[1] * inv_denom;
	n[2] = normal[2] * inv_denom;

	dst[0] = p[0] - d * n[0];
	dst[1] = p[1] - d * n[1];
	dst[2] = p[2] - d * n[2];
}

/*
** assumes "src" is normalized
*/
void PerpendicularVector( vec3_t dst, const vec3_t src ){
	int pos;
	int i;
	vec_t minelem = 1.0F;
	vec3_t tempvec;

	/*
	** find the smallest magnitude axially aligned vector
	*/
	for ( pos = 0, i = 0; i < 3; i++ )
	{
		if ( fabs( src[i] ) < minelem ) {
			pos = i;
			minelem = (vec_t)fabs( src[i] );
		}
	}
	tempvec[0] = tempvec[1] = tempvec[2] = 0.0F;
	tempvec[pos] = 1.0F;

	/*
	** project the point onto the plane defined by src
	*/
	ProjectPointOnPlane( dst, tempvec, src );

	/*
	** normalize the result
	*/
	VectorNormalize( dst, dst );
}

/*
   ===============
   RotatePointAroundVector

   This is not implemented very well...
   ===============
 */
void RotatePointAroundVector( vec3_t dst, const vec3_t dir, const vec3_t point,
							  float degrees ) {
	float m[3][3];
	float im[3][3];
	float zrot[3][3];
	float tmpmat[3][3];
	float rot[3][3];
	int i;
	vec3_t vr, vup, vf;
	float rad;

	vf[0] = dir[0];
	vf[1] = dir[1];
	vf[2] = dir[2];

	PerpendicularVector( vr, dir );
	CrossProduct( vr, vf, vup );

	m[0][0] = vr[0];
	m[1][0] = vr[1];
	m[2][0] = vr[2];

	m[0][1] = vup[0];
	m[1][1] = vup[1];
	m[2][1] = vup[2];

	m[0][2] = vf[0];
	m[1][2] = vf[1];
	m[2][2] = vf[2];

	memcpy( im, m, sizeof( im ) );

	im[0][1] = m[1][0];
	im[0][2] = m[2][0];
	im[1][0] = m[0][1];
	im[1][2] = m[2][1];
	im[2][0] = m[0][2];
	im[2][1] = m[1][2];

	memset( zrot, 0, sizeof( zrot ) );
	zrot[0][0] = zrot[1][1] = zrot[2][2] = 1.0F;

	rad = (float)DEG2RAD( degrees );
	zrot[0][0] = (vec_t)cos( rad );
	zrot[0][1] = (vec_t)sin( rad );
	zrot[1][0] = (vec_t)-sin( rad );
	zrot[1][1] = (vec_t)cos( rad );

	MatrixMultiply( m, zrot, tmpmat );
	MatrixMultiply( tmpmat, im, rot );

	for ( i = 0; i < 3; i++ ) {
		dst[i] = rot[i][0] * point[0] + rot[i][1] * point[1] + rot[i][2] * point[2];
	}
}


////////////////////////////////////////////////////////////////////////////////
// Below is double-precision math stuff.  This was initially needed by the new
// "base winding" code in q3map2 brush processing in order to fix the famous
// "disappearing triangles" issue.  These definitions can be used wherever extra
// precision is needed.
////////////////////////////////////////////////////////////////////////////////

/*
   =================
   VectorLengthAccu
   =================
 */
vec_accu_t VectorLengthAccu( const vec3_accu_t v ){
	return (vec_accu_t) sqrt( ( v[0] * v[0] ) + ( v[1] * v[1] ) + ( v[2] * v[2] ) );
}

/*
   =================
   DotProductAccu
   =================
 */
vec_accu_t DotProductAccu( const vec3_accu_t a, const vec3_accu_t b ){
	return ( a[0] * b[0] ) + ( a[1] * b[1] ) + ( a[2] * b[2] );
}

/*
   =================
   VectorSubtractAccu
   =================
 */
void VectorSubtractAccu( const vec3_accu_t a, const vec3_accu_t b, vec3_accu_t out ){
	out[0] = a[0] - b[0];
	out[1] = a[1] - b[1];
	out[2] = a[2] - b[2];
}

/*
   =================
   VectorAddAccu
   =================
 */
void VectorAddAccu( const vec3_accu_t a, const vec3_accu_t b, vec3_accu_t out ){
	out[0] = a[0] + b[0];
	out[1] = a[1] + b[1];
	out[2] = a[2] + b[2];
}

/*
   =================
   VectorCopyAccu
   =================
 */
void VectorCopyAccu( const vec3_accu_t in, vec3_accu_t out ){
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
}

/*
   =================
   VectorScaleAccu
   =================
 */
void VectorScaleAccu( const vec3_accu_t in, vec_accu_t scaleFactor, vec3_accu_t out ){
	out[0] = in[0] * scaleFactor;
	out[1] = in[1] * scaleFactor;
	out[2] = in[2] * scaleFactor;
}

/*
   =================
   CrossProductAccu
   =================
 */
void CrossProductAccu( const vec3_accu_t a, const vec3_accu_t b, vec3_accu_t out ){
	out[0] = ( a[1] * b[2] ) - ( a[2] * b[1] );
	out[1] = ( a[2] * b[0] ) - ( a[0] * b[2] );
	out[2] = ( a[0] * b[1] ) - ( a[1] * b[0] );
}

/*
   =================
   Q_rintAccu
   =================
 */
vec_accu_t Q_rintAccu( vec_accu_t val ){
	return (vec_accu_t) floor( val + 0.5 );
}

/*
   =================
   VectorCopyAccuToRegular
   =================
 */
void VectorCopyAccuToRegular( const vec3_accu_t in, vec3_t out ){
	out[0] = (vec_t) in[0];
	out[1] = (vec_t) in[1];
	out[2] = (vec_t) in[2];
}

/*
   =================
   VectorCopyRegularToAccu
   =================
 */
void VectorCopyRegularToAccu( const vec3_t in, vec3_accu_t out ){
	out[0] = (vec_accu_t) in[0];
	out[1] = (vec_accu_t) in[1];
	out[2] = (vec_accu_t) in[2];
}

/*
   =================
   VectorNormalizeAccu
   =================
 */
vec_accu_t VectorNormalizeAccu( const vec3_accu_t in, vec3_accu_t out ){
	// The sqrt() function takes double as an input and returns double as an
	// output according the the man pages on Debian and on FreeBSD.  Therefore,
	// I don't see a reason why using a double outright (instead of using the
	// vec_accu_t alias for example) could possibly be frowned upon.

	vec_accu_t length;

	length = (vec_accu_t) sqrt( ( in[0] * in[0] ) + ( in[1] * in[1] ) + ( in[2] * in[2] ) );
	if ( length == 0 ) {
		VectorClear( out );
		return 0;
	}

	out[0] = in[0] / length;
	out[1] = in[1] / length;
	out[2] = in[2] / length;

	return length;
}
