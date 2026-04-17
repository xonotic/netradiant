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
// Base dialog class, provides a way to run modal dialogs and
// set/get the widget values in member variables.
//
// Leonardo Zide (leo@lokigames.com)
//

#include "dialog.h"

#include <gtk/gtk.h>

#include "debugging/debugging.h"


#include "mainframe.h"

#include <stdlib.h>

#include "stream/stringstream.h"
#include "convert.h"
#include "gtkutil/dialog.h"
#include "gtkutil/button.h"
#include "gtkutil/entry.h"
#include "gtkutil/image.h"

#include "gtkmisc.h"


ui::Entry DialogEntry_new(){
	auto entry = ui::Entry(ui::New);
	entry.show();
	entry.dimensions(64, -1);
	return entry;
}

class DialogEntryRow
{
public:
DialogEntryRow( ui::Widget row, ui::Entry entry ) : m_row( row ), m_entry( entry ){
}
ui::Widget m_row;
ui::Entry m_entry;
};

DialogEntryRow DialogEntryRow_new( const char* name ){
	auto alignment = ui::Alignment( 0.0, 0.5, 0.0, 0.0 );
	alignment.show();

	auto entry = DialogEntry_new();
	alignment.add(entry);

	return DialogEntryRow( ui::Widget(DialogRow_new( name, alignment  )), entry );
}


ui::SpinButton DialogSpinner_new( double value, double lower, double upper, int fraction ){
	double step = 1.0 / double(fraction);
	unsigned int digits = 0;
	for (; fraction > 1; fraction /= 10 )
	{
		++digits;
	}
	auto spin = ui::SpinButton( ui::Adjustment( value, lower, upper, step, 10, 0 ), step, digits );
	spin.show();
	spin.dimensions(64, -1);
	return spin;
}

class DialogSpinnerRow
{
public:
DialogSpinnerRow( ui::Widget row, ui::SpinButton spin ) : m_row( row ), m_spin( spin ){
}
ui::Widget m_row;
ui::SpinButton m_spin;
};

DialogSpinnerRow DialogSpinnerRow_new( const char* name, double value, double lower, double upper, int fraction ){
	auto alignment = ui::Alignment( 0.0, 0.5, 0.0, 0.0 );
	alignment.show();

	auto spin = DialogSpinner_new( value, lower, upper, fraction );
	alignment.add(spin);

	return DialogSpinnerRow( ui::Widget(DialogRow_new( name, alignment  )), spin );
}


struct BoolToggle {
	static void Export(const ui::ToggleButton &self, const Callback<void(bool)> &returnz) {
		returnz(self.active());
	}

	static void Import(ui::ToggleButton &self, bool value) {
		self.active(value);
	}
};

using BoolToggleImportExport = PropertyAdaptor<ui::ToggleButton, bool, BoolToggle>;

struct IntEntry {
	static void Export(const ui::Entry &self, const Callback<void(int)> &returnz) {
		returnz(atoi(gtk_entry_get_text(self)));
	}

	static void Import(ui::Entry &self, int value) {
		entry_set_int(self, value);
	}
};

using IntEntryImportExport = PropertyAdaptor<ui::Entry, int, IntEntry>;

struct IntRadio {
	static void Export(const ui::RadioButton &self, const Callback<void(int)> &returnz) {
		returnz(radio_button_get_active(self));
	}

	static void Import(ui::RadioButton &self, int value) {
		radio_button_set_active(self, value);
	}
};

using IntRadioImportExport = PropertyAdaptor<ui::RadioButton, int, IntRadio>;

struct IntCombo {
	static void Export(const ui::ComboBox &self, const Callback<void(int)> &returnz) {
		returnz(gtk_combo_box_get_active(self));
	}

	static void Import(ui::ComboBox &self, int value) {
		gtk_combo_box_set_active(self, value);
	}
};

using IntComboImportExport = PropertyAdaptor<ui::ComboBox, int, IntCombo>;

struct IntAdjustment {
	static void Export(const ui::Adjustment &self, const Callback<void(int)> &returnz) {
		returnz(int(gtk_adjustment_get_value(self)));
	}

	static void Import(ui::Adjustment &self, int value) {
		gtk_adjustment_set_value(self, value);
	}
};

using IntAdjustmentImportExport = PropertyAdaptor<ui::Adjustment, int, IntAdjustment>;

struct IntSpinner {
	static void Export(const ui::SpinButton &self, const Callback<void(int)> &returnz) {
		returnz(gtk_spin_button_get_value_as_int(self));
	}

	static void Import(ui::SpinButton &self, int value) {
		gtk_spin_button_set_value(self, value);
	}
};

using IntSpinnerImportExport = PropertyAdaptor<ui::SpinButton, int, IntSpinner>;

struct TextEntry {
	static void Export(const ui::Entry &self, const Callback<void(const char *)> &returnz) {
		returnz(gtk_entry_get_text(self));
	}

	static void Import(ui::Entry &self, const char *value) {
		self.text(value);
	}
};

using TextEntryImportExport = PropertyAdaptor<ui::Entry, const char *, TextEntry>;

struct SizeEntry {
	static void Export(const ui::Entry &self, const Callback<void(std::size_t)> &returnz) {
		int value = atoi(gtk_entry_get_text(self));
		if (value < 0) {
			value = 0;
		}
		returnz(value);
	}

	static void Import(ui::Entry &self, std::size_t value) {
		entry_set_int(self, int(value));
	}
};

using SizeEntryImportExport = PropertyAdaptor<ui::Entry, std::size_t, SizeEntry>;

struct FloatEntry {
	static void Export(const ui::Entry &self, const Callback<void(float)> &returnz) {
		returnz(float(atof(gtk_entry_get_text(self))));
	}

	static void Import(ui::Entry &self, float value) {
		entry_set_float(self, value);
	}
};

using FloatEntryImportExport = PropertyAdaptor<ui::Entry, float, FloatEntry>;

struct FloatSpinner {
	static void Export(const ui::SpinButton &self, const Callback<void(float)> &returnz) {
		returnz(float(gtk_spin_button_get_value(self)));
	}

	static void Import(ui::SpinButton &self, float value) {
		gtk_spin_button_set_value(self, value);
	}
};

using FloatSpinnerImportExport = PropertyAdaptor<ui::SpinButton, float, FloatSpinner>;



template<typename T>
class CallbackDialogData : public DLG_DATA {
	Property<T> m_pWidget;
	Property<T> m_pData;

public:
	CallbackDialogData(const Property<T> &pWidget, const Property<T> &pData)
			: m_pWidget(pWidget), m_pData(pData) {
	}

	void release() {
		delete this;
	}

	void importData() const {
		m_pData.get(m_pWidget.set);
	}

	void exportData() const {
		m_pWidget.get(m_pData.set);
	}
};

template<class Widget, class Self, class T, class native>
struct AddDataCustom_Wrapper {
    static void Export(const native &self, const Callback<void(T)> &returnz) {
        native *p = &const_cast<native &>(self);
        auto widget = Self::from(p);
        Widget::Get::thunk_(widget, returnz);
    }

    static void Import(native &self, T value) {
        native *p = &self;
        auto widget = Self::from(p);
        Widget::Set::thunk_(widget, value);
    }
};

template<class Widget>
void AddDataCustom(DialogDataList &self, typename Widget::Type widget, Property<typename Widget::Other> const &property) {
    using Self = typename Widget::Type;
    using T = typename Widget::Other;
    using native = typename std::remove_pointer<typename Self::native>::type;
    using Wrapper = AddDataCustom_Wrapper<Widget, Self, T, native>;
    
    self.push_back(new CallbackDialogData<typename Widget::Other>(
            make_property<PropertyAdaptor<native, T, Wrapper>>(*static_cast<native *>(widget)),
            property
    ));
}

template<class Widget, class D>
void AddData(DialogDataList &self, typename Widget::Type widget, D &data) {
    AddDataCustom<Widget>(self, widget, make_property<PropertyAdaptor<D, typename Widget::Other>>(data));
}

// =============================================================================
// Dialog class

Dialog::Dialog() : m_window( ui::null ), m_parent( ui::null ){
}

Dialog::~Dialog(){
	for ( DialogDataList::iterator i = m_data.begin(); i != m_data.end(); ++i )
	{
		( *i )->release();
	}

	ASSERT_MESSAGE( !m_window, "dialog window not destroyed" );
}

void Dialog::ShowDlg(){
	ASSERT_MESSAGE( m_window, "dialog was not constructed" );
	importData();
	m_window.show();
}

void Dialog::HideDlg(){
	ASSERT_MESSAGE( m_window, "dialog was not constructed" );
	exportData();
	m_window.hide();
}

static gint delete_event_callback( ui::Widget widget, GdkEvent* event, gpointer data ){
	reinterpret_cast<Dialog*>( data )->HideDlg();
	reinterpret_cast<Dialog*>( data )->EndModal( eIDCANCEL );
	return TRUE;
}

void Dialog::Create(){
	ASSERT_MESSAGE( !m_window, "dialog cannot be constructed" );

	m_window = BuildDialog();
	m_window.connect( "delete_event", G_CALLBACK( delete_event_callback ), this );
}

void Dialog::Destroy(){
	ASSERT_MESSAGE( m_window, "dialog cannot be destroyed" );

	m_window.destroy();
	m_window = ui::Window{ui::null};
}


void Dialog::AddBoolToggleData( ui::ToggleButton widget, Property<bool> const &cb ){
    AddDataCustom<BoolToggleImportExport>( m_data, widget, cb );
}

void Dialog::AddIntRadioData( ui::RadioButton widget, Property<int> const &cb ){
    AddDataCustom<IntRadioImportExport>( m_data, widget, cb );
}

void Dialog::AddTextEntryData( ui::Entry widget, Property<const char *> const &cb ){
    AddDataCustom<TextEntryImportExport>( m_data, widget, cb );
}

void Dialog::AddIntEntryData( ui::Entry widget, Property<int> const &cb ){
    AddDataCustom<IntEntryImportExport>( m_data, widget, cb );
}

void Dialog::AddSizeEntryData( ui::Entry widget, Property<std::size_t> const &cb ){
    AddDataCustom<SizeEntryImportExport>( m_data, widget, cb );
}

void Dialog::AddFloatEntryData( ui::Entry widget, Property<float> const &cb ){
    AddDataCustom<FloatEntryImportExport>( m_data, widget, cb );
}

void Dialog::AddFloatSpinnerData( ui::SpinButton widget, Property<float> const &cb ){
    AddDataCustom<FloatSpinnerImportExport>( m_data, widget, cb );
}

void Dialog::AddIntSpinnerData( ui::SpinButton widget, Property<int> const &cb ){
    AddDataCustom<IntSpinnerImportExport>( m_data, widget, cb );
}

void Dialog::AddIntAdjustmentData( ui::Adjustment widget, Property<int> const &cb ){
    AddDataCustom<IntAdjustmentImportExport>( m_data, widget, cb );
}

void Dialog::AddIntComboData( ui::ComboBox widget, Property<int> const &cb ){
    AddDataCustom<IntComboImportExport>( m_data, widget, cb );
}


void Dialog::AddDialogData( ui::ToggleButton widget, bool& data ){
	AddData<BoolToggleImportExport>( m_data, widget, data );
}
void Dialog::AddDialogData( ui::RadioButton widget, int& data ){
	AddData<IntRadioImportExport>( m_data, widget, data );
}
void Dialog::AddDialogData( ui::Entry widget, CopiedString& data ){
	AddData<TextEntryImportExport>( m_data, widget, data );
}
void Dialog::AddDialogData( ui::Entry widget, int& data ){
	AddData<IntEntryImportExport>( m_data, widget, data );
}
void Dialog::AddDialogData( ui::Entry widget, std::size_t& data ){
	AddData<SizeEntryImportExport>( m_data, widget, data );
}
void Dialog::AddDialogData( ui::Entry widget, float& data ){
	AddData<FloatEntryImportExport>( m_data, widget, data );
}
void Dialog::AddDialogData( ui::SpinButton widget, float& data ){
	AddData<FloatSpinnerImportExport>( m_data, widget, data );
}
void Dialog::AddDialogData( ui::SpinButton widget, int& data ){
	AddData<IntSpinnerImportExport>( m_data, widget, data );
}
void Dialog::AddDialogData( ui::Adjustment widget, int& data ){
	AddData<IntAdjustmentImportExport>( m_data, widget, data );
}
void Dialog::AddDialogData( ui::ComboBox widget, int& data ){
	AddData<IntComboImportExport>( m_data, widget, data );
}

void Dialog::exportData(){
	for ( DialogDataList::iterator i = m_data.begin(); i != m_data.end(); ++i )
	{
		( *i )->exportData();
	}
}

void Dialog::importData(){
	for ( DialogDataList::iterator i = m_data.begin(); i != m_data.end(); ++i )
	{
		( *i )->importData();
	}
}

void Dialog::EndModal( EMessageBoxReturn code ){
	m_modal.loop = 0;
	m_modal.ret = code;
}

EMessageBoxReturn Dialog::DoModal(){
	importData();

	PreModal();

	EMessageBoxReturn ret = modal_dialog_show( m_window, m_modal );
	ASSERT_TRUE( (bool) m_window );
	if ( ret == eIDOK ) {
		exportData();
	}

	m_window.hide();

	PostModal( m_modal.ret );

	return m_modal.ret;
}

ui::VBox Dialog::addSpacer( ui::VBox vbox, int dimension ){
	auto spacer = ui::VBox( FALSE, 0 );
	gtk_widget_set_size_request( GTK_WIDGET( spacer ), dimension, dimension );
	spacer.show();

	DialogVBox_packRow( vbox, spacer );
	return spacer;
}

ui::Label Dialog::addLabel( ui::VBox vbox, const char* name, const char* text ){
	auto label = ui::Label( text );
	gtk_misc_set_alignment( GTK_MISC( label ), 0.0, 0.5 );
	label.show();

	DialogVBox_packRow( vbox, ui::Widget(DialogRow_new( name, label ) ));
	return label;
}

ui::CheckButton Dialog::addCheckBox( ui::VBox vbox, const char* name, const char* flag, Property<bool> const &cb ){
	auto check = ui::CheckButton( flag );
	check.show();
	AddBoolToggleData( check, cb );

	DialogVBox_packRow( vbox, ui::Widget(DialogRow_new( name, check  ) ));
	return check;
}

ui::CheckButton Dialog::addCheckBox( ui::VBox vbox, const char* name, const char* flag, bool& data ){
	return addCheckBox(vbox, name, flag, make_property(data));
}

void Dialog::addCombo( ui::VBox vbox, const char* name, StringArrayRange values, Property<int> const &cb ){
	auto alignment = ui::Alignment( 0.0, 0.5, 0.0, 0.0 );
	alignment.show();
	{
		auto combo = ui::ComboBoxText(ui::New);

		for ( StringArrayRange::Iterator i = values.first; i != values.last; ++i )
		{
			gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT( combo ), *i );
		}

		AddIntComboData( combo, cb );

		combo.show();
		alignment.add(combo);
	}

	auto row = DialogRow_new( name, alignment );
	DialogVBox_packRow( vbox, row );
}

void Dialog::addCombo( ui::VBox vbox, const char* name, int& data, StringArrayRange values ){
	addCombo(vbox, name, values, make_property(data));
}

void Dialog::addSlider( ui::VBox vbox, const char* name, int& data, gboolean draw_value, const char* low, const char* high, double value, double lower, double upper, double step_increment, double page_increment ){
#if 0
	if ( draw_value == FALSE ) {
		auto hbox2 = ui::HBox( FALSE, 0 );
		hbox2.show();
		vbox.pack_start( hbox2 , FALSE, FALSE, 0 );
		{
			ui::Widget label = ui::Label( low );
			label.show();
			hbox2.pack_start( label, FALSE, FALSE, 0 );
		}
		{
			ui::Widget label = ui::Label( high );
			label.show();
			hbox2.pack_end(label, FALSE, FALSE, 0);
		}
	}
#endif

	// adjustment
	auto adj = ui::Adjustment( value, lower, upper, step_increment, page_increment, 0 );
	AddIntAdjustmentData(adj, make_property(data));

	// scale
	auto alignment = ui::Alignment( 0.0, 0.5, 1.0, 0.0 );
	alignment.show();

	ui::Widget scale = ui::HScale( adj );
	gtk_scale_set_value_pos( GTK_SCALE( scale ), GTK_POS_LEFT );
	scale.show();
	alignment.add(scale);

	gtk_scale_set_draw_value( GTK_SCALE( scale ), draw_value );
	gtk_scale_set_digits( GTK_SCALE( scale ), 0 );

	auto row = DialogRow_new( name, alignment );
	DialogVBox_packRow( vbox, row );
}

void Dialog::addRadio( ui::VBox vbox, const char* name, StringArrayRange names, Property<int> const &cb ){
	auto alignment = ui::Alignment( 0.0, 0.5, 0.0, 0.0 );
	alignment.show();;
	{
		RadioHBox radioBox = RadioHBox_new( names );
		alignment.add(radioBox.m_hbox);
		AddIntRadioData( radioBox.m_radio, cb );
	}

	auto row = DialogRow_new( name, alignment );
	DialogVBox_packRow( vbox, row );
}

void Dialog::addRadio( ui::VBox vbox, const char* name, int& data, StringArrayRange names ){
	addRadio(vbox, name, names, make_property(data));
}

void Dialog::addRadioIcons( ui::VBox vbox, const char* name, StringArrayRange icons, Property<int> const &cb ){
    auto table = ui::Table(2, icons.last - icons.first, FALSE);
    table.show();

    gtk_table_set_row_spacings(table, 5);
    gtk_table_set_col_spacings(table, 5);

	GSList* group = 0;
	ui::RadioButton radio{ui::null};
	for ( StringArrayRange::Iterator icon = icons.first; icon != icons.last; ++icon )
	{
		guint pos = static_cast<guint>( icon - icons.first );
		auto image = new_local_image( *icon );
		image.show();
        table.attach(image, {pos, pos + 1, 0, 1}, {0, 0});

		radio = ui::RadioButton::from(gtk_radio_button_new( group ));
		radio.show();
        table.attach(radio, {pos, pos + 1, 1, 2}, {0, 0});

#if defined(WORKAROUND_GTK3_SINGLE_WINDOW_LAYOUT)
		/* HACK: Disable the floating windows layouts because
		GTK3 doesn't support context sharing accross windows. */
		const char* disabledWindow2Layout = "window2-disabled.png";
		if (!strcmp(*icon, "window2-disabled.png")
                    || !strcmp(*icon, "window3-disabled.png")) {
			gtk_widget_set_sensitive(GTK_WIDGET(radio), FALSE);
			gtk_widget_set_opacity(GTK_WIDGET(radio), 0.35);
			gtk_widget_set_opacity(GTK_WIDGET(image), 0.35);
		}
#endif
		group = gtk_radio_button_get_group( GTK_RADIO_BUTTON( radio ) );
	}

	AddIntRadioData( radio, cb );

	DialogVBox_packRow( vbox, DialogRow_new( name, table ) );
}

void Dialog::addRadioIcons( ui::VBox vbox, const char* name, int& data, StringArrayRange icons ){
	addRadioIcons(vbox, name, icons, make_property(data));
}

ui::Widget Dialog::addIntEntry( ui::VBox vbox, const char* name, Property<int> const &cb ){
	DialogEntryRow row( DialogEntryRow_new( name ) );
	AddIntEntryData( row.m_entry, cb );
	DialogVBox_packRow( vbox, row.m_row );
	return row.m_row;
}

ui::Widget Dialog::addSizeEntry( ui::VBox vbox, const char* name, Property<std::size_t> const &cb ){
	DialogEntryRow row( DialogEntryRow_new( name ) );
	AddSizeEntryData( row.m_entry, cb );
	DialogVBox_packRow( vbox, row.m_row );
	return row.m_row;
}

ui::Widget Dialog::addFloatEntry( ui::VBox vbox, const char* name, Property<float> const &cb ){
	DialogEntryRow row( DialogEntryRow_new( name ) );
	AddFloatEntryData( row.m_entry, cb );
	DialogVBox_packRow( vbox, row.m_row );
	return row.m_row;
}

ui::Widget Dialog::addPathEntry( ui::VBox vbox, const char* name, bool browse_directory, Property<const char *> const &cb ){
	PathEntry pathEntry = PathEntry_new();
	pathEntry.m_button.connect( "clicked", G_CALLBACK( browse_directory ? button_clicked_entry_browse_directory : button_clicked_entry_browse_file ), pathEntry.m_entry );

	AddTextEntryData( pathEntry.m_entry, cb );

	auto row = DialogRow_new( name, ui::Widget(pathEntry.m_frame ) );
	DialogVBox_packRow( vbox, row );

	return row;
}

ui::Widget Dialog::addPathEntry( ui::VBox vbox, const char* name, CopiedString& data, bool browse_directory ){
    return addPathEntry(vbox, name, browse_directory, make_property<CopiedString, const char *>(data));
}

ui::SpinButton Dialog::addSpinner( ui::VBox vbox, const char* name, double value, double lower, double upper, Property<int> const &cb ){
	DialogSpinnerRow row( DialogSpinnerRow_new( name, value, lower, upper, 1 ) );
	AddIntSpinnerData( row.m_spin, cb );
	DialogVBox_packRow( vbox, row.m_row );
	return row.m_spin;
}

ui::SpinButton Dialog::addSpinner( ui::VBox vbox, const char* name, int& data, double value, double lower, double upper ){
	return addSpinner(vbox, name, value, lower, upper, make_property(data));
}

ui::SpinButton Dialog::addSpinner( ui::VBox vbox, const char* name, double value, double lower, double upper, Property<float> const &cb ){
	DialogSpinnerRow row( DialogSpinnerRow_new( name, value, lower, upper, 10 ) );
	AddFloatSpinnerData( row.m_spin, cb );
	DialogVBox_packRow( vbox, row.m_row );
	return row.m_spin;
}
