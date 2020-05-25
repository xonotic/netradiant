/*
   Copyright (C) 2001-2006, William Joseph.
   All Rights Reserved.

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

#if !defined( INCLUDED_GTKUTIL_CURSOR_H )
#define INCLUDED_GTKUTIL_CURSOR_H

#include <uilib/uilib.h>

#include "debugging/debugging.h"

typedef struct _GdkCursor GdkCursor;
typedef struct _GdkEventMotion GdkEventMotion;

#if 0
GdkCursor* create_blank_cursor();
void blank_cursor( ui::Widget widget );
void default_cursor( ui::Widget widget );
#endif
void Sys_GetCursorPos( ui::Window window, int *x, int *y );
void Sys_SetCursorPos( ui::Window window, int x, int y );



class DeferredMotion
{
guint m_handler;
typedef void ( *MotionFunction )( gdouble x, gdouble y, guint state, void* data );
MotionFunction m_function;
void* m_data;
gdouble m_x;
gdouble m_y;
guint m_state;

static gboolean deferred( DeferredMotion* self ){
	self->m_handler = 0;
	self->m_function( self->m_x, self->m_y, self->m_state, self->m_data );
	return FALSE;
}
public:
DeferredMotion( MotionFunction function, void* data ) : m_handler( 0 ), m_function( function ), m_data( data ){
}
void motion( gdouble x, gdouble y, guint state ){
	m_x = x;
	m_y = y;
	m_state = state;
	if ( m_handler == 0 ) {
		m_handler = g_idle_add( (GSourceFunc)deferred, this );
	}
}
static gboolean gtk_motion( ui::Widget widget, GdkEventMotion *event, DeferredMotion* self );
};

class DeferredMotionDelta
{
int m_delta_x;
int m_delta_y;
guint m_motion_handler;
typedef void ( *MotionDeltaFunction )( int x, int y, void* data );
MotionDeltaFunction m_function;
void* m_data;

static gboolean deferred_motion( gpointer data ){
	reinterpret_cast<DeferredMotionDelta*>( data )->m_function(
		reinterpret_cast<DeferredMotionDelta*>( data )->m_delta_x,
		reinterpret_cast<DeferredMotionDelta*>( data )->m_delta_y,
		reinterpret_cast<DeferredMotionDelta*>( data )->m_data
		);
	reinterpret_cast<DeferredMotionDelta*>( data )->m_motion_handler = 0;
	reinterpret_cast<DeferredMotionDelta*>( data )->m_delta_x = 0;
	reinterpret_cast<DeferredMotionDelta*>( data )->m_delta_y = 0;
	return FALSE;
}
public:
DeferredMotionDelta( MotionDeltaFunction function, void* data ) : m_delta_x( 0 ), m_delta_y( 0 ), m_motion_handler( 0 ), m_function( function ), m_data( data ){
}
void flush(){
	if ( m_motion_handler != 0 ) {
		g_source_remove( m_motion_handler );
		deferred_motion( this );
	}
}
void motion_delta( int x, int y, unsigned int state ){
	m_delta_x += x;
	m_delta_y += y;
	if ( m_motion_handler == 0 ) {
		m_motion_handler = g_idle_add( deferred_motion, this );
	}
}
};

class FreezePointer
{
unsigned int handle_motion;
int recorded_x, recorded_y, last_x, last_y, center_x, center_y;
ui::Widget m_weedjet{ui::null};
typedef void ( *MotionDeltaFunction )( int x, int y, unsigned int state, void* data );
MotionDeltaFunction m_function;
void* m_data;
public:
FreezePointer() : handle_motion( 0 ), m_function( 0 ), m_data( 0 ){
}
static gboolean motion_delta( ui::Window widget, GdkEventMotion *event, FreezePointer* self );

void freeze_pointer( ui::Window window, ui::Widget widget, MotionDeltaFunction function, void* data );

void unfreeze_pointer( ui::Window window, bool centerize );
};

#endif
