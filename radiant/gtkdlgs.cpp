/*
   Copyright (c) 2001, Loki software, inc.
   All rights reserved.

   Redistribution and use in source and binary forms, with or without modification,
   are permitted provided that the following conditions are met:

   Redistributions of source code must retain the above copyright notice, this list
   of conditions and the following disclaimer.

   Redistributions in binary form must reproduce the above copyright notice, this
   list of conditions and the following disclaimer in the documentation and/or
   other materials provided with the distribution.

   Neither the name of Loki software nor the names of its contributors may be used
   to endorse or promote products derived from this software without specific prior
   written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
   DIRECT,INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
   ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

//
// Some small dialogs that don't need much
//
// Leonardo Zide (leo@lokigames.com)
//

#include "gtkdlgs.h"
#include "globaldefs.h"

#include <gtk/gtk.h>

#include "debugging/debugging.h"
#include "aboutmsg.h"

#include "igl.h"
#include "iscenegraph.h"
#include "iselection.h"

#include <gdk/gdkkeysyms.h>
#include <uilib/uilib.h>

#include "os/path.h"
#include "math/aabb.h"
#include "container/array.h"
#include "generic/static.h"
#include "stream/stringstream.h"
#include "convert.h"
#include "gtkutil/messagebox.h"
#include "gtkutil/image.h"

#include "gtkmisc.h"
#include "brushmanip.h"
#include "build.h"
#include "qe3.h"
#include "texwindow.h"
#include "xywindow.h"
#include "mainframe.h"
#include "preferences.h"
#include "url.h"
#include "cmdlib.h"

#include "qerplugin.h"
#include "os/file.h"



// =============================================================================
// Project settings dialog

class GameComboConfiguration
{
public:
const char* basegame_dir;
const char* basegame;
const char* known_dir;
const char* known;
const char* custom;

GameComboConfiguration() :
	basegame_dir( g_pGameDescription->getRequiredKeyValue( "basegame" ) ),
	basegame( g_pGameDescription->getRequiredKeyValue( "basegamename" ) ),
	known_dir( g_pGameDescription->getKeyValue( "knowngame" ) ),
	known( g_pGameDescription->getKeyValue( "knowngamename" ) ),
	custom( g_pGameDescription->getRequiredKeyValue( "unknowngamename" ) ){
}
};

typedef LazyStatic<GameComboConfiguration> LazyStaticGameComboConfiguration;

inline GameComboConfiguration& globalGameComboConfiguration(){
	return LazyStaticGameComboConfiguration::instance();
}


struct gamecombo_t
{
	gamecombo_t( int _game, const char* _fs_game, bool _sensitive )
		: game( _game ), fs_game( _fs_game ), sensitive( _sensitive )
	{}
	int game;
	const char* fs_game;
	bool sensitive;
};

gamecombo_t gamecombo_for_dir( const char* dir ){
	if ( string_equal( dir, globalGameComboConfiguration().basegame_dir ) ) {
		return gamecombo_t( 0, "", false );
	}
	else if ( string_equal( dir, globalGameComboConfiguration().known_dir ) ) {
		return gamecombo_t( 1, dir, false );
	}
	else
	{
		return gamecombo_t( string_empty( globalGameComboConfiguration().known_dir ) ? 1 : 2, dir, true );
	}
}

gamecombo_t gamecombo_for_gamename( const char* gamename ){
	if ( ( strlen( gamename ) == 0 ) || !strcmp( gamename, globalGameComboConfiguration().basegame ) ) {
		return gamecombo_t( 0, "", false );
	}
	else if ( !strcmp( gamename, globalGameComboConfiguration().known ) ) {
		return gamecombo_t( 1, globalGameComboConfiguration().known_dir, false );
	}
	else
	{
		return gamecombo_t( string_empty( globalGameComboConfiguration().known_dir ) ? 1 : 2, "", true );
	}
}

inline void path_copy_clean( char* destination, const char* source ){
	char* i = destination;

	while ( *source != '\0' )
	{
		*i++ = ( *source == '\\' ) ? '/' : *source;
		++source;
	}

	if ( i != destination && *( i - 1 ) != '/' ) {
		*( i++ ) = '/';
	}

	*i = '\0';
}


struct GameCombo
{
	ui::ComboBoxText game_select{ui::null};
	ui::Entry fsgame_entry{ui::null};
};

gboolean OnSelchangeComboWhatgame( ui::Widget widget, GameCombo* combo ){
	const char *gamename;
	{
		GtkTreeIter iter;
		gtk_combo_box_get_active_iter( combo->game_select, &iter );
		gtk_tree_model_get( gtk_combo_box_get_model( combo->game_select ), &iter, 0, (gpointer*)&gamename, -1 );
	}

	gamecombo_t gamecombo = gamecombo_for_gamename( gamename );

	combo->fsgame_entry.text( gamecombo.fs_game );
	gtk_widget_set_sensitive( combo->fsgame_entry , gamecombo.sensitive );

	return FALSE;
}

class MappingMode
{
public:
bool do_mapping_mode;
const char* sp_mapping_mode;
const char* mp_mapping_mode;

MappingMode() :
	do_mapping_mode( !string_empty( g_pGameDescription->getKeyValue( "show_gamemode" ) ) ),
	sp_mapping_mode( "Single Player mapping mode" ),
	mp_mapping_mode( "Multiplayer mapping mode" ){
}
};

typedef LazyStatic<MappingMode> LazyStaticMappingMode;

inline MappingMode& globalMappingMode(){
	return LazyStaticMappingMode::instance();
}

class ProjectSettingsDialog
{
public:
GameCombo game_combo;
ui::ComboBox gamemode_combo{ui::null};
};

ui::Window ProjectSettingsDialog_construct( ProjectSettingsDialog& dialog, ModalDialog& modal ){
	auto window = MainFrame_getWindow().create_dialog_window("Project Settings", G_CALLBACK(dialog_delete_callback ), &modal );

	{
		auto table1 = create_dialog_table( 1, 2, 4, 4, 4 );
		window.add(table1);
		{
            auto vbox = create_dialog_vbox( 4 );
            table1.attach(vbox, {1, 2, 0, 1}, {GTK_FILL, GTK_FILL});
			{
                auto button = create_dialog_button( "OK", G_CALLBACK( dialog_button_ok ), &modal );
				vbox.pack_start( button, FALSE, FALSE, 0 );
			}
			{
                auto button = create_dialog_button( "Cancel", G_CALLBACK( dialog_button_cancel ), &modal );
				vbox.pack_start( button, FALSE, FALSE, 0 );
			}
		}
		{
			auto frame = create_dialog_frame( "Project settings" );
            table1.attach(frame, {0, 1, 0, 1}, {GTK_EXPAND | GTK_FILL, GTK_FILL});
			{
				auto table2 = create_dialog_table( ( globalMappingMode().do_mapping_mode ) ? 4 : 3, 2, 4, 4, 4 );
				frame.add(table2);

				{
					auto label = ui::Label( "Select mod" );
					label.show();
                    table2.attach(label, {0, 1, 0, 1}, {GTK_FILL, 0});
					gtk_misc_set_alignment( GTK_MISC( label ), 1, 0.5 );
				}
				{
					dialog.game_combo.game_select = ui::ComboBoxText(ui::New);

					gtk_combo_box_text_append_text( dialog.game_combo.game_select, globalGameComboConfiguration().basegame );
					if ( globalGameComboConfiguration().known[0] != '\0' ) {
						gtk_combo_box_text_append_text( dialog.game_combo.game_select, globalGameComboConfiguration().known );
					}
					gtk_combo_box_text_append_text( dialog.game_combo.game_select, globalGameComboConfiguration().custom );

					dialog.game_combo.game_select.show();
                    table2.attach(dialog.game_combo.game_select, {1, 2, 0, 1}, {GTK_EXPAND | GTK_FILL, 0});

					dialog.game_combo.game_select.connect( "changed", G_CALLBACK( OnSelchangeComboWhatgame ), &dialog.game_combo );
				}

				{
					auto label = ui::Label( "fs_game" );
					label.show();
                    table2.attach(label, {0, 1, 1, 2}, {GTK_FILL, 0});
					gtk_misc_set_alignment( GTK_MISC( label ), 1, 0.5 );
				}
				{
					auto entry = ui::Entry(ui::New);
					entry.show();
                    table2.attach(entry, {1, 2, 1, 2}, {GTK_EXPAND | GTK_FILL, 0});

					dialog.game_combo.fsgame_entry = entry;
				}

				if ( globalMappingMode().do_mapping_mode ) {
					auto label = ui::Label( "Mapping mode" );
					label.show();
                    table2.attach(label, {0, 1, 3, 4}, {GTK_FILL, 0});
					gtk_misc_set_alignment( GTK_MISC( label ), 1, 0.5 );

					auto combo = ui::ComboBoxText(ui::New);
					gtk_combo_box_text_append_text( combo, globalMappingMode().sp_mapping_mode );
					gtk_combo_box_text_append_text( combo, globalMappingMode().mp_mapping_mode );

					combo.show();
                    table2.attach(combo, {1, 2, 3, 4}, {GTK_EXPAND | GTK_FILL, 0});

					dialog.gamemode_combo = combo;
				}
			}
		}
	}

	// initialise the fs_game selection from the project settings into the dialog
	const char* dir = gamename_get();
	gamecombo_t gamecombo = gamecombo_for_dir( dir );

	gtk_combo_box_set_active( dialog.game_combo.game_select, gamecombo.game );
	dialog.game_combo.fsgame_entry.text( gamecombo.fs_game );
	gtk_widget_set_sensitive( dialog.game_combo.fsgame_entry , gamecombo.sensitive );

	if ( globalMappingMode().do_mapping_mode ) {
		const char *gamemode = gamemode_get();
		if ( string_empty( gamemode ) || string_equal( gamemode, "sp" ) ) {
			gtk_combo_box_set_active( dialog.gamemode_combo, 0 );
		}
		else
		{
			gtk_combo_box_set_active( dialog.gamemode_combo, 1 );
		}
	}

	return window;
}

void ProjectSettingsDialog_ok( ProjectSettingsDialog& dialog ){
	const char* dir = gtk_entry_get_text( dialog.game_combo.fsgame_entry );

	const char* new_gamename = path_equal( dir, globalGameComboConfiguration().basegame_dir )
							   ? ""
							   : dir;

	if ( !path_equal( new_gamename, gamename_get() ) ) {
		ScopeDisableScreenUpdates disableScreenUpdates( "Processing...", "Changing Game Name" );

		EnginePath_Unrealise();

		gamename_set( new_gamename );

		EnginePath_Realise();
	}

	if ( globalMappingMode().do_mapping_mode ) {
		// read from gamemode_combo
		int active = gtk_combo_box_get_active( dialog.gamemode_combo );
		if ( active == -1 || active == 0 ) {
			gamemode_set( "sp" );
		}
		else
		{
			gamemode_set( "mp" );
		}
	}
}

void DoProjectSettings(){
	//if ( ConfirmModified( "Edit Project Settings" ) ) {
		ModalDialog modal;
		ProjectSettingsDialog dialog;

		ui::Window window = ProjectSettingsDialog_construct( dialog, modal );

		if ( modal_dialog_show( window, modal ) == eIDOK ) {
			ProjectSettingsDialog_ok( dialog );
		}

		window.destroy();
	//}
}

// =============================================================================
// Arbitrary Sides dialog

void DoSides( int type, int axis ){
	ModalDialog dialog;

	auto window = MainFrame_getWindow().create_dialog_window("Arbitrary sides", G_CALLBACK(dialog_delete_callback ), &dialog );

	auto accel = ui::AccelGroup(ui::New);
	window.add_accel_group( accel );

	auto sides_entry = ui::Entry(ui::New);
	{
		auto hbox = create_dialog_hbox( 4, 4 );
		window.add(hbox);
		{
			auto label = ui::Label( "Sides:" );
			label.show();
			hbox.pack_start( label, FALSE, FALSE, 0 );
		}
		{
			auto entry = sides_entry;
			entry.show();
			hbox.pack_start( entry, FALSE, FALSE, 0 );
			gtk_widget_grab_focus( entry  );
		}
		{
            auto vbox = create_dialog_vbox( 4 );
			hbox.pack_start( vbox, TRUE, TRUE, 0 );
			{
				auto button = create_dialog_button( "OK", G_CALLBACK( dialog_button_ok ), &dialog );
				vbox.pack_start( button, FALSE, FALSE, 0 );
				widget_make_default( button );
				gtk_widget_add_accelerator( button , "clicked", accel, GDK_KEY_Return, (GdkModifierType)0, (GtkAccelFlags)0 );
			}
			{
                auto button = create_dialog_button( "Cancel", G_CALLBACK( dialog_button_cancel ), &dialog );
				vbox.pack_start( button, FALSE, FALSE, 0 );
				gtk_widget_add_accelerator( button , "clicked", accel, GDK_KEY_Escape, (GdkModifierType)0, (GtkAccelFlags)0 );
			}
		}
	}

	if ( modal_dialog_show( window, dialog ) == eIDOK ) {
		const char *str = gtk_entry_get_text( sides_entry );

		Scene_BrushConstructPrefab( GlobalSceneGraph(), (EBrushPrefab)type, atoi( str ), TextureBrowser_GetSelectedShader( GlobalTextureBrowser() ) );
	}

	window.destroy();
}

// =============================================================================
// About dialog (no program is complete without one)

void about_button_changelog( ui::Widget widget, gpointer data ){
	StringOutputStream log( 256 );
	log << "https://gitlab.com/xonotic/netradiant/commits/master";
	OpenURL( log.c_str() );
}

void about_button_credits( ui::Widget widget, gpointer data ){
	StringOutputStream cred( 256 );
	cred << "https://gitlab.com/xonotic/netradiant/graphs/master";
	OpenURL( cred.c_str() );
}

void about_button_issues( ui::Widget widget, gpointer data ){
	StringOutputStream cred( 256 );
	cred << "https://gitlab.com/xonotic/netradiant/issues";
	OpenURL( cred.c_str() );
}

static void AddParagraph( ui::VBox vbox, const char* text, bool use_markup ){
	auto label = ui::Label( text );
	gtk_label_set_use_markup( GTK_LABEL( label ), use_markup );
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0 );
	gtk_label_set_justify( label, GTK_JUSTIFY_LEFT );
	label.show();
	vbox.pack_start( label, TRUE, TRUE, 0 );
}

static void AddParagraph( ui::VBox vbox, const char* text ){
	AddParagraph( vbox, text, false );
}

void DoAbout(){
	ModalDialog dialog;
	ModalDialogButton ok_button( dialog, eIDOK );

	auto window = MainFrame_getWindow().create_modal_dialog_window("About " RADIANT_NAME, dialog );

	{
		auto vbox = create_dialog_vbox( 4, 4 );
		window.add(vbox);

		{
            auto hbox = create_dialog_hbox( 4 );
			vbox.pack_start( hbox, FALSE, FALSE, 0 );

			{
                auto vbox2 = create_dialog_vbox( 4 );
				hbox.pack_start( vbox2, FALSE, FALSE, 5 );
				{
					auto frame = create_dialog_frame( 0, ui::Shadow::IN );
					vbox2.pack_start( frame, FALSE, FALSE, 0 );
					{
						auto image = new_local_image( "logo.png" );
						image.show();
						frame.add(image);
					}
				}
			}

			{
				// HACK: that may not be related to font size
				auto about_vbox = ui::VBox( FALSE, 5 );
				about_vbox.show();
				hbox.pack_start( about_vbox, FALSE, FALSE, 0 );

				AddParagraph( about_vbox,
					RADIANT_NAME " " RADIANT_VERSION_STRING " (" __DATE__ ")\n"
					RADIANT_ABOUTMSG );
				AddParagraph( about_vbox,
					"Get news and latest build at "
					"<a href='https://netradiant.gitlab.io/'>"
						"netradiant.gitlab.io"
					"</a>\n"
					"Please report your issues at "
					"<a href='https://gitlab.com/xonotic/netradiant/issues'>"
						"gitlab.com/xonotic/netradiant/issues"
					"</a>\n"
					"The team cannot provide support for custom builds.", true );
				AddParagraph( about_vbox,
					RADIANT_NAME " is a community project maintained by "
					"<a href='https://xonotic.org'>"
						"Xonotic"
					"</a>\n"
					"and developed with help from "
					"<a href='https://netradiant.gitlab.io/page/about/'>"
						"other game projects"
					"</a> and individuals. ", true );
				AddParagraph( about_vbox,
					"This program is free software licensed under the GNU GPL." );
			}

			{
                auto vbox2 = create_dialog_vbox( 4 );
				hbox.pack_start( vbox2, TRUE, TRUE, 0 );
				{
                    auto button = create_modal_dialog_button( "OK", ok_button );
					vbox2.pack_start( button, FALSE, FALSE, 0 );
					gtk_widget_grab_focus( GTK_WIDGET( button ) );
				}
				{
                    auto button = create_dialog_button( "Credits", G_CALLBACK( about_button_credits ), 0 );
					vbox2.pack_start( button, FALSE, FALSE, 0 );
					gtk_widget_set_sensitive( GTK_WIDGET( button ), FALSE);
				}
				{
                    auto button = create_dialog_button( "Changes", G_CALLBACK( about_button_changelog ), 0 );
					vbox2.pack_start( button, FALSE, FALSE, 0 );
				}
				{
                    auto button = create_dialog_button( "Issues", G_CALLBACK( about_button_issues ), 0 );
					vbox2.pack_start( button, FALSE, FALSE, 0 );
					gtk_widget_set_sensitive( GTK_WIDGET( button ), FALSE);
				}
			}
		}
		{
			auto frame = create_dialog_frame( "OpenGL Properties" );
			vbox.pack_start( frame, FALSE, FALSE, 0 );
			{
				auto table = create_dialog_table( 3, 2, 4, 4, 4 );
				frame.add(table);
				{
					auto label = ui::Label( "Vendor:" );
					label.show();
                    table.attach(label, {0, 1, 0, 1}, {GTK_FILL, 0});
					gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
				}
				{
					auto label = ui::Label( "Version:" );
					label.show();
                    table.attach(label, {0, 1, 1, 2}, {GTK_FILL, 0});
					gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
				}
				{
					auto label = ui::Label( "Renderer:" );
					label.show();
                    table.attach(label, {0, 1, 2, 3}, {GTK_FILL, 0});
					gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
				}
				{
					auto label = ui::Label( reinterpret_cast<const char*>( glGetString( GL_VENDOR ) ) );
					label.show();
                    table.attach(label, {1, 2, 0, 1}, {GTK_FILL, 0});
					gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
				}
				{
					auto label = ui::Label( reinterpret_cast<const char*>( glGetString( GL_VERSION ) ) );
					label.show();
                    table.attach(label, {1, 2, 1, 2}, {GTK_FILL, 0});
					gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
				}
				{
					auto label = ui::Label( reinterpret_cast<const char*>( glGetString( GL_RENDERER ) ) );
					label.show();
                    table.attach(label, {1, 2, 2, 3}, {GTK_FILL, 0});
					gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
				}
			}
			{
				auto frame = create_dialog_frame( "OpenGL Extensions" );
				vbox.pack_start( frame, TRUE, TRUE, 0 );
				{
					auto sc_extensions = create_scrolled_window( ui::Policy::AUTOMATIC, ui::Policy::ALWAYS, 4 );
					frame.add(sc_extensions);
					{
						auto text_extensions = ui::TextView(ui::New);
						gtk_text_view_set_editable( text_extensions, FALSE );
						sc_extensions.add(text_extensions);
						text_extensions.text(reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS)));
						gtk_text_view_set_wrap_mode( text_extensions, GTK_WRAP_WORD );
						text_extensions.show();
					}
				}
			}
		}
	}

	modal_dialog_show( window, dialog );

	window.destroy();
}

// =============================================================================
// TextureLayout dialog

// Last used texture scale values
static float last_used_texture_layout_scale_x = 4.0;
static float last_used_texture_layout_scale_y = 4.0;

EMessageBoxReturn DoTextureLayout( float *fx, float *fy ){
	ModalDialog dialog;
	ModalDialogButton ok_button( dialog, eIDOK );
	ModalDialogButton cancel_button( dialog, eIDCANCEL );
	ui::Entry x{ui::null};
	ui::Entry y{ui::null};

	auto window = MainFrame_getWindow().create_modal_dialog_window("Patch texture layout", dialog );

	auto accel = ui::AccelGroup(ui::New);
	window.add_accel_group( accel );

	{
		auto hbox = create_dialog_hbox( 4, 4 );
		window.add(hbox);
		{
            auto vbox = create_dialog_vbox( 4 );
			hbox.pack_start( vbox, TRUE, TRUE, 0 );
			{
				auto label = ui::Label( "Texture will be fit across the patch based\n"
															"on the x and y values given. Values of 1x1\n"
															"will \"fit\" the texture. 2x2 will repeat\n"
															"it twice, etc." );
				label.show();
				vbox.pack_start( label, TRUE, TRUE, 0 );
				gtk_label_set_justify( label, GTK_JUSTIFY_LEFT );
			}
			{
				auto table = create_dialog_table( 2, 2, 4, 4 );
				table.show();
				vbox.pack_start( table, TRUE, TRUE, 0 );
				{
					auto label = ui::Label( "Texture x:" );
					label.show();
                    table.attach(label, {0, 1, 0, 1}, {GTK_FILL, 0});
					gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
				}
				{
					auto label = ui::Label( "Texture y:" );
					label.show();
                    table.attach(label, {0, 1, 1, 2}, {GTK_FILL, 0});
					gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
				}
				{
					auto entry = ui::Entry(ui::New);
					entry.show();
                    table.attach(entry, {1, 2, 0, 1}, {GTK_EXPAND | GTK_FILL, 0});

					x = entry;
				}
				{
					auto entry = ui::Entry(ui::New);
					entry.show();
                    table.attach(entry, {1, 2, 1, 2}, {GTK_EXPAND | GTK_FILL, 0});

					y = entry;
				}
			}
		}
		{
            auto vbox = create_dialog_vbox( 4 );
			hbox.pack_start( vbox, FALSE, FALSE, 0 );
			{
				auto button = create_modal_dialog_button( "OK", ok_button );
				vbox.pack_start( button, FALSE, FALSE, 0 );
				widget_make_default( button );
				gtk_widget_add_accelerator( button , "clicked", accel, GDK_KEY_Return, (GdkModifierType)0, (GtkAccelFlags)0 );
			}
			{
                auto button = create_modal_dialog_button( "Cancel", cancel_button );
				vbox.pack_start( button, FALSE, FALSE, 0 );
				gtk_widget_add_accelerator( button , "clicked", accel, GDK_KEY_Escape, (GdkModifierType)0, (GtkAccelFlags)0 );
			}
		}
	}

	// Initialize with last used values
	char buf[16];

	sprintf( buf, "%f", last_used_texture_layout_scale_x );
	x.text( buf );

	sprintf( buf, "%f", last_used_texture_layout_scale_y );
	y.text( buf );

	// Set focus after intializing the values
	gtk_widget_grab_focus( x  );

	EMessageBoxReturn ret = modal_dialog_show( window, dialog );
	if ( ret == eIDOK ) {
		*fx = static_cast<float>( atof( gtk_entry_get_text( x ) ) );
		*fy = static_cast<float>( atof( gtk_entry_get_text( y ) ) );

		// Remember last used values
		last_used_texture_layout_scale_x = *fx;
		last_used_texture_layout_scale_y = *fy;
	}

	window.destroy();

	return ret;
}

// =============================================================================
// Text Editor dialog

// master window widget
static ui::Window text_editor{ui::null};
static ui::Widget text_widget{ui::null}; // slave, text widget from the gtk editor
static GtkTextBuffer* text_buffer_;

static gint editor_delete( ui::Widget widget, gpointer data ){
/*	if ( ui::alert( widget.window(), "Close the shader editor ?", "Radiant", ui::alert_type::YESNO, ui::alert_icon::Question ) == ui::alert_response::NO ) {
		return TRUE;
	}
*/
	text_editor.hide();

	return TRUE;
}

static void editor_save( ui::Widget widget, gpointer data ){
	FILE *f = fopen( (char*)g_object_get_data( G_OBJECT( data ), "filename" ), "w" );
	//gpointer text = g_object_get_data( G_OBJECT( data ), "text" );

	if ( f == 0 ) {
		ui::alert( ui::Widget::from(data).window(), "Error saving file !" );
		return;
	}

	/* Obtain iters for the start and end of points of the buffer */
	GtkTextIter start;
	GtkTextIter end;
	gtk_text_buffer_get_start_iter (text_buffer_, &start);
	gtk_text_buffer_get_end_iter (text_buffer_, &end);

	/* Get the entire buffer text. */
	char *str = gtk_text_buffer_get_text (text_buffer_, &start, &end, FALSE);

	//char *str = gtk_editable_get_chars( GTK_EDITABLE( text ), 0, -1 );
	fwrite( str, 1, strlen( str ), f );
	fclose( f );
	g_free (str);
}

static void editor_close( ui::Widget widget, gpointer data ){
/*	if ( ui::alert( text_editor.window(), "Close the shader editor ?", "Radiant", ui::alert_type::YESNO, ui::alert_icon::Question ) == ui::alert_response::NO ) {
		return;
	}
*/
	text_editor.hide();
}

static void CreateGtkTextEditor(){
	auto dlg = ui::Window( ui::window_type::TOP );

	dlg.connect( "delete_event",
					  G_CALLBACK( editor_delete ), 0 );
	gtk_window_set_default_size( dlg, 400, 300 );

	auto vbox = ui::VBox( FALSE, 5 );
	vbox.show();
	dlg.add(vbox);
	gtk_container_set_border_width( GTK_CONTAINER( vbox ), 5 );

	auto scr = ui::ScrolledWindow(ui::New);
	scr.show();
	vbox.pack_start( scr, TRUE, TRUE, 0 );
	gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( scr ), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
	gtk_scrolled_window_set_shadow_type( GTK_SCROLLED_WINDOW( scr ), GTK_SHADOW_IN );

	auto text = ui::TextView(ui::New);
	scr.add(text);
	text.show();
	g_object_set_data( G_OBJECT( dlg ), "text", (gpointer) text );
	gtk_text_view_set_editable( text, TRUE );

	auto hbox = ui::HBox( FALSE, 5 );
	hbox.show();
	vbox.pack_start( hbox, FALSE, TRUE, 0 );

	auto button = ui::Button( "Close" );
	button.show();
	hbox.pack_end(button, FALSE, FALSE, 0);
	button.connect( "clicked",
					  G_CALLBACK( editor_close ), dlg );
	button.dimensions(60, -1);

	button = ui::Button( "Save" );
	button.show();
	hbox.pack_end(button, FALSE, FALSE, 0);
	button.connect( "clicked",
					  G_CALLBACK( editor_save ), dlg );
	button.dimensions(60, -1);

	text_editor = dlg;
	text_widget = text;
}

static void DoGtkTextEditor( const char* filename, guint cursorpos, int length ){
	if ( !text_editor ) {
		CreateGtkTextEditor(); // build it the first time we need it

	}
	// Load file
	FILE *f = fopen( filename, "r" );

	if ( f == 0 ) {
		globalOutputStream() << "Unable to load file " << filename << " in shader editor.\n";
		text_editor.hide();
	}
	else
	{
		fseek( f, 0, SEEK_END );
		int len = ftell( f );
		void *buf = malloc( len );
		void *old_filename;

		rewind( f );
		fread( buf, 1, len, f );

		gtk_window_set_title( text_editor, filename );

		auto text_buffer = gtk_text_view_get_buffer(ui::TextView::from(text_widget));
		gtk_text_buffer_set_text( text_buffer, (char*)buf, length );

		old_filename = g_object_get_data( G_OBJECT( text_editor ), "filename" );
		if ( old_filename ) {
			free( old_filename );
		}
		g_object_set_data( G_OBJECT( text_editor ), "filename", strdup( filename ) );

		// trying to show later
		text_editor.show();
		gtk_window_present( GTK_WINDOW( text_editor ) );

#if GDEF_OS_WINDOWS
		ui::process();
#endif

		// only move the cursor if it's not exceeding the size..
		// NOTE: this is erroneous, cursorpos is the offset in bytes, not in characters
		// len is the max size in bytes, not in characters either, but the character count is below that limit..
		// thinking .. the difference between character count and byte count would be only because of CR/LF?
		{
			GtkTextIter text_iter;
			// character offset, not byte offset
			gtk_text_buffer_get_iter_at_offset( text_buffer, &text_iter, cursorpos );
			gtk_text_buffer_place_cursor( text_buffer, &text_iter );
			gtk_text_view_scroll_to_iter( GTK_TEXT_VIEW( text_widget ), &text_iter, 0, TRUE, 0, 0);
		}

#if GDEF_OS_WINDOWS
		gtk_widget_queue_draw( text_widget );
#endif

		text_buffer_ = text_buffer;
		free( buf );
		fclose( f );
	}
}

// =============================================================================
// Light Intensity dialog

EMessageBoxReturn DoLightIntensityDlg( int *intensity ){
	ModalDialog dialog;
	ui::Entry intensity_entry{ui::null};
	ModalDialogButton ok_button( dialog, eIDOK );
	ModalDialogButton cancel_button( dialog, eIDCANCEL );

	ui::Window window = MainFrame_getWindow().create_modal_dialog_window("Light intensity", dialog, -1, -1 );

	auto accel_group = ui::AccelGroup(ui::New);
	window.add_accel_group( accel_group );

	{
		auto hbox = create_dialog_hbox( 4, 4 );
		window.add(hbox);
		{
            auto vbox = create_dialog_vbox( 4 );
			hbox.pack_start( vbox, TRUE, TRUE, 0 );
			{
				auto label = ui::Label( "ESC for default, ENTER to validate" );
				label.show();
				vbox.pack_start( label, FALSE, FALSE, 0 );
			}
			{
				auto entry = ui::Entry(ui::New);
				entry.show();
				vbox.pack_start( entry, TRUE, TRUE, 0 );

				gtk_widget_grab_focus( entry  );

				intensity_entry = entry;
			}
		}
		{
            auto vbox = create_dialog_vbox( 4 );
			hbox.pack_start( vbox, FALSE, FALSE, 0 );

			{
				auto button = create_modal_dialog_button( "OK", ok_button );
				vbox.pack_start( button, FALSE, FALSE, 0 );
				widget_make_default( button );
				gtk_widget_add_accelerator( button , "clicked", accel_group, GDK_KEY_Return, (GdkModifierType)0, GTK_ACCEL_VISIBLE );
			}
			{
                auto button = create_modal_dialog_button( "Cancel", cancel_button );
				vbox.pack_start( button, FALSE, FALSE, 0 );
				gtk_widget_add_accelerator( button , "clicked", accel_group, GDK_KEY_Escape, (GdkModifierType)0, GTK_ACCEL_VISIBLE );
			}
		}
	}

	char buf[16];
	sprintf( buf, "%d", *intensity );
	intensity_entry.text(buf);

	EMessageBoxReturn ret = modal_dialog_show( window, dialog );
	if ( ret == eIDOK ) {
		*intensity = atoi( gtk_entry_get_text( intensity_entry ) );
	}

	window.destroy();

	return ret;
}

// =============================================================================
// Add new shader tag dialog

EMessageBoxReturn DoShaderTagDlg( CopiedString* tag, const char* title ){
	ModalDialog dialog;
	ModalDialogButton ok_button( dialog, eIDOK );
	ModalDialogButton cancel_button( dialog, eIDCANCEL );

	auto window = MainFrame_getWindow().create_modal_dialog_window(title, dialog, -1, -1 );

	auto accel_group = ui::AccelGroup(ui::New);
	window.add_accel_group( accel_group );

	auto textentry = ui::Entry(ui::New);
	{
		auto hbox = create_dialog_hbox( 4, 4 );
		window.add(hbox);
		{
            auto vbox = create_dialog_vbox( 4 );
			hbox.pack_start( vbox, TRUE, TRUE, 0 );
			{
				//GtkLabel* label = GTK_LABEL(gtk_label_new("Enter one ore more tags separated by spaces"));
				auto label = ui::Label( "ESC to cancel, ENTER to validate" );
				label.show();
				vbox.pack_start( label, FALSE, FALSE, 0 );
			}
			{
				auto entry = textentry;
				entry.show();
				vbox.pack_start( entry, TRUE, TRUE, 0 );

				gtk_widget_grab_focus( entry  );
			}
		}
		{
            auto vbox = create_dialog_vbox( 4 );
			hbox.pack_start( vbox, FALSE, FALSE, 0 );

			{
				auto button = create_modal_dialog_button( "OK", ok_button );
				vbox.pack_start( button, FALSE, FALSE, 0 );
				widget_make_default( button );
				gtk_widget_add_accelerator( button , "clicked", accel_group, GDK_KEY_Return, (GdkModifierType)0, GTK_ACCEL_VISIBLE );
			}
			{
                auto button = create_modal_dialog_button( "Cancel", cancel_button );
				vbox.pack_start( button, FALSE, FALSE, 0 );
				gtk_widget_add_accelerator( button , "clicked", accel_group, GDK_KEY_Escape, (GdkModifierType)0, GTK_ACCEL_VISIBLE );
			}
		}
	}

	EMessageBoxReturn ret = modal_dialog_show( window, dialog );
	if ( ret == eIDOK ) {
		*tag = gtk_entry_get_text( textentry );
	}

	window.destroy();

	return ret;
}

EMessageBoxReturn DoShaderInfoDlg( const char* name, const char* filename, const char* title ){
	ModalDialog dialog;
	ModalDialogButton ok_button( dialog, eIDOK );

	auto window = MainFrame_getWindow().create_modal_dialog_window(title, dialog, -1, -1 );

	auto accel_group = ui::AccelGroup(ui::New);
	window.add_accel_group( accel_group );

	{
		auto hbox = create_dialog_hbox( 4, 4 );
		window.add(hbox);
		{
            auto vbox = create_dialog_vbox( 4 );
			hbox.pack_start( vbox, FALSE, FALSE, 0 );
			{
				auto label = ui::Label( "The selected shader" );
				label.show();
				vbox.pack_start( label, FALSE, FALSE, 0 );
			}
			{
				auto label = ui::Label( name );
				label.show();
				vbox.pack_start( label, FALSE, FALSE, 0 );
			}
			{
				auto label = ui::Label( "is located in file" );
				label.show();
				vbox.pack_start( label, FALSE, FALSE, 0 );
			}
			{
				auto label = ui::Label( filename );
				label.show();
				vbox.pack_start( label, FALSE, FALSE, 0 );
			}
			{
				auto button = create_modal_dialog_button( "OK", ok_button );
				vbox.pack_start( button, FALSE, FALSE, 0 );
				widget_make_default( button );
				gtk_widget_add_accelerator( button , "clicked", accel_group, GDK_KEY_Return, (GdkModifierType)0, GTK_ACCEL_VISIBLE );
			}
		}
	}

	EMessageBoxReturn ret = modal_dialog_show( window, dialog );

	window.destroy();

	return ret;
}



#if GDEF_OS_WINDOWS
#include <gdk/gdkwin32.h>
#endif

#if GDEF_OS_WINDOWS
// use the file associations to open files instead of builtin Gtk editor
bool g_TextEditor_useWin32Editor = false;
#else
// custom shader editor
bool g_TextEditor_useCustomEditor = false;
CopiedString g_TextEditor_editorCommand( "" );
#endif

void DoTextEditor( const char* filename, int cursorpos, int length ){
#if GDEF_OS_WINDOWS
	if ( g_TextEditor_useWin32Editor ) {
		StringOutputStream path( 256 );
		StringOutputStream modpath( 256 );
		const char* gamename = GlobalRadiant().getGameName();
		const char* basegame = GlobalRadiant().getRequiredGameDescriptionKeyValue( "basegame" );
		const char* enginePath = GlobalRadiant().getEnginePath();
		path << enginePath << basegame << '/' << filename;
		modpath << enginePath << gamename << '/' << filename;
		if ( file_exists( modpath.c_str() ) ){
			globalOutputStream() << "opening file '" << modpath.c_str() << "' (line " << cursorpos << " info ignored)\n";
			ShellExecute( (HWND)GDK_WINDOW_HWND( gtk_widget_get_window( MainFrame_getWindow() ) ), "open", modpath.c_str(), 0, 0, SW_SHOW );
		}
		else if ( file_exists( path.c_str() ) ){
			globalOutputStream() << "opening file '" << path.c_str() << "' (line " << cursorpos << " info ignored)\n";
			ShellExecute( (HWND)GDK_WINDOW_HWND( gtk_widget_get_window( MainFrame_getWindow() ) ), "open", path.c_str(), 0, 0, SW_SHOW );
		}
		else{
			globalOutputStream() << "Failed to open '" << filename << "'\nOne sits in .pk3 most likely!\n";
		}
		return;
	}
	else{
		StringOutputStream path( 256 );
		StringOutputStream modpath( 256 );
		const char* gamename = GlobalRadiant().getGameName();
		const char* basegame = GlobalRadiant().getRequiredGameDescriptionKeyValue( "basegame" );
		const char* enginePath = GlobalRadiant().getEnginePath();
		path << enginePath << basegame << '/' << filename;
		modpath << enginePath << gamename << '/' << filename;
		if ( file_exists( modpath.c_str() ) ){
			globalOutputStream() << "opening file '" << modpath.c_str() << "' (line " << cursorpos << " info ignored)\n";
			DoGtkTextEditor( modpath.c_str(), cursorpos, length );
		}
		else if ( file_exists( path.c_str() ) ){
			globalOutputStream() << "opening file '" << path.c_str() << "' (line " << cursorpos << " info ignored)\n";
			DoGtkTextEditor( path.c_str(), cursorpos, length );
		}
		else{
			globalOutputStream() << "Failed to open '" << filename << "'\nOne sits in .pk3 most likely!\n";
		}
		return;
	}
#else
	// check if a custom editor is set
	if ( g_TextEditor_useCustomEditor && !g_TextEditor_editorCommand.empty() ) {
		StringOutputStream strEditCommand( 256 );
		strEditCommand << g_TextEditor_editorCommand.c_str() << " \"" << filename << "\"";

		globalOutputStream() << "Launching: " << strEditCommand.c_str() << "\n";
		// note: linux does not return false if the command failed so it will assume success
		if ( Q_Exec( 0, const_cast<char*>( strEditCommand.c_str() ), 0, true, false ) == false ) {
			globalOutputStream() << "Failed to execute " << strEditCommand.c_str() << ", using default\n";
		}
		else
		{
			// the command (appeared) to run successfully, no need to do anything more
			return;
		}
	}

	DoGtkTextEditor( filename, cursorpos, length );
#endif
}
