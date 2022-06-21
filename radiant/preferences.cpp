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

//
// User preferences
//
// Leonardo Zide (leo@lokigames.com)
//

#include "preferences.h"
#include "globaldefs.h"

#include <gtk/gtk.h>
#include "environment.h"

#include "debugging/debugging.h"

#include "generic/callback.h"
#include "math/vector.h"
#include "string/string.h"
#include "stream/stringstream.h"
#include "os/file.h"
#include "os/path.h"
#include "os/dir.h"
#include "gtkutil/filechooser.h"
#include "gtkutil/messagebox.h"
#include "cmdlib.h"

#include "error.h"
#include "console.h"
#include "xywindow.h"
#include "mainframe.h"
#include "qe3.h"
#include "gtkdlgs.h"



void Global_constructPreferences( PreferencesPage& page ){
	page.appendCheckBox( "Console", "Enable Logging", g_Console_enableLogging );
}

void Interface_constructPreferences( PreferencesPage& page ){
#if GDEF_OS_WINDOWS
	page.appendCheckBox( "", "External Shader Editor", g_TextEditor_useWin32Editor );
#else
	{
		ui::CheckButton use_custom = page.appendCheckBox( "Text Editor", "Custom", g_TextEditor_useCustomEditor );
		ui::Widget custom_editor = page.appendPathEntry( "Text Editor Command", g_TextEditor_editorCommand, true );
		Widget_connectToggleDependency( custom_editor, use_custom );
	}
#endif
}

void Mouse_constructPreferences( PreferencesPage& page ){
	{
		const char* buttons[] = { "2 button", "3 button", };
		page.appendRadio( "Mouse Type",  g_glwindow_globals.m_nMouseType, STRING_ARRAY_RANGE( buttons ) );
	}
	page.appendCheckBox( "Right Button", "Activates Context Menu", g_xywindow_globals.m_bRightClick );
	page.appendCheckBox( "", "Improved mousewheel zoom", g_xywindow_globals.m_bImprovedWheelZoom );
}
void Mouse_constructPage( PreferenceGroup& group ){
	PreferencesPage page( group.createPage( "Mouse", "Mouse Preferences" ) );
	Mouse_constructPreferences( page );
}
void Mouse_registerPreferencesPage(){
	PreferencesDialog_addInterfacePage( makeCallbackF(Mouse_constructPage) );
}


/*!
   =========================================================
   Games selection dialog
   =========================================================
 */

#include <map>
#include <uilib/uilib.h>

inline const char* xmlAttr_getName( xmlAttrPtr attr ){
	return reinterpret_cast<const char*>( attr->name );
}

inline const char* xmlAttr_getValue( xmlAttrPtr attr ){
	return reinterpret_cast<const char*>( attr->children->content );
}

CGameDescription::CGameDescription( xmlDocPtr pDoc, const CopiedString& gameFile ){
	// read the user-friendly game name
	xmlNodePtr pNode = pDoc->children;

	while ( pNode != 0 && strcmp( (const char*)pNode->name, "game" ) )
	{
		pNode = pNode->next;
	}
	if ( !pNode ) {
		Error( "Didn't find 'game' node in the game description file '%s'\n", pDoc->URL );
	}

	for ( xmlAttrPtr attr = pNode->properties; attr != 0; attr = attr->next )
	{
		m_gameDescription.insert( GameDescription::value_type( xmlAttr_getName( attr ), xmlAttr_getValue( attr ) ) );
	}

	{
		StringOutputStream path( 256 );
		path << DataPath_get() << "gamepacks/" << gameFile.c_str() << "/";
		mGameToolsPath = path.c_str();
	}

	ASSERT_MESSAGE( file_exists( mGameToolsPath.c_str() ), "game directory not found: " << makeQuoted( mGameToolsPath.c_str() ) );

	mGameFile = gameFile;

	{
		GameDescription::iterator i = m_gameDescription.find( "type" );
		if ( i == m_gameDescription.end() ) {
			globalErrorStream() << "Warning, 'type' attribute not found in '" << reinterpret_cast<const char*>( pDoc->URL ) << "'\n";
			// default
			mGameType = "q3";
		}
		else
		{
			mGameType = ( *i ).second.c_str();
		}
	}
}

void CGameDescription::Dump(){
	globalOutputStream() << "game description file: " << makeQuoted( mGameFile.c_str() ) << "\n";
	for ( GameDescription::iterator i = m_gameDescription.begin(); i != m_gameDescription.end(); ++i )
	{
		globalOutputStream() << ( *i ).first.c_str() << " = " << makeQuoted( ( *i ).second.c_str() ) << "\n";
	}
}

CGameDescription *g_pGameDescription; ///< shortcut to g_GamesDialog.m_pCurrentDescription


#include "warnings.h"
#include "stream/textfilestream.h"
#include "container/array.h"
#include "xml/ixml.h"
#include "xml/xmlparser.h"
#include "xml/xmlwriter.h"

#include "preferencedictionary.h"
#include "stringio.h"

const char* const PREFERENCES_VERSION = "1.0";

bool Preferences_Load( PreferenceDictionary& preferences, const char* filename, const char *cmdline_prefix ){
	bool ret = false;
	TextFileInputStream file( filename );
	if ( !file.failed() ) {
		XMLStreamParser parser( file );
		XMLPreferenceDictionaryImporter importer( preferences, PREFERENCES_VERSION );
		parser.exportXML( importer );
		ret = true;
	}

	int l = strlen( cmdline_prefix );
	for ( int i = 1; i < g_argc - 1; ++i )
	{
		if ( g_argv[i][0] == '-' ) {
			if ( !strncmp( g_argv[i] + 1, cmdline_prefix, l ) ) {
				if ( g_argv[i][l + 1] == '-' ) {
					preferences.importPref( g_argv[i] + l + 2, g_argv[i + 1] );
				}
			}
			++i;
		}
	}

	return ret;
}

bool Preferences_Save( PreferenceDictionary& preferences, const char* filename ){
	TextFileOutputStream file( filename );
	if ( !file.failed() ) {
		XMLStreamWriter writer( file );
		XMLPreferenceDictionaryExporter exporter( preferences, PREFERENCES_VERSION );
		exporter.exportXML( writer );
		return true;
	}
	return false;
}

bool Preferences_Save_Safe( PreferenceDictionary& preferences, const char* filename ){
	std::string tmpName( filename );
	tmpName += "TMP";

	return Preferences_Save( preferences, tmpName.c_str() )
		   && ( !file_exists( filename ) || file_remove( filename ) )
		   && file_move( tmpName.data(), filename );
}


struct LogConsole {
	static void Export(const Callback<void(bool)> &returnz) {
		returnz(g_Console_enableLogging);
	}

	static void Import(bool value) {
		g_Console_enableLogging = value;
		Sys_EnableLogFile(g_Console_enableLogging);
	}
};


void RegisterGlobalPreferences( PreferenceSystem& preferences ){
	preferences.registerPreference( "gamefile", make_property_string( g_GamesDialog.m_sGameFile ) );
	preferences.registerPreference( "gamePrompt", make_property_string( g_GamesDialog.m_bGamePrompt ) );
	preferences.registerPreference( "skipGamePromptOnce", make_property_string( g_GamesDialog.m_bSkipGamePromptOnce ) );
	preferences.registerPreference( "log console", make_property_string<LogConsole>() );
}


PreferenceDictionary g_global_preferences;

void GlobalPreferences_Init(){
	RegisterGlobalPreferences( g_global_preferences );
}

void CGameDialog::LoadPrefs(){
	// load global .pref file
	StringOutputStream strGlobalPref( 256 );
	strGlobalPref << g_Preferences.m_global_rc_path->str << "global.pref";

	globalOutputStream() << "loading global preferences from " << makeQuoted( strGlobalPref.c_str() ) << "\n";

	if ( !Preferences_Load( g_global_preferences, strGlobalPref.c_str(), "global" ) ) {
		globalOutputStream() << "failed to load global preferences from " << strGlobalPref.c_str() << "\n";
	}
}

void CGameDialog::SavePrefs(){
	StringOutputStream strGlobalPref( 256 );
	strGlobalPref << g_Preferences.m_global_rc_path->str << "global.pref";

	globalOutputStream() << "saving global preferences to " << strGlobalPref.c_str() << "\n";

	if ( !Preferences_Save_Safe( g_global_preferences, strGlobalPref.c_str() ) ) {
		globalOutputStream() << "failed to save global preferences to " << strGlobalPref.c_str() << "\n";
	}
}

void CGameDialog::DoGameDialog(){
	// show the UI
	DoModal();

	// we save the prefs file
	SavePrefs();
}

void CGameDialog::GameFileImport( int value ){
	m_nComboSelect = value;
	// use value to set m_sGameFile
	std::list<CGameDescription *>::iterator iGame = mGames.begin();
	int i;
	for ( i = 0; i < value; i++ )
	{
		++iGame;
	}

	if ( ( *iGame )->mGameFile != m_sGameFile ) {
	m_sGameFile = ( *iGame )->mGameFile;

		// do not trigger radiant restart when switching game on startup using Global Preferences dialog
		if ( !onStartup ) {
			PreferencesDialog_restartRequired( "Selected Game" );
		}
	}

	// onStartup can only be true once, when Global Preferences are displayed at startup
	onStartup = false;
}

void CGameDialog::GameFileExport( const Callback<void(int)> & importCallback ) const {
	// use m_sGameFile to set value
	std::list<CGameDescription *>::const_iterator iGame;
	int i = 0;
	for ( iGame = mGames.begin(); iGame != mGames.end(); ++iGame )
	{
		if ( ( *iGame )->mGameFile == m_sGameFile ) {
			m_nComboSelect = i;
			break;
		}
		i++;
	}
	importCallback( m_nComboSelect );
}

struct CGameDialog_GameFile {
	static void Export(const CGameDialog &self, const Callback<void(int)> &returnz) {
		self.GameFileExport(returnz);
	}

	static void Import(CGameDialog &self, int value) {
		self.GameFileImport(value);
	}
};

void CGameDialog::CreateGlobalFrame( PreferencesPage& page ){
	std::vector<const char*> games;
	games.reserve( mGames.size() );
	for ( std::list<CGameDescription *>::iterator i = mGames.begin(); i != mGames.end(); ++i )
	{
		games.push_back( ( *i )->getRequiredKeyValue( "name" ) );
	}
	page.appendCombo(
		"Select the game",
		StringArrayRange( &( *games.begin() ), &( *games.end() ) ),
		make_property<CGameDialog_GameFile>(*this)
		);
	page.appendCheckBox( "Startup", "Show Global Preferences", m_bGamePrompt );
}

ui::Window CGameDialog::BuildDialog(){
	auto frame = create_dialog_frame( "Game settings", ui::Shadow::ETCHED_IN );

	auto vbox2 = create_dialog_vbox( 0, 4 );
	frame.add(vbox2);

	{
		PreferencesPage preferencesPage( *this, vbox2 );
		Global_constructPreferences( preferencesPage );
		CreateGlobalFrame( preferencesPage );
	}

	return create_simple_modal_dialog_window( "Global Preferences", m_modal, frame );
}

static void StringReplace( std::string& input, const std::string& first, const std::string& second )
{
	size_t found = 0;
	while ( ( found = input.find(first, found) ) != std::string::npos )
	{
		input.replace( found, first.length(), second );
	}
}

// FIXME, for some unknown reason it sorts “Quake 3” after “Quake 4”.
static bool CompareGameName( CGameDescription *first, CGameDescription *second )
{
	std::string string1( first->getRequiredKeyValue( "name" ) );
	std::string string2( second->getRequiredKeyValue( "name" ) );

	// HACK: Replace some roman numerals.
	StringReplace( string1, " III", " 3" );
	StringReplace( string2, " III", " 3" );
	StringReplace( string1, " II", " 2" );
	StringReplace( string2, " II", " 2" );

	return string1 < string2;
}

void CGameDialog::ScanForGames(){
	StringOutputStream strGamesPath( 256 );
	strGamesPath << DataPath_get() << "gamepacks/games/";
	const char *path = strGamesPath.c_str();

	globalOutputStream() << "Scanning for game description files: " << path << '\n';

	/*!
	   \todo FIXME LINUX:
	   do we put game description files below AppPath, or in ~/.radiant
	   i.e. read only or read/write?
	   my guess .. readonly cause it's an install
	   we will probably want to add ~/.radiant/<version>/games/ scanning on top of that for developers
	   (if that's really needed)
	 */

	Directory_forEach(path, [&](const char *name) {
		if (!extension_equal(path_get_extension(name), "game")) {
			return;
		}
		StringOutputStream strPath(256);
		strPath << path << name;
		globalOutputStream() << strPath.c_str() << '\n';

		xmlDocPtr pDoc = xmlParseFile(strPath.c_str());
		if (pDoc) {
			mGames.push_front(new CGameDescription(pDoc, name));
			xmlFreeDoc(pDoc);
		} else {
			globalErrorStream() << "XML parser failed on '" << strPath.c_str() << "'\n";
		}

		mGames.sort(CompareGameName);
	});
}

CGameDescription* CGameDialog::GameDescriptionForComboItem(){
	std::list<CGameDescription *>::iterator iGame;
	int i = 0;
	for ( iGame = mGames.begin(); iGame != mGames.end(); ++iGame,i++ )
	{
		if ( i == m_nComboSelect ) {
			return ( *iGame );
		}
	}
	return 0; // not found
}

void CGameDialog::InitGlobalPrefPath(){
	g_Preferences.m_global_rc_path = g_string_new( SettingsPath_get() );
}

void CGameDialog::Reset(){
	if ( !g_Preferences.m_global_rc_path ) {
		InitGlobalPrefPath();
	}
	StringOutputStream strGlobalPref( 256 );
	strGlobalPref << g_Preferences.m_global_rc_path->str << "global.pref";
	file_remove( strGlobalPref.c_str() );
}

void CGameDialog::Init(){
	bool gamePrompt = false;

	InitGlobalPrefPath();
	LoadPrefs();
	ScanForGames();

	if ( mGames.empty() ) {
		Error( "Didn't find any valid game file descriptions, aborting\n" );
	}
	else
	{
		std::list<CGameDescription *>::iterator iGame, iPrevGame;
		for ( iGame = mGames.begin(), iPrevGame = mGames.end(); iGame != mGames.end(); iPrevGame = iGame, ++iGame )
		{
			if ( iPrevGame != mGames.end() ) {
				if ( strcmp( ( *iGame )->getRequiredKeyValue( "name" ), ( *iPrevGame )->getRequiredKeyValue( "name" ) ) < 0 ) {
					CGameDescription *h = *iGame;
					*iGame = *iPrevGame;
					*iPrevGame = h;
				}
			}
		}
	}

	CGameDescription* currentGameDescription = 0;

	// m_bSkipGamePromptOnce is used to not prompt for game on restart, only on fresh startup
	if ( m_bGamePrompt && !m_bSkipGamePromptOnce ) {
		gamePrompt = true;
	}

	m_bSkipGamePromptOnce = false;
	g_GamesDialog.SavePrefs();

	if ( !gamePrompt ) {
		// search by .game name
		std::list<CGameDescription *>::iterator iGame;
		for ( iGame = mGames.begin(); iGame != mGames.end(); ++iGame )
		{
			if ( ( *iGame )->mGameFile == m_sGameFile ) {
				currentGameDescription = ( *iGame );
				break;
			}
		}
	}

	if ( gamePrompt || !currentGameDescription ) {
		onStartup = true;
		Create();
		DoGameDialog();
		// use m_nComboSelect to identify the game to run as and set the globals
		currentGameDescription = GameDescriptionForComboItem();
		ASSERT_NOTNULL( currentGameDescription );
	}
	else {
		onStartup = false;
	}

	g_pGameDescription = currentGameDescription;

	g_pGameDescription->Dump();
}

CGameDialog::~CGameDialog(){
	// free all the game descriptions
	std::list<CGameDescription *>::iterator iGame;
	for ( iGame = mGames.begin(); iGame != mGames.end(); ++iGame )
	{
		delete ( *iGame );
		*iGame = 0;
	}
	if ( GetWidget() ) {
		Destroy();
	}
}

inline const char* GameDescription_getIdentifier( const CGameDescription& gameDescription ){
	const char* identifier = gameDescription.getKeyValue( "index" );
	if ( string_empty( identifier ) ) {
		identifier = "1";
	}
	return identifier;
}


CGameDialog g_GamesDialog;


// =============================================================================
// Widget callbacks for PrefsDlg

static void OnButtonClean( ui::Widget widget, gpointer data ){
	// make sure this is what the user wants
	if ( ui::alert( g_Preferences.GetWidget(), "This will close " RADIANT_NAME " and clean the corresponding registry entries.\n"
																  "Next time you start " RADIANT_NAME " it will be good as new. Do you wish to continue?",
						 "Reset Registry", ui::alert_type::YESNO, ui::alert_icon::Asterisk ) == ui::alert_response::YES ) {
		PrefsDlg *dlg = (PrefsDlg*)data;
		dlg->EndModal( eIDCANCEL );

		g_preferences_globals.disable_ini = true;
		Preferences_Reset();
		gtk_main_quit();
	}
}

// =============================================================================
// PrefsDlg class

/*
   ========

   very first prefs init deals with selecting the game and the game tools path
   then we can load .ini stuff

   using prefs / ini settings:
   those are per-game

   look in ~/.radiant/<version>/gamename
   ========
 */

const char *PREFS_LOCAL_FILENAME = "local.pref";

void PrefsDlg::Init(){
	// m_global_rc_path has been set above
	// m_rc_path is for game specific preferences
	// takes the form: global-pref-path/gamename/prefs-file

	// this is common to win32 and Linux init now
	m_rc_path = g_string_new( m_global_rc_path->str );

	// game sub-dir
	g_string_append( m_rc_path, g_pGameDescription->mGameFile.c_str() );
	g_string_append( m_rc_path, "/" );
	Q_mkdir( m_rc_path->str );

	// then the ini file
	m_inipath = g_string_new( m_rc_path->str );
	g_string_append( m_inipath, PREFS_LOCAL_FILENAME );
}

void notebook_set_page( ui::Widget notebook, ui::Widget page ){
	int pagenum = gtk_notebook_page_num( GTK_NOTEBOOK( notebook ), page );
	if ( gtk_notebook_get_current_page( GTK_NOTEBOOK( notebook ) ) != pagenum ) {
		gtk_notebook_set_current_page( GTK_NOTEBOOK( notebook ), pagenum );
	}
}

void PrefsDlg::showPrefPage( ui::Widget prefpage ){
	notebook_set_page( m_notebook, prefpage );
	return;
}

static void treeSelection( ui::TreeSelection selection, gpointer data ){
	PrefsDlg *dlg = (PrefsDlg*)data;

	GtkTreeModel* model;
	GtkTreeIter selected;
	if ( gtk_tree_selection_get_selected( selection, &model, &selected ) ) {
		ui::Widget prefpage{ui::null};
		gtk_tree_model_get( model, &selected, 1, (gpointer*)&prefpage, -1 );
		dlg->showPrefPage( prefpage );
	}
}

typedef std::list<PreferenceGroupCallback> PreferenceGroupCallbacks;

inline void PreferenceGroupCallbacks_constructGroup( const PreferenceGroupCallbacks& callbacks, PreferenceGroup& group ){
	for ( PreferenceGroupCallbacks::const_iterator i = callbacks.begin(); i != callbacks.end(); ++i )
	{
		( *i )( group );
	}
}


inline void PreferenceGroupCallbacks_pushBack( PreferenceGroupCallbacks& callbacks, const PreferenceGroupCallback& callback ){
	callbacks.push_back( callback );
}

typedef std::list<PreferencesPageCallback> PreferencesPageCallbacks;

inline void PreferencesPageCallbacks_constructPage( const PreferencesPageCallbacks& callbacks, PreferencesPage& page ){
	for ( PreferencesPageCallbacks::const_iterator i = callbacks.begin(); i != callbacks.end(); ++i )
	{
		( *i )( page );
	}
}

inline void PreferencesPageCallbacks_pushBack( PreferencesPageCallbacks& callbacks, const PreferencesPageCallback& callback ){
	callbacks.push_back( callback );
}

PreferencesPageCallbacks g_interfacePreferences;
void PreferencesDialog_addInterfacePreferences( const PreferencesPageCallback& callback ){
	PreferencesPageCallbacks_pushBack( g_interfacePreferences, callback );
}
PreferenceGroupCallbacks g_interfaceCallbacks;
void PreferencesDialog_addInterfacePage( const PreferenceGroupCallback& callback ){
	PreferenceGroupCallbacks_pushBack( g_interfaceCallbacks, callback );
}

PreferencesPageCallbacks g_displayPreferences;
void PreferencesDialog_addDisplayPreferences( const PreferencesPageCallback& callback ){
	PreferencesPageCallbacks_pushBack( g_displayPreferences, callback );
}
PreferenceGroupCallbacks g_displayCallbacks;
void PreferencesDialog_addDisplayPage( const PreferenceGroupCallback& callback ){
	PreferenceGroupCallbacks_pushBack( g_displayCallbacks, callback );
}

PreferencesPageCallbacks g_settingsPreferences;
void PreferencesDialog_addSettingsPreferences( const PreferencesPageCallback& callback ){
	PreferencesPageCallbacks_pushBack( g_settingsPreferences, callback );
}
PreferenceGroupCallbacks g_settingsCallbacks;
void PreferencesDialog_addSettingsPage( const PreferenceGroupCallback& callback ){
	PreferenceGroupCallbacks_pushBack( g_settingsCallbacks, callback );
}

void Widget_updateDependency( ui::Widget self, ui::Widget toggleButton ){
	gtk_widget_set_sensitive( self, gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( toggleButton ) ) && gtk_widget_is_sensitive( toggleButton ) );
}

void ToggleButton_toggled_Widget_updateDependency( ui::Widget toggleButton, ui::Widget self ){
	Widget_updateDependency( self, toggleButton );
}

void ToggleButton_state_changed_Widget_updateDependency( ui::Widget toggleButton, GtkStateType state, ui::Widget self ){
	if ( state == GTK_STATE_INSENSITIVE ) {
		Widget_updateDependency( self, toggleButton );
	}
}

void Widget_connectToggleDependency( ui::Widget self, ui::Widget toggleButton ){
	toggleButton.connect( "state_changed", G_CALLBACK( ToggleButton_state_changed_Widget_updateDependency ), self );
	toggleButton.connect( "toggled", G_CALLBACK( ToggleButton_toggled_Widget_updateDependency ), self );
	Widget_updateDependency( self, toggleButton );
}


inline ui::VBox getVBox( ui::Bin page ){
	return ui::VBox::from(gtk_bin_get_child(page));
}

GtkTreeIter PreferenceTree_appendPage( ui::TreeStore store, GtkTreeIter* parent, const char* name, ui::Widget page ){
	GtkTreeIter group;
	gtk_tree_store_append( store, &group, parent );
	gtk_tree_store_set( store, &group, 0, name, 1, page, -1 );
	return group;
}

ui::Bin PreferencePages_addPage( ui::Widget notebook, const char* name ){
	ui::Widget preflabel = ui::Label( name );
	preflabel.show();

	auto pageframe = ui::Frame( name );
	gtk_container_set_border_width( GTK_CONTAINER( pageframe ), 4 );
	pageframe.show();

	ui::Widget vbox = ui::VBox( FALSE, 4 );
	vbox.show();
	gtk_container_set_border_width( GTK_CONTAINER( vbox ), 4 );
	pageframe.add(vbox);

	// Add the page to the notebook
	gtk_notebook_append_page( GTK_NOTEBOOK( notebook ), pageframe, preflabel );

	return pageframe;
}

class PreferenceTreeGroup : public PreferenceGroup
{
Dialog& m_dialog;
ui::Widget m_notebook;
ui::TreeStore m_store;
GtkTreeIter m_group;
public:
PreferenceTreeGroup( Dialog& dialog, ui::Widget notebook, ui::TreeStore store, GtkTreeIter group ) :
	m_dialog( dialog ),
	m_notebook( notebook ),
	m_store( store ),
	m_group( group ){
}
PreferencesPage createPage( const char* treeName, const char* frameName ){
	auto page = PreferencePages_addPage( m_notebook, frameName );
	PreferenceTree_appendPage( m_store, &m_group, treeName, page );
	return PreferencesPage( m_dialog, getVBox( page ) );
}
};

ui::Window PrefsDlg::BuildDialog(){
	PreferencesDialog_addInterfacePreferences( makeCallbackF(Interface_constructPreferences) );
	Mouse_registerPreferencesPage();

	ui::Window dialog = ui::Window(create_floating_window( RADIANT_NAME " Preferences", m_parent ));

	gtk_window_set_transient_for( dialog, m_parent );
	gtk_window_set_position( dialog, GTK_WIN_POS_CENTER_ON_PARENT );

	{
		auto mainvbox = ui::VBox( FALSE, 5 );
		dialog.add(mainvbox);
		gtk_container_set_border_width( GTK_CONTAINER( mainvbox ), 5 );
		mainvbox.show();

		{
			auto hbox = ui::HBox( FALSE, 5 );
			hbox.show();
			mainvbox.pack_end(hbox, FALSE, TRUE, 0);

			{
				auto button = create_dialog_button( "OK", G_CALLBACK( dialog_button_ok ), &m_modal );
				hbox.pack_end(button, FALSE, FALSE, 0);
			}
			{
				auto button = create_dialog_button( "Cancel", G_CALLBACK( dialog_button_cancel ), &m_modal );
				hbox.pack_end(button, FALSE, FALSE, 0);
			}
			{
				auto button = create_dialog_button( "Clean", G_CALLBACK( OnButtonClean ), this );
				hbox.pack_end(button, FALSE, FALSE, 0);
			}
		}

		{
			auto hbox = ui::HBox( FALSE, 5 );
			mainvbox.pack_start( hbox, TRUE, TRUE, 0 );
			hbox.show();

			{
				auto sc_win = ui::ScrolledWindow(ui::New);
				gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( sc_win ), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );
				hbox.pack_start( sc_win, FALSE, FALSE, 0 );
				sc_win.show();
				gtk_scrolled_window_set_shadow_type( GTK_SCROLLED_WINDOW( sc_win ), GTK_SHADOW_IN );

				// prefs pages notebook
				m_notebook = ui::Widget::from(gtk_notebook_new());
				// hide the notebook tabs since its not supposed to look like a notebook
				gtk_notebook_set_show_tabs( GTK_NOTEBOOK( m_notebook ), FALSE );
				hbox.pack_start( m_notebook, TRUE, TRUE, 0 );
				m_notebook.show();


				{
					auto store = ui::TreeStore::from(gtk_tree_store_new( 2, G_TYPE_STRING, G_TYPE_POINTER ));

					auto view = ui::TreeView(ui::TreeModel::from(store._handle));
					gtk_tree_view_set_headers_visible(view, FALSE );

					{
						auto renderer = ui::CellRendererText(ui::New);
                        auto column = ui::TreeViewColumn( "Preferences", renderer, {{"text", 0}} );
						gtk_tree_view_append_column(view, column );
					}

					{
						auto selection = ui::TreeSelection::from(gtk_tree_view_get_selection(view));
						selection.connect( "changed", G_CALLBACK( treeSelection ), this );
					}

					view.show();

					sc_win.add(view);

					{
						/********************************************************************/
						/* Add preference tree options                                      */
						/********************************************************************/
						// Front page...
						//GtkWidget* front =
						PreferencePages_addPage( m_notebook, "Front Page" );

						{
							auto global = PreferencePages_addPage( m_notebook, "Global Preferences" );
							{
								PreferencesPage preferencesPage( *this, getVBox( global ) );
								Global_constructPreferences( preferencesPage );
							}
                            auto group = PreferenceTree_appendPage( store, 0, "Global", global );
							{
								auto game = PreferencePages_addPage( m_notebook, "Game" );
								PreferencesPage preferencesPage( *this, getVBox( game ) );
								g_GamesDialog.CreateGlobalFrame( preferencesPage );

								PreferenceTree_appendPage( store, &group, "Game", game );
							}
						}

						{
							auto interfacePage = PreferencePages_addPage( m_notebook, "Interface Preferences" );
							{
								PreferencesPage preferencesPage( *this, getVBox( interfacePage ) );
								PreferencesPageCallbacks_constructPage( g_interfacePreferences, preferencesPage );
							}

                            auto group = PreferenceTree_appendPage( store, 0, "Interface", interfacePage );
							PreferenceTreeGroup preferenceGroup( *this, m_notebook, store, group );

							PreferenceGroupCallbacks_constructGroup( g_interfaceCallbacks, preferenceGroup );
						}

						{
							auto display = PreferencePages_addPage( m_notebook, "Display Preferences" );
							{
								PreferencesPage preferencesPage( *this, getVBox( display ) );
								PreferencesPageCallbacks_constructPage( g_displayPreferences, preferencesPage );
							}
                            auto group = PreferenceTree_appendPage( store, 0, "Display", display );
							PreferenceTreeGroup preferenceGroup( *this, m_notebook, store, group );

							PreferenceGroupCallbacks_constructGroup( g_displayCallbacks, preferenceGroup );
						}

						{
							auto settings = PreferencePages_addPage( m_notebook, "General Settings" );
							{
								PreferencesPage preferencesPage( *this, getVBox( settings ) );
								PreferencesPageCallbacks_constructPage( g_settingsPreferences, preferencesPage );
							}

                            auto group = PreferenceTree_appendPage( store, 0, "Settings", settings );
							PreferenceTreeGroup preferenceGroup( *this, m_notebook, store, group );

							PreferenceGroupCallbacks_constructGroup( g_settingsCallbacks, preferenceGroup );
						}
					}

					gtk_tree_view_expand_all(view );

					g_object_unref( G_OBJECT( store ) );
				}
			}
		}
	}

	gtk_notebook_set_current_page( GTK_NOTEBOOK( m_notebook ), 0 );

	return dialog;
}

preferences_globals_t g_preferences_globals;

PrefsDlg g_Preferences;               // global prefs instance


void PreferencesDialog_constructWindow( ui::Window main_window ){
	g_Preferences.m_parent = main_window;
	g_Preferences.Create();
}
void PreferencesDialog_destroyWindow(){
	g_Preferences.Destroy();
}


PreferenceDictionary g_preferences;

PreferenceSystem& GetPreferenceSystem(){
	return g_preferences;
}

class PreferenceSystemAPI
{
PreferenceSystem* m_preferencesystem;
public:
typedef PreferenceSystem Type;
STRING_CONSTANT( Name, "*" );

PreferenceSystemAPI(){
	m_preferencesystem = &GetPreferenceSystem();
}
PreferenceSystem* getTable(){
	return m_preferencesystem;
}
};

#include "modulesystem/singletonmodule.h"
#include "modulesystem/moduleregistry.h"

typedef SingletonModule<PreferenceSystemAPI> PreferenceSystemModule;
typedef Static<PreferenceSystemModule> StaticPreferenceSystemModule;
StaticRegisterModule staticRegisterPreferenceSystem( StaticPreferenceSystemModule::instance() );

void Preferences_Load(){
	g_GamesDialog.LoadPrefs();

	globalOutputStream() << "loading local preferences from " << g_Preferences.m_inipath->str << "\n";

	if ( !Preferences_Load( g_preferences, g_Preferences.m_inipath->str, g_GamesDialog.m_sGameFile.c_str() ) ) {
		globalOutputStream() << "failed to load local preferences from " << g_Preferences.m_inipath->str << "\n";
	}
}

void Preferences_Save(){
	if ( g_preferences_globals.disable_ini ) {
		return;
	}

	// save global preferences
	g_GamesDialog.SavePrefs();

	globalOutputStream() << "saving local preferences to " << g_Preferences.m_inipath->str << "\n";

	if ( !Preferences_Save_Safe( g_preferences, g_Preferences.m_inipath->str ) ) {
		globalOutputStream() << "failed to save local preferences to " << g_Preferences.m_inipath->str << "\n";
	}
}

void Preferences_Reset(){
	file_remove( g_Preferences.m_inipath->str );
}


void PrefsDlg::PostModal( EMessageBoxReturn code ){
	if ( code == eIDOK ) {
		Preferences_Save();
		UpdateAllWindows();
	}
}

std::vector<const char*> g_restart_required;

void PreferencesDialog_restartRequired( const char* staticName ){
	g_restart_required.push_back( staticName );
}

bool PreferencesDialog_isRestartRequired(){
	return !g_restart_required.empty();
}

void PreferencesDialog_restartIfRequired(){
	if ( !g_restart_required.empty() ) {
		StringOutputStream message( 256 );
		message << "Preference changes require a restart:\n\n";

		for ( std::vector<const char*>::iterator i = g_restart_required.begin(); i != g_restart_required.end(); ++i )
		{
			message << ( *i ) << '\n';
		}

		message << "\nRestart now?";

		auto ret = ui::alert( MainFrame_getWindow(), message.c_str(), "Restart " RADIANT_NAME "?", ui::alert_type::YESNO, ui::alert_icon::Question );

		g_restart_required.clear();

		if ( ret == ui::alert_response::YES ) {
			g_GamesDialog.m_bSkipGamePromptOnce = true;
			Radiant_Restart();
		}
	}
}

void PreferencesDialog_showDialog(){
	if ( ConfirmModified( "Edit Preferences" ) && g_Preferences.DoModal() == eIDOK ) {
		PreferencesDialog_restartIfRequired();
	}
}

struct GameName {
	static void Export(const Callback<void(const char *)> &returnz) {
		returnz(gamename_get());
	}

	static void Import(const char *value) {
		gamename_set(value);
	}
};

struct GameMode {
	static void Export(const Callback<void(const char *)> &returnz) {
		returnz(gamemode_get());
	}

	static void Import(const char *value) {
		gamemode_set(value);
	}
};

void RegisterPreferences( PreferenceSystem& preferences ){
#if GDEF_OS_WINDOWS
	preferences.registerPreference( "UseCustomShaderEditor", make_property_string( g_TextEditor_useWin32Editor ) );
#else
	preferences.registerPreference( "UseCustomShaderEditor", make_property_string( g_TextEditor_useCustomEditor ) );
	preferences.registerPreference( "CustomShaderEditorCommand", make_property_string( g_TextEditor_editorCommand ) );
#endif

	preferences.registerPreference( "GameName", make_property<GameName>() );
	preferences.registerPreference( "GameMode", make_property<GameMode>() );
}

void Preferences_Init(){
	RegisterPreferences( GetPreferenceSystem() );
}
