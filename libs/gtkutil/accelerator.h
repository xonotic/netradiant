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

#if !defined( INCLUDED_GTKUTIL_ACCELERATOR_H )
#define INCLUDED_GTKUTIL_ACCELERATOR_H

#include <uilib/uilib.h>
#include <gdk/gdk.h>

#include "generic/callback.h"
#include "property.h"
#include "globaldefs.h"

// ignore numlock
#define ALLOWED_MODIFIERS ( ~( GDK_MOD2_MASK | GDK_LOCK_MASK | GDK_MOD3_MASK | GDK_MOD4_MASK | GDK_MOD5_MASK ) )

// On macOS, remap Command (Meta) to Control so that native
// macOS shortcuts (Cmd+Z, Cmd+C, etc.) work like Ctrl+ on Linux/Windows.
inline GdkModifierType normalize_modifiers( GdkModifierType modifiers ){
#if GDEF_OS_MACOS
#ifdef GDK_META_MASK
	if ( modifiers & GDK_META_MASK ) {
		modifiers = (GdkModifierType)( ( modifiers & ~GDK_META_MASK ) | GDK_CONTROL_MASK );
	}
#endif
#endif
	return modifiers;
}

struct Accelerator
{
	Accelerator( guint _key )
		: key( gdk_keyval_to_upper( _key ) ), modifiers( ( GdkModifierType ) 0 ){
	}
	Accelerator( guint _key, GdkModifierType _modifiers )
		: key( gdk_keyval_to_upper( _key ) ), modifiers( ( GdkModifierType )( _modifiers & ALLOWED_MODIFIERS ) ){
	}
	Accelerator( const Accelerator &src )
		: key( gdk_keyval_to_upper( src.key ) ), modifiers( ( GdkModifierType )( src.modifiers & ALLOWED_MODIFIERS ) ){
	}
	bool operator<( const Accelerator& other ) const {
		guint k1 = key;
		guint k2 = other.key;
		int mod1 = modifiers & ALLOWED_MODIFIERS;
		int mod2 = other.modifiers & ALLOWED_MODIFIERS;
		return k1 < k2 || ( !( k2 < k1 ) && mod1 < mod2 );
	}
	bool operator==( const Accelerator& other ) const {
		guint k1 = key;
		guint k2 = other.key;
		int mod1 = modifiers & ALLOWED_MODIFIERS;
		int mod2 = other.modifiers & ALLOWED_MODIFIERS;
		return k1 == k2 && mod1 == mod2;
	}
	Accelerator &operator=( const Accelerator& other ){
		key = other.key;
		modifiers = (GdkModifierType) ( other.modifiers & ALLOWED_MODIFIERS );
		return *this;
	}
	guint key;
	GdkModifierType modifiers;
};

inline Accelerator accelerator_null(){
	return Accelerator( 0, (GdkModifierType)0 );
}

const char* global_keys_find( unsigned int key );
unsigned int global_keys_find( const char* name );

class TextOutputStream;
void accelerator_write( const Accelerator& accelerator, TextOutputStream& ostream );

template<typename TextOutputStreamType>
TextOutputStreamType& ostream_write( TextOutputStreamType& ostream, const Accelerator& accelerator ){
	accelerator_write( accelerator, ostream );
	return ostream;
}

void keydown_accelerators_add( Accelerator accelerator, const Callback<void()>& callback );
void keydown_accelerators_remove( Accelerator accelerator );
void keyup_accelerators_add( Accelerator accelerator, const Callback<void()>& callback );
void keyup_accelerators_remove( Accelerator accelerator );

void global_accel_connect_window( ui::Window window );
void global_accel_disconnect_window( ui::Window window );

void GlobalPressedKeys_releaseAll();

extern ui::AccelGroup global_accel;

GClosure* global_accel_group_find( Accelerator accelerator );

void global_accel_group_connect( const Accelerator& accelerator, const Callback<void()>& callback );
void global_accel_group_disconnect( const Accelerator& accelerator, const Callback<void()>& callback );


class Command
{
public:
Callback<void()> m_callback;
const Accelerator& m_accelerator;
Command( const Callback<void()>& callback, const Accelerator& accelerator ) : m_callback( callback ), m_accelerator( accelerator ){
}
};

class Toggle
{
public:
Command m_command;
Callback<void(const Callback<void(bool)> &)> m_exportCallback;
Toggle( const Callback<void()>& callback, const Accelerator& accelerator, const Callback<void(const Callback<void(bool)> &)>& exportCallback ) : m_command( callback, accelerator ), m_exportCallback( exportCallback ){
}
};

class KeyEvent
{
public:
const Accelerator& m_accelerator;
Callback<void()> m_keyDown;
Callback<void()> m_keyUp;
KeyEvent( const Accelerator& accelerator, const Callback<void()>& keyDown, const Callback<void()>& keyUp ) : m_accelerator( accelerator ), m_keyDown( keyDown ), m_keyUp( keyUp ){
}
};



struct PressedButtons;
void PressedButtons_connect( PressedButtons& pressedButtons, ui::Widget widget );

extern PressedButtons g_pressedButtons;

#endif
