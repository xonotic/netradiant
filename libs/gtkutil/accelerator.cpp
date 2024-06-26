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

#include "accelerator.h"

#include "debugging/debugging.h"

#include <map>
#include <set>
#include <gtk/gtk.h>

#include "generic/callback.h"
#include "generic/bitfield.h"
#include "string/string.h"

#include "pointer.h"
#include "closure.h"

#include <gdk/gdkkeysyms.h>



const char* global_keys_find( unsigned int key ){
	const char *s;
	if ( key == 0 ) {
		return "";
	}
	s = gdk_keyval_name( key );
	if ( !s ) {
		return "";
	}
	return s;
}

unsigned int global_keys_find( const char* name ){
	guint k;
	if ( !name || !*name ) {
		return 0;
	}
	k = gdk_keyval_from_name( name );
	if ( k == GDK_KEY_VoidSymbol ) {
		return 0;
	}
	return k;
}

void accelerator_write( const Accelerator& accelerator, TextOutputStream& ostream ){
#if 0
	if ( accelerator.modifiers & GDK_SHIFT_MASK ) {
		ostream << "Shift + ";
	}
	if ( accelerator.modifiers & GDK_MOD1_MASK ) {
		ostream << "Alt + ";
	}
	if ( accelerator.modifiers & GDK_CONTROL_MASK ) {
		ostream << "Control + ";
	}

	const char* keyName = global_keys_find( accelerator.key );
	if ( !string_empty( keyName ) ) {
		ostream << keyName;
	}
	else
	{
		ostream << static_cast<char>( accelerator.key );
	}
#endif
	ostream << gtk_accelerator_get_label( accelerator.key, accelerator.modifiers );
}

typedef std::map<Accelerator, Callback<void()>> AcceleratorMap;
typedef std::set<Accelerator> AcceleratorSet;

bool accelerator_map_insert( AcceleratorMap& acceleratorMap, Accelerator accelerator, const Callback<void()>& callback ){
	if ( accelerator.key != 0 ) {
		return acceleratorMap.insert( AcceleratorMap::value_type( accelerator, callback ) ).second;
	}
	return true;
}

bool accelerator_map_erase( AcceleratorMap& acceleratorMap, Accelerator accelerator ){
	if ( accelerator.key != 0 ) {
		AcceleratorMap::iterator i = acceleratorMap.find( accelerator );
		if ( i == acceleratorMap.end() ) {
			return false;
		}
		acceleratorMap.erase( i );
	}
	return true;
}

Accelerator accelerator_for_event_key( guint keyval, guint state ){
	keyval = gdk_keyval_to_upper( keyval );
	if ( keyval == GDK_KEY_ISO_Left_Tab ) {
		keyval = GDK_KEY_Tab;
	}
	return Accelerator( keyval, (GdkModifierType)( state & gtk_accelerator_get_default_mod_mask() ) );
}

bool AcceleratorMap_activate( const AcceleratorMap& acceleratorMap, const Accelerator& accelerator ){
	AcceleratorMap::const_iterator i = acceleratorMap.find( accelerator );
	if ( i != acceleratorMap.end() ) {
		( *i ).second();
		return true;
	}

	return false;
}

static gboolean accelerator_key_event( ui::Window window, GdkEventKey* event, AcceleratorMap* acceleratorMap ){
	return AcceleratorMap_activate( *acceleratorMap, accelerator_for_event_key( event->keyval, event->state ) );
}


AcceleratorMap g_special_accelerators;


namespace MouseButton
{
enum
{
	Left = 1 << 0,
	Right = 1 << 1,
	Middle = 1 << 2,
};
}

typedef unsigned int ButtonMask;

void print_buttons( ButtonMask mask ){
	globalOutputStream() << "button state: ";
	if ( ( mask & MouseButton::Left ) != 0 ) {
		globalOutputStream() << "Left ";
	}
	if ( ( mask & MouseButton::Right ) != 0 ) {
		globalOutputStream() << "Right ";
	}
	if ( ( mask & MouseButton::Middle ) != 0 ) {
		globalOutputStream() << "Middle ";
	}
	globalOutputStream() << "\n";
}

ButtonMask ButtonMask_for_event_button( guint button ){
	switch ( button )
	{
	case 1:
		return MouseButton::Left;
	case 2:
		return MouseButton::Middle;
	case 3:
		return MouseButton::Right;
	}
	return 0;
}

bool window_has_accel( ui::Window toplevel ){
	return g_slist_length( gtk_accel_groups_from_object( G_OBJECT( toplevel ) ) ) != 0;
}

namespace
{
bool g_accel_enabled = true;
}

bool global_accel_enabled(){
	return g_accel_enabled;
}


GClosure* accel_group_add_accelerator(ui::AccelGroup group, Accelerator accelerator, const Callback<void()>& callback );
void accel_group_remove_accelerator(ui::AccelGroup group, Accelerator accelerator );

AcceleratorMap g_queuedAcceleratorsAdd;
AcceleratorSet g_queuedAcceleratorsRemove;

void globalQueuedAccelerators_add( Accelerator accelerator, const Callback<void()>& callback ){
	if ( !g_queuedAcceleratorsAdd.insert( AcceleratorMap::value_type( accelerator, callback ) ).second ) {
		globalErrorStream() << "globalQueuedAccelerators_add: accelerator already queued: " << accelerator << "\n";
	}
}

void globalQueuedAccelerators_remove( Accelerator accelerator ){
	if ( g_queuedAcceleratorsAdd.erase( accelerator ) == 0 ) {
		if ( !g_queuedAcceleratorsRemove.insert( accelerator ).second ) {
			globalErrorStream() << "globalQueuedAccelerators_remove: accelerator already queued: " << accelerator << "\n";
		}
	}
}

void globalQueuedAccelerators_commit(){
	for ( AcceleratorSet::const_iterator i = g_queuedAcceleratorsRemove.begin(); i != g_queuedAcceleratorsRemove.end(); ++i )
	{
		//globalOutputStream() << "removing: " << (*i).first << "\n";
		accel_group_remove_accelerator( global_accel, *i );
	}
	g_queuedAcceleratorsRemove.clear();
	for ( AcceleratorMap::const_iterator i = g_queuedAcceleratorsAdd.begin(); i != g_queuedAcceleratorsAdd.end(); ++i )
	{
		//globalOutputStream() << "adding: " << (*i).first << "\n";
		accel_group_add_accelerator( global_accel, ( *i ).first, ( *i ).second );
	}
	g_queuedAcceleratorsAdd.clear();
}

typedef std::set<ui::Window> WindowSet;
WindowSet g_accel_windows;

bool Buttons_press( ButtonMask& buttons, guint button, guint state ){
	if ( buttons == 0 && bitfield_enable( buttons, ButtonMask_for_event_button( button ) ) != 0 ) {
		ASSERT_MESSAGE( g_accel_enabled, "Buttons_press: accelerators not enabled" );
		g_accel_enabled = false;
		for ( WindowSet::iterator i = g_accel_windows.begin(); i != g_accel_windows.end(); ++i )
		{
			ui::Window toplevel = *i;
			ASSERT_MESSAGE( window_has_accel( toplevel ), "ERROR" );
			ASSERT_MESSAGE( gtk_widget_is_toplevel( toplevel ), "disabling accel for non-toplevel window" );
			gtk_window_remove_accel_group( toplevel,  global_accel );
#if 0
			globalOutputStream() << reinterpret_cast<unsigned int>( toplevel ) << ": disabled global accelerators\n";
#endif
		}
	}
	buttons = bitfield_enable( buttons, ButtonMask_for_event_button( button ) );
#if 0
	globalOutputStream() << "Buttons_press: ";
	print_buttons( buttons );
#endif
	return false;
}

bool Buttons_release( ButtonMask& buttons, guint button, guint state ){
	if ( buttons != 0 && bitfield_disable( buttons, ButtonMask_for_event_button( button ) ) == 0 ) {
		ASSERT_MESSAGE( !g_accel_enabled, "Buttons_release: accelerators are enabled" );
		g_accel_enabled = true;
		for ( WindowSet::iterator i = g_accel_windows.begin(); i != g_accel_windows.end(); ++i )
		{
			ui::Window toplevel = *i;
			ASSERT_MESSAGE( !window_has_accel( toplevel ), "ERROR" );
			ASSERT_MESSAGE( gtk_widget_is_toplevel( toplevel ), "enabling accel for non-toplevel window" );
			toplevel.add_accel_group( global_accel );
#if 0
			globalOutputStream() << reinterpret_cast<unsigned int>( toplevel ) << ": enabled global accelerators\n";
#endif
		}
		globalQueuedAccelerators_commit();
	}
	buttons = bitfield_disable( buttons, ButtonMask_for_event_button( button ) );
#if 0
	globalOutputStream() << "Buttons_release: ";
	print_buttons( buttons );
#endif
	return false;
}

bool Buttons_releaseAll( ButtonMask& buttons ){
	Buttons_release( buttons, MouseButton::Left | MouseButton::Middle | MouseButton::Right, 0 );
	return false;
}

struct PressedButtons
{
	ButtonMask buttons;

	PressedButtons() : buttons( 0 ){
	}
};

gboolean PressedButtons_button_press(ui::Widget widget, GdkEventButton* event, PressedButtons* pressed ){
	if ( event->type == GDK_BUTTON_PRESS ) {
		return Buttons_press( pressed->buttons, event->button, event->state );
	}
	return FALSE;
}

gboolean PressedButtons_button_release(ui::Widget widget, GdkEventButton* event, PressedButtons* pressed ){
	if ( event->type == GDK_BUTTON_RELEASE ) {
		return Buttons_release( pressed->buttons, event->button, event->state );
	}
	return FALSE;
}

gboolean PressedButtons_focus_out(ui::Widget widget, GdkEventFocus* event, PressedButtons* pressed ){
	Buttons_releaseAll( pressed->buttons );
	return FALSE;
}

void PressedButtons_connect( PressedButtons& pressedButtons, ui::Widget widget ){
	widget.connect( "button_press_event", G_CALLBACK( PressedButtons_button_press ), &pressedButtons );
	widget.connect( "button_release_event", G_CALLBACK( PressedButtons_button_release ), &pressedButtons );
	widget.connect( "focus_out_event", G_CALLBACK( PressedButtons_focus_out ), &pressedButtons );
}

PressedButtons g_pressedButtons;


#include <set>
#include <uilib/uilib.h>

struct PressedKeys
{
	typedef std::set<guint> Keys;
	Keys keys;
	std::size_t refcount;

	PressedKeys() : refcount( 0 ){
	}
};

AcceleratorMap g_keydown_accelerators;
AcceleratorMap g_keyup_accelerators;

bool Keys_press( PressedKeys::Keys& keys, guint keyval ){
	if ( keys.insert( gdk_keyval_to_upper( keyval ) ).second ) {
		return AcceleratorMap_activate( g_keydown_accelerators, accelerator_for_event_key( keyval, 0 ) );
	}
	return g_keydown_accelerators.find( accelerator_for_event_key( keyval, 0 ) ) != g_keydown_accelerators.end();
}

bool Keys_release( PressedKeys::Keys& keys, guint keyval ){
	if ( keys.erase( gdk_keyval_to_upper( keyval ) ) != 0 ) {
		return AcceleratorMap_activate( g_keyup_accelerators, accelerator_for_event_key( keyval, 0 ) );
	}
	return g_keyup_accelerators.find( accelerator_for_event_key( keyval, 0 ) ) != g_keyup_accelerators.end();
}

void Keys_releaseAll( PressedKeys::Keys& keys, guint state ){
	for ( PressedKeys::Keys::iterator i = keys.begin(); i != keys.end(); ++i )
	{
		AcceleratorMap_activate( g_keyup_accelerators, accelerator_for_event_key( *i, state ) );
	}
	keys.clear();
}

gboolean PressedKeys_key_press(ui::Widget widget, GdkEventKey* event, PressedKeys* pressedKeys ){
	//globalOutputStream() << "pressed: " << event->keyval << "\n";
	//return event->state == 0 && Keys_press( pressedKeys->keys, event->keyval );
	//NumLock perspective window fix
	return ( event->state & ALLOWED_MODIFIERS ) == 0 && Keys_press( pressedKeys->keys, event->keyval );
}

gboolean PressedKeys_key_release(ui::Widget widget, GdkEventKey* event, PressedKeys* pressedKeys ){
	//globalOutputStream() << "released: " << event->keyval << "\n";
	return Keys_release( pressedKeys->keys, event->keyval );
}

gboolean PressedKeys_focus_in(ui::Widget widget, GdkEventFocus* event, PressedKeys* pressedKeys ){
	++pressedKeys->refcount;
	return FALSE;
}

gboolean PressedKeys_focus_out(ui::Widget widget, GdkEventFocus* event, PressedKeys* pressedKeys ){
	if ( --pressedKeys->refcount == 0 ) {
		Keys_releaseAll( pressedKeys->keys, 0 );
	}
	return FALSE;
}

PressedKeys g_pressedKeys;

void GlobalPressedKeys_releaseAll(){
	Keys_releaseAll( g_pressedKeys.keys, 0 );
}

void GlobalPressedKeys_connect( ui::Window window ){
	unsigned int key_press_handler = window.connect( "key_press_event", G_CALLBACK( PressedKeys_key_press ), &g_pressedKeys );
	unsigned int key_release_handler = window.connect( "key_release_event", G_CALLBACK( PressedKeys_key_release ), &g_pressedKeys );
	g_object_set_data( G_OBJECT( window ), "key_press_handler", gint_to_pointer( key_press_handler ) );
	g_object_set_data( G_OBJECT( window ), "key_release_handler", gint_to_pointer( key_release_handler ) );
	unsigned int focus_in_handler = window.connect( "focus_in_event", G_CALLBACK( PressedKeys_focus_in ), &g_pressedKeys );
	unsigned int focus_out_handler = window.connect( "focus_out_event", G_CALLBACK( PressedKeys_focus_out ), &g_pressedKeys );
	g_object_set_data( G_OBJECT( window ), "focus_in_handler", gint_to_pointer( focus_in_handler ) );
	g_object_set_data( G_OBJECT( window ), "focus_out_handler", gint_to_pointer( focus_out_handler ) );
}

void GlobalPressedKeys_disconnect( ui::Window window ){
	g_signal_handler_disconnect( G_OBJECT( window ), gpointer_to_int( g_object_get_data( G_OBJECT( window ), "key_press_handler" ) ) );
	g_signal_handler_disconnect( G_OBJECT( window ), gpointer_to_int( g_object_get_data( G_OBJECT( window ), "key_release_handler" ) ) );
	g_signal_handler_disconnect( G_OBJECT( window ), gpointer_to_int( g_object_get_data( G_OBJECT( window ), "focus_in_handler" ) ) );
	g_signal_handler_disconnect( G_OBJECT( window ), gpointer_to_int( g_object_get_data( G_OBJECT( window ), "focus_out_handler" ) ) );
}



void special_accelerators_add( Accelerator accelerator, const Callback<void()>& callback ){
	//globalOutputStream() << "special_accelerators_add: " << makeQuoted(accelerator) << "\n";
	if ( !accelerator_map_insert( g_special_accelerators, accelerator, callback ) ) {
		globalErrorStream() << "special_accelerators_add: already exists: " << makeQuoted( accelerator ) << "\n";
	}
}
void special_accelerators_remove( Accelerator accelerator ){
	//globalOutputStream() << "special_accelerators_remove: " << makeQuoted(accelerator) << "\n";
	if ( !accelerator_map_erase( g_special_accelerators, accelerator ) ) {
		globalErrorStream() << "special_accelerators_remove: not found: " << makeQuoted( accelerator ) << "\n";
	}
}

void keydown_accelerators_add( Accelerator accelerator, const Callback<void()>& callback ){
	//globalOutputStream() << "keydown_accelerators_add: " << makeQuoted(accelerator) << "\n";
	if ( !accelerator_map_insert( g_keydown_accelerators, accelerator, callback ) ) {
		globalErrorStream() << "keydown_accelerators_add: already exists: " << makeQuoted( accelerator ) << "\n";
	}
}
void keydown_accelerators_remove( Accelerator accelerator ){
	//globalOutputStream() << "keydown_accelerators_remove: " << makeQuoted(accelerator) << "\n";
	if ( !accelerator_map_erase( g_keydown_accelerators, accelerator ) ) {
		globalErrorStream() << "keydown_accelerators_remove: not found: " << makeQuoted( accelerator ) << "\n";
	}
}

void keyup_accelerators_add( Accelerator accelerator, const Callback<void()>& callback ){
	//globalOutputStream() << "keyup_accelerators_add: " << makeQuoted(accelerator) << "\n";
	if ( !accelerator_map_insert( g_keyup_accelerators, accelerator, callback ) ) {
		globalErrorStream() << "keyup_accelerators_add: already exists: " << makeQuoted( accelerator ) << "\n";
	}
}
void keyup_accelerators_remove( Accelerator accelerator ){
	//globalOutputStream() << "keyup_accelerators_remove: " << makeQuoted(accelerator) << "\n";
	if ( !accelerator_map_erase( g_keyup_accelerators, accelerator ) ) {
		globalErrorStream() << "keyup_accelerators_remove: not found: " << makeQuoted( accelerator ) << "\n";
	}
}


gboolean accel_closure_callback(ui::AccelGroup group, ui::Widget widget, guint key, GdkModifierType modifiers, gpointer data ){
	( *reinterpret_cast<Callback<void()>*>( data ) )( );
	return TRUE;
}

GClosure* accel_group_add_accelerator(ui::AccelGroup group, Accelerator accelerator, const Callback<void()>& callback ){
	if ( accelerator.key != 0 && gtk_accelerator_valid( accelerator.key, accelerator.modifiers ) ) {
		//globalOutputStream() << "global_accel_connect: " << makeQuoted(accelerator) << "\n";
		GClosure* closure = create_cclosure( G_CALLBACK( accel_closure_callback ), callback );
		gtk_accel_group_connect( group, accelerator.key, accelerator.modifiers, GTK_ACCEL_VISIBLE, closure );
		return closure;
	}
	else
	{
		special_accelerators_add( accelerator, callback );
		return 0;
	}
}

void accel_group_remove_accelerator(ui::AccelGroup group, Accelerator accelerator ){
	if ( accelerator.key != 0 && gtk_accelerator_valid( accelerator.key, accelerator.modifiers ) ) {
		//globalOutputStream() << "global_accel_disconnect: " << makeQuoted(accelerator) << "\n";
		gtk_accel_group_disconnect_key( group, accelerator.key, accelerator.modifiers );
	}
	else
	{
		special_accelerators_remove( accelerator );
	}
}

ui::AccelGroup global_accel{ui::New};

GClosure* global_accel_group_add_accelerator( Accelerator accelerator, const Callback<void()>& callback ){
	if ( !global_accel_enabled() ) {
		// workaround: cannot add to GtkAccelGroup while it is disabled
		//globalOutputStream() << "queued for add: " << accelerator << "\n";
		globalQueuedAccelerators_add( accelerator, callback );
		return 0;
	}
	return accel_group_add_accelerator( global_accel, accelerator, callback );
}
void global_accel_group_remove_accelerator( Accelerator accelerator ){
	if ( !global_accel_enabled() ) {
		//globalOutputStream() << "queued for remove: " << accelerator << "\n";
		globalQueuedAccelerators_remove( accelerator );
		return;
	}
	accel_group_remove_accelerator( global_accel, accelerator );
}

/// \brief Propagates key events to the focus-widget, overriding global accelerators.
static gboolean override_global_accelerators( ui::Window window, GdkEventKey* event, gpointer data ){
	gboolean b = gtk_window_propagate_key_event( window, event );
	return b;
}

void global_accel_connect_window( ui::Window window ){
#if 1
	unsigned int override_handler = window.connect( "key_press_event", G_CALLBACK( override_global_accelerators ), 0 );
	g_object_set_data( G_OBJECT( window ), "override_handler", gint_to_pointer( override_handler ) );

	GlobalPressedKeys_connect( window );

	unsigned int special_key_press_handler = window.connect( "key_press_event", G_CALLBACK( accelerator_key_event ), &g_special_accelerators );
	g_object_set_data( G_OBJECT( window ), "special_key_press_handler", gint_to_pointer( special_key_press_handler ) );
#else
	unsigned int key_press_handler = window.connect( "key_press_event", G_CALLBACK( accelerator_key_event ), &g_keydown_accelerators );
	unsigned int key_release_handler = window.connect( "key_release_event", G_CALLBACK( accelerator_key_event ), &g_keyup_accelerators );
	g_object_set_data( G_OBJECT( window ), "key_press_handler", gint_to_pointer( key_press_handler ) );
	g_object_set_data( G_OBJECT( window ), "key_release_handler", gint_to_pointer( key_release_handler ) );
#endif
	g_accel_windows.insert( window );
	window.add_accel_group( global_accel );
}
void global_accel_disconnect_window( ui::Window window ){
#if 1
	GlobalPressedKeys_disconnect( window );

	g_signal_handler_disconnect( G_OBJECT( window ), gpointer_to_int( g_object_get_data( G_OBJECT( window ), "override_handler" ) ) );
	g_signal_handler_disconnect( G_OBJECT( window ), gpointer_to_int( g_object_get_data( G_OBJECT( window ), "special_key_press_handler" ) ) );
#else
	g_signal_handler_disconnect( G_OBJECT( window ), gpointer_to_int( g_object_get_data( G_OBJECT( window ), "key_press_handler" ) ) );
	g_signal_handler_disconnect( G_OBJECT( window ), gpointer_to_int( g_object_get_data( G_OBJECT( window ), "key_release_handler" ) ) );
#endif
	gtk_window_remove_accel_group( window, global_accel );
	std::size_t count = g_accel_windows.erase( window );
	ASSERT_MESSAGE( count == 1, "failed to remove accel group\n" );
}


GClosure* global_accel_group_find( Accelerator accelerator ){
	guint numEntries = 0;
	GtkAccelGroupEntry* entry = gtk_accel_group_query( global_accel, accelerator.key, accelerator.modifiers, &numEntries );
	if ( numEntries != 0 ) {
		if ( numEntries != 1 ) {
			char* name = gtk_accelerator_name( accelerator.key, accelerator.modifiers );
			globalErrorStream() << "accelerator already in-use: " << name << "\n";
			g_free( name );
		}
		return entry->closure;
	}
	return 0;
}

void global_accel_group_connect( const Accelerator& accelerator, const Callback<void()>& callback ){
	if ( accelerator.key != 0 ) {
		global_accel_group_add_accelerator( accelerator, callback );
	}
}

void global_accel_group_disconnect( const Accelerator& accelerator, const Callback<void()>& callback ){
	if ( accelerator.key != 0 ) {
		global_accel_group_remove_accelerator( accelerator );
	}
}
