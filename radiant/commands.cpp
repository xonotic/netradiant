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

#include "commands.h"

#include "gtk/gtk.h"
#include "debugging/debugging.h"
#include "warnings.h"

#include <map>
#include "string/string.h"
#include "versionlib.h"
#include "gtkutil/messagebox.h"
#include "gtkmisc.h"

#define NETRADIANT_CUSTOM_FULLY_MERGED 0
#if NETRADIANT_CUSTOM_FULLY_MERGED
// For deleting old shortcuts.ini file
#include "preferences.h"
#include "unistd.h"
#endif // NETRADIANT_CUSTOM_FULLY_MERGED

typedef std::pair<Accelerator, int> ShortcutValue; // accelerator, isRegistered
typedef std::map<CopiedString, ShortcutValue> Shortcuts;

void Shortcuts_foreach( Shortcuts& shortcuts, CommandVisitor& visitor ){
	for ( Shortcuts::iterator i = shortcuts.begin(); i != shortcuts.end(); ++i )
	{
		visitor.visit( ( *i ).first.c_str(), ( *i ).second.first );
	}
}

Shortcuts g_shortcuts;

const Accelerator& GlobalShortcuts_insert( const char* name, const Accelerator& accelerator ){
	return ( *g_shortcuts.insert( Shortcuts::value_type( name, ShortcutValue( accelerator, false ) ) ).first ).second.first;
}

void GlobalShortcuts_foreach( CommandVisitor& visitor ){
	Shortcuts_foreach( g_shortcuts, visitor );
}

void GlobalShortcuts_register( const char* name, int type ){
	Shortcuts::iterator i = g_shortcuts.find( name );
	if ( i != g_shortcuts.end() ) {
		( *i ).second.second = type;
	}
}

void GlobalShortcuts_reportUnregistered(){
	for ( Shortcuts::iterator i = g_shortcuts.begin(); i != g_shortcuts.end(); ++i )
	{
		if ( ( *i ).second.first.key != 0 && !( *i ).second.second ) {
			globalOutputStream() << "shortcut not registered: " << ( *i ).first.c_str() << "\n";
		}
	}
}

typedef std::map<CopiedString, Command> Commands;

Commands g_commands;

void GlobalCommands_insert( const char* name, const Callback<void()>& callback, const Accelerator& accelerator ){
	bool added = g_commands.insert( Commands::value_type( name, Command( callback, GlobalShortcuts_insert( name, accelerator ) ) ) ).second;
	ASSERT_MESSAGE( added, "command already registered: " << makeQuoted( name ) );
}

const Command& GlobalCommands_find( const char* command ){
	Commands::iterator i = g_commands.find( command );
	ASSERT_MESSAGE( i != g_commands.end(), "failed to lookup command " << makeQuoted( command ) );
	return ( *i ).second;
}

typedef std::map<CopiedString, Toggle> Toggles;


Toggles g_toggles;

void GlobalToggles_insert( const char* name, const Callback<void()>& callback, const Callback<void(const Callback<void(bool)> &)>& exportCallback, const Accelerator& accelerator ){
	bool added = g_toggles.insert( Toggles::value_type( name, Toggle( callback, GlobalShortcuts_insert( name, accelerator ), exportCallback ) ) ).second;
	ASSERT_MESSAGE( added, "toggle already registered: " << makeQuoted( name ) );
}
const Toggle& GlobalToggles_find( const char* name ){
	Toggles::iterator i = g_toggles.find( name );
	ASSERT_MESSAGE( i != g_toggles.end(), "failed to lookup toggle " << makeQuoted( name ) );
	return ( *i ).second;
}

typedef std::map<CopiedString, KeyEvent> KeyEvents;


KeyEvents g_keyEvents;

void GlobalKeyEvents_insert( const char* name, const Accelerator& accelerator, const Callback<void()>& keyDown, const Callback<void()>& keyUp ){
	bool added = g_keyEvents.insert( KeyEvents::value_type( name, KeyEvent( GlobalShortcuts_insert( name, accelerator ), keyDown, keyUp ) ) ).second;
	ASSERT_MESSAGE( added, "command already registered: " << makeQuoted( name ) );
}
const KeyEvent& GlobalKeyEvents_find( const char* name ){
	KeyEvents::iterator i = g_keyEvents.find( name );
	ASSERT_MESSAGE( i != g_keyEvents.end(), "failed to lookup keyEvent " << makeQuoted( name ) );
	return ( *i ).second;
}


void disconnect_accelerator( const char *name ){
	Shortcuts::iterator i = g_shortcuts.find( name );
	if ( i != g_shortcuts.end() ) {
		switch ( ( *i ).second.second )
		{
		case 1:
			// command
			command_disconnect_accelerator( name );
			break;
		case 2:
			// toggle
			toggle_remove_accelerator( name );
			break;
		}
	}
}

void connect_accelerator( const char *name ){
	Shortcuts::iterator i = g_shortcuts.find( name );
	if ( i != g_shortcuts.end() ) {
		switch ( ( *i ).second.second )
		{
		case 1:
			// command
			command_connect_accelerator( name );
			break;
		case 2:
			// toggle
			toggle_add_accelerator( name );
			break;
		}
	}
}


#include <uilib/uilib.h>
#include <gdk/gdkkeysyms.h>

#include "gtkutil/dialog.h"
#include "mainframe.h"

#include "stream/textfilestream.h"
#include "stream/stringstream.h"


struct command_list_dialog_t : public ModalDialog
{
	command_list_dialog_t()
		: m_close_button( *this, eIDCANCEL ), m_list( ui::null ), m_command_iter(), m_model( ui::null ), m_waiting_for_key( false ){
	}
	ModalDialogButton m_close_button;

	ui::TreeView m_list;
	GtkTreeIter m_command_iter;
	ui::TreeModel m_model;
	bool m_waiting_for_key;
};

void accelerator_clear_button_clicked( ui::Button btn, gpointer dialogptr ){
	command_list_dialog_t &dialog = *(command_list_dialog_t *) dialogptr;

	if ( dialog.m_waiting_for_key ) {
		// just unhighlight, user wanted to cancel
		dialog.m_waiting_for_key = false;
		gtk_list_store_set( ui::ListStore::from( dialog.m_model ), &dialog.m_command_iter, 2, false, -1 );
		gtk_widget_set_sensitive( dialog.m_list , true );
		dialog.m_model = ui::TreeModel(ui::null);
		return;
	}

	auto sel = gtk_tree_view_get_selection( dialog.m_list );
	GtkTreeModel *model;
	GtkTreeIter iter;
	if ( !gtk_tree_selection_get_selected( sel, &model, &iter ) ) {
		return;
	}

	GValue val;
	memset( &val, 0, sizeof( val ) );
	gtk_tree_model_get_value(model, &iter, 0, &val );
	const char *commandName = g_value_get_string( &val );;

	// clear the ACTUAL accelerator too!
	disconnect_accelerator( commandName );

	Shortcuts::iterator thisShortcutIterator = g_shortcuts.find( commandName );
	if ( thisShortcutIterator == g_shortcuts.end() ) {
		return;
	}
	thisShortcutIterator->second.first = accelerator_null();

	gtk_list_store_set( ui::ListStore::from( model ), &iter, 1, "", -1 );

	g_value_unset( &val );
}

void accelerator_edit_button_clicked( ui::Button btn, gpointer dialogptr ){
	command_list_dialog_t &dialog = *(command_list_dialog_t *) dialogptr;

	// 1. find selected row
	auto sel = gtk_tree_view_get_selection( dialog.m_list );
	GtkTreeModel *model;
	GtkTreeIter iter;
	if ( !gtk_tree_selection_get_selected( sel, &model, &iter ) ) {
		return;
	}
	dialog.m_command_iter = iter;
	dialog.m_model = ui::TreeModel::from(model);

	// 2. disallow changing the row
	//gtk_widget_set_sensitive(dialog.m_list, false);

	// 3. highlight the row
	gtk_list_store_set( ui::ListStore::from( model ), &iter, 2, true, -1 );

	// 4. grab keyboard focus
	dialog.m_waiting_for_key = true;
}

bool accelerator_window_key_press( ui::Window widget, GdkEventKey *event, gpointer dialogptr ){
	command_list_dialog_t &dialog = *(command_list_dialog_t *) dialogptr;

	if ( !dialog.m_waiting_for_key ) {
		return false;
	}

#if 0
	if ( event->is_modifier ) {
		return false;
	}
#else
	switch ( event->keyval )
	{
	case GDK_KEY_Shift_L:
	case GDK_KEY_Shift_R:
	case GDK_KEY_Control_L:
	case GDK_KEY_Control_R:
	case GDK_KEY_Caps_Lock:
	case GDK_KEY_Shift_Lock:
	case GDK_KEY_Meta_L:
	case GDK_KEY_Meta_R:
	case GDK_KEY_Alt_L:
	case GDK_KEY_Alt_R:
	case GDK_KEY_Super_L:
	case GDK_KEY_Super_R:
	case GDK_KEY_Hyper_L:
	case GDK_KEY_Hyper_R:
		return false;
	}
#endif

	dialog.m_waiting_for_key = false;

	// 7. find the name of the accelerator
	GValue val;
	memset( &val, 0, sizeof( val ) );
	gtk_tree_model_get_value(dialog.m_model, &dialog.m_command_iter, 0, &val );
	const char *commandName = g_value_get_string( &val );;
	Shortcuts::iterator thisShortcutIterator = g_shortcuts.find( commandName );
	if ( thisShortcutIterator == g_shortcuts.end() ) {
		gtk_list_store_set( ui::ListStore::from( dialog.m_model ), &dialog.m_command_iter, 2, false, -1 );
		gtk_widget_set_sensitive( dialog.m_list , true );
		return true;
	}

	// 8. build an Accelerator
	Accelerator newAccel( event->keyval, (GdkModifierType) event->state );

	// 8. verify the key is still free, show a dialog to ask what to do if not
	class VerifyAcceleratorNotTaken : public CommandVisitor
	{
	const char *commandName;
	const Accelerator &newAccel;
	ui::Widget widget;
	ui::TreeModel model;
public:
	bool allow;
	VerifyAcceleratorNotTaken( const char *name, const Accelerator &accelerator, ui::Widget w, ui::TreeModel m ) : commandName( name ), newAccel( accelerator ), widget( w ), model( m ), allow( true ){
	}
	void visit( const char* name, Accelerator& accelerator ){
		if ( !strcmp( name, commandName ) ) {
			return;
		}
		if ( !allow ) {
			return;
		}
		if ( accelerator.key == 0 ) {
			return;
		}
		if ( accelerator == newAccel ) {
			StringOutputStream msg;
			msg << "The command " << name << " is already assigned to the key " << accelerator << ".\n\n"
				<< "Do you want to unassign " << name << " first?";
			auto r = ui::alert( widget.window(), msg.c_str(), "Key already used", ui::alert_type::YESNOCANCEL );
			if ( r == ui::alert_response::YES ) {
				// clear the ACTUAL accelerator too!
				disconnect_accelerator( name );
				// delete the modifier
				accelerator = accelerator_null();
				// empty the cell of the key binds dialog
				GtkTreeIter i;
				if ( gtk_tree_model_get_iter_first(model, &i ) ) {
					for (;; )
					{
						GValue val;
						memset( &val, 0, sizeof( val ) );
						gtk_tree_model_get_value(model, &i, 0, &val );
						const char *thisName = g_value_get_string( &val );;
						if ( !strcmp( thisName, name ) ) {
							gtk_list_store_set( ui::ListStore::from( model ), &i, 1, "", -1 );
						}
						g_value_unset( &val );
						if ( !gtk_tree_model_iter_next(model, &i ) ) {
							break;
						}
					}
				}
			}
			else if ( r == ui::alert_response::CANCEL ) {
				// aborted
				allow = false;
			}
		}
	}
	} verify_visitor( commandName, newAccel, widget, dialog.m_model );
	GlobalShortcuts_foreach( verify_visitor );

	gtk_list_store_set( ui::ListStore::from( dialog.m_model ), &dialog.m_command_iter, 2, false, -1 );
	gtk_widget_set_sensitive( dialog.m_list , true );

	if ( verify_visitor.allow ) {
		// clear the ACTUAL accelerator first
		disconnect_accelerator( commandName );

		thisShortcutIterator->second.first = newAccel;

		// write into the cell
		StringOutputStream modifiers;
		modifiers << newAccel;
		gtk_list_store_set( ui::ListStore::from( dialog.m_model ), &dialog.m_command_iter, 1, modifiers.c_str(), -1 );

		// set the ACTUAL accelerator too!
		connect_accelerator( commandName );
	}

	g_value_unset( &val );

	dialog.m_model = ui::TreeModel(ui::null);

	return true;
}

/*
    GtkTreeIter row;
    GValue val;
    if(!model) {g_error("Unable to get model from cell renderer");}
    gtk_tree_model_get_iter_from_string(model, &row, path_string);

    gtk_tree_model_get_value(model, &row, 0, &val);
    const char *name = g_value_get_string(&val);
    Shortcuts::iterator i = g_shortcuts.find(name);
    if(i != g_shortcuts.end())
    {
        accelerator_parse(i->second.first, new_text);
        StringOutputStream modifiers;
        modifiers << i->second.first;
        gtk_list_store_set(ui::ListStore::from(model), &row, 1, modifiers.c_str(), -1);
    }
   };
 */

void DoCommandListDlg(){
	command_list_dialog_t dialog;

	ui::Window window = MainFrame_getWindow().create_modal_dialog_window("Mapped Commands", dialog, -1, 400);
	window.on_key_press([](ui::Widget widget, GdkEventKey *event, gpointer dialogptr) {
		return accelerator_window_key_press(ui::Window::from(widget), event, dialogptr);
	}, &dialog);

	auto accel = ui::AccelGroup(ui::New);
	window.add_accel_group( accel );

	auto hbox = create_dialog_hbox( 4, 4 );
	window.add(hbox);

	{
		auto scr = create_scrolled_window( ui::Policy::NEVER, ui::Policy::AUTOMATIC );
		hbox.pack_start( scr, TRUE, TRUE, 0 );

		{
			auto store = ui::ListStore::from(gtk_list_store_new( 4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_INT ));

			auto view = ui::TreeView(ui::TreeModel::from(store._handle));
			dialog.m_list = view;

			gtk_tree_view_set_enable_search(view, false ); // annoying

			{
				auto renderer = ui::CellRendererText(ui::New);
				auto column = ui::TreeViewColumn( "Command", renderer, {{"text", 0}, {"weight-set", 2}, {"weight", 3}} );
				gtk_tree_view_append_column(view, column );
			}

			{
				auto renderer = ui::CellRendererText(ui::New);
				auto column = ui::TreeViewColumn( "Key", renderer, {{"text", 1}, {"weight-set", 2}, {"weight", 3}} );
				gtk_tree_view_append_column(view, column );
			}

			view.show();
			scr.add(view);

			{
				// Initialize dialog
				class BuildCommandList : public CommandVisitor
				{
				ui::ListStore m_store;
public:
				BuildCommandList( ui::ListStore store ) : m_store( store ){
				}
				void visit( const char* name, Accelerator& accelerator ){
					StringOutputStream modifiers;
					modifiers << accelerator;

					m_store.append(0, name, 1, modifiers.c_str(), 2, false, 3, 800);
				}
				} visitor( store );

				GlobalShortcuts_foreach( visitor );
			}

			store.unref();
		}
	}

	auto vbox = create_dialog_vbox( 4 );
	hbox.pack_start( vbox, TRUE, TRUE, 0 );
	{
		auto editbutton = create_dialog_button( "Edit", (GCallback) accelerator_edit_button_clicked, &dialog );
		vbox.pack_start( editbutton, FALSE, FALSE, 0 );

		auto clearbutton = create_dialog_button( "Clear", (GCallback) accelerator_clear_button_clicked, &dialog );
		vbox.pack_start( clearbutton, FALSE, FALSE, 0 );

		ui::Widget spacer = ui::Image(ui::New);
		spacer.show();
		vbox.pack_start( spacer, TRUE, TRUE, 0 );

		auto button = create_modal_dialog_button( "Close", dialog.m_close_button );
		vbox.pack_start( button, FALSE, FALSE, 0 );
		widget_make_default( button );
		gtk_widget_grab_default( button  );
		gtk_widget_add_accelerator( button , "clicked", accel, GDK_KEY_Return, (GdkModifierType)0, (GtkAccelFlags)0 );
		gtk_widget_add_accelerator( button , "clicked", accel, GDK_KEY_Escape, (GdkModifierType)0, (GtkAccelFlags)0 );
	}

	modal_dialog_show( window, dialog );
	window.destroy();
}

#include "profile/profile.h"

const char* const COMMANDS_VERSION = "1.0-gtk-accelnames";

void DeleteOldCommandMap(){
#if NETRADIANT_CUSTOM_FULLY_MERGED
// To enable when NetRadiant and NetRadiant-custom are fully merged together.
	StringOutputStream path( 256 );
	path << SettingsPath_get() << g_pGameDescription->mGameFile.c_str() << '/';
	path << "shortcuts.ini";
	unlink( path.c_str() );
#endif
}

void SaveCommandMap(){
	StringOutputStream strINI( 256 );
	strINI << SettingsPath_get() << "shortcuts.ini";

	TextFileOutputStream file( strINI.c_str() );
	if ( !file.failed() ) {
		file << "[Version]\n";
		file << "number=" << COMMANDS_VERSION << "\n";
		file << "\n";
		file << "[Commands]\n";
		class WriteCommandMap : public CommandVisitor
		{
		TextFileOutputStream& m_file;
public:
		WriteCommandMap( TextFileOutputStream& file ) : m_file( file ){
		}
		void visit( const char* name, Accelerator& accelerator ){
			m_file << name << "=";

			const char* key = gtk_accelerator_name( accelerator.key, accelerator.modifiers );
			m_file << key;
			m_file << "\n";
		}
		} visitor( file );
		GlobalShortcuts_foreach( visitor );
	}
	
	DeleteOldCommandMap();
}

const char* stringrange_find( const char* first, const char* last, char c ){
	const char* p = strchr( first, '+' );
	if ( p == 0 ) {
		return last;
	}
	return p;
}

class ReadCommandMap : public CommandVisitor
{
const char* m_filename;
std::size_t m_count;
public:
ReadCommandMap( const char* filename ) : m_filename( filename ), m_count( 0 ){
}
void visit( const char* name, Accelerator& accelerator ){
	char value[1024];
	if ( read_var( m_filename, "Commands", name, value ) ) {
		if ( string_empty( value ) ) {
			accelerator.key = 0;
			accelerator.modifiers = (GdkModifierType)0;
			return;
		}

		gtk_accelerator_parse( value, &accelerator.key, &accelerator.modifiers );
		accelerator = accelerator; // fix modifiers

		if ( accelerator.key != 0 ) {
			++m_count;
		}
		else
		{
			globalOutputStream() << "WARNING: failed to parse user command " << makeQuoted( name ) << ": unknown key " << makeQuoted( value ) << "\n";
		}
	}
}
std::size_t count() const {
	return m_count;
}
};

void LoadCommandMap(){
	StringOutputStream strINI( 256 );
	strINI << SettingsPath_get() << "shortcuts.ini";

	FILE* f = fopen( strINI.c_str(), "r" );
	if ( f != 0 ) {
		fclose( f );
		globalOutputStream() << "loading custom shortcuts list from " << makeQuoted( strINI.c_str() ) << "\n";

		Version version = version_parse( COMMANDS_VERSION );
		Version dataVersion = { 0, 0 };

		{
			char value[1024];
			if ( read_var( strINI.c_str(), "Version", "number", value ) ) {
				dataVersion = version_parse( value );
			}
		}

		if ( version_compatible( version, dataVersion ) ) {
			globalOutputStream() << "commands import: data version " << dataVersion << " is compatible with code version " << version << "\n";
			ReadCommandMap visitor( strINI.c_str() );
			GlobalShortcuts_foreach( visitor );
			globalOutputStream() << "parsed " << Unsigned( visitor.count() ) << " custom shortcuts\n";
		}
		else
		{
			globalOutputStream() << "commands import: data version " << dataVersion << " is not compatible with code version " << version << "\n";
		}
	}
	else
	{
		globalOutputStream() << "failed to load custom shortcuts from " << makeQuoted( strINI.c_str() ) << "\n";
	}
}
