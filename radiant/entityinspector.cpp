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

#include "entityinspector.h"

#include "debugging/debugging.h"
#include <gtk/gtk.h>

#include "ientity.h"
#include "ifilesystem.h"
#include "imodel.h"
#include "iscenegraph.h"
#include "iselection.h"
#include "iundo.h"

#include <map>
#include <set>
#include <gdk/gdkkeysyms.h>
#include <uilib/uilib.h>
#include <gtk/gtkstock.h>


#include "os/path.h"
#include "eclasslib.h"
#include "scenelib.h"
#include "generic/callback.h"
#include "os/file.h"
#include "stream/stringstream.h"
#include "moduleobserver.h"
#include "convert.h"
#include "stringio.h"

#include "gtkutil/accelerator.h"
#include "gtkutil/dialog.h"
#include "gtkutil/filechooser.h"
#include "gtkutil/messagebox.h"
#include "gtkutil/nonmodal.h"
#include "gtkutil/button.h"
#include "gtkutil/entry.h"
#include "gtkutil/container.h"

#include "qe3.h"
#include "gtkmisc.h"
#include "gtkdlgs.h"
#include "entity.h"
#include "mainframe.h"
#include "textureentry.h"
#include "groupdialog.h"

#include "select.h"

ui::Entry numeric_entry_new(){
	auto entry = ui::Entry(ui::New);
	entry.show();
	entry.dimensions(64, -1);
	return entry;
}

namespace
{
typedef std::map<CopiedString, CopiedString> KeyValues;
KeyValues g_selectedKeyValues;
KeyValues g_selectedDefaultKeyValues;
}

const char* SelectedEntity_getValueForKey( const char* key ){
	{
		KeyValues::const_iterator i = g_selectedKeyValues.find( key );
		if ( i != g_selectedKeyValues.end() ) {
			return ( *i ).second.c_str();
		}
	}
	{
		KeyValues::const_iterator i = g_selectedDefaultKeyValues.find( key );
		if ( i != g_selectedDefaultKeyValues.end() ) {
			return ( *i ).second.c_str();
		}
	}
	return "";
}

void Scene_EntitySetKeyValue_Selected_Undoable( const char* key, const char* value ){
	StringOutputStream command( 256 );
	command << "entitySetKeyValue -key " << makeQuoted( key ) << " -value " << makeQuoted( value );
	UndoableCommand undo( command.c_str() );
	Scene_EntitySetKeyValue_Selected( key, value );
}

class EntityAttribute
{
public:
virtual ~EntityAttribute() = default;
virtual ui::Widget getWidget() const = 0;
virtual void update() = 0;
virtual void release() = 0;
};

class BooleanAttribute : public EntityAttribute
{
CopiedString m_key;
ui::CheckButton m_check;

static gboolean toggled( ui::Widget widget, BooleanAttribute* self ){
	self->apply();
	return FALSE;
}
public:
BooleanAttribute( const char* key ) :
	m_key( key ),
	m_check( ui::null ){
	auto check = ui::CheckButton(ui::New);
	check.show();

	m_check = check;

	guint handler = check.connect( "toggled", G_CALLBACK( toggled ), this );
	g_object_set_data( G_OBJECT( check ), "handler", gint_to_pointer( handler ) );

	update();
}
ui::Widget getWidget() const {
	return m_check;
}
void release(){
	delete this;
}
void apply(){
	Scene_EntitySetKeyValue_Selected_Undoable( m_key.c_str(), m_check.active() ? "1" : "" );
}
typedef MemberCaller<BooleanAttribute, void(), &BooleanAttribute::apply> ApplyCaller;

void update(){
	const char* value = SelectedEntity_getValueForKey( m_key.c_str() );
	if ( !string_empty( value ) ) {
		toggle_button_set_active_no_signal( m_check, atoi( value ) != 0 );
	}
	else
	{
		toggle_button_set_active_no_signal( m_check, false );
	}
}
typedef MemberCaller<BooleanAttribute, void(), &BooleanAttribute::update> UpdateCaller;
};


class StringAttribute : public EntityAttribute
{
CopiedString m_key;
ui::Entry m_entry;
NonModalEntry m_nonModal;
public:
StringAttribute( const char* key ) :
	m_key( key ),
	m_entry( ui::null ),
	m_nonModal( ApplyCaller( *this ), UpdateCaller( *this ) ){
	auto entry = ui::Entry(ui::New);
	entry.show();
	entry.dimensions(50, -1);

	m_entry = entry;
	m_nonModal.connect( m_entry );
}
ui::Widget getWidget() const {
	return m_entry;
}
ui::Entry getEntry() const {
	return m_entry;
}

void release(){
	delete this;
}
void apply(){
	StringOutputStream value( 64 );
	value << m_entry.text();
	Scene_EntitySetKeyValue_Selected_Undoable( m_key.c_str(), value.c_str() );
}
typedef MemberCaller<StringAttribute, void(), &StringAttribute::apply> ApplyCaller;

void update(){
	StringOutputStream value( 64 );
	value << SelectedEntity_getValueForKey( m_key.c_str() );
	m_entry.text(value.c_str());
}
typedef MemberCaller<StringAttribute, void(), &StringAttribute::update> UpdateCaller;
};

class ShaderAttribute : public StringAttribute
{
public:
ShaderAttribute( const char* key ) : StringAttribute( key ){
	GlobalShaderEntryCompletion::instance().connect( StringAttribute::getEntry() );
}
};


class ColorAttribute : public EntityAttribute
{
CopiedString m_key;
BrowsedPathEntry m_entry;
NonModalEntry m_nonModal;
public:
ColorAttribute( const char* key ) :
	m_key( key ),
	m_entry( BrowseCaller( *this ) ),
	m_nonModal( ApplyCaller( *this ), UpdateCaller( *this ) ){
	m_nonModal.connect( m_entry.m_entry.m_entry );
}
void release(){
	delete this;
}
ui::Widget getWidget() const {
	return ui::Widget( m_entry.m_entry.m_frame );
}
void apply(){
	StringOutputStream value( 64 );
	value << gtk_entry_get_text( GTK_ENTRY( m_entry.m_entry.m_entry ) );
	Scene_EntitySetKeyValue_Selected_Undoable( m_key.c_str(), value.c_str() );
}
typedef MemberCaller<ColorAttribute, void(), &ColorAttribute::apply> ApplyCaller;
void update(){
	StringOutputStream value( 64 );
	value << SelectedEntity_getValueForKey( m_key.c_str() );
	gtk_entry_set_text( GTK_ENTRY( m_entry.m_entry.m_entry ), value.c_str() );
}
typedef MemberCaller<ColorAttribute, void(), &ColorAttribute::update> UpdateCaller;
void browse( const BrowsedPathEntry::SetPathCallback& setPath ){
	//const char *filename = misc_model_dialog( gtk_widget_get_toplevel( GTK_WIDGET( m_entry.m_entry.m_frame ) ) );

	/* hijack BrowsedPathEntry to call colour chooser */
	Entity_setColour();

//	if ( filename != 0 ) {
//		setPath( filename );
//		apply();
//	}
	update();
}
typedef MemberCaller<ColorAttribute, void(const BrowsedPathEntry::SetPathCallback&), &ColorAttribute::browse> BrowseCaller;
};


class ModelAttribute : public EntityAttribute
{
CopiedString m_key;
BrowsedPathEntry m_entry;
NonModalEntry m_nonModal;
public:
ModelAttribute( const char* key ) :
	m_key( key ),
	m_entry( BrowseCaller( *this ) ),
	m_nonModal( ApplyCaller( *this ), UpdateCaller( *this ) ){
	m_nonModal.connect( m_entry.m_entry.m_entry );
}
void release(){
	delete this;
}
ui::Widget getWidget() const {
	return m_entry.m_entry.m_frame;
}
void apply(){
	StringOutputStream value( 64 );
	value << m_entry.m_entry.m_entry.text();
	Scene_EntitySetKeyValue_Selected_Undoable( m_key.c_str(), value.c_str() );
}
typedef MemberCaller<ModelAttribute, void(), &ModelAttribute::apply> ApplyCaller;
void update(){
	StringOutputStream value( 64 );
	value << SelectedEntity_getValueForKey( m_key.c_str() );
	m_entry.m_entry.m_entry.text(value.c_str());
}
typedef MemberCaller<ModelAttribute, void(), &ModelAttribute::update> UpdateCaller;
void browse( const BrowsedPathEntry::SetPathCallback& setPath ){
	const char *filename = misc_model_dialog( m_entry.m_entry.m_frame.window() );

	if ( filename != 0 ) {
		setPath( filename );
		apply();
	}
}
typedef MemberCaller<ModelAttribute, void(const BrowsedPathEntry::SetPathCallback&), &ModelAttribute::browse> BrowseCaller;
};

const char* browse_sound( ui::Widget parent ){
	StringOutputStream buffer( 1024 );

	buffer << g_qeglobals.m_userGamePath.c_str() << "sound/";

	if ( !file_readable( buffer.c_str() ) ) {
		// just go to fsmain
		buffer.clear();
		buffer << g_qeglobals.m_userGamePath.c_str() << "/";
	}

	const char* filename = parent.file_dialog(TRUE, "Open Wav File", buffer.c_str(), "sound" );
	if ( filename != 0 ) {
		const char* relative = path_make_relative( filename, GlobalFileSystem().findRoot( filename ) );
		if ( relative == filename ) {
			globalOutputStream() << "WARNING: could not extract the relative path, using full path instead\n";
		}
		return relative;
	}
	return filename;
}

class SoundAttribute : public EntityAttribute
{
CopiedString m_key;
BrowsedPathEntry m_entry;
NonModalEntry m_nonModal;
public:
SoundAttribute( const char* key ) :
	m_key( key ),
	m_entry( BrowseCaller( *this ) ),
	m_nonModal( ApplyCaller( *this ), UpdateCaller( *this ) ){
	m_nonModal.connect( m_entry.m_entry.m_entry );
}
void release(){
	delete this;
}
ui::Widget getWidget() const {
	return ui::Widget(m_entry.m_entry.m_frame );
}
void apply(){
	StringOutputStream value( 64 );
	value << m_entry.m_entry.m_entry.text();
	Scene_EntitySetKeyValue_Selected_Undoable( m_key.c_str(), value.c_str() );
}
typedef MemberCaller<SoundAttribute, void(), &SoundAttribute::apply> ApplyCaller;
void update(){
	StringOutputStream value( 64 );
	value << SelectedEntity_getValueForKey( m_key.c_str() );
	m_entry.m_entry.m_entry.text(value.c_str());
}
typedef MemberCaller<SoundAttribute, void(), &SoundAttribute::update> UpdateCaller;
void browse( const BrowsedPathEntry::SetPathCallback& setPath ){
	const char *filename = browse_sound( m_entry.m_entry.m_frame.window() );

	if ( filename != 0 ) {
		setPath( filename );
		apply();
	}
}
typedef MemberCaller<SoundAttribute, void(const BrowsedPathEntry::SetPathCallback&), &SoundAttribute::browse> BrowseCaller;
};

inline double angle_normalised( double angle ){
	return float_mod( angle, 360.0 );
}

class AngleAttribute : public EntityAttribute
{
CopiedString m_key;
ui::Entry m_entry;
NonModalEntry m_nonModal;
public:
AngleAttribute( const char* key ) :
	m_key( key ),
	m_entry( ui::null ),
	m_nonModal( ApplyCaller( *this ), UpdateCaller( *this ) ){
	auto entry = numeric_entry_new();
	m_entry = entry;
	m_nonModal.connect( m_entry );
}
void release(){
	delete this;
}
ui::Widget getWidget() const {
	return ui::Widget(m_entry );
}
void apply(){
	StringOutputStream angle( 32 );
	angle << angle_normalised( entry_get_float( m_entry ) );
	Scene_EntitySetKeyValue_Selected_Undoable( m_key.c_str(), angle.c_str() );
}
typedef MemberCaller<AngleAttribute, void(), &AngleAttribute::apply> ApplyCaller;

void update(){
	const char* value = SelectedEntity_getValueForKey( m_key.c_str() );
	if ( !string_empty( value ) ) {
		StringOutputStream angle( 32 );
		angle << angle_normalised( atof( value ) );
		m_entry.text(angle.c_str());
	}
	else
	{
		m_entry.text("0");
	}
}
typedef MemberCaller<AngleAttribute, void(), &AngleAttribute::update> UpdateCaller;
};

namespace
{
typedef const char* String;
const String buttons[] = { "up", "down", "z-axis" };
}

class DirectionAttribute : public EntityAttribute
{
CopiedString m_key;
ui::Entry m_entry;
NonModalEntry m_nonModal;
RadioHBox m_radio;
NonModalRadio m_nonModalRadio;
ui::HBox m_hbox{ui::null};
public:
DirectionAttribute( const char* key ) :
	m_key( key ),
	m_entry( ui::null ),
	m_nonModal( ApplyCaller( *this ), UpdateCaller( *this ) ),
	m_radio( RadioHBox_new( STRING_ARRAY_RANGE( buttons ) ) ),
	m_nonModalRadio( ApplyRadioCaller( *this ) ){
	auto entry = numeric_entry_new();
	m_entry = entry;
	m_nonModal.connect( m_entry );

	m_nonModalRadio.connect( m_radio.m_radio );

	m_hbox = ui::HBox( FALSE, 4 );
	m_hbox.show();

	m_hbox.pack_start( m_radio.m_hbox, TRUE, TRUE, 0 );
	m_hbox.pack_start( m_entry, TRUE, TRUE, 0 );
}
void release(){
	delete this;
}
ui::Widget getWidget() const {
	return ui::Widget(m_hbox );
}
void apply(){
	StringOutputStream angle( 32 );
	angle << angle_normalised( entry_get_float( m_entry ) );
	Scene_EntitySetKeyValue_Selected_Undoable( m_key.c_str(), angle.c_str() );
}
typedef MemberCaller<DirectionAttribute, void(), &DirectionAttribute::apply> ApplyCaller;

void update(){
	const char* value = SelectedEntity_getValueForKey( m_key.c_str() );
	if ( !string_empty( value ) ) {
		float f = float(atof( value ) );
		if ( f == -1 ) {
			gtk_widget_set_sensitive( m_entry , FALSE );
			radio_button_set_active_no_signal( m_radio.m_radio, 0 );
			m_entry.text("");
		}
		else if ( f == -2 ) {
			gtk_widget_set_sensitive( m_entry , FALSE );
			radio_button_set_active_no_signal( m_radio.m_radio, 1 );
			m_entry.text("");
		}
		else
		{
			gtk_widget_set_sensitive( m_entry , TRUE );
			radio_button_set_active_no_signal( m_radio.m_radio, 2 );
			StringOutputStream angle( 32 );
			angle << angle_normalised( f );
			m_entry.text(angle.c_str());
		}
	}
	else
	{
		m_entry.text("0");
	}
}
typedef MemberCaller<DirectionAttribute, void(), &DirectionAttribute::update> UpdateCaller;

void applyRadio(){
	int index = radio_button_get_active( m_radio.m_radio );
	if ( index == 0 ) {
		Scene_EntitySetKeyValue_Selected_Undoable( m_key.c_str(), "-1" );
	}
	else if ( index == 1 ) {
		Scene_EntitySetKeyValue_Selected_Undoable( m_key.c_str(), "-2" );
	}
	else if ( index == 2 ) {
		apply();
	}
}
typedef MemberCaller<DirectionAttribute, void(), &DirectionAttribute::applyRadio> ApplyRadioCaller;
};


class AnglesEntry
{
public:
ui::Entry m_roll;
ui::Entry m_pitch;
ui::Entry m_yaw;
AnglesEntry() : m_roll( ui::null ), m_pitch( ui::null ), m_yaw( ui::null ){
}
};

typedef BasicVector3<double> DoubleVector3;

class AnglesAttribute : public EntityAttribute
{
CopiedString m_key;
AnglesEntry m_angles;
NonModalEntry m_nonModal;
ui::HBox m_hbox;
public:
AnglesAttribute( const char* key ) :
	m_key( key ),
	m_nonModal( ApplyCaller( *this ), UpdateCaller( *this ) ),
	m_hbox(ui::HBox( TRUE, 4 ))
{
	m_hbox.show();
	{
		auto entry = numeric_entry_new();
		m_hbox.pack_start( entry, TRUE, TRUE, 0 );
		m_angles.m_pitch = entry;
		m_nonModal.connect( m_angles.m_pitch );
	}
	{
		auto entry = numeric_entry_new();
		m_hbox.pack_start( entry, TRUE, TRUE, 0 );
		m_angles.m_yaw = entry;
		m_nonModal.connect( m_angles.m_yaw );
	}
	{
		auto entry = numeric_entry_new();
		m_hbox.pack_start( entry, TRUE, TRUE, 0 );
		m_angles.m_roll = entry;
		m_nonModal.connect( m_angles.m_roll );
	}
}
void release(){
	delete this;
}
ui::Widget getWidget() const {
	return ui::Widget(m_hbox );
}
void apply(){
	StringOutputStream angles( 64 );
	angles << angle_normalised( entry_get_float( m_angles.m_pitch ) )
		   << " " << angle_normalised( entry_get_float( m_angles.m_yaw ) )
		   << " " << angle_normalised( entry_get_float( m_angles.m_roll ) );
	Scene_EntitySetKeyValue_Selected_Undoable( m_key.c_str(), angles.c_str() );
}
typedef MemberCaller<AnglesAttribute, void(), &AnglesAttribute::apply> ApplyCaller;

void update(){
	StringOutputStream angle( 32 );
	const char* value = SelectedEntity_getValueForKey( m_key.c_str() );
	if ( !string_empty( value ) ) {
		DoubleVector3 pitch_yaw_roll;
		if ( !string_parse_vector3( value, pitch_yaw_roll ) ) {
			pitch_yaw_roll = DoubleVector3( 0, 0, 0 );
		}

		angle << angle_normalised( pitch_yaw_roll.x() );
		m_angles.m_pitch.text(angle.c_str());
		angle.clear();

		angle << angle_normalised( pitch_yaw_roll.y() );
		m_angles.m_yaw.text(angle.c_str());
		angle.clear();

		angle << angle_normalised( pitch_yaw_roll.z() );
		m_angles.m_roll.text(angle.c_str());
		angle.clear();
	}
	else
	{
		m_angles.m_pitch.text("0");
		m_angles.m_yaw.text("0");
		m_angles.m_roll.text("0");
	}
}
typedef MemberCaller<AnglesAttribute, void(), &AnglesAttribute::update> UpdateCaller;
};

class Vector3Entry
{
public:
ui::Entry m_x;
ui::Entry m_y;
ui::Entry m_z;
Vector3Entry() : m_x( ui::null ), m_y( ui::null ), m_z( ui::null ){
}
};

class Vector3Attribute : public EntityAttribute
{
CopiedString m_key;
Vector3Entry m_vector3;
NonModalEntry m_nonModal;
ui::Box m_hbox{ui::null};
public:
Vector3Attribute( const char* key ) :
	m_key( key ),
	m_nonModal( ApplyCaller( *this ), UpdateCaller( *this ) ){
	m_hbox = ui::HBox( TRUE, 4 );
	m_hbox.show();
	{
		auto entry = numeric_entry_new();
		m_hbox.pack_start( entry, TRUE, TRUE, 0 );
		m_vector3.m_x = entry;
		m_nonModal.connect( m_vector3.m_x );
	}
	{
		auto entry = numeric_entry_new();
		m_hbox.pack_start( entry, TRUE, TRUE, 0 );
		m_vector3.m_y = entry;
		m_nonModal.connect( m_vector3.m_y );
	}
	{
		auto entry = numeric_entry_new();
		m_hbox.pack_start( entry, TRUE, TRUE, 0 );
		m_vector3.m_z = entry;
		m_nonModal.connect( m_vector3.m_z );
	}
}
void release(){
	delete this;
}
ui::Widget getWidget() const {
	return ui::Widget(m_hbox );
}
void apply(){
	StringOutputStream vector3( 64 );
	vector3 << entry_get_float( m_vector3.m_x )
			<< " " << entry_get_float( m_vector3.m_y )
			<< " " << entry_get_float( m_vector3.m_z );
	Scene_EntitySetKeyValue_Selected_Undoable( m_key.c_str(), vector3.c_str() );
}
typedef MemberCaller<Vector3Attribute, void(), &Vector3Attribute::apply> ApplyCaller;

void update(){
	StringOutputStream buffer( 32 );
	const char* value = SelectedEntity_getValueForKey( m_key.c_str() );
	if ( !string_empty( value ) ) {
		DoubleVector3 x_y_z;
		if ( !string_parse_vector3( value, x_y_z ) ) {
			x_y_z = DoubleVector3( 0, 0, 0 );
		}

		buffer << x_y_z.x();
		m_vector3.m_x.text(buffer.c_str());
		buffer.clear();

		buffer << x_y_z.y();
		m_vector3.m_y.text(buffer.c_str());
		buffer.clear();

		buffer << x_y_z.z();
		m_vector3.m_z.text(buffer.c_str());
		buffer.clear();
	}
	else
	{
		m_vector3.m_x.text("0");
		m_vector3.m_y.text("0");
		m_vector3.m_z.text("0");
	}
}
typedef MemberCaller<Vector3Attribute, void(), &Vector3Attribute::update> UpdateCaller;
};

class NonModalComboBox
{
Callback<void()> m_changed;
guint m_changedHandler;

static gboolean changed( ui::ComboBox widget, NonModalComboBox* self ){
	self->m_changed();
	return FALSE;
}

public:
NonModalComboBox( const Callback<void()>& changed ) : m_changed( changed ), m_changedHandler( 0 ){
}
void connect( ui::ComboBox combo ){
	m_changedHandler = combo.connect( "changed", G_CALLBACK( changed ), this );
}
void setActive( ui::ComboBox combo, int value ){
	g_signal_handler_disconnect( G_OBJECT( combo ), m_changedHandler );
	gtk_combo_box_set_active( combo, value );
	connect( combo );
}
};

class ListAttribute : public EntityAttribute
{
CopiedString m_key;
ui::ComboBox m_combo;
NonModalComboBox m_nonModal;
const ListAttributeType& m_type;
public:
ListAttribute( const char* key, const ListAttributeType& type ) :
	m_key( key ),
	m_combo( ui::null ),
	m_nonModal( ApplyCaller( *this ) ),
	m_type( type ){
	auto combo = ui::ComboBoxText(ui::New);

	for ( ListAttributeType::const_iterator i = type.begin(); i != type.end(); ++i )
	{
		gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT( combo ), ( *i ).first.c_str() );
	}

	combo.show();
	m_nonModal.connect( combo );

	m_combo = combo;
}
void release(){
	delete this;
}
ui::Widget getWidget() const {
	return ui::Widget(m_combo );
}
void apply(){
	Scene_EntitySetKeyValue_Selected_Undoable( m_key.c_str(), m_type[gtk_combo_box_get_active( m_combo )].second.c_str() );
}
typedef MemberCaller<ListAttribute, void(), &ListAttribute::apply> ApplyCaller;

void update(){
	const char* value = SelectedEntity_getValueForKey( m_key.c_str() );
	ListAttributeType::const_iterator i = m_type.findValue( value );
	if ( i != m_type.end() ) {
		m_nonModal.setActive( m_combo, static_cast<int>( std::distance( m_type.begin(), i ) ) );
	}
	else
	{
		m_nonModal.setActive( m_combo, 0 );
	}
}
typedef MemberCaller<ListAttribute, void(), &ListAttribute::update> UpdateCaller;
};


namespace
{
GtkWidget* g_entity_split0 = 0;
ui::Widget g_entity_split1{ui::null};
ui::Widget g_entity_split2{ui::null};
int g_entitysplit0_position;
int g_entitysplit1_position;
int g_entitysplit2_position;

bool g_entityInspector_windowConstructed = false;

ui::TreeView g_entityClassList{ui::null};
ui::TextView g_entityClassComment{ui::null};

GtkCheckButton* g_entitySpawnflagsCheck[MAX_FLAGS];

ui::Entry g_entityKeyEntry{ui::null};
ui::Entry g_entityValueEntry{ui::null};

GtkToggleButton* g_focusToggleButton;

ui::ListStore g_entlist_store{ui::null};
ui::ListStore g_entprops_store{ui::null};
const EntityClass* g_current_flags = 0;
const EntityClass* g_current_comment = 0;
const EntityClass* g_current_attributes = 0;

// the number of active spawnflags
int g_spawnflag_count;
// table: index, match spawnflag item to the spawnflag index (i.e. which bit)
int spawn_table[MAX_FLAGS];
// we change the layout depending on how many spawn flags we need to display
// the table is a 4x4 in which we need to put the comment box g_entityClassComment and the spawn flags..
ui::Table g_spawnflagsTable{ui::null};

ui::VBox g_attributeBox{ui::null};
typedef std::vector<EntityAttribute*> EntityAttributes;
EntityAttributes g_entityAttributes;
}

void GlobalEntityAttributes_clear(){
	for ( EntityAttributes::iterator i = g_entityAttributes.begin(); i != g_entityAttributes.end(); ++i )
	{
		( *i )->release();
	}
	g_entityAttributes.clear();
}

class GetKeyValueVisitor : public Entity::Visitor
{
KeyValues& m_keyvalues;
public:
GetKeyValueVisitor( KeyValues& keyvalues )
	: m_keyvalues( keyvalues ){
}

void visit( const char* key, const char* value ){
	m_keyvalues.insert( KeyValues::value_type( CopiedString( key ), CopiedString( value ) ) );
}

};

void Entity_GetKeyValues( const Entity& entity, KeyValues& keyvalues, KeyValues& defaultValues ){
	GetKeyValueVisitor visitor( keyvalues );

	entity.forEachKeyValue( visitor );

	const EntityClassAttributes& attributes = entity.getEntityClass().m_attributes;

	for ( EntityClassAttributes::const_iterator i = attributes.begin(); i != attributes.end(); ++i )
	{
		defaultValues.insert( KeyValues::value_type( ( *i ).first, ( *i ).second.m_value ) );
	}
}

void Entity_GetKeyValues_Selected( KeyValues& keyvalues, KeyValues& defaultValues ){
	class EntityGetKeyValues : public SelectionSystem::Visitor
	{
	KeyValues& m_keyvalues;
	KeyValues& m_defaultValues;
	mutable std::set<Entity*> m_visited;
public:
	EntityGetKeyValues( KeyValues& keyvalues, KeyValues& defaultValues )
		: m_keyvalues( keyvalues ), m_defaultValues( defaultValues ){
	}
	void visit( scene::Instance& instance ) const {
		Entity* entity = Node_getEntity( instance.path().top() );
		if ( entity == 0 && instance.path().size() != 1 ) {
			entity = Node_getEntity( instance.path().parent() );
		}
		if ( entity != 0 && m_visited.insert( entity ).second ) {
			Entity_GetKeyValues( *entity, m_keyvalues, m_defaultValues );
		}
	}
	} visitor( keyvalues, defaultValues );
	GlobalSelectionSystem().foreachSelected( visitor );
}

const char* keyvalues_valueforkey( KeyValues& keyvalues, const char* key ){
	KeyValues::iterator i = keyvalues.find( CopiedString( key ) );
	if ( i != keyvalues.end() ) {
		return ( *i ).second.c_str();
	}
	return "";
}

class EntityClassListStoreAppend : public EntityClassVisitor
{
ui::ListStore store;
public:
EntityClassListStoreAppend( ui::ListStore store_ ) : store( store_ ){
}
void visit( EntityClass* e ){
	store.append(0, e->name(), 1, e);
}
};

void EntityClassList_fill(){
	EntityClassListStoreAppend append( g_entlist_store );
	GlobalEntityClassManager().forEach( append );
}

void EntityClassList_clear(){
	g_entlist_store.clear();
}

void SetComment( EntityClass* eclass ){
	if ( eclass == g_current_comment ) {
		return;
	}

	g_current_comment = eclass;

	g_entityClassComment.text(eclass->comments());

	GtkTextBuffer* buffer = gtk_text_view_get_buffer( g_entityClassComment );
	//gtk_text_buffer_set_text( buffer, eclass->comments(), -1 );
	const char* comment = eclass->comments(), *c;
	int offset = 0, pattern_start = -1, spaces = 0;

	gtk_text_buffer_set_text( buffer, comment, -1 );

	// Catch patterns like "\nstuff :" used to describe keys and spawnflags, and make them bold for readability.

	for( c = comment; *c; ++c, ++offset ) {
		if( *c == '\n' ) {
			pattern_start = offset;
			spaces = 0;
		}
		else if( pattern_start >= 0 && ( *c < 'a' || *c > 'z' ) && ( *c < 'A' || *c > 'Z' ) && ( *c < '0' || *c > '9' ) && ( *c != '_' ) ) {
			if( *c == ':' && spaces <= 1 ) {
				GtkTextIter iter_start, iter_end;

				gtk_text_buffer_get_iter_at_offset( buffer, &iter_start, pattern_start );
				gtk_text_buffer_get_iter_at_offset( buffer, &iter_end, offset );
				gtk_text_buffer_apply_tag_by_name( buffer, "bold", &iter_start, &iter_end );
			}

			if( *c == ' ' )
				++spaces;
			else
				pattern_start = -1;
		}
	}
}

void SurfaceFlags_setEntityClass( EntityClass* eclass ){
	if ( eclass == g_current_flags ) {
		return;
	}

	g_current_flags = eclass;

	unsigned int spawnflag_count = 0;

	{
		// do a first pass to count the spawn flags, don't touch the widgets, we don't know in what state they are
		for ( int i = 0 ; i < MAX_FLAGS ; i++ )
		{
			if ( eclass->flagnames[i] && eclass->flagnames[i][0] != 0 && strcmp( eclass->flagnames[i],"-" ) ) {
				spawn_table[spawnflag_count] = i;
				spawnflag_count++;
			}
		}
	}

	// disable all remaining boxes
	// NOTE: these boxes might not even be on display
	{
		for ( int i = 0; i < g_spawnflag_count; ++i )
		{
			auto widget = ui::CheckButton::from(g_entitySpawnflagsCheck[i]);
			auto label = ui::Label::from(gtk_bin_get_child(GTK_BIN(widget)));
			label.text(" ");
			widget.hide();
			widget.ref();
			g_spawnflagsTable.remove(widget);
		}
	}

	g_spawnflag_count = spawnflag_count;

	{
		for (unsigned int i = 0; (int) i < g_spawnflag_count; ++i)
		{
			auto widget = ui::CheckButton::from(g_entitySpawnflagsCheck[i] );
			widget.show();

			StringOutputStream str( 16 );
			str << LowerCase( eclass->flagnames[spawn_table[i]] );

			g_spawnflagsTable.attach(widget, {i % 4, i % 4 + 1, i / 4, i / 4 + 1}, {GTK_FILL, GTK_FILL});
			widget.unref();

			auto label = ui::Label::from(gtk_bin_get_child(GTK_BIN(widget)) );
			label.text(str.c_str());
		}
	}
}

void EntityClassList_selectEntityClass( EntityClass* eclass ){
	auto model = g_entlist_store;
	GtkTreeIter iter;
	for ( gboolean good = gtk_tree_model_get_iter_first( model, &iter ); good != FALSE; good = gtk_tree_model_iter_next( model, &iter ) )
	{
		char* text;
		gtk_tree_model_get( model, &iter, 0, &text, -1 );
		if ( strcmp( text, eclass->name() ) == 0 ) {
			auto view = ui::TreeView(g_entityClassList);
			auto path = gtk_tree_model_get_path( model, &iter );
			gtk_tree_selection_select_path( gtk_tree_view_get_selection( view ), path );
			if ( gtk_widget_get_realized( view ) ) {
				gtk_tree_view_scroll_to_cell( view, path, 0, FALSE, 0, 0 );
			}
			gtk_tree_path_free( path );
			good = FALSE;
		}
		g_free( text );
	}
}

void EntityInspector_appendAttribute( const char* name, EntityAttribute& attribute ){
	auto row = DialogRow_new( name, attribute.getWidget() );
	DialogVBox_packRow( ui::VBox(g_attributeBox), row );
}


template<typename Attribute>
class StatelessAttributeCreator
{
public:
static EntityAttribute* create( const char* name ){
	return new Attribute( name );
}
};

class EntityAttributeFactory
{
typedef EntityAttribute* ( *CreateFunc )( const char* name );
typedef std::map<const char*, CreateFunc, RawStringLess> Creators;
Creators m_creators;
public:
EntityAttributeFactory(){
	m_creators.insert( Creators::value_type( "string", &StatelessAttributeCreator<StringAttribute>::create ) );
	m_creators.insert( Creators::value_type( "color", &StatelessAttributeCreator<ColorAttribute>::create ) );
	m_creators.insert( Creators::value_type( "integer", &StatelessAttributeCreator<StringAttribute>::create ) );
	m_creators.insert( Creators::value_type( "real", &StatelessAttributeCreator<StringAttribute>::create ) );
	m_creators.insert( Creators::value_type( "shader", &StatelessAttributeCreator<ShaderAttribute>::create ) );
	m_creators.insert( Creators::value_type( "boolean", &StatelessAttributeCreator<BooleanAttribute>::create ) );
	m_creators.insert( Creators::value_type( "angle", &StatelessAttributeCreator<AngleAttribute>::create ) );
	m_creators.insert( Creators::value_type( "direction", &StatelessAttributeCreator<DirectionAttribute>::create ) );
	m_creators.insert( Creators::value_type( "angles", &StatelessAttributeCreator<AnglesAttribute>::create ) );
	m_creators.insert( Creators::value_type( "model", &StatelessAttributeCreator<ModelAttribute>::create ) );
	m_creators.insert( Creators::value_type( "sound", &StatelessAttributeCreator<SoundAttribute>::create ) );
	m_creators.insert( Creators::value_type( "vector3", &StatelessAttributeCreator<Vector3Attribute>::create ) );
	m_creators.insert( Creators::value_type( "real3", &StatelessAttributeCreator<Vector3Attribute>::create ) );
}
EntityAttribute* create( const char* type, const char* name ){
	Creators::iterator i = m_creators.find( type );
	if ( i != m_creators.end() ) {
		return ( *i ).second( name );
	}
	const ListAttributeType* listType = GlobalEntityClassManager().findListType( type );
	if ( listType != 0 ) {
		return new ListAttribute( name, *listType );
	}
	return 0;
}
};

typedef Static<EntityAttributeFactory> GlobalEntityAttributeFactory;

void EntityInspector_setEntityClass( EntityClass *eclass ){
	EntityClassList_selectEntityClass( eclass );
	SurfaceFlags_setEntityClass( eclass );

	if ( eclass != g_current_attributes ) {
		g_current_attributes = eclass;

		container_remove_all( g_attributeBox );
		GlobalEntityAttributes_clear();

		for ( EntityClassAttributes::const_iterator i = eclass->m_attributes.begin(); i != eclass->m_attributes.end(); ++i )
		{
			EntityAttribute* attribute = GlobalEntityAttributeFactory::instance().create( ( *i ).second.m_type.c_str(), ( *i ).first.c_str() );
			if ( attribute != 0 ) {
				g_entityAttributes.push_back( attribute );
				EntityInspector_appendAttribute( EntityClassAttributePair_getName( *i ), *g_entityAttributes.back() );
			}
		}
	}
}

void EntityInspector_updateSpawnflags(){
	{
		int f = atoi( SelectedEntity_getValueForKey( "spawnflags" ) );
		for ( int i = 0; i < g_spawnflag_count; ++i )
		{
			int v = !!( f & ( 1 << spawn_table[i] ) );

			toggle_button_set_active_no_signal( ui::ToggleButton::from( g_entitySpawnflagsCheck[i] ), v );
		}
	}
	{
		// take care of the remaining ones
		for ( int i = g_spawnflag_count; i < MAX_FLAGS; ++i )
		{
			toggle_button_set_active_no_signal( ui::ToggleButton::from( g_entitySpawnflagsCheck[i] ), FALSE );
		}
	}
}

void EntityInspector_applySpawnflags(){
	int f, i, v;
	char sz[32];

	f = 0;
	for ( i = 0; i < g_spawnflag_count; ++i )
	{
		v = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( g_entitySpawnflagsCheck[i] ) );
		f |= v << spawn_table[i];
	}

	sprintf( sz, "%i", f );
	const char* value = ( f == 0 ) ? "" : sz;

	{
		StringOutputStream command;
		command << "entitySetFlags -flags " << f;
		UndoableCommand undo( "entitySetSpawnflags" );

		Scene_EntitySetKeyValue_Selected( "spawnflags", value );
	}
}


void EntityInspector_updateKeyValues(){
	g_selectedKeyValues.clear();
	g_selectedDefaultKeyValues.clear();
	Entity_GetKeyValues_Selected( g_selectedKeyValues, g_selectedDefaultKeyValues );

	EntityInspector_setEntityClass( GlobalEntityClassManager().findOrInsert( keyvalues_valueforkey( g_selectedKeyValues, "classname" ), false ) );

	EntityInspector_updateSpawnflags();

	ui::ListStore store = g_entprops_store;

	// save current key/val pair around filling epair box
	// row_select wipes it and sets to first in list
	CopiedString strKey( g_entityKeyEntry.text() );
	CopiedString strVal( g_entityValueEntry.text() );

	store.clear();
	// Walk through list and add pairs
	for ( KeyValues::iterator i = g_selectedKeyValues.begin(); i != g_selectedKeyValues.end(); ++i )
	{
		StringOutputStream key( 64 );
		key << ( *i ).first.c_str();
		StringOutputStream value( 64 );
		value << ( *i ).second.c_str();
		store.append(0, key.c_str(), 1, value.c_str());
	}

	g_entityKeyEntry.text( strKey.c_str() );
	g_entityValueEntry.text( strVal.c_str() );

	for ( EntityAttributes::const_iterator i = g_entityAttributes.begin(); i != g_entityAttributes.end(); ++i )
	{
		( *i )->update();
	}
}

class EntityInspectorDraw
{
IdleDraw m_idleDraw;
public:
EntityInspectorDraw() : m_idleDraw( makeCallbackF(EntityInspector_updateKeyValues) ){
}
void queueDraw(){
	m_idleDraw.queueDraw();
}
};

EntityInspectorDraw g_EntityInspectorDraw;


void EntityInspector_keyValueChanged(){
	g_EntityInspectorDraw.queueDraw();
}
void EntityInspector_selectionChanged( const Selectable& ){
	EntityInspector_keyValueChanged();
}

// Creates a new entity based on the currently selected brush and entity type.
//
void EntityClassList_createEntity(){
	auto view = g_entityClassList;

	// find out what type of entity we are trying to create
	GtkTreeModel* model;
	GtkTreeIter iter;
	if ( gtk_tree_selection_get_selected( gtk_tree_view_get_selection( g_entityClassList ), &model, &iter ) == FALSE ) {
		ui::alert( view.window(), "You must have a selected class to create an entity", "info" );
		return;
	}

	char* text;
	gtk_tree_model_get( model, &iter, 0, &text, -1 );

	{
		StringOutputStream command;
		command << "entityCreate -class " << text;

		UndoableCommand undo( command.c_str() );

		Entity_createFromSelection( text, g_vector3_identity );
	}
	g_free( text );
}

void EntityInspector_applyKeyValue(){
	// Get current selection text
	StringOutputStream key( 64 );
	key << gtk_entry_get_text( g_entityKeyEntry );
	StringOutputStream value( 64 );
	value << gtk_entry_get_text( g_entityValueEntry );


	// TTimo: if you change the classname to worldspawn you won't merge back in the structural brushes but create a parasite entity
	if ( !strcmp( key.c_str(), "classname" ) && !strcmp( value.c_str(), "worldspawn" ) ) {
		ui::alert( g_entityKeyEntry.window(), "Cannot change \"classname\" key back to worldspawn.", 0, ui::alert_type::OK );
		return;
	}


	// RR2DO2: we don't want spaces in entity keys
	if ( strstr( key.c_str(), " " ) ) {
		ui::alert( g_entityKeyEntry.window(), "No spaces are allowed in entity keys.", 0, ui::alert_type::OK );
		return;
	}

	if ( strcmp( key.c_str(), "classname" ) == 0 ) {
		StringOutputStream command;
		command << "entitySetClass -class " << value.c_str();
		UndoableCommand undo( command.c_str() );
		Scene_EntitySetClassname_Selected( value.c_str() );
	}
	else
	{
		Scene_EntitySetKeyValue_Selected_Undoable( key.c_str(), value.c_str() );
	}
}

void EntityInspector_clearKeyValue(){
	// Get current selection text
	StringOutputStream key( 64 );
	key << gtk_entry_get_text( g_entityKeyEntry );

	if ( strcmp( key.c_str(), "classname" ) != 0 ) {
		StringOutputStream command;
		command << "entityDeleteKey -key " << key.c_str();
		UndoableCommand undo( command.c_str() );
		Scene_EntitySetKeyValue_Selected( key.c_str(), "" );
	}
}

static gint EntityInspector_clearKeyValueKB( GtkEntry* widget, GdkEventKey* event, gpointer data ){
	if ( event->keyval == GDK_Delete ) {
		EntityInspector_clearKeyValue();
		return TRUE;
	}
	return FALSE;
}

void EntityInspector_clearAllKeyValues(){
	UndoableCommand undo( "entityClear" );

	// remove all keys except classname
	for ( KeyValues::iterator i = g_selectedKeyValues.begin(); i != g_selectedKeyValues.end(); ++i )
	{
		if ( strcmp( ( *i ).first.c_str(), "classname" ) != 0 ) {
			Scene_EntitySetKeyValue_Selected( ( *i ).first.c_str(), "" );
		}
	}
}

// =============================================================================
// callbacks

static void EntityClassList_selection_changed( ui::TreeSelection selection, gpointer data ){
	GtkTreeModel* model;
	GtkTreeIter selected;
	if ( gtk_tree_selection_get_selected( selection, &model, &selected ) ) {
		EntityClass* eclass;
		gtk_tree_model_get( model, &selected, 1, &eclass, -1 );
		if ( eclass != 0 ) {
			SetComment( eclass );
		}
	}
}

static gint EntityClassList_button_press( ui::Widget widget, GdkEventButton *event, gpointer data ){
	if ( event->type == GDK_2BUTTON_PRESS ) {
		EntityClassList_createEntity();
		return TRUE;
	}
	return FALSE;
}

static gint EntityClassList_keypress( ui::Widget widget, GdkEventKey* event, gpointer data ){
	if ( event->keyval == GDK_KEY_Return ) {
		EntityClassList_createEntity();
		return TRUE;
	}

	// select the entity that starts with the key pressed
/*
	unsigned int code = gdk_keyval_to_upper( event->keyval );
	if ( code <= 'Z' && code >= 'A' && event->state == 0 ) {
		auto view = ui::TreeView(g_entityClassList);
		GtkTreeModel* model;
		GtkTreeIter iter;
		if ( gtk_tree_selection_get_selected( gtk_tree_view_get_selection( view ), &model, &iter ) == FALSE
			 || gtk_tree_model_iter_next( model, &iter ) == FALSE ) {
			gtk_tree_model_get_iter_first( model, &iter );
		}

		for ( std::size_t count = gtk_tree_model_iter_n_children( model, 0 ); count > 0; --count )
		{
			char* text;
			gtk_tree_model_get( model, &iter, 0, &text, -1 );

			if ( toupper( text[0] ) == (int)code ) {
				auto path = gtk_tree_model_get_path( model, &iter );
				gtk_tree_selection_select_path( gtk_tree_view_get_selection( view ), path );
				if ( gtk_widget_get_realized( view ) ) {
					gtk_tree_view_scroll_to_cell( view, path, 0, FALSE, 0, 0 );
				}
				gtk_tree_path_free( path );
				count = 1;
			}

			g_free( text );

			if ( gtk_tree_model_iter_next( model, &iter ) == FALSE ) {
				gtk_tree_model_get_iter_first( model, &iter );
			}
		}

		return TRUE;
	}
*/
	return FALSE;
}

static void EntityProperties_selection_changed( ui::TreeSelection selection, gpointer data ){
	// find out what type of entity we are trying to create
	GtkTreeModel* model;
	GtkTreeIter iter;
	if ( gtk_tree_selection_get_selected( selection, &model, &iter ) == FALSE ) {
		return;
	}

	char* key;
	char* val;
	gtk_tree_model_get( model, &iter, 0, &key, 1, &val, -1 );

	g_entityKeyEntry.text( key );
	g_entityValueEntry.text( val );

	g_free( key );
	g_free( val );
}

static void SpawnflagCheck_toggled( ui::Widget widget, gpointer data ){
	EntityInspector_applySpawnflags();
}

static gint EntityEntry_keypress( ui::Entry widget, GdkEventKey* event, gpointer data ){
	if ( event->keyval == GDK_KEY_Return ) {
		if ( widget._handle == g_entityKeyEntry._handle ) {
			// g_entityValueEntry.text( "" );
			gtk_window_set_focus( widget.window(), g_entityValueEntry  );
		}
		else
		{
			EntityInspector_applyKeyValue();
		}
		return TRUE;
	}
	if ( event->keyval == GDK_Tab ) {
		if ( widget._handle == g_entityKeyEntry._handle ) {
			gtk_window_set_focus( widget.window(), g_entityValueEntry );
		}
		else
		{
			gtk_window_set_focus( widget.window(), g_entityKeyEntry );
		}
		return TRUE;
	}
	if ( event->keyval == GDK_Escape ) {
		// gtk_window_set_focus( widget.window(), NULL );
		return TRUE;
	}

	return FALSE;
}

void EntityInspector_destroyWindow( ui::Widget widget, gpointer data ){
	g_entitysplit0_position = gtk_paned_get_position( GTK_PANED( g_entity_split0 ) );
	g_entitysplit1_position = gtk_paned_get_position( GTK_PANED( g_entity_split1 ) );
	g_entitysplit2_position = gtk_paned_get_position( GTK_PANED( g_entity_split2 ) );
	g_entityInspector_windowConstructed = false;
	GlobalEntityAttributes_clear();
}

static gint EntityInspector_hideWindowKB( GtkWidget* widget, GdkEventKey* event, gpointer data ){
	//if ( event->keyval == GDK_Escape && GTK_WIDGET_VISIBLE( GTK_WIDGET( widget ) ) ) {
	if ( event->keyval == GDK_Escape  ) {
		//GroupDialog_showPage( g_page_entity );
		gtk_widget_hide( GTK_WIDGET( GroupDialog_getWindow() ) );
		return TRUE;
	}
	/* this doesn't work, if tab is bound (func is not called then) */
	if ( event->keyval == GDK_Tab ) {
		gtk_window_set_focus( GTK_WINDOW( gtk_widget_get_toplevel( GTK_WIDGET( widget ) ) ), GTK_WIDGET( g_entityKeyEntry ) );
		return TRUE;
	}
	return FALSE;
}

void EntityInspector_selectTargeting( GtkButton *button, gpointer user_data ){
	bool focus = gtk_toggle_button_get_active( g_focusToggleButton );
	Select_ConnectedEntities( true, false, focus );
}

void EntityInspector_selectTargets( GtkButton *button, gpointer user_data ){
	bool focus = gtk_toggle_button_get_active( g_focusToggleButton );
	Select_ConnectedEntities( false, true, focus );
}

void EntityInspector_selectConnected( GtkButton *button, gpointer user_data ){
	bool focus = gtk_toggle_button_get_active( g_focusToggleButton );
	Select_ConnectedEntities( true, true, focus );
}

ui::Widget EntityInspector_constructWindow( ui::Window toplevel ){
    auto vbox = ui::VBox( FALSE, 2 );
	vbox.show();
	gtk_container_set_border_width( GTK_CONTAINER( vbox ), 2 );

	g_signal_connect( G_OBJECT( toplevel ), "key_press_event", G_CALLBACK( EntityInspector_hideWindowKB ), 0 );
	vbox.connect( "destroy", G_CALLBACK( EntityInspector_destroyWindow ), 0 );

	{
        auto split1 = ui::VPaned(ui::New);
		vbox.pack_start( split1, TRUE, TRUE, 0 );
		split1.show();

		g_entity_split1 = split1;

		{
			ui::Widget split2 = ui::VPaned(ui::New);
			//gtk_paned_add1( GTK_PANED( split1 ), split2 );
			gtk_paned_pack1( GTK_PANED( split1 ), split2, FALSE, FALSE );
			split2.show();

			g_entity_split2 = split2;

			{
				// class list
				auto scr = ui::ScrolledWindow(ui::New);
				scr.show();
				//gtk_paned_add1( GTK_PANED( split2 ), scr );
				gtk_paned_pack1( GTK_PANED( split2 ), scr, FALSE, FALSE );
				gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( scr ), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS );
				gtk_scrolled_window_set_shadow_type( GTK_SCROLLED_WINDOW( scr ), GTK_SHADOW_IN );

				{
					ui::ListStore store = ui::ListStore::from(gtk_list_store_new( 2, G_TYPE_STRING, G_TYPE_POINTER ));

					auto view = ui::TreeView( ui::TreeModel::from( store._handle ));
					// gtk_tree_view_set_enable_search(view, FALSE );
					gtk_tree_view_set_headers_visible( view, FALSE );
					view.connect( "button_press_event", G_CALLBACK( EntityClassList_button_press ), 0 );
					view.connect( "key_press_event", G_CALLBACK( EntityClassList_keypress ), 0 );

					{
						auto renderer = ui::CellRendererText(ui::New);
						auto column = ui::TreeViewColumn( "Key", renderer, {{"text", 0}} );
						gtk_tree_view_append_column( view, column );
					}

					{
						auto selection = ui::TreeSelection::from(gtk_tree_view_get_selection( view ));
						selection.connect( "changed", G_CALLBACK( EntityClassList_selection_changed ), 0 );
					}

					view.show();

					scr.add(view);

					store.unref();
					g_entityClassList = view;
					g_entlist_store = store;
				}
			}

			{
				auto scr = ui::ScrolledWindow(ui::New);
				scr.show();
				//gtk_paned_add2( GTK_PANED( split2 ), scr );
				gtk_paned_pack2( GTK_PANED( split2 ), scr, FALSE, FALSE );
				gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( scr ), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS );
				gtk_scrolled_window_set_shadow_type( GTK_SCROLLED_WINDOW( scr ), GTK_SHADOW_IN );

				{
					auto text = ui::TextView(ui::New);
					text.dimensions(0, -1); // allow shrinking
					gtk_text_view_set_wrap_mode( text, GTK_WRAP_WORD );
					gtk_text_view_set_editable( text, FALSE );
					text.show();
					scr.add(text);
					g_entityClassComment = text;
					{
						GtkTextBuffer *buffer = gtk_text_view_get_buffer( text );
						gtk_text_buffer_create_tag( buffer, "bold", "weight", PANGO_WEIGHT_BOLD, NULL );
					}
				}
			}
		}

		{
			ui::Widget split0 = ui::VPaned(ui::New);
			//gtk_paned_add2( GTK_PANED( split1 ), split0 );
			gtk_paned_pack2( GTK_PANED( split1 ), split0, FALSE, FALSE );
			split0.show();
			g_entity_split0 = split0;

			{
                auto vbox2 = ui::VBox( FALSE, 2 );
				vbox2.show();
				gtk_paned_pack1( GTK_PANED( split0 ), vbox2, FALSE, FALSE );

				{
					// Spawnflags (4 colums wide max, or window gets too wide.)
					auto table = ui::Table( 4, 4, FALSE );
					vbox2.pack_start( table, FALSE, TRUE, 0 );
					table.show();

					g_spawnflagsTable = table;

					for ( int i = 0; i < MAX_FLAGS; i++ )
					{
						auto check = ui::CheckButton( "" );
						check.ref();
						g_object_set_data( G_OBJECT( check ), "handler", gint_to_pointer( check.connect( "toggled", G_CALLBACK( SpawnflagCheck_toggled ), 0 ) ) );
						g_entitySpawnflagsCheck[i] = check;
					}
				}

				{
					// key/value list
					auto scr = ui::ScrolledWindow(ui::New);
					scr.show();
					vbox2.pack_start( scr, TRUE, TRUE, 0 );
					gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( scr ), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
					gtk_scrolled_window_set_shadow_type( GTK_SCROLLED_WINDOW( scr ), GTK_SHADOW_IN );

					{
						ui::ListStore store = ui::ListStore::from(gtk_list_store_new( 2, G_TYPE_STRING, G_TYPE_STRING ));

						auto view = ui::TreeView(ui::TreeModel::from(store._handle));
						gtk_tree_view_set_enable_search(view, FALSE );
						gtk_tree_view_set_headers_visible(view, FALSE );
						g_signal_connect( G_OBJECT( view ), "key_press_event", G_CALLBACK( EntityInspector_clearKeyValueKB ), 0 );

						{
							auto renderer = ui::CellRendererText(ui::New);
							auto column = ui::TreeViewColumn( "", renderer, {{"text", 0}} );
							gtk_tree_view_append_column(view, column );
						}

						{
							auto renderer = ui::CellRendererText(ui::New);
							auto column = ui::TreeViewColumn( "", renderer, {{"text", 1}} );
							gtk_tree_view_append_column(view, column );
						}

						{
							auto selection = ui::TreeSelection::from(gtk_tree_view_get_selection(view));
							selection.connect( "changed", G_CALLBACK( EntityProperties_selection_changed ), 0 );
						}

						view.show();

						scr.add(view);

						store.unref();

						g_entprops_store = store;
					}
				}

				{
					// key/value entry
					auto table = ui::Table( 2, 2, FALSE );
					table.show();
					vbox2.pack_start( table, FALSE, TRUE, 0 );
					gtk_table_set_row_spacings( table, 3 );
					gtk_table_set_col_spacings( table, 5 );

					{
						auto entry = ui::Entry(ui::New);
						entry.show();
						table.attach(entry, {1, 2, 0, 1}, {GTK_EXPAND | GTK_FILL, 0});
						gtk_widget_set_events( entry , GDK_KEY_PRESS_MASK );
						entry.connect( "key_press_event", G_CALLBACK( EntityEntry_keypress ), 0 );
						g_entityKeyEntry = entry;
					}

					{
						auto entry = ui::Entry(ui::New);
						entry.show();
						table.attach(entry, {1, 2, 1, 2}, {GTK_EXPAND | GTK_FILL, 0});
						gtk_widget_set_events( entry , GDK_KEY_PRESS_MASK );
						entry.connect( "key_press_event", G_CALLBACK( EntityEntry_keypress ), 0 );
						g_entityValueEntry = entry;
					}

					{
						auto label = ui::Label( "Value" );
						label.show();
						table.attach(label, {0, 1, 1, 2}, {GTK_FILL, 0});
						gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
					}

					{
						auto label = ui::Label( "Key" );
						label.show();
						table.attach(label, {0, 1, 0, 1}, {GTK_FILL, 0});
						gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
					}
				}

				{
					auto hbox = ui::HBox( FALSE, 4 );
					hbox.show();
					vbox2.pack_start( hbox, FALSE, TRUE, 0 );

					{
						auto button = ui::Button( "Clear All" );
#define GARUX_DISABLE_BUTTON_NOFOCUS
#ifndef GARUX_DISABLE_BUTTON_NOFOCUS
						GTK_WIDGET_UNSET_FLAGS( GTK_WIDGET( button ), GTK_CAN_FOCUS );
#endif // GARUX_DISABLE_BUTTON_NOFOCUS
						button.show();
						button.connect( "clicked", G_CALLBACK( EntityInspector_clearAllKeyValues ), 0 );
						hbox.pack_start( button, TRUE, TRUE, 0 );
					}
					{
						auto button = ui::Button( "Delete Key" );
#ifndef GARUX_DISABLE_BUTTON_NOFOCUS
						GTK_WIDGET_UNSET_FLAGS( GTK_WIDGET( button ), GTK_CAN_FOCUS );
#endif // GARUX_DISABLE_BUTTON_NOFOCUS
						button.show();
						button.connect( "clicked", G_CALLBACK( EntityInspector_clearKeyValue ), 0 );
						hbox.pack_start( button, TRUE, TRUE, 0 );
					}
					{
						GtkButton* button = GTK_BUTTON( gtk_button_new_with_label( "<" ) );
						gtk_widget_set_tooltip_text( GTK_WIDGET( button ), "Select targeting entities" );
#ifndef GARUX_DISABLE_BUTTON_NOFOCUS
						GTK_WIDGET_UNSET_FLAGS( GTK_WIDGET( button ), GTK_CAN_FOCUS );
#endif // GARUX_DISABLE_BUTTON_NOFOCUS
						gtk_widget_show( GTK_WIDGET( button ) );
						g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( EntityInspector_selectTargeting ), 0 );
						gtk_box_pack_start( hbox, GTK_WIDGET( button ), FALSE, FALSE, 0 );
					}
					{
						GtkButton* button = GTK_BUTTON( gtk_button_new_with_label( ">" ) );
						gtk_widget_set_tooltip_text( GTK_WIDGET( button ), "Select targets" );
#ifndef GARUX_DISABLE_BUTTON_NOFOCUS
						GTK_WIDGET_UNSET_FLAGS( GTK_WIDGET( button ), GTK_CAN_FOCUS );
#endif // GARUX_DISABLE_BUTTON_NOFOCUS
						gtk_widget_show( GTK_WIDGET( button ) );
						g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( EntityInspector_selectTargets ), 0 );
						gtk_box_pack_start( hbox, GTK_WIDGET( button ), FALSE, FALSE, 0 );
					}
					{
						GtkButton* button = GTK_BUTTON( gtk_button_new_with_label( "<->" ) );
						gtk_widget_set_tooltip_text( GTK_WIDGET( button ), "Select connected entities" );
#ifndef GARUX_DISABLE_BUTTON_NOFOCUS
						GTK_WIDGET_UNSET_FLAGS( GTK_WIDGET( button ), GTK_CAN_FOCUS );
#endif // GARUX_DISABLE_BUTTON_NOFOCUS
						gtk_widget_show( GTK_WIDGET( button ) );
						g_signal_connect( G_OBJECT( button ), "clicked", G_CALLBACK( EntityInspector_selectConnected ), 0 );
						gtk_box_pack_start( hbox, GTK_WIDGET( button ), FALSE, FALSE, 0 );
					}
					{
						GtkWidget* button = gtk_toggle_button_new();
						GtkImage* image = GTK_IMAGE( gtk_image_new_from_stock( GTK_STOCK_ZOOM_IN, GTK_ICON_SIZE_SMALL_TOOLBAR ) );
						gtk_widget_show( GTK_WIDGET( image ) );
						gtk_container_add( GTK_CONTAINER( button ), GTK_WIDGET( image ) );
						gtk_button_set_relief( GTK_BUTTON( button ), GTK_RELIEF_NONE );
#ifndef GARUX_DISABLE_BUTTON_NOFOCUS
						GTK_WIDGET_UNSET_FLAGS( button, GTK_CAN_FOCUS );
#endif // GARUX_DISABLE_BUTTON_NOFOCUS
						gtk_box_pack_start( hbox, button, FALSE, FALSE, 0 );
						gtk_widget_set_tooltip_text( button, "Focus on Selected" );
						gtk_widget_show( button );
						g_focusToggleButton = GTK_TOGGLE_BUTTON( button );
					}
				}
			}

			{
				auto scr = ui::ScrolledWindow(ui::New);
				scr.show();
				gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( scr ), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC );

				auto viewport = ui::Container::from(gtk_viewport_new( 0, 0 ));
				viewport.show();
				gtk_viewport_set_shadow_type( GTK_VIEWPORT( viewport ), GTK_SHADOW_NONE );

				g_attributeBox = ui::VBox( FALSE, 2 );
				g_attributeBox.show();

				viewport.add(g_attributeBox);
				scr.add(viewport);
				gtk_paned_pack2( GTK_PANED( split0 ), scr, FALSE, FALSE );
			}
		}
	}


	{
		// show the sliders in any case //no need, gtk can care
		/*if ( g_entitysplit2_position < 22 ) {
			g_entitysplit2_position = 22;
		}*/
		gtk_paned_set_position( GTK_PANED( g_entity_split2 ), g_entitysplit2_position );
		/*if ( ( g_entitysplit1_position - g_entitysplit2_position ) < 27 ) {
			g_entitysplit1_position = g_entitysplit2_position + 27;
		}*/
		gtk_paned_set_position( GTK_PANED( g_entity_split1 ), g_entitysplit1_position );
		gtk_paned_set_position( GTK_PANED( g_entity_split0 ), g_entitysplit0_position );
	}

	g_entityInspector_windowConstructed = true;
	EntityClassList_fill();

	typedef FreeCaller<void(const Selectable&), EntityInspector_selectionChanged> EntityInspectorSelectionChangedCaller;
	GlobalSelectionSystem().addSelectionChangeCallback( EntityInspectorSelectionChangedCaller() );
	GlobalEntityCreator().setKeyValueChangedFunc( EntityInspector_keyValueChanged );

	// hack
	gtk_container_set_focus_chain( GTK_CONTAINER( vbox ), NULL );

	return vbox;
}

class EntityInspector : public ModuleObserver
{
std::size_t m_unrealised;
public:
EntityInspector() : m_unrealised( 1 ){
}
void realise(){
	if ( --m_unrealised == 0 ) {
		if ( g_entityInspector_windowConstructed ) {
			//globalOutputStream() << "Entity Inspector: realise\n";
			EntityClassList_fill();
		}
	}
}
void unrealise(){
	if ( ++m_unrealised == 1 ) {
		if ( g_entityInspector_windowConstructed ) {
			//globalOutputStream() << "Entity Inspector: unrealise\n";
			EntityClassList_clear();
		}
	}
}
};

EntityInspector g_EntityInspector;

#include "preferencesystem.h"
#include "stringio.h"

void EntityInspector_construct(){
	GlobalEntityClassManager().attach( g_EntityInspector );

	GlobalPreferenceSystem().registerPreference( "EntitySplit0", make_property_string( g_entitysplit0_position ) );
	GlobalPreferenceSystem().registerPreference( "EntitySplit1", make_property_string( g_entitysplit1_position ) );
	GlobalPreferenceSystem().registerPreference( "EntitySplit2", make_property_string( g_entitysplit2_position ) );

}

void EntityInspector_destroy(){
	GlobalEntityClassManager().detach( g_EntityInspector );
}

const char *EntityInspector_getCurrentKey(){
	if ( !GroupDialog_isShown() ) {
		return 0;
	}
	if ( GroupDialog_getPage() != g_page_entity ) {
		return 0;
	}
	return gtk_entry_get_text( g_entityKeyEntry );
}
