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

/*
   The following source code is licensed by Id Software and subject to the terms of
   its LIMITED USE SOFTWARE LICENSE AGREEMENT, a copy of which is included with
   GtkRadiant. If you did not receive a LIMITED USE SOFTWARE LICENSE AGREEMENT,
   please contact Id Software immediately at info@idsoftware.com.
 */

#if !defined( INCLUDED_PREFERENCES_H )
#define INCLUDED_PREFERENCES_H

#include "libxml/parser.h"
#include "dialog.h"
#include <list>
#include <map>
#include "property.h"

void Widget_connectToggleDependency( ui::Widget self, ui::Widget toggleButton );

class PreferencesPage
{
Dialog& m_dialog;
ui::VBox m_vbox;
public:
PreferencesPage( Dialog& dialog, ui::VBox vbox ) : m_dialog( dialog ), m_vbox( vbox ){
}
ui::CheckButton appendCheckBox( const char* name, const char* flag, bool& data ){
	return m_dialog.addCheckBox( m_vbox, name, flag, data );
}
ui::CheckButton appendCheckBox( const char* name, const char* flag, Property<bool> const &cb ){
	return m_dialog.addCheckBox( m_vbox, name, flag, cb );
}
void appendCombo( const char* name, StringArrayRange values, Property<int> const &cb ){
	m_dialog.addCombo( m_vbox, name, values, cb );
}
void appendCombo( const char* name, int& data, StringArrayRange values ){
	m_dialog.addCombo( m_vbox, name, data, values );
}
void appendSlider( const char* name, int& data, gboolean draw_value, const char* low, const char* high, double value, double lower, double upper, double step_increment, double page_increment ){
	m_dialog.addSlider( m_vbox, name, data, draw_value, low, high, value, lower, upper, step_increment, page_increment );
}
void appendRadio( const char* name, StringArrayRange names, Property<int> const &cb ){
	m_dialog.addRadio( m_vbox, name, names, cb );
}
void appendRadio( const char* name, int& data, StringArrayRange names ){
	m_dialog.addRadio( m_vbox, name, data, names );
}
void appendRadioIcons( const char* name, StringArrayRange icons, Property<int> const &cb ){
	m_dialog.addRadioIcons( m_vbox, name, icons, cb );
}
void appendRadioIcons( const char* name, int& data, StringArrayRange icons ){
	m_dialog.addRadioIcons( m_vbox, name, data, icons );
}
ui::Widget appendSpacer( int dimension ){
	return m_dialog.addSpacer( m_vbox, dimension );
}
ui::Widget appendLabel( const char* name, const char* text ){
	return m_dialog.addLabel( m_vbox, name, text );
}
ui::Widget appendEntry( const char* name, Property<int> const &cb ){
	return m_dialog.addIntEntry( m_vbox, name, cb );
}
ui::Widget appendEntry( const char* name, int& data ){
	return m_dialog.addEntry( m_vbox, name, data );
}
ui::Widget appendEntry( const char* name, Property<std::size_t> const &cb){
	return m_dialog.addSizeEntry( m_vbox, name, cb );
}
ui::Widget appendEntry( const char* name, std::size_t& data ){
	return m_dialog.addEntry( m_vbox, name, data );
}
ui::Widget appendEntry( const char* name, Property<float> const &cb ){
	return m_dialog.addFloatEntry( m_vbox, name, cb );
}
ui::Widget appendEntry( const char* name, float& data ){
	return m_dialog.addEntry( m_vbox, name, data );
}
ui::Widget appendPathEntry( const char* name, bool browse_directory, Property<const char *> const &cb ){
	return m_dialog.addPathEntry( m_vbox, name, browse_directory, cb );
}
ui::Widget appendPathEntry( const char* name, CopiedString& data, bool directory ){
	return m_dialog.addPathEntry( m_vbox, name, data, directory );
}
ui::SpinButton appendSpinner( const char* name, int& data, double value, double lower, double upper ){
	return m_dialog.addSpinner( m_vbox, name, data, value, lower, upper );
}
ui::SpinButton appendSpinner( const char* name, double value, double lower, double upper, Property<int> const &cb ){
	return m_dialog.addSpinner( m_vbox, name, value, lower, upper, cb );
}
ui::SpinButton appendSpinner( const char* name, double value, double lower, double upper, Property<float> const &cb ){
	return m_dialog.addSpinner( m_vbox, name, value, lower, upper, cb );
}
};

typedef Callback<void(PreferencesPage&)> PreferencesPageCallback;

class PreferenceGroup
{
public:
virtual PreferencesPage createPage( const char* treeName, const char* frameName ) = 0;
};

typedef Callback<void(PreferenceGroup&)> PreferenceGroupCallback;

void PreferencesDialog_addInterfacePreferences( const PreferencesPageCallback& callback );
void PreferencesDialog_addInterfacePage( const PreferenceGroupCallback& callback );
void PreferencesDialog_addDisplayPreferences( const PreferencesPageCallback& callback );
void PreferencesDialog_addDisplayPage( const PreferenceGroupCallback& callback );
void PreferencesDialog_addSettingsPreferences( const PreferencesPageCallback& callback );
void PreferencesDialog_addSettingsPage( const PreferenceGroupCallback& callback );

bool PreferencesDialog_isRestartRequired();
void PreferencesDialog_restartRequired( const char* staticName );

template<typename Value>
class LatchedValue {
public:
    Value m_value;
    Value m_latched;
    const char *m_description;

    LatchedValue(Value value, const char *description) : m_latched(value), m_description(description) {
    }

    void useLatched() {
        m_value = m_latched;
    }
};

template<class T>
struct PropertyImpl<LatchedValue<T>, T> {
	static void Export(const LatchedValue<T> &self, const Callback<void(T)> &returnz) {
		returnz(self.m_latched);
	}

	static void Import(LatchedValue<T> &self, T value) {
		self.m_latched = value;
		if (value != self.m_value) {
			PreferencesDialog_restartRequired(self.m_description);
		}
	}
};

template<class T>
Property<T> make_property(LatchedValue<T> &self) {
	return make_property<LatchedValue<T>, T>(self);
}

/*!
   holds information for a given game
   I'm a bit unclear on that still
   it holds game specific configuration stuff
   such as base names, engine names, some game specific features to activate in the various modules
   it is not strictly a prefs thing since the user is not supposed to edit that (unless he is hacking
   support for a new game)

   what we do now is fully generate the information for this during the setup. We might want to
   generate a piece that just says "the game pack is there", but put the rest of the config somwhere
   else (i.e. not generated, copied over during setup .. for instance in the game tools directory)
 */
class CGameDescription
{
typedef std::map<CopiedString, CopiedString> GameDescription;

public:
CopiedString mGameFile;   ///< the .game file that describes this game
GameDescription m_gameDescription;

CopiedString mGameToolsPath;   ///< the explicit path to the game-dependent modules
CopiedString mGameType;   ///< the type of the engine

const char* getKeyValue( const char* key ) const {
	GameDescription::const_iterator i = m_gameDescription.find( key );
	if ( i != m_gameDescription.end() ) {
		return ( *i ).second.c_str();
	}
	return "";
}
const char* getRequiredKeyValue( const char* key ) const {
	GameDescription::const_iterator i = m_gameDescription.find( key );
	if ( i != m_gameDescription.end() ) {
		return ( *i ).second.c_str();
	}
	ERROR_MESSAGE( "game attribute " << makeQuoted( key ) << " not found in " << makeQuoted( mGameFile.c_str() ) );
	return "";
}

CGameDescription( xmlDocPtr pDoc, const CopiedString &GameFile );

void Dump();
};

extern CGameDescription *g_pGameDescription;

class PrefsDlg;

class PreferencesPage;

class StringOutputStream;

/*!
   standalone dialog for games selection, and more generally global settings
 */
class CGameDialog : public Dialog
{
protected:

mutable int m_nComboSelect;   ///< intermediate int value for combo in dialog box

public:

/*!
  used to no ask for restart when switching game from Gobal Preferences window displayed on startup
*/

bool onStartup;
/*!
   those settings are saved in the global prefs file
   I'm too lazy to wrap behind protected access, not sure this needs to be public
   NOTE: those are preference settings. if you change them it is likely that you would
   have to restart the editor for them to take effect
 */
/*@{*/
/*!
   what game has been selected
   this is the name of the .game file
 */
CopiedString m_sGameFile;
/*!
   prompt which game to load on startup
 */
bool m_bGamePrompt;
/*!
   when if m_bGamePrompt is true
   do not prompt at startup which game to load this time, but prompt the next times
   this is used to not uselessly prompt game after having restarted because user switched game
 */
bool m_bSkipGamePromptOnce;
/*!
   log console to radiant.log
   m_bForceLogConsole is an obscure forced latching situation
 */
bool m_bForceLogConsole;
/*@}*/

/*!
   the list of game descriptions we scanned from the game/ dir
 */
std::list<CGameDescription*> mGames;

CGameDialog() :
	m_sGameFile( "" ),
	m_bGamePrompt( true ),
	m_bSkipGamePromptOnce( false ),
	m_bForceLogConsole( false ){
}
virtual ~CGameDialog();

/*!
   intialize the game dialog, called at CPrefsDlg::Init
   will scan for games, load prefs, and do game selection dialog if needed
 */
void Init();

/*!
   reset the global settings by removing the file
 */
void Reset();

/*!
   run the dialog UI for the list of games
 */
void DoGameDialog();

/*!
   Dialog API
   this is only called when the dialog is built at startup for main engine select
 */
ui::Window BuildDialog();

void GameFileImport( int value );
void GameFileExport( const Callback<void(int)> & importCallback ) const;

/*!
   construction of the dialog frame
   this is the part to be re-used in prefs dialog
   for the standalone dialog, we include this in a modal box
   for prefs, we hook the frame in the main notebook
   build the frame on-demand (only once)
 */
void CreateGlobalFrame( PreferencesPage& page );

/*!
   global preferences subsystem
   XML-based this time, hopefully this will generalize to other prefs
   LoadPrefs has hardcoded defaults
   NOTE: it may not be strictly 'CGameDialog' to put the global prefs here
   could have named the class differently I guess
 */
/*@{*/
void LoadPrefs();   ///< load from file into variables
void SavePrefs();   ///< save pref variables to file
/*@}*/

private:
/*!
   scan for .game files, load them
 */
void ScanForGames();

/*!
   inits g_Preferences.m_global_rc_path
 */
void InitGlobalPrefPath();

/*!
   uses m_nComboItem to find the right mGames
 */
CGameDescription *GameDescriptionForComboItem();
};

/*!
   this holds global level preferences
 */
extern CGameDialog g_GamesDialog;


class texdef_t;

class PrefsDlg : public Dialog
{
public:
protected:
std::list<CGameDescription *> mGames;

public:

ui::Widget m_notebook{ui::null};

virtual ~PrefsDlg(){
	g_string_free( m_rc_path, true );
	g_string_free( m_inipath, true );
}

/*!
   path for global settings
   win32: AppPath
   linux: ~/.radiant/[version]/
 */
GString *m_global_rc_path;

/*!
   path to per-game settings
   used for various game dependant storage
   win32: GameToolsPath
   linux: ~/.radiant/[version]/[gamename]/
 */
GString *m_rc_path;

/*!
   holds per-game settings
   m_rc_path+"local.pref"
   \todo FIXME at some point this should become XML property bag code too
 */
GString *m_inipath;

// initialize the above paths
void Init();

/*! Utility function for swapping notebook pages for tree list selections */
void showPrefPage( ui::Widget prefpage );

protected:

/*! Dialog API */
ui::Window BuildDialog();
void PostModal( EMessageBoxReturn code );
};

extern PrefsDlg g_Preferences;

struct preferences_globals_t
{
	// disabled all INI / registry read write .. used when shutting down after registry cleanup
	bool disable_ini;
	preferences_globals_t() : disable_ini( false ){
	}
};
extern preferences_globals_t g_preferences_globals;

void PreferencesDialog_constructWindow( ui::Window main_window );
void PreferencesDialog_destroyWindow();


void PreferencesDialog_restartIfRequired();
void PreferencesDialog_showDialog();

void GlobalPreferences_Init();
void Preferences_Init();

void Preferences_Load();
void Preferences_Save();

void Preferences_Reset();


#endif
