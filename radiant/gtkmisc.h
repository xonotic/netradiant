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

#if !defined( INCLUDED_GTKMISC_H )
#define INCLUDED_GTKMISC_H

#include <uilib/uilib.h>
#include "gtkutil/toolbar.h"

void command_connect_accelerator( const char* commandName );
void command_disconnect_accelerator( const char* commandName );
void toggle_add_accelerator( const char* commandName );
void toggle_remove_accelerator( const char* name );

// this also sets up the shortcut using command_connect_accelerator
ui::MenuItem create_menu_item_with_mnemonic( ui::Menu menu, const char *mnemonic, const char* commandName );
// this also sets up the shortcut using command_connect_accelerator
ui::CheckMenuItem create_check_menu_item_with_mnemonic( ui::Menu menu, const char* mnemonic, const char* commandName );


// this DOES NOT set up the shortcut using command_connect_accelerator
ui::ToolButton toolbar_append_button( ui::Toolbar toolbar, const char* description, const char* icon, const char* commandName );
// this DOES NOT set up the shortcut using command_connect_accelerator
ui::ToggleToolButton toolbar_append_toggle_button( ui::Toolbar toolbar, const char* description, const char* icon, const char* commandName );


template<typename Element> class BasicVector3;
typedef BasicVector3<float> Vector3;
bool color_dialog( ui::Window parent, Vector3& color, const char* title = "Choose Color" );

void button_clicked_entry_browse_file( ui::Widget widget, ui::Entry entry );
void button_clicked_entry_browse_directory( ui::Widget widget, ui::Entry entry );

#endif
