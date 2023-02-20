/*
   BobToolz plugin for GtkRadiant
   Copyright (C) 2001 Gordon Biggans

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "dialogs-gtk.h"
#include "../funchandlers.h"

#include "str.h"
#include <list>
#include <gtk/gtk.h>
#include "gtkutil/pointer.h"
#include "gtkutil/dialog.h"

#include "../lists.h"
#include "../misc.h"

#include "../bobToolz-GTK.h"


/*--------------------------------
        Callback Functions
   ---------------------------------*/

typedef struct {
	ui::Widget cbTexChange{ui::null};
	ui::Widget editTexOld{ui::null}, editTexNew{ui::null};

	ui::Widget cbScaleHor{ui::null}, cbScaleVert{ui::null};
	ui::Widget editScaleHor{ui::null}, editScaleVert{ui::null};

	ui::Widget cbShiftHor{ui::null}, cbShiftVert{ui::null};
	ui::Widget editShiftHor{ui::null}, editShiftVert{ui::null};

	ui::Widget cbRotation{ui::null};
	ui::Widget editRotation{ui::null};
}dlg_texReset_t;

dlg_texReset_t dlgTexReset;

void Update_TextureReseter();

static void dialog_button_callback_texreset_update(ui::Widget widget, gpointer data ){
	Update_TextureReseter();
}

void Update_TextureReseter(){
	gboolean check;

	check = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( dlgTexReset.cbTexChange ) );
	gtk_editable_set_editable( GTK_EDITABLE( dlgTexReset.editTexNew ), check );
	gtk_editable_set_editable( GTK_EDITABLE( dlgTexReset.editTexOld ), check );

	check = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( dlgTexReset.cbScaleHor ) );
	gtk_editable_set_editable( GTK_EDITABLE( dlgTexReset.editScaleHor ), check );

	check = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( dlgTexReset.cbScaleVert ) );
	gtk_editable_set_editable( GTK_EDITABLE( dlgTexReset.editScaleVert ), check );

	check = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( dlgTexReset.cbShiftHor ) );
	gtk_editable_set_editable( GTK_EDITABLE( dlgTexReset.editShiftHor ), check );

	check = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( dlgTexReset.cbShiftVert ) );
	gtk_editable_set_editable( GTK_EDITABLE( dlgTexReset.editShiftVert ), check );

	check = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( dlgTexReset.cbRotation ) );
	gtk_editable_set_editable( GTK_EDITABLE( dlgTexReset.editRotation ), check );
}

static void dialog_button_callback( ui::Widget widget, gpointer data ){
	int *loop;
	EMessageBoxReturn *ret;

	auto parent = widget.window();
	loop = (int*)g_object_get_data( G_OBJECT( parent ), "loop" );
	ret = (EMessageBoxReturn*)g_object_get_data( G_OBJECT( parent ), "ret" );

	*loop = 0;
	*ret = (EMessageBoxReturn)gpointer_to_int( data );
}

static gint custom_dialog_delete_callback( ui::Widget widget, GdkEvent* event, gpointer data ){
	widget.hide();
	int *loop = (int *) g_object_get_data(G_OBJECT(widget), "loop");
	*loop = 0;
	return TRUE;
}

static void dialog_button_callback_settex(ui::Widget widget, gpointer data ){
	TwinWidget* tw = (TwinWidget*)data;

    auto entry = ui::Entry::from( tw->one );
	auto combo = tw->two;

	const gchar *tex = gtk_entry_get_text(GTK_ENTRY (gtk_bin_get_child(combo)));
	gtk_entry_set_text( entry, tex );
}

/*--------------------------------
    Data validation Routines
   ---------------------------------*/

bool ValidateTextFloat( const char* pData, const char* error_title, float* value ){
	if ( pData ) {
		float testNum = (float)atof( pData );

		if ( ( testNum == 0.0f ) && strcmp( pData, "0" ) ) {
			DoMessageBox( "Please Enter A Floating Point Number", error_title, eMB_OK );
			return FALSE;
		}
		else
		{
			*value = testNum;
			return TRUE;
		}
	}

	DoMessageBox( "Please Enter A Floating Point Number", error_title, eMB_OK );
	return FALSE;
}

bool ValidateTextFloatRange( const char* pData, float min, float max, const char* error_title, float* value ){
	char error_buffer[256];
	sprintf( error_buffer, "Please Enter A Floating Point Number Between %.3f and %.3f", min, max );

	if ( pData ) {
		float testNum = (float)atof( pData );

		if ( ( testNum < min ) || ( testNum > max ) ) {
			DoMessageBox( error_buffer, error_title, eMB_OK );
			return FALSE;
		}
		else
		{
			*value = testNum;
			return TRUE;
		}
	}

	DoMessageBox( error_buffer, error_title, eMB_OK );
	return FALSE;
}

bool ValidateTextIntRange( const char* pData, int min, int max, const char* error_title, int* value ){
	char error_buffer[256];
	sprintf( error_buffer, "Please Enter An Integer Between %i and %i", min, max );

	if ( pData ) {
		int testNum = atoi( pData );

		if ( ( testNum < min ) || ( testNum > max ) ) {
			DoMessageBox( error_buffer, error_title, eMB_OK );
			return FALSE;
		}
		else
		{
			*value = testNum;
			return TRUE;
		}
	}

	DoMessageBox( error_buffer, error_title, eMB_OK );
	return FALSE;
}

bool ValidateTextInt( const char* pData, const char* error_title, int* value ){
	if ( pData ) {
		int testNum = atoi( pData );

		if ( ( testNum == 0 ) && strcmp( pData, "0" ) ) {
			DoMessageBox( "Please Enter An Integer", error_title, eMB_OK );
			return FALSE;
		}
		else
		{
			*value = testNum;
			return TRUE;
		}
	}

	DoMessageBox( "Please Enter An Integer", error_title, eMB_OK );
	return FALSE;
}

/*--------------------------------
        Modal Dialog Boxes
   ---------------------------------*/

/*

   Major clean up of variable names etc required, excluding Mars's ones,
   which are nicely done :)

 */

EMessageBoxReturn DoMessageBox( const char* lpText, const char* lpCaption, EMessageBoxType type ){
	ui::Widget w{ui::null};
	EMessageBoxReturn ret;
	int loop = 1;

	auto window = ui::Window( ui::window_type::TOP );
	gtk_window_set_transient_for( GTK_WINDOW( window ), main_window );
	window.connect( "delete_event", G_CALLBACK( custom_dialog_delete_callback ), NULL );
	window.connect( "destroy", G_CALLBACK( gtk_widget_destroy ), NULL );
	gtk_window_set_title( window, lpCaption );
	gtk_container_set_border_width( GTK_CONTAINER( window ), 10 );
	g_object_set_data( G_OBJECT( window ), "loop", &loop );
	g_object_set_data( G_OBJECT( window ), "ret", &ret );
	gtk_widget_realize( window );

	auto vbox = ui::VBox( FALSE, 10 );
	window.add(vbox);
	vbox.show();

	w = ui::Label( lpText );
	vbox.pack_start( w, FALSE, FALSE, 2 );
	gtk_label_set_justify( GTK_LABEL( w ), GTK_JUSTIFY_LEFT );
	w.show();

	w = ui::Widget::from(gtk_hseparator_new());
	vbox.pack_start( w, FALSE, FALSE, 2 );
	w.show();

	auto hbox = ui::HBox( FALSE, 10 );
	vbox.pack_start( hbox, FALSE, FALSE, 2 );
	hbox.show();

	if ( type == eMB_OK ) {
		w = ui::Button( "Ok" );
		hbox.pack_start( w, TRUE, TRUE, 0 );
		w.connect( "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDOK ) );
		gtk_widget_set_can_default(w, true);
		gtk_widget_grab_default( w );
		w.show();
		ret = eIDOK;
	}
	else if ( type ==  eMB_OKCANCEL ) {
		w = ui::Button( "Ok" );
		hbox.pack_start( w, TRUE, TRUE, 0 );
		w.connect( "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDOK ) );
		gtk_widget_set_can_default( w, true );
		gtk_widget_grab_default( w );
		w.show();

		w = ui::Button( "Cancel" );
		hbox.pack_start( w, TRUE, TRUE, 0 );
		w.connect( "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDCANCEL ) );
		w.show();
		ret = eIDCANCEL;
	}
	else if ( type == eMB_YESNOCANCEL ) {
		w = ui::Button( "Yes" );
		hbox.pack_start( w, TRUE, TRUE, 0 );
		w.connect( "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDYES ) );
		gtk_widget_set_can_default( w, true );
		gtk_widget_grab_default( w );
		w.show();

		w = ui::Button( "No" );
		hbox.pack_start( w, TRUE, TRUE, 0 );
		w.connect( "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDNO ) );
		w.show();

		w = ui::Button( "Cancel" );
		hbox.pack_start( w, TRUE, TRUE, 0 );
		w.connect( "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDCANCEL ) );
		w.show();
		ret = eIDCANCEL;
	}
	else /* if (mode == MB_YESNO) */
	{
		w = ui::Button( "Yes" );
		hbox.pack_start( w, TRUE, TRUE, 0 );
		w.connect( "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDYES ) );
		gtk_widget_set_can_default( w, true );
		gtk_widget_grab_default( w );
		w.show();

		w = ui::Button( "No" );
		hbox.pack_start( w, TRUE, TRUE, 0 );
		w.connect( "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDNO ) );
		w.show();
		ret = eIDNO;
	}

	gtk_window_set_position( GTK_WINDOW( window ),GTK_WIN_POS_CENTER_ON_PARENT );
	window.show();
	gtk_grab_add( window );

	while ( loop )
		gtk_main_iteration();

	gtk_grab_remove( window );
	window.destroy();

	return ret;
}

EMessageBoxReturn DoIntersectBox( IntersectRS* rs ){
	EMessageBoxReturn ret;
	int loop = 1;

	auto window = ui::Window( ui::window_type::TOP );

	window.connect( "delete_event", G_CALLBACK( custom_dialog_delete_callback ), NULL );
	window.connect( "destroy", G_CALLBACK( gtk_widget_destroy ), NULL );

	gtk_window_set_title( window, "Intersect" );
	gtk_container_set_border_width( GTK_CONTAINER( window ), 10 );

	g_object_set_data( G_OBJECT( window ), "loop", &loop );
	g_object_set_data( G_OBJECT( window ), "ret", &ret );

	gtk_widget_realize( window );



	auto vbox = ui::VBox( FALSE, 10 );
	window.add(vbox);
	vbox.show();

	// ---- vbox ----


	auto radio1 = ui::Widget::from(gtk_radio_button_new_with_label( NULL, "Use Whole Map" ));
	vbox.pack_start( radio1, FALSE, FALSE, 2 );
	radio1.show();

	auto radio2 = ui::Widget::from(gtk_radio_button_new_with_label( gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio1)), "Use Selected Brushes" ));
	vbox.pack_start( radio2, FALSE, FALSE, 2 );
	radio2.show();

	auto hsep = ui::Widget::from(gtk_hseparator_new());
	vbox.pack_start( hsep, FALSE, FALSE, 2 );
	hsep.show();

	auto check1 = ui::CheckButton( "Include Detail Brushes" );
	vbox.pack_start( check1, FALSE, FALSE, 0 );
	check1.show();

	auto check2 = ui::CheckButton( "Select Duplicate Brushes Only" );
	vbox.pack_start( check2, FALSE, FALSE, 0 );
	check2.show();

	auto hbox = ui::HBox( FALSE, 10 );
	vbox.pack_start( hbox, FALSE, FALSE, 2 );
	hbox.show();

	// ---- hbox ---- ok/cancel buttons

	auto w = ui::Button( "Ok" );
	hbox.pack_start( w, TRUE, TRUE, 0 );
	w.connect( "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDOK ) );

	gtk_widget_set_can_default( w, true );
	gtk_widget_grab_default( w );
	w.show();

	w = ui::Button( "Cancel" );
	hbox.pack_start( w, TRUE, TRUE, 0 );
	w.connect( "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDCANCEL ) );
	w.show();
	ret = eIDCANCEL;

	// ---- /hbox ----

	// ---- /vbox ----

	gtk_window_set_position( window, GTK_WIN_POS_CENTER );
	window.show();
	gtk_grab_add( window );

	while ( loop )
		gtk_main_iteration();

	if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(radio1) ) ) {
		rs->nBrushOptions = BRUSH_OPT_WHOLE_MAP;
	}
	else if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(radio2) ) ) {
		rs->nBrushOptions = BRUSH_OPT_SELECTED;
	}

	rs->bUseDetail = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(check1) ) ? true : false;
	rs->bDuplicateOnly = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(check2) ) ? true : false;

	gtk_grab_remove( window );
	window.destroy();

	return ret;
}

EMessageBoxReturn DoPolygonBox( PolygonRS* rs ){
	EMessageBoxReturn ret;
	int loop = 1;

	auto window = ui::Window( ui::window_type::TOP );
	gtk_window_set_transient_for( GTK_WINDOW( window ), main_window );

	window.connect( "delete_event", G_CALLBACK( custom_dialog_delete_callback ), NULL );
	window.connect( "destroy", G_CALLBACK( gtk_widget_destroy ), NULL );

	gtk_window_set_title( window, "Polygon Builder" );
	gtk_container_set_border_width( GTK_CONTAINER( window ), 10 );

	g_object_set_data( G_OBJECT( window ), "loop", &loop );
	g_object_set_data( G_OBJECT( window ), "ret", &ret );

	gtk_widget_realize( window );



	auto vbox = ui::VBox( FALSE, 10 );
	window.add(vbox);
	vbox.show();

	// ---- vbox ----

    auto hbox = ui::HBox( FALSE, 10 );
	vbox.pack_start( hbox, FALSE, FALSE, 2 );
	hbox.show();

	// ---- hbox ----


    auto vbox2 = ui::VBox( FALSE, 10 );
	hbox.pack_start( vbox2, FALSE, FALSE, 2 );
	vbox2.show();

	// ---- vbox2 ----

    auto hbox2 = ui::HBox( FALSE, 10 );
	vbox2.pack_start( hbox2, FALSE, FALSE, 2 );
	hbox2.show();

	// ---- hbox2 ----

    auto text1 = ui::Entry( 256 );
	gtk_entry_set_text( text1, "3" );
	hbox2.pack_start( text1, FALSE, FALSE, 2 );
	text1.show();

	auto l = ui::Label( "Number Of Sides" );
	hbox2.pack_start( l, FALSE, FALSE, 2 );
	gtk_label_set_justify( GTK_LABEL( l ), GTK_JUSTIFY_LEFT );
	l.show();

	// ---- /hbox2 ----

	hbox2 = ui::HBox( FALSE, 10 );
	vbox2.pack_start( hbox2, FALSE, FALSE, 2 );
	hbox2.show();

	// ---- hbox2 ----

    auto text2 = ui::Entry( 256 );
	gtk_entry_set_text( text2, "8" );
	hbox2.pack_start( text2, FALSE, FALSE, 2 );
	text2.show();

	l = ui::Label( "Border Width" );
	hbox2.pack_start( l, FALSE, FALSE, 2 );
	gtk_label_set_justify( GTK_LABEL( l ), GTK_JUSTIFY_LEFT );
	l.show();

	// ---- /hbox2 ----

	// ---- /vbox2 ----



	vbox2 = ui::VBox( FALSE, 10 );
	hbox.pack_start( vbox2, FALSE, FALSE, 2 );
	vbox2.show();

	// ---- vbox2 ----

    auto check1 = ui::CheckButton( "Use Border" );
	vbox2.pack_start( check1, FALSE, FALSE, 0 );
	check1.show();


    auto check2 = ui::CheckButton( "Inverse Polygon" );
	vbox2.pack_start( check2, FALSE, FALSE, 0 );
	check2.show();


    auto check3 = ui::CheckButton( "Align Top Edge" );
	vbox2.pack_start( check3, FALSE, FALSE, 0 );
	check3.show();

	// ---- /vbox2 ----

	// ---- /hbox ----

	hbox = ui::HBox( FALSE, 10 );
	vbox.pack_start( hbox, FALSE, FALSE, 2 );
	hbox.show();

	// ---- hbox ----

	auto w = ui::Button( "Ok" );
	hbox.pack_start( w, TRUE, TRUE, 0 );
	w.connect( "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDOK ) );

	gtk_widget_set_can_default( w, true );
	gtk_widget_grab_default( w );
	w.show();

	w = ui::Button( "Cancel" );
	hbox.pack_start( w, TRUE, TRUE, 0 );
	w.connect( "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDCANCEL ) );
	w.show();
	ret = eIDCANCEL;

	// ---- /hbox ----

	// ---- /vbox ----

	gtk_window_set_position( GTK_WINDOW( window ),GTK_WIN_POS_CENTER_ON_PARENT );
	gtk_window_set_modal( GTK_WINDOW( window ), TRUE );
	window.show();
	gtk_grab_add( window );

	bool dialogError = TRUE;
	while ( dialogError )
	{
		loop = 1;
		while ( loop )
			gtk_main_iteration();

		dialogError = FALSE;

		if ( ret == eIDOK ) {
			rs->bUseBorder = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(check1) ) ? true : false;
			rs->bInverse = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(check2) ) ? true : false;
			rs->bAlignTop = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(check3) ) ? true : false;

			if ( !ValidateTextIntRange( gtk_entry_get_text( text1 ), 3, 32, "Number Of Sides", &rs->nSides ) ) {
				dialogError = TRUE;
			}

			if ( rs->bUseBorder ) {
				if ( !ValidateTextIntRange( gtk_entry_get_text( text2 ), 8, 256, "Border Width", &rs->nBorderWidth ) ) {
					dialogError = TRUE;
				}
			}
		}
	}

	gtk_grab_remove( window );
	window.destroy();

	return ret;
}

// mars
// for stair builder stuck as close as i could to the MFC version
// obviously feel free to change it at will :)
EMessageBoxReturn DoBuildStairsBox( BuildStairsRS* rs ){
	GSList      *radioDirection, *radioStyle;
	EMessageBoxReturn ret;
	int loop = 1;

	const char *text = "Please set a value in the boxes below and press 'OK' to build the stairs";

	auto window = ui::Window( ui::window_type::TOP );

	window.connect( "delete_event", G_CALLBACK( custom_dialog_delete_callback ), NULL );
	window.connect( "destroy", G_CALLBACK( gtk_widget_destroy ), NULL );

	gtk_window_set_title( window, "Stair Builder" );

	gtk_container_set_border_width( GTK_CONTAINER( window ), 10 );

	g_object_set_data( G_OBJECT( window ), "loop", &loop );
	g_object_set_data( G_OBJECT( window ), "ret", &ret );

	gtk_widget_realize( window );

	// new vbox
	auto vbox = ui::VBox( FALSE, 10 );
	window.add(vbox);
	vbox.show();

	auto hbox = ui::HBox( FALSE, 10 );
	vbox.add(hbox);
	hbox.show();

	// dunno if you want this text or not ...
	ui::Widget w = ui::Label( text );
	hbox.pack_start( w, FALSE, FALSE, 0 ); // not entirely sure on all the parameters / what they do ...
	w.show();

	w = ui::Widget::from(gtk_hseparator_new());
	vbox.pack_start( w, FALSE, FALSE, 0 );
	w.show();

	// ------------------------- // indenting == good way of keeping track of lines :)

	// new hbox
	hbox = ui::HBox( FALSE, 10 );
	vbox.pack_start( hbox, FALSE, FALSE, 0 );
	hbox.show();

    auto textStairHeight = ui::Entry( 256 );
	hbox.pack_start( textStairHeight, FALSE, FALSE, 1 );
	textStairHeight.show();

	w = ui::Label( "Stair Height" );
	hbox.pack_start( w, FALSE, FALSE, 1 );
	w.show();

	// ------------------------- //

	hbox = ui::HBox( FALSE, 10 );
	vbox.pack_start( hbox, FALSE, FALSE, 0 );
	hbox.show();

	w = ui::Label( "Direction:" );
	hbox.pack_start( w, FALSE, FALSE, 5 );
	w.show();

	// -------------------------- //

	hbox = ui::HBox( FALSE, 10 );
	vbox.pack_start( hbox, FALSE, FALSE, 0 );
	hbox.show();

	// radio buttons confuse me ...
	// but this _looks_ right

	// djbob: actually it looks very nice :), slightly better than the way i did it
	// edit: actually it doesn't work :P, you must pass the last radio item each time, ugh

    auto radioNorth = ui::Widget::from(gtk_radio_button_new_with_label( NULL, "North" ));
	hbox.pack_start( radioNorth, FALSE, FALSE, 3 );
	radioNorth.show();

	radioDirection = gtk_radio_button_get_group( GTK_RADIO_BUTTON( radioNorth ) );

    auto radioSouth = ui::Widget::from(gtk_radio_button_new_with_label( radioDirection, "South" ));
	hbox.pack_start( radioSouth, FALSE, FALSE, 2 );
	radioSouth.show();

	radioDirection = gtk_radio_button_get_group( GTK_RADIO_BUTTON( radioSouth ) );

    auto radioEast = ui::Widget::from(gtk_radio_button_new_with_label( radioDirection, "East" ));
	hbox.pack_start( radioEast, FALSE, FALSE, 1 );
	radioEast.show();

	radioDirection = gtk_radio_button_get_group( GTK_RADIO_BUTTON( radioEast ) );

    auto radioWest = ui::Widget::from(gtk_radio_button_new_with_label( radioDirection, "West" ));
	hbox.pack_start( radioWest, FALSE, FALSE, 0 );
	radioWest.show();

	// --------------------------- //

	hbox = ui::HBox( FALSE, 10 );
	vbox.pack_start( hbox, FALSE, FALSE, 0 );
	hbox.show();

	w = ui::Label( "Style:" );
	hbox.pack_start( w, FALSE, FALSE, 5 );
	w.show();

	// --------------------------- //

	hbox = ui::HBox( FALSE, 10 );
	vbox.pack_start( hbox, FALSE, FALSE, 0 );
	hbox.show();

    auto radioOldStyle = ui::Widget::from(gtk_radio_button_new_with_label( NULL, "Original" ));
	hbox.pack_start( radioOldStyle, FALSE, FALSE, 0 );
	radioOldStyle.show();

	radioStyle = gtk_radio_button_get_group( GTK_RADIO_BUTTON( radioOldStyle ) );

    auto radioBobStyle = ui::Widget::from(gtk_radio_button_new_with_label( radioStyle, "Bob's Style" ));
	hbox.pack_start( radioBobStyle, FALSE, FALSE, 0 );
	radioBobStyle.show();

	radioStyle = gtk_radio_button_get_group( GTK_RADIO_BUTTON( radioBobStyle ) );

    auto radioCornerStyle = ui::Widget::from(gtk_radio_button_new_with_label( radioStyle, "Corner Style" ));
	hbox.pack_start( radioCornerStyle, FALSE, FALSE, 0 );
	radioCornerStyle.show();

	// err, the q3r has an if or something so you need bob style checked before this
	// is "ungreyed out" but you'll need to do that, as i suck :)

	// djbob: er.... yeah um, im not at all sure how i'm gonna sort this
	// djbob: think we need some button callback functions or smuffin
	// FIXME: actually get around to doing what i suggested!!!!

    auto checkUseDetail = ui::CheckButton( "Use Detail Brushes" );
	hbox.pack_start( checkUseDetail, FALSE, FALSE, 0 );
	checkUseDetail.show();

	// --------------------------- //

	hbox = ui::HBox( FALSE, 10 );
	vbox.pack_start( hbox, FALSE, FALSE, 0 );
	hbox.show();

    auto textMainTex = ui::Entry( 512 );
	gtk_entry_set_text( GTK_ENTRY( textMainTex ), rs->mainTexture );
	hbox.pack_start( textMainTex, FALSE, FALSE, 0 );
	textMainTex.show();

	w = ui::Label( "Main Texture" );
	hbox.pack_start( w, FALSE, FALSE, 1 );
	w.show();

	// -------------------------- //

	hbox = ui::HBox( FALSE, 10 );
	vbox.pack_start( hbox, FALSE, FALSE, 0 );
	hbox.show();

	auto textRiserTex = ui::Entry( 512 );
	hbox.pack_start( textRiserTex, FALSE, FALSE, 0 );
	textRiserTex.show();

	w = ui::Label( "Riser Texture" );
	hbox.pack_start( w, FALSE, FALSE, 1 );
	w.show();

	// -------------------------- //
	w = ui::Widget::from(gtk_hseparator_new());
	vbox.pack_start( w, FALSE, FALSE, 0 );
	w.show();

	hbox = ui::HBox( FALSE, 10 );
	vbox.pack_start( hbox, FALSE, FALSE, 0 );
	hbox.show();

	w = ui::Button( "OK" );
	hbox.pack_start( w, TRUE, TRUE, 0 );
	w.connect( "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDOK ) );
	gtk_widget_set_can_default( w, true );
	gtk_widget_grab_default( w );
	w.show();

	w = ui::Button( "Cancel" );
	hbox.pack_start( w, TRUE, TRUE, 0 );
	w.connect( "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDCANCEL ) );
	w.show();

	ret = eIDCANCEL;

// +djbob: need our "little" modal loop mars :P
	gtk_window_set_position( window, GTK_WIN_POS_CENTER );
	window.show();
	gtk_grab_add( window );

	bool dialogError = TRUE;
	while ( dialogError )
	{
		loop = 1;
		while ( loop )
			gtk_main_iteration();

		dialogError = FALSE;

		if ( ret == eIDOK ) {
			rs->bUseDetail = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(checkUseDetail) ) ? true : false;

			strcpy( rs->riserTexture, gtk_entry_get_text( textRiserTex ) );
			strcpy( rs->mainTexture, gtk_entry_get_text( textMainTex ) );

			if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(radioNorth) ) ) {
				rs->direction = MOVE_NORTH;
			}
			else if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(radioSouth) ) ) {
				rs->direction = MOVE_SOUTH;
			}
			else if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(radioEast) ) ) {
				rs->direction = MOVE_EAST;
			}
			else if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(radioWest) ) ) {
				rs->direction = MOVE_WEST;
			}

			if ( !ValidateTextInt( gtk_entry_get_text( textStairHeight ), "Stair Height", &rs->stairHeight ) ) {
				dialogError = TRUE;
			}

			if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(radioOldStyle) ) ) {
				rs->style = STYLE_ORIGINAL;
			}
			else if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(radioBobStyle) ) ) {
				rs->style = STYLE_BOB;
			}
			else if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(radioCornerStyle) ) ) {
				rs->style = STYLE_CORNER;
			}
		}
	}

	gtk_grab_remove( window );
	window.destroy();

	return ret;
// -djbob

	// there we go, all done ... on my end at least, not bad for a night's work
}

EMessageBoxReturn DoDoorsBox( DoorRS* rs ){
	GSList      *radioOrientation;
	TwinWidget tw1, tw2;
	EMessageBoxReturn ret;
	int loop = 1;

	auto window = ui::Window( ui::window_type::TOP );

	window.connect( "delete_event", G_CALLBACK( custom_dialog_delete_callback ), NULL );
	window.connect( "destroy", G_CALLBACK( gtk_widget_destroy ), NULL );

	gtk_window_set_title( window, "Door Builder" );

	gtk_container_set_border_width( GTK_CONTAINER( window ), 10 );

	g_object_set_data( G_OBJECT( window ), "loop", &loop );
	g_object_set_data( G_OBJECT( window ), "ret", &ret );

	gtk_widget_realize( window );

	char buffer[256];
	auto listMainTextures = ui::ListStore::from(gtk_list_store_new( 1, G_TYPE_STRING ));
	auto listTrimTextures = ui::ListStore::from(gtk_list_store_new( 1, G_TYPE_STRING ));
	LoadGList( GetFilename( buffer, "plugins/bt/door-tex.txt" ), listMainTextures );
	LoadGList( GetFilename( buffer, "plugins/bt/door-tex-trim.txt" ), listTrimTextures );

	auto vbox = ui::VBox( FALSE, 10 );
	window.add(vbox);
	vbox.show();

	// -------------------------- //

    auto hbox = ui::HBox( FALSE, 10 );
	vbox.pack_start( hbox, FALSE, FALSE, 0 );
	hbox.show();

	auto textFrontBackTex = ui::Entry( 512 );
	gtk_entry_set_text( GTK_ENTRY( textFrontBackTex ), rs->mainTexture );
	hbox.pack_start( textFrontBackTex, FALSE, FALSE, 0 );
	textFrontBackTex.show();

	ui::Widget w = ui::Label( "Door Front/Back Texture" );
	hbox.pack_start( w, FALSE, FALSE, 0 );
	w.show();

	// ------------------------ //

	hbox = ui::HBox( FALSE, 10 );
	vbox.pack_start( hbox, FALSE, FALSE, 0 );
	hbox.show();

	auto textTrimTex = ui::Entry( 512 );
	hbox.pack_start( textTrimTex, FALSE, FALSE, 0 );
	textTrimTex.show();

	w = ui::Label( "Door Trim Texture" );
	hbox.pack_start( w, FALSE, FALSE, 0 );
	w.show();

	// ----------------------- //

	hbox = ui::HBox( FALSE, 10 );
	vbox.pack_start( hbox, FALSE, FALSE, 0 );
	hbox.show();

	// sp: horizontally ????
	// djbob: yes mars, u can spell :]
    auto checkScaleMainH = ui::CheckButton( "Scale Main Texture Horizontally" );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( checkScaleMainH ), TRUE );
	hbox.pack_start( checkScaleMainH, FALSE, FALSE, 0 );
	checkScaleMainH.show();

    auto checkScaleTrimH = ui::CheckButton( "Scale Trim Texture Horizontally" );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( checkScaleTrimH ), TRUE );
	hbox.pack_start( checkScaleTrimH, FALSE, FALSE, 0 );
	checkScaleTrimH.show();

	// ---------------------- //

	hbox = ui::HBox( FALSE, 10 );
	vbox.pack_start( hbox, FALSE, FALSE, 0 );
	hbox.show();

    auto checkScaleMainV = ui::CheckButton( "Scale Main Texture Vertically" );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( checkScaleMainV ), TRUE );
	hbox.pack_start( checkScaleMainV, FALSE, FALSE, 0 );
	checkScaleMainV.show();

    auto checkScaleTrimV = ui::CheckButton( "Scale Trim Texture Vertically" );
	hbox.pack_start( checkScaleTrimV, FALSE, FALSE, 0 );
	checkScaleTrimV.show();

	// --------------------- //

	hbox = ui::HBox( FALSE, 10 );
	vbox.pack_start( hbox, FALSE, FALSE, 0 );
	hbox.show();

	// djbob: lists added

	auto comboMain = ui::ComboBox::from(gtk_combo_box_new_with_model_and_entry(listMainTextures));
	gtk_combo_box_set_entry_text_column(GTK_COMBO_BOX(comboMain), 0);
	hbox.pack_start( comboMain, FALSE, FALSE, 0 );
	comboMain.show();

	tw1.one = textFrontBackTex;
	tw1.two = comboMain;

	auto buttonSetMain = ui::Button( "Set As Main Texture" );
	buttonSetMain.connect( "clicked", G_CALLBACK( dialog_button_callback_settex ), &tw1 );
	hbox.pack_start( buttonSetMain, FALSE, FALSE, 0 );
	buttonSetMain.show();

	// ------------------- //

	hbox = ui::HBox( FALSE, 10 );
	vbox.pack_start( hbox, FALSE, FALSE, 0 );
	hbox.show();

	auto comboTrim = ui::ComboBox::from(gtk_combo_box_new_with_model_and_entry(listTrimTextures));
	gtk_combo_box_set_entry_text_column(GTK_COMBO_BOX(comboMain), 0);
	hbox.pack_start( comboTrim, FALSE, FALSE, 0 );
	comboTrim.show();

	tw2.one = textTrimTex;
	tw2.two = comboTrim;

	auto buttonSetTrim = ui::Button( "Set As Trim Texture" );
	buttonSetTrim.connect( "clicked", G_CALLBACK( dialog_button_callback_settex ), &tw2 );
	hbox.pack_start( buttonSetTrim, FALSE, FALSE, 0 );
	buttonSetTrim.show();

	// ------------------ //

	hbox = ui::HBox( FALSE, 10 );
	vbox.pack_start( hbox, FALSE, FALSE, 0 );
	hbox.show();

	w = ui::Label( "Orientation" );
	hbox.pack_start( w, FALSE, FALSE, 0 );
	w.show();

	// argh more radio buttons!
    auto radioNS = ui::Widget::from(gtk_radio_button_new_with_label( NULL, "North - South" ));
	hbox.pack_start( radioNS, FALSE, FALSE, 0 );
	radioNS.show();

	radioOrientation = gtk_radio_button_get_group( GTK_RADIO_BUTTON( radioNS ) );

    auto radioEW = ui::Widget::from(gtk_radio_button_new_with_label( radioOrientation, "East - West" ));
	hbox.pack_start( radioEW, FALSE, FALSE, 0 );
	radioEW.show();

	// ----------------- //

	w = ui::Widget::from(gtk_hseparator_new());
	vbox.pack_start( w, FALSE, FALSE, 0 );
	w.show();

	// ----------------- //

	hbox = ui::HBox( FALSE, 10 );
	vbox.pack_start( hbox, FALSE, FALSE, 0 );
	hbox.show();

	w = ui::Button( "OK" );
	hbox.pack_start( w, TRUE, TRUE, 0 );
	w.connect( "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDOK ) );
	gtk_widget_set_can_default( w, true );
	gtk_widget_grab_default( w );
	w.show();

	w = ui::Button( "Cancel" );
	hbox.pack_start( w, TRUE, TRUE, 0 );
	w.connect( "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDCANCEL ) );
	w.show();
	ret = eIDCANCEL;

	// ----------------- //

//+djbob
	gtk_window_set_position( window, GTK_WIN_POS_CENTER );
	window.show();
	gtk_grab_add( window );

	while ( loop )
		gtk_main_iteration();

	strcpy( rs->mainTexture, gtk_entry_get_text( GTK_ENTRY( textFrontBackTex ) ) );
	strcpy( rs->trimTexture, gtk_entry_get_text( GTK_ENTRY( textTrimTex ) ) );

	rs->bScaleMainH = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( checkScaleMainH ) ) ? true : false;
	rs->bScaleMainV = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( checkScaleMainV ) ) ? true : false;
	rs->bScaleTrimH = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( checkScaleTrimH ) ) ? true : false;
	rs->bScaleTrimV = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( checkScaleTrimV ) ) ? true : false;

	if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( radioNS ) ) ) {
		rs->nOrientation = DIRECTION_NS;
	}
	else if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( radioEW ) ) ) {
		rs->nOrientation = DIRECTION_EW;
	}

	gtk_grab_remove( window );
	window.destroy();

	return ret;
//-djbob
}

EMessageBoxReturn DoPathPlotterBox( PathPlotterRS* rs, ui::Window main_window ){
	ui::Widget w{ui::null};
	ModalDialog dialog;

	EMessageBoxReturn ret;
	int loop = 1;

	auto window = main_window.create_dialog_window( "Path Plotter", G_CALLBACK( custom_dialog_delete_callback ), &dialog );

	window.connect( "destroy", G_CALLBACK( gtk_widget_destroy ), NULL );

	gtk_container_set_border_width( GTK_CONTAINER( window ), 10 );

	g_object_set_data( G_OBJECT( window ), "loop", &loop );
	g_object_set_data( G_OBJECT( window ), "ret", &ret );

	gtk_widget_realize( window );

	auto vbox = ui::VBox( FALSE, 10 );
	window.add(vbox);
	vbox.show();

	// ---- vbox ----

	auto hbox = ui::HBox( FALSE, 10 );
	vbox.pack_start( hbox, FALSE, FALSE, 2 );
	hbox.show();

	// ---- hbox ----

	auto text1 = ui::Entry( 256 );
	gtk_entry_set_text( text1, "25" );
	hbox.pack_start( text1, FALSE, FALSE, 2 );
	text1.show();

	w = ui::Label( "Number Of Points" );
	hbox.pack_start( w, FALSE, FALSE, 2 );
	gtk_label_set_justify( GTK_LABEL( w ), GTK_JUSTIFY_LEFT );
	w.show();

	// ---- /hbox ----

	hbox = ui::HBox( FALSE, 10 );
	vbox.pack_start( hbox, FALSE, FALSE, 2 );
	hbox.show();

	// ---- hbox ----

	auto text2 = ui::Entry( 256 );
	gtk_entry_set_text( text2, "3" );
	hbox.pack_start( text2, FALSE, FALSE, 2 );
	text2.show();

	w = ui::Label( "Multipler" );
	hbox.pack_start( w, FALSE, FALSE, 2 );
	gtk_label_set_justify( GTK_LABEL( w ), GTK_JUSTIFY_LEFT );
	w.show();

	// ---- /hbox ----

	w = ui::Label( "Path Distance = dist(start -> apex) * multiplier" );
	vbox.pack_start( w, FALSE, FALSE, 0 );
	gtk_label_set_justify( GTK_LABEL( w ), GTK_JUSTIFY_LEFT );
	w.show();

	hbox = ui::HBox( FALSE, 10 );
	vbox.pack_start( hbox, FALSE, FALSE, 2 );
	hbox.show();

	// ---- hbox ----

	auto text3 = ui::Entry( 256 );
	gtk_entry_set_text( text3, "-800" );
	hbox.pack_start( text3, FALSE, FALSE, 2 );
	text3.show();

	w = ui::Label( "Gravity" );
	hbox.pack_start( w, FALSE, FALSE, 2 );
	gtk_label_set_justify( GTK_LABEL( w ), GTK_JUSTIFY_LEFT );
	w.show();

	// ---- /hbox ----

	w = ui::Widget::from(gtk_hseparator_new());
	vbox.pack_start( w, FALSE, FALSE, 0 );
	w.show();

	auto check1 = ui::CheckButton( "No Dynamic Update" );
	vbox.pack_start( check1, FALSE, FALSE, 0 );
	check1.show();

	auto check2 = ui::CheckButton( "Show Bounding Lines" );
	vbox.pack_start( check2, FALSE, FALSE, 0 );
	check2.show();

	// ---- /vbox ----


	// ----------------- //

	w = ui::Widget::from(gtk_hseparator_new());
	vbox.pack_start( w, FALSE, FALSE, 0 );
	w.show();

	// ----------------- //

	hbox = ui::HBox( FALSE, 10 );
	vbox.pack_start( hbox, FALSE, FALSE, 0 );
	hbox.show();

	w = ui::Button( "Enable" );
	hbox.pack_start( w, TRUE, TRUE, 0 );
	w.connect( "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDYES ) );
	w.show();

	gtk_widget_set_can_default( w, true );
	gtk_widget_grab_default( w );

	w = ui::Button( "Disable" );
	hbox.pack_start( w, TRUE, TRUE, 0 );
	w.connect( "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDNO ) );
	w.show();

	w = ui::Button( "Cancel" );
	hbox.pack_start( w, TRUE, TRUE, 0 );
	w.connect( "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDCANCEL ) );
	w.show();

	ret = eIDCANCEL;

	// ----------------- //

	gtk_window_set_position( window, GTK_WIN_POS_CENTER );
	window.show();
	gtk_grab_add( window );

	bool dialogError = TRUE;
	while ( dialogError )
	{
		loop = 1;
		while ( loop )
			gtk_main_iteration();

		dialogError = FALSE;

		if ( ret == eIDYES ) {
			if ( !ValidateTextIntRange( gtk_entry_get_text( GTK_ENTRY( text1 ) ), 1, 200, "Number Of Points", &rs->nPoints ) ) {
				dialogError = TRUE;
			}

			if ( !ValidateTextFloatRange( gtk_entry_get_text( GTK_ENTRY( text2 ) ), 1.0f, 10.0f, "Multiplier", &rs->fMultiplier ) ) {
				dialogError = TRUE;
			}

			if ( !ValidateTextFloatRange( gtk_entry_get_text( GTK_ENTRY( text3 ) ), -10000.0f, -1.0f, "Gravity", &rs->fGravity ) ) {
				dialogError = TRUE;
			}

			rs->bNoUpdate = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( check1 ) ) ? true : false;
			rs->bShowExtra = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( check2 ) ) ? true : false;
		}
	}

	gtk_grab_remove( window );
	window.destroy();

	return ret;
}

EMessageBoxReturn DoCTFColourChangeBox(){
	ui::Widget w{ui::null};
	EMessageBoxReturn ret;
	int loop = 1;

	auto window = ui::Window( ui::window_type::TOP );

	window.connect( "delete_event", G_CALLBACK( custom_dialog_delete_callback ), NULL );
	window.connect( "destroy", G_CALLBACK( gtk_widget_destroy ), NULL );

	gtk_window_set_title( window, "CTF Colour Changer" );
	gtk_container_set_border_width( GTK_CONTAINER( window ), 10 );

	g_object_set_data( G_OBJECT( window ), "loop", &loop );
	g_object_set_data( G_OBJECT( window ), "ret", &ret );

	gtk_widget_realize( window );



	auto vbox = ui::VBox( FALSE, 10 );
	window.add(vbox);
	vbox.show();

	// ---- vbox ----

	auto hbox = ui::HBox( FALSE, 10 );
	vbox.pack_start( hbox, TRUE, TRUE, 0 );
	hbox.show();

	// ---- hbox ---- ok/cancel buttons

	w = ui::Button( "Red->Blue" );
	hbox.pack_start( w, TRUE, TRUE, 0 );
	w.connect( "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDOK ) );

	gtk_widget_set_can_default( w, true );
	gtk_widget_grab_default( w );
	w.show();

	w = ui::Button( "Blue->Red" );
	hbox.pack_start( w, TRUE, TRUE, 0 );
	w.connect( "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDYES ) );
	w.show();

	w = ui::Button( "Cancel" );
	hbox.pack_start( w, TRUE, TRUE, 0 );
	w.connect( "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDCANCEL ) );
	w.show();
	ret = eIDCANCEL;

	// ---- /hbox ----

	// ---- /vbox ----

	gtk_window_set_position( window, GTK_WIN_POS_CENTER );
	window.show();
	gtk_grab_add( window );

	while ( loop )
		gtk_main_iteration();

	gtk_grab_remove( window );
	window.destroy();

	return ret;
}

EMessageBoxReturn DoResetTextureBox( ResetTextureRS* rs, ui::Window main_window ){
	Str texSelected;

	ui::Widget w{ui::null};
	ModalDialog dialog;

	EMessageBoxReturn ret;
	int loop = 1;

	auto window = main_window.create_dialog_window( "Texture Reset", G_CALLBACK( custom_dialog_delete_callback ), &dialog );
	gtk_window_set_transient_for( GTK_WINDOW( window ), main_window );
	gtk_window_set_modal( GTK_WINDOW( window ), TRUE );

	window.connect( "destroy", G_CALLBACK( gtk_widget_destroy ), NULL );

	gtk_container_set_border_width( GTK_CONTAINER( window ), 10 );

	g_object_set_data( G_OBJECT( window ), "loop", &loop );
	g_object_set_data( G_OBJECT( window ), "ret", &ret );

	gtk_widget_realize( window );

	auto vbox = ui::VBox( FALSE, 10 );
	window.add(vbox);
	vbox.show();

	// ---- vbox ----

	auto hbox = ui::HBox( FALSE, 10 );
	vbox.pack_start( hbox, FALSE, FALSE, 2 );
	hbox.show();

	// ---- hbox ----

	texSelected = "Currently Selected Texture:   ";
	texSelected += GetCurrentTexture();

	w = ui::Label( texSelected );
	hbox.pack_start( w, FALSE, FALSE, 2 );
	gtk_label_set_justify( GTK_LABEL( w ), GTK_JUSTIFY_LEFT );
	w.show();

	// ---- /hbox ----

	auto frame = ui::Frame( "Reset Texture Names" );
	frame.show();
	vbox.pack_start( frame, FALSE, TRUE, 0 );

	auto table = ui::Table( 2, 3, TRUE );
	table.show();
	frame.add(table);
	gtk_table_set_row_spacings(table, 5);
	gtk_table_set_col_spacings(table, 5);
	gtk_container_set_border_width( GTK_CONTAINER( table ), 5 );

	// ---- frame ----

	dlgTexReset.cbTexChange = ui::CheckButton( "Enabled" );
	dlgTexReset.cbTexChange.connect( "toggled", G_CALLBACK( dialog_button_callback_texreset_update ), NULL );
	dlgTexReset.cbTexChange.show();
	table.attach(dlgTexReset.cbTexChange, {0, 1, 0, 1}, {GTK_FILL, 0});

	w = ui::Label( "Old Name: " );
	table.attach(w, {1, 2, 0, 1}, {GTK_FILL, 0});
	w.show();

	dlgTexReset.editTexOld = ui::Entry( 256 );
	gtk_entry_set_text( GTK_ENTRY( dlgTexReset.editTexOld ), rs->textureName );
	table.attach(dlgTexReset.editTexOld, {2, 3, 0, 1}, {GTK_FILL, 0});
	dlgTexReset.editTexOld.show();

	w = ui::Label( "New Name: " );
	table.attach(w, {1, 2, 1, 2}, {GTK_FILL, 0});
	w.show();

	dlgTexReset.editTexNew = ui::Entry( 256 );
	gtk_entry_set_text( GTK_ENTRY( dlgTexReset.editTexNew ), rs->textureName );
	table.attach(dlgTexReset.editTexNew, {2, 3, 1, 2}, {GTK_FILL, 0});
	dlgTexReset.editTexNew.show();

	// ---- /frame ----

	frame = ui::Frame( "Reset Scales" );
	frame.show();
	vbox.pack_start( frame, FALSE, TRUE, 0 );

	table = ui::Table( 2, 3, TRUE );
	table.show();
	frame.add(table);
	gtk_table_set_row_spacings(table, 5);
	gtk_table_set_col_spacings(table, 5);
	gtk_container_set_border_width( GTK_CONTAINER( table ), 5 );

	// ---- frame ----

	dlgTexReset.cbScaleHor = ui::CheckButton( "Enabled" );
	dlgTexReset.cbScaleHor.connect( "toggled", G_CALLBACK( dialog_button_callback_texreset_update ), NULL );
	dlgTexReset.cbScaleHor.show();
	table.attach(dlgTexReset.cbScaleHor, {0, 1, 0, 1}, {GTK_FILL, 0});

	w = ui::Label( "New Horizontal Scale: " );
	table.attach(w, {1, 2, 0, 1}, {GTK_FILL, 0});
	w.show();

	dlgTexReset.editScaleHor = ui::Entry( 256 );
	gtk_entry_set_text( GTK_ENTRY( dlgTexReset.editScaleHor ), "0.5" );
	table.attach(dlgTexReset.editScaleHor, {2, 3, 0, 1}, {GTK_FILL, 0});
	dlgTexReset.editScaleHor.show();


	dlgTexReset.cbScaleVert = ui::CheckButton( "Enabled" );
	dlgTexReset.cbScaleVert.connect( "toggled", G_CALLBACK( dialog_button_callback_texreset_update ), NULL );
	dlgTexReset.cbScaleVert.show();
	table.attach(dlgTexReset.cbScaleVert, {0, 1, 1, 2}, {GTK_FILL, 0});

	w = ui::Label( "New Vertical Scale: " );
	table.attach(w, {1, 2, 1, 2}, {GTK_FILL, 0});
	w.show();

	dlgTexReset.editScaleVert = ui::Entry( 256 );
	gtk_entry_set_text( GTK_ENTRY( dlgTexReset.editScaleVert ), "0.5" );
	table.attach(dlgTexReset.editScaleVert, {2, 3, 1, 2}, {GTK_FILL, 0});
	dlgTexReset.editScaleVert.show();

	// ---- /frame ----

	frame = ui::Frame( "Reset Shift" );
	frame.show();
	vbox.pack_start( frame, FALSE, TRUE, 0 );

	table = ui::Table( 2, 3, TRUE );
	table.show();
	frame.add(table);
	gtk_table_set_row_spacings(table, 5);
	gtk_table_set_col_spacings(table, 5);
	gtk_container_set_border_width( GTK_CONTAINER( table ), 5 );

	// ---- frame ----

	dlgTexReset.cbShiftHor = ui::CheckButton( "Enabled" );
	dlgTexReset.cbShiftHor.connect( "toggled", G_CALLBACK( dialog_button_callback_texreset_update ), NULL );
	dlgTexReset.cbShiftHor.show();
	table.attach(dlgTexReset.cbShiftHor, {0, 1, 0, 1}, {GTK_FILL, 0});

	w = ui::Label( "New Horizontal Shift: " );
	table.attach(w, {1, 2, 0, 1}, {GTK_FILL, 0});
	w.show();

	dlgTexReset.editShiftHor = ui::Entry( 256 );
	gtk_entry_set_text( GTK_ENTRY( dlgTexReset.editShiftHor ), "0" );
	table.attach(dlgTexReset.editShiftHor, {2, 3, 0, 1}, {GTK_FILL, 0});
	dlgTexReset.editShiftHor.show();


	dlgTexReset.cbShiftVert = ui::CheckButton( "Enabled" );
	dlgTexReset.cbShiftVert.connect( "toggled", G_CALLBACK( dialog_button_callback_texreset_update ), NULL );
	dlgTexReset.cbShiftVert.show();
	table.attach(dlgTexReset.cbShiftVert, {0, 1, 1, 2}, {GTK_FILL, 0});

	w = ui::Label( "New Vertical Shift: " );
	table.attach(w, {1, 2, 1, 2}, {GTK_FILL, 0});
	w.show();

	dlgTexReset.editShiftVert = ui::Entry( 256 );
	gtk_entry_set_text( GTK_ENTRY( dlgTexReset.editShiftVert ), "0" );
	table.attach(dlgTexReset.editShiftVert, {2, 3, 1, 2}, {GTK_FILL, 0});
	dlgTexReset.editShiftVert.show();

	// ---- /frame ----

	frame = ui::Frame( "Reset Rotation" );
	frame.show();
	vbox.pack_start( frame, FALSE, TRUE, 0 );

	table = ui::Table( 1, 3, TRUE );
	table.show();
	frame.add(table);
	gtk_table_set_row_spacings(table, 5);
	gtk_table_set_col_spacings(table, 5);
	gtk_container_set_border_width( GTK_CONTAINER( table ), 5 );

	// ---- frame ----

	dlgTexReset.cbRotation = ui::CheckButton( "Enabled" );
	dlgTexReset.cbRotation.show();
	table.attach(dlgTexReset.cbRotation, {0, 1, 0, 1}, {GTK_FILL, 0});

	w = ui::Label( "New Rotation Value: " );
	table.attach(w, {1, 2, 0, 1}, {GTK_FILL, 0});
	w.show();

	dlgTexReset.editRotation = ui::Entry( 256 );
	gtk_entry_set_text( GTK_ENTRY( dlgTexReset.editRotation ), "0" );
	table.attach(dlgTexReset.editRotation, {2, 3, 0, 1}, {GTK_FILL, 0});
	dlgTexReset.editRotation.show();

	// ---- /frame ----

	hbox = ui::HBox( FALSE, 10 );
	vbox.pack_start( hbox, FALSE, FALSE, 2 );
	hbox.show();

	// ---- hbox ----

	w = ui::Button( "Use Selected Brushes" );
	hbox.pack_start( w, TRUE, TRUE, 0 );
	w.connect( "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDOK ) );

	gtk_widget_set_can_default( w, true );
	gtk_widget_grab_default( w );
	w.show();

	w = ui::Button( "Use All Brushes" );
	hbox.pack_start( w, TRUE, TRUE, 0 );
	w.connect( "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDYES ) );
	w.show();

	w = ui::Button( "Cancel" );
	hbox.pack_start( w, TRUE, TRUE, 0 );
	w.connect( "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDCANCEL ) );
	w.show();
	ret = eIDCANCEL;

	// ---- /hbox ----

	// ---- /vbox ----

	gtk_window_set_position( GTK_WINDOW( window ),GTK_WIN_POS_CENTER_ON_PARENT );
	window.show();
	gtk_grab_add( window );

	Update_TextureReseter();

	bool dialogError = TRUE;
	while ( dialogError )
	{
		loop = 1;
		while ( loop )
			gtk_main_iteration();

		dialogError = FALSE;

		if ( ret != eIDCANCEL ) {
			rs->bResetRotation =  gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( dlgTexReset.cbRotation ) );
			if ( rs->bResetRotation ) {
				if ( !ValidateTextInt( gtk_entry_get_text( GTK_ENTRY( dlgTexReset.editRotation ) ), "Rotation", &rs->rotation ) ) {
					dialogError = TRUE;
				}
			}

			rs->bResetScale[0] =  gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( dlgTexReset.cbScaleHor ) );
			if ( rs->bResetScale[0] ) {
				if ( !ValidateTextFloat( gtk_entry_get_text( GTK_ENTRY( dlgTexReset.editScaleHor ) ), "Horizontal Scale", &rs->fScale[0] ) ) {
					dialogError = TRUE;
				}
			}

			rs->bResetScale[1] =  gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( dlgTexReset.cbScaleVert ) );
			if ( rs->bResetScale[1] ) {
				if ( !ValidateTextFloat( gtk_entry_get_text( GTK_ENTRY( dlgTexReset.editScaleVert ) ), "Vertical Scale", &rs->fScale[1] ) ) {
					dialogError = TRUE;
				}
			}

			rs->bResetShift[0] =  gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( dlgTexReset.cbShiftHor ) );
			if ( rs->bResetShift[0] ) {
				if ( !ValidateTextFloat( gtk_entry_get_text( GTK_ENTRY( dlgTexReset.editShiftHor ) ), "Horizontal Shift", &rs->fShift[0] ) ) {
					dialogError = TRUE;
				}
			}

			rs->bResetShift[1] =  gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( dlgTexReset.cbShiftVert ) );
			if ( rs->bResetShift[1] ) {
				if ( !ValidateTextFloat( gtk_entry_get_text( GTK_ENTRY( dlgTexReset.editShiftVert ) ), "Vertical Shift", &rs->fShift[1] ) ) {
					dialogError = TRUE;
				}
			}

			rs->bResetTextureName =  gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( dlgTexReset.cbTexChange ) );
			if ( rs->bResetTextureName ) {
				strcpy( rs->textureName,     gtk_entry_get_text( GTK_ENTRY( dlgTexReset.editTexOld ) ) );
				strcpy( rs->newTextureName,  gtk_entry_get_text( GTK_ENTRY( dlgTexReset.editTexNew ) ) );
			}
		}
	}

	gtk_grab_remove( window );
	window.destroy();

	return ret;
}

EMessageBoxReturn DoTrainThingBox( TrainThingRS* rs ){
	Str texSelected;

	ui::Widget w{ui::null};

	ui::Widget radiusX{ui::null}, radiusY{ui::null};
	ui::Widget angleStart{ui::null}, angleEnd{ui::null};
	ui::Widget heightStart{ui::null}, heightEnd{ui::null};
	ui::Widget numPoints{ui::null};

	EMessageBoxReturn ret;
	int loop = 1;

	auto window = ui::Window( ui::window_type::TOP );

	window.connect( "delete_event", G_CALLBACK( custom_dialog_delete_callback ), NULL );
	window.connect( "destroy", G_CALLBACK( gtk_widget_destroy ), NULL );

	gtk_window_set_title( window, "Train Thing" );
	gtk_container_set_border_width( GTK_CONTAINER( window ), 10 );

	g_object_set_data( G_OBJECT( window ), "loop", &loop );
	g_object_set_data( G_OBJECT( window ), "ret", &ret );

	gtk_widget_realize( window );

	auto vbox = ui::VBox( FALSE, 10 );
	window.add(vbox);
	vbox.show();

	// ---- vbox ----

	auto hbox = ui::HBox( FALSE, 10 );
	vbox.pack_start( hbox, FALSE, FALSE, 2 );
	hbox.show();

	// ---- /hbox ----

	auto frame = ui::Frame( "Radii" );
	frame.show();
	vbox.pack_start( frame, FALSE, TRUE, 0 );

	auto table = ui::Table( 2, 3, TRUE );
	table.show();
	frame.add(table);
	gtk_table_set_row_spacings(table, 5);
	gtk_table_set_col_spacings(table, 5);
	gtk_container_set_border_width( GTK_CONTAINER( table ), 5 );

	// ---- frame ----

	w = ui::Label( "X: " );
	table.attach(w, {0, 1, 0, 1}, {GTK_FILL, 0});
	w.show();

	radiusX = ui::Entry( 256 );
	gtk_entry_set_text( GTK_ENTRY( radiusX ), "100" );
	table.attach(radiusX, {1, 2, 0, 1}, {GTK_FILL, 0});
	radiusX.show();



	w = ui::Label( "Y: " );
	table.attach(w, {0, 1, 1, 2}, {GTK_FILL, 0});
	w.show();

	radiusY = ui::Entry( 256 );
	gtk_entry_set_text( GTK_ENTRY( radiusY ), "100" );
	table.attach(radiusY, {1, 2, 1, 2}, {GTK_FILL, 0});
	radiusY.show();



	frame = ui::Frame( "Angles" );
	frame.show();
	vbox.pack_start( frame, FALSE, TRUE, 0 );

	table = ui::Table( 2, 3, TRUE );
	table.show();
	frame.add(table);
	gtk_table_set_row_spacings((table), 5);
	gtk_table_set_col_spacings((table), 5);
	gtk_container_set_border_width( GTK_CONTAINER( table ), 5 );

	// ---- frame ----

	w = ui::Label( "Start: " );
	table.attach(w, {0, 1, 0, 1}, {GTK_FILL, 0});
	w.show();

	angleStart = ui::Entry( 256 );
	gtk_entry_set_text( GTK_ENTRY( angleStart ), "0" );
	table.attach(angleStart, {1, 2, 0, 1}, {GTK_FILL, 0});
	angleStart.show();



	w = ui::Label( "End: " );
	table.attach(w, {0, 1, 1, 2}, {GTK_FILL, 0});
	w.show();

	angleEnd = ui::Entry( 256 );
	gtk_entry_set_text( GTK_ENTRY( angleEnd ), "90" );
	table.attach(angleEnd, {1, 2, 1, 2}, {GTK_FILL, 0});
	angleEnd.show();


	frame = ui::Frame( "Height" );
	frame.show();
	vbox.pack_start( frame, FALSE, TRUE, 0 );

	table = ui::Table( 2, 3, TRUE );
	table.show();
	frame.add(table);
	gtk_table_set_row_spacings(table, 5);
	gtk_table_set_col_spacings(table, 5);
	gtk_container_set_border_width( GTK_CONTAINER( table ), 5 );

	// ---- frame ----

	w = ui::Label( "Start: " );
	table.attach(w, {0, 1, 0, 1}, {GTK_FILL, 0});
	w.show();

	heightStart = ui::Entry( 256 );
	gtk_entry_set_text( GTK_ENTRY( heightStart ), "0" );
	table.attach(heightStart, {1, 2, 0, 1}, {GTK_FILL, 0});
	heightStart.show();



	w = ui::Label( "End: " );
	table.attach(w, {0, 1, 1, 2}, {GTK_FILL, 0});
	w.show();

	heightEnd = ui::Entry( 256 );
	gtk_entry_set_text( GTK_ENTRY( heightEnd ), "0" );
	table.attach(heightEnd, {1, 2, 1, 2}, {GTK_FILL, 0});
	heightEnd.show();



	frame = ui::Frame( "Points" );
	frame.show();
	vbox.pack_start( frame, FALSE, TRUE, 0 );

	table = ui::Table( 2, 3, TRUE );
	table.show();
	frame.add(table);
	gtk_table_set_row_spacings(table, 5);
	gtk_table_set_col_spacings(table, 5);
	gtk_container_set_border_width( GTK_CONTAINER( table ), 5 );

	// ---- frame ----

	w = ui::Label( "Number: " );
	table.attach(w, {0, 1, 0, 1}, {GTK_FILL, 0});
	w.show();

	numPoints = ui::Entry( 256 );
	gtk_entry_set_text( GTK_ENTRY( numPoints ), "0" );
	table.attach(numPoints, {1, 2, 0, 1}, {GTK_FILL, 0});
	numPoints.show();


	hbox = ui::HBox( FALSE, 10 );
	vbox.pack_start( hbox, FALSE, FALSE, 2 );
	hbox.show();

	// ---- hbox ----

	w = ui::Button( "Ok" );
	hbox.pack_start( w, TRUE, TRUE, 0 );
	w.connect( "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDOK ) );

	gtk_widget_set_can_default( w, true );
	gtk_widget_grab_default( w );
	w.show();

	w = ui::Button( "Cancel" );
	hbox.pack_start( w, TRUE, TRUE, 0 );
	w.connect( "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDCANCEL ) );
	w.show();
	ret = eIDCANCEL;

	// ---- /hbox ----



	gtk_window_set_position( window, GTK_WIN_POS_CENTER );
	window.show();
	gtk_grab_add( window );

	bool dialogError = TRUE;
	while ( dialogError )
	{
		loop = 1;
		while ( loop )
			gtk_main_iteration();

		dialogError = FALSE;

		if ( ret != eIDCANCEL ) {
			if ( !ValidateTextFloat( gtk_entry_get_text( GTK_ENTRY( radiusX ) ), "Radius (X)", &rs->fRadiusX ) ) {
				dialogError = TRUE;
			}

			if ( !ValidateTextFloat( gtk_entry_get_text( GTK_ENTRY( radiusY ) ), "Radius (Y)", &rs->fRadiusY ) ) {
				dialogError = TRUE;
			}

			if ( !ValidateTextFloat( gtk_entry_get_text( GTK_ENTRY( angleStart ) ), "Angle (Start)", &rs->fStartAngle ) ) {
				dialogError = TRUE;
			}

			if ( !ValidateTextFloat( gtk_entry_get_text( GTK_ENTRY( angleEnd ) ), "Angle (End)", &rs->fEndAngle ) ) {
				dialogError = TRUE;
			}

			if ( !ValidateTextFloat( gtk_entry_get_text( GTK_ENTRY( heightStart ) ), "Height (Start)", &rs->fStartHeight ) ) {
				dialogError = TRUE;
			}

			if ( !ValidateTextFloat( gtk_entry_get_text( GTK_ENTRY( heightEnd ) ), "Height (End)", &rs->fEndHeight ) ) {
				dialogError = TRUE;
			}

			if ( !ValidateTextInt( gtk_entry_get_text( GTK_ENTRY( numPoints ) ), "Num Points", &rs->iNumPoints ) ) {
				dialogError = TRUE;
			}
		}
	}

	gtk_grab_remove( window );
	window.destroy();

	return ret;
}
// ailmanki
// add a simple input for the MakeChain thing..
EMessageBoxReturn DoMakeChainBox( MakeChainRS* rs ){
	ui::Widget   w{ui::null};
	ui::Entry textlinkNum{ui::null}, textlinkName{ui::null};
	EMessageBoxReturn ret;
	int loop = 1;

	const char *text = "Please set a value in the boxes below and press 'OK' to make a chain";

	auto window = ui::Window( ui::window_type::TOP );

	window.connect( "delete_event", G_CALLBACK( custom_dialog_delete_callback ), NULL );
	window.connect( "destroy", G_CALLBACK( gtk_widget_destroy ), NULL );

	gtk_window_set_title( window, "Make Chain" );

	gtk_container_set_border_width( GTK_CONTAINER( window ), 10 );

	g_object_set_data( G_OBJECT( window ), "loop", &loop );
	g_object_set_data( G_OBJECT( window ), "ret", &ret );

	gtk_widget_realize( window );

	// new vbox
	auto vbox = ui::VBox( FALSE, 10 );
	window.add(vbox);
	vbox.show();

	auto hbox = ui::HBox( FALSE, 10 );
	vbox.add(hbox);
	hbox.show();

	// dunno if you want this text or not ...
	w = ui::Label( text );
	hbox.pack_start( w, FALSE, FALSE, 0 );
	w.show();

	w = ui::Widget::from(gtk_hseparator_new());
	vbox.pack_start( w, FALSE, FALSE, 0 );
	w.show();

	// ------------------------- //

	// new hbox
	hbox = ui::HBox( FALSE, 10 );
	vbox.pack_start( hbox, FALSE, FALSE, 0 );
	hbox.show();

	textlinkNum = ui::Entry( 256 );
	hbox.pack_start( textlinkNum, FALSE, FALSE, 1 );
	textlinkNum.show();

	w = ui::Label( "Number of elements in chain" );
	hbox.pack_start( w, FALSE, FALSE, 1 );
	w.show();

	// -------------------------- //

	hbox = ui::HBox( FALSE, 10 );
	vbox.pack_start( hbox, FALSE, FALSE, 0 );
	hbox.show();

	textlinkName = ui::Entry( 256 );
	hbox.pack_start( textlinkName, FALSE, FALSE, 0 );
	textlinkName.show();

	w = ui::Label( "Basename for chain's targetnames." );
	hbox.pack_start( w, FALSE, FALSE, 1 );
	w.show();


	w = ui::Button( "OK" );
	hbox.pack_start( w, TRUE, TRUE, 0 );
	w.connect( "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDOK ) );
	gtk_widget_set_can_default( w, true );
	gtk_widget_grab_default( w );
	w.show();

	w = ui::Button( "Cancel" );
	hbox.pack_start( w, TRUE, TRUE, 0 );
	w.connect( "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( eIDCANCEL ) );
	w.show();

	ret = eIDCANCEL;

	gtk_window_set_position(  window, GTK_WIN_POS_CENTER );
	window.show();
	gtk_grab_add( window );

	bool dialogError = TRUE;
	while ( dialogError )
	{
		loop = 1;
		while ( loop )
			gtk_main_iteration();

		dialogError = FALSE;

		if ( ret == eIDOK ) {
			strcpy( rs->linkName, gtk_entry_get_text( textlinkName ) );
			if ( !ValidateTextInt( gtk_entry_get_text( textlinkNum ), "Elements", &rs->linkNum ) ) {
				dialogError = TRUE;
			}
		}
	}

	gtk_grab_remove( window );
	window.destroy();

	return ret;
}
