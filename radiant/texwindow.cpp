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
// Texture Window
//
// Leonardo Zide (leo@lokigames.com)
//

#include "texwindow.h"

#include <gtk/gtk.h>

#include "debugging/debugging.h"
#include "warnings.h"

#include "defaults.h"
#include "ifilesystem.h"
#include "iundo.h"
#include "igl.h"
#include "iarchive.h"
#include "moduleobserver.h"

#include <set>
#include <string>
#include <vector>

#include <uilib/uilib.h>

#include "signal/signal.h"
#include "math/vector.h"
#include "texturelib.h"
#include "string/string.h"
#include "shaderlib.h"
#include "os/file.h"
#include "os/path.h"
#include "stream/memstream.h"
#include "stream/textfilestream.h"
#include "stream/stringstream.h"
#include "cmdlib.h"
#include "texmanip.h"
#include "textures.h"
#include "convert.h"

#include "gtkutil/menu.h"
#include "gtkutil/nonmodal.h"
#include "gtkutil/cursor.h"
#include "gtkutil/widget.h"
#include "gtkutil/glwidget.h"
#include "gtkutil/messagebox.h"

#include "error.h"
#include "map.h"
#include "qgl.h"
#include "select.h"
#include "brush_primit.h"
#include "brushmanip.h"
#include "patchmanip.h"
#include "plugin.h"
#include "qe3.h"
#include "gtkdlgs.h"
#include "gtkmisc.h"
#include "mainframe.h"
#include "findtexturedialog.h"
#include "surfacedialog.h"
#include "patchdialog.h"
#include "groupdialog.h"
#include "preferences.h"
#include "shaders.h"
#include "commands.h"

bool TextureBrowser_showWads(){
	return !string_empty( g_pGameDescription->getKeyValue( "show_wads" ) );
}

void TextureBrowser_queueDraw( TextureBrowser& textureBrowser );

bool string_equal_start( const char* string, StringRange start ){
	return string_equal_n( string, start.first, start.last - start.first );
}

typedef std::set<CopiedString> TextureGroups;

void TextureGroups_addWad( TextureGroups& groups, const char* archive ){
	if ( extension_equal( path_get_extension( archive ), "wad" ) ) {
		groups.insert( archive );
	}
}

typedef ReferenceCaller<TextureGroups, void(const char*), TextureGroups_addWad> TextureGroupsAddWadCaller;

namespace
{
bool g_TextureBrowser_shaderlistOnly = false;
bool g_TextureBrowser_fixedSize = true;
bool g_TextureBrowser_filterMissing = false;
bool g_TextureBrowser_filterFallback = true;
bool g_TextureBrowser_enableAlpha = false;
}

CopiedString g_notex;
CopiedString g_shadernotex;

bool isMissing(const char* name);

bool isNotex(const char* name);

bool isMissing(const char* name){
	if ( string_equal( g_notex.c_str(), name ) ) {
		return true;
	}
	if ( string_equal( g_shadernotex.c_str(), name ) ) {
		return true;
	}
	return false;
}

bool isNotex(const char* name){
	if ( string_equal_suffix( name, "/" DEFAULT_NOTEX_BASENAME ) ) {
		return true;
	}
	if ( string_equal_suffix( name, "/" DEFAULT_SHADERNOTEX_BASENAME ) ) {
		return true;
	}
	return false;
}

void TextureGroups_addShader( TextureGroups& groups, const char* shaderName ){
	const char* texture = path_make_relative( shaderName, "textures/" );

	// hide notex / shadernotex images
	if ( g_TextureBrowser_filterFallback ) {
		if ( isNotex( shaderName ) ) {
			return;
		}
		if ( isNotex( texture ) ) {
			return;
		}
	}

	if ( texture != shaderName ) {
		const char* last = path_remove_directory( texture );
		if ( !string_empty( last ) ) {
			groups.insert( CopiedString( StringRange( texture, --last ) ) );
		}
	}
}

typedef ReferenceCaller<TextureGroups, void(const char*), TextureGroups_addShader> TextureGroupsAddShaderCaller;

void TextureGroups_addDirectory( TextureGroups& groups, const char* directory ){
	groups.insert( directory );
}

typedef ReferenceCaller<TextureGroups, void(const char*), TextureGroups_addDirectory> TextureGroupsAddDirectoryCaller;

class DeferredAdjustment
{
gdouble m_value;
guint m_handler;

typedef void ( *ValueChangedFunction )( void* data, gdouble value );

ValueChangedFunction m_function;
void* m_data;

static gboolean deferred_value_changed( gpointer data ){
	reinterpret_cast<DeferredAdjustment*>( data )->m_function(
		reinterpret_cast<DeferredAdjustment*>( data )->m_data,
		reinterpret_cast<DeferredAdjustment*>( data )->m_value
		);
	reinterpret_cast<DeferredAdjustment*>( data )->m_handler = 0;
	reinterpret_cast<DeferredAdjustment*>( data )->m_value = 0;
	return FALSE;
}

public:
DeferredAdjustment( ValueChangedFunction function, void* data ) : m_value( 0 ), m_handler( 0 ), m_function( function ), m_data( data ){
}

void flush(){
	if ( m_handler != 0 ) {
		g_source_remove( m_handler );
		deferred_value_changed( this );
	}
}

void value_changed( gdouble value ){
	m_value = value;
	if ( m_handler == 0 ) {
		m_handler = g_idle_add( deferred_value_changed, this );
	}
}

static void adjustment_value_changed(ui::Adjustment adjustment, DeferredAdjustment* self ){
	self->value_changed( gtk_adjustment_get_value(adjustment) );
}
};


class TextureBrowser;

typedef ReferenceCaller<TextureBrowser, void(), TextureBrowser_queueDraw> TextureBrowserQueueDrawCaller;

void TextureBrowser_scrollChanged( void* data, gdouble value );


enum StartupShaders
{
	STARTUPSHADERS_NONE = 0,
	STARTUPSHADERS_COMMON,
};

void TextureBrowser_hideUnusedExport( const Callback<void(bool)> & importer );

typedef FreeCaller<void(const Callback<void(bool)> &), TextureBrowser_hideUnusedExport> TextureBrowserHideUnusedExport;

void TextureBrowser_showShadersExport( const Callback<void(bool)> & importer );

typedef FreeCaller<void(const Callback<void(bool)> &), TextureBrowser_showShadersExport> TextureBrowserShowShadersExport;

void TextureBrowser_showTexturesExport( const Callback<void(bool)> & importer );

typedef FreeCaller<void(const Callback<void(bool)> &), TextureBrowser_showTexturesExport> TextureBrowserShowTexturesExport;

void TextureBrowser_showShaderlistOnly( const Callback<void(bool)> & importer );

typedef FreeCaller<void(const Callback<void(bool)> &), TextureBrowser_showShaderlistOnly> TextureBrowserShowShaderlistOnlyExport;

void TextureBrowser_fixedSize( const Callback<void(bool)> & importer );

typedef FreeCaller<void(const Callback<void(bool)> &), TextureBrowser_fixedSize> TextureBrowserFixedSizeExport;

void TextureBrowser_filterMissing( const Callback<void(bool)> & importer );

typedef FreeCaller<void(const Callback<void(bool)> &), TextureBrowser_filterMissing> TextureBrowserFilterMissingExport;

void TextureBrowser_filterFallback( const Callback<void(bool)> & importer );

typedef FreeCaller<void(const Callback<void(bool)> &), TextureBrowser_filterFallback> TextureBrowserFilterFallbackExport;

void TextureBrowser_enableAlpha( const Callback<void(bool)> & importer );

typedef FreeCaller<void(const Callback<void(bool)> &), TextureBrowser_enableAlpha> TextureBrowserEnableAlphaExport;

class TextureBrowser
{
public:
int width, height;
int originy;
int m_nTotalHeight;

CopiedString shader;

ui::Window m_parent{ui::null};
#ifdef WORKAROUND_MACOS_GTK2_GLWIDGET
ui::VBox m_vframe{ui::null};
ui::VBox m_vfiller{ui::null};
ui::HBox m_hframe{ui::null};
ui::HBox m_hfiller{ui::null};
#else // !WORKAROUND_MACOS_GTK2_GLWIDGET
ui::VBox m_frame{ui::null};
#endif // !WORKAROUND_MACOS_GTK2_GLWIDGET
ui::GLArea m_gl_widget{ui::null};
ui::Widget m_texture_scroll{ui::null};
ui::TreeView m_treeViewTree{ui::New};
ui::TreeView m_treeViewTags{ui::null};
ui::Frame m_tag_frame{ui::null};
ui::ListStore m_assigned_store{ui::null};
ui::ListStore m_available_store{ui::null};
ui::TreeView m_assigned_tree{ui::null};
ui::TreeView m_available_tree{ui::null};
ui::Widget m_scr_win_tree{ui::null};
ui::Widget m_scr_win_tags{ui::null};
ui::Widget m_tag_notebook{ui::null};
ui::Button m_search_button{ui::null};
ui::Widget m_shader_info_item{ui::null};

std::set<CopiedString> m_all_tags;
ui::ListStore m_all_tags_list{ui::null};
std::vector<CopiedString> m_copied_tags;
std::set<CopiedString> m_found_shaders;

ToggleItem m_hideunused_item;
ToggleItem m_hidenotex_item;
ToggleItem m_showshaders_item;
ToggleItem m_showtextures_item;
ToggleItem m_showshaderlistonly_item;
ToggleItem m_fixedsize_item;
ToggleItem m_filternotex_item;
ToggleItem m_enablealpha_item;

guint m_sizeHandler;
guint m_exposeHandler;

bool m_heightChanged;
bool m_originInvalid;

DeferredAdjustment m_scrollAdjustment;
FreezePointer m_freezePointer;

Vector3 color_textureback;
// the increment step we use against the wheel mouse
std::size_t m_mouseWheelScrollIncrement;
std::size_t m_textureScale;
// make the texture increments match the grid changes
bool m_showShaders;
bool m_showTextures;
bool m_showTextureScrollbar;
StartupShaders m_startupShaders;
// if true, the texture window will only display in-use shaders
// if false, all the shaders in memory are displayed
bool m_hideUnused;
bool m_rmbSelected;
bool m_searchedTags;
bool m_tags;
bool m_move_started;
// The uniform size (in pixels) that textures are resized to when m_resizeTextures is true.
int m_uniformTextureSize;
int m_uniformTextureMinSize;

// Return the display width of a texture in the texture browser
void getTextureWH( qtexture_t* tex, int &W, int &H ){
		// Don't use uniform size
		W = (int)( tex->width * ( (float)m_textureScale / 100 ) );
		H = (int)( tex->height * ( (float)m_textureScale / 100 ) );
		if ( W < 1 ) W = 1;
		if ( H < 1 ) H = 1;

	if ( g_TextureBrowser_fixedSize ){
		if	( W >= H ) {
			// Texture is square, or wider than it is tall
			if ( W >= m_uniformTextureSize ){
				H = m_uniformTextureSize * H / W;
				W = m_uniformTextureSize;
			}
			else if ( W <= m_uniformTextureMinSize ){
				H = m_uniformTextureMinSize * H / W;
				W = m_uniformTextureMinSize;
			}
		}
		else {
			// Texture taller than it is wide
			if ( H >= m_uniformTextureSize ){
				W = m_uniformTextureSize * W / H;
				H = m_uniformTextureSize;
			}
			else if ( H <= m_uniformTextureMinSize ){
				W = m_uniformTextureMinSize * W / H;
				H = m_uniformTextureMinSize;
			}
		}
	}
}

TextureBrowser() :
	m_texture_scroll( ui::null ),
	m_hideunused_item( TextureBrowserHideUnusedExport() ),
	m_hidenotex_item( TextureBrowserFilterFallbackExport() ),
	m_showshaders_item( TextureBrowserShowShadersExport() ),
	m_showtextures_item( TextureBrowserShowTexturesExport() ),
	m_showshaderlistonly_item( TextureBrowserShowShaderlistOnlyExport() ),
	m_fixedsize_item( TextureBrowserFixedSizeExport() ),
	m_filternotex_item( TextureBrowserFilterMissingExport() ),
	m_enablealpha_item( TextureBrowserEnableAlphaExport() ),
	m_heightChanged( true ),
	m_originInvalid( true ),
	m_scrollAdjustment( TextureBrowser_scrollChanged, this ),
	color_textureback( 0.25f, 0.25f, 0.25f ),
	m_mouseWheelScrollIncrement( 64 ),
	m_textureScale( 50 ),
	m_showShaders( true ),
	m_showTextures( true ),
	m_showTextureScrollbar( true ),
	m_startupShaders( STARTUPSHADERS_NONE ),
	m_hideUnused( false ),
	m_rmbSelected( false ),
	m_searchedTags( false ),
	m_tags( false ),
	m_uniformTextureSize( 160 ),
	m_uniformTextureMinSize( 48 ),
	m_move_started( false ){
}
};

void ( *TextureBrowser_textureSelected )( const char* shader );


void TextureBrowser_updateScroll( TextureBrowser& textureBrowser );


const char* TextureBrowser_getCommonShadersName(){
	const char* value = g_pGameDescription->getKeyValue( "common_shaders_name" );
	if ( !string_empty( value ) ) {
		return value;
	}
	return "Common";
}

const char* TextureBrowser_getCommonShadersDir(){
	const char* value = g_pGameDescription->getKeyValue( "common_shaders_dir" );
	if ( !string_empty( value ) ) {
		return value;
	}
	return "common/";
}

inline int TextureBrowser_fontHeight( TextureBrowser& textureBrowser ){
	return GlobalOpenGL().m_font->getPixelHeight();
}

const char* TextureBrowser_GetSelectedShader( TextureBrowser& textureBrowser ){
	return textureBrowser.shader.c_str();
}

void TextureBrowser_SetStatus( TextureBrowser& textureBrowser, const char* name ){
	IShader* shader = QERApp_Shader_ForName( name );
	qtexture_t* q = shader->getTexture();
	StringOutputStream strTex( 256 );
	strTex << name << " W: " << Unsigned( q->width ) << " H: " << Unsigned( q->height );
	shader->DecRef();
	g_pParentWnd->SetStatusText( g_pParentWnd->m_texture_status, strTex.c_str() );
}

void TextureBrowser_Focus( TextureBrowser& textureBrowser, const char* name );

void TextureBrowser_SetSelectedShader( TextureBrowser& textureBrowser, const char* shader ){
	textureBrowser.shader = shader;
	TextureBrowser_SetStatus( textureBrowser, shader );
	TextureBrowser_Focus( textureBrowser, shader );

	if ( FindTextureDialog_isOpen() ) {
		FindTextureDialog_selectTexture( shader );
	}

	// disable the menu item "shader info" if no shader was selected
	IShader* ishader = QERApp_Shader_ForName( shader );
	CopiedString filename = ishader->getShaderFileName();

	if ( filename.empty() ) {
		if ( textureBrowser.m_shader_info_item != NULL ) {
			gtk_widget_set_sensitive( textureBrowser.m_shader_info_item, FALSE );
		}
	}
	else {
		gtk_widget_set_sensitive( textureBrowser.m_shader_info_item, TRUE );
	}

	ishader->DecRef();
}


CopiedString g_TextureBrowser_currentDirectory;

/*
   ============================================================================

   TEXTURE LAYOUT

   TTimo: now based on a rundown through all the shaders
   NOTE: we expect the Active shaders count doesn't change during a Texture_StartPos .. Texture_NextPos cycle
   otherwise we may need to rely on a list instead of an array storage
   ============================================================================
 */

class TextureLayout
{
public:
// texture layout functions
// TTimo: now based on shaders
int current_x, current_y, current_row;
};

void Texture_StartPos( TextureLayout& layout ){
	layout.current_x = 8;
	layout.current_y = -8;
	layout.current_row = 0;
}

void Texture_NextPos( TextureBrowser& textureBrowser, TextureLayout& layout, qtexture_t* current_texture, int *x, int *y ){
	qtexture_t* q = current_texture;

	int nWidth, nHeight;
	textureBrowser.getTextureWH( q, nWidth, nHeight );
	if ( layout.current_x + nWidth > textureBrowser.width - 8 && layout.current_row ) { // go to the next row unless the texture is the first on the row
		layout.current_x = 8;
		layout.current_y -= layout.current_row + TextureBrowser_fontHeight( textureBrowser ) + 4;//+4
		layout.current_row = 0;
	}

	*x = layout.current_x;
	*y = layout.current_y;

	// Is our texture larger than the row? If so, grow the
	// row height to match it

	if ( layout.current_row < nHeight ) {
		layout.current_row = nHeight;
	}

	// never go less than 96, or the names get all crunched up
	layout.current_x += nWidth < 96 ? 96 : nWidth;
	layout.current_x += 8;
}

bool TextureSearch_IsShown( const char* name ){
	std::set<CopiedString>::iterator iter;

	iter = GlobalTextureBrowser().m_found_shaders.find( name );

	if ( iter == GlobalTextureBrowser().m_found_shaders.end() ) {
		return false;
	}
	else {
		return true;
	}
}

// if texture_showinuse jump over non in-use textures
bool Texture_IsShown( IShader* shader, bool show_shaders, bool show_textures, bool hideUnused ){
	// filter missing shaders
	// ugly: filter on built-in fallback name after substitution
	if ( g_TextureBrowser_filterMissing ) {
		if ( isMissing( shader->getTexture()->name ) ) {
			return false;
		}
	}
	// filter the fallback (notex/shadernotex) for missing shaders or editor image
	if ( g_TextureBrowser_filterFallback ) {
		if ( isNotex( shader->getName() ) ) {
			return false;
		}
		if ( isNotex( shader->getTexture()->name ) ) {
			return false;
		}
	}

	if ( g_TextureBrowser_currentDirectory == "Untagged" ) {
		std::set<CopiedString>::iterator iter;

		iter = GlobalTextureBrowser().m_found_shaders.find( shader->getName() );

		if ( iter == GlobalTextureBrowser().m_found_shaders.end() ) {
			return false;
		}
		else {
			return true;
		}
	}

	if ( !shader_equal_prefix( shader->getName(), "textures/" ) ) {
		return false;
	}

	if ( !show_shaders && !shader->IsDefault() ) {
		return false;
	}

	if ( !show_textures && shader->IsDefault() ) {
		return false;
	}

	if ( hideUnused && !shader->IsInUse() ) {
		return false;
	}

	if ( GlobalTextureBrowser().m_searchedTags ) {
		if ( !TextureSearch_IsShown( shader->getName() ) ) {
			return false;
		}
		else {
			return true;
		}
	}
	else {
		if ( TextureBrowser_showWads() )
		{
			if ( g_TextureBrowser_currentDirectory != ""
				&& !string_equal( shader->getWadName(), g_TextureBrowser_currentDirectory.c_str() ) )
			{
				return false;
			}
		}
		else if ( !shader_equal_prefix( shader_get_textureName( shader->getName() ), g_TextureBrowser_currentDirectory.c_str() ) ) {
			return false;
		}
	}

	return true;
}

void TextureBrowser_heightChanged( TextureBrowser& textureBrowser ){
	textureBrowser.m_heightChanged = true;

	TextureBrowser_updateScroll( textureBrowser );
	TextureBrowser_queueDraw( textureBrowser );
}

void TextureBrowser_evaluateHeight( TextureBrowser& textureBrowser ){
	if ( textureBrowser.m_heightChanged ) {
		textureBrowser.m_heightChanged = false;

		textureBrowser.m_nTotalHeight = 0;

		TextureLayout layout;
		Texture_StartPos( layout );
		for ( QERApp_ActiveShaders_IteratorBegin(); !QERApp_ActiveShaders_IteratorAtEnd(); QERApp_ActiveShaders_IteratorIncrement() )
		{
			IShader* shader = QERApp_ActiveShaders_IteratorCurrent();

			if ( !Texture_IsShown( shader, textureBrowser.m_showShaders, textureBrowser.m_showTextures, textureBrowser.m_hideUnused ) ) {
				continue;
			}

			int x, y;
			Texture_NextPos( textureBrowser, layout, shader->getTexture(), &x, &y );
			int nWidth, nHeight;
			textureBrowser.getTextureWH( shader->getTexture(), nWidth, nHeight );
			textureBrowser.m_nTotalHeight = std::max( textureBrowser.m_nTotalHeight, abs( layout.current_y ) + TextureBrowser_fontHeight( textureBrowser ) + nHeight + 4 );
		}
	}
}

int TextureBrowser_TotalHeight( TextureBrowser& textureBrowser ){
	TextureBrowser_evaluateHeight( textureBrowser );
	return textureBrowser.m_nTotalHeight;
}

inline const int& min_int( const int& left, const int& right ){
	return std::min( left, right );
}

void TextureBrowser_clampOriginY( TextureBrowser& textureBrowser ){
	if ( textureBrowser.originy > 0 ) {
		textureBrowser.originy = 0;
	}
	int lower = min_int( textureBrowser.height - TextureBrowser_TotalHeight( textureBrowser ), 0 );
	if ( textureBrowser.originy < lower ) {
		textureBrowser.originy = lower;
	}
}

int TextureBrowser_getOriginY( TextureBrowser& textureBrowser ){
	if ( textureBrowser.m_originInvalid ) {
		textureBrowser.m_originInvalid = false;
		TextureBrowser_clampOriginY( textureBrowser );
		TextureBrowser_updateScroll( textureBrowser );
	}
	return textureBrowser.originy;
}

void TextureBrowser_setOriginY( TextureBrowser& textureBrowser, int originy ){
	textureBrowser.originy = originy;
	TextureBrowser_clampOriginY( textureBrowser );
	TextureBrowser_updateScroll( textureBrowser );
	TextureBrowser_queueDraw( textureBrowser );
}


Signal0 g_activeShadersChangedCallbacks;

void TextureBrowser_addActiveShadersChangedCallback( const SignalHandler& handler ){
	g_activeShadersChangedCallbacks.connectLast( handler );
}

void TextureBrowser_constructTreeStore();

class ShadersObserver : public ModuleObserver
{
Signal0 m_realiseCallbacks;
public:
void realise(){
	m_realiseCallbacks();
	/* texturebrowser tree update on vfs restart */
//	TextureBrowser_constructTreeStore();
}

void unrealise(){
}

void insert( const SignalHandler& handler ){
	m_realiseCallbacks.connectLast( handler );
}
};

namespace
{
ShadersObserver g_ShadersObserver;
}

void TextureBrowser_addShadersRealiseCallback( const SignalHandler& handler ){
	g_ShadersObserver.insert( handler );
}

void TextureBrowser_activeShadersChanged( TextureBrowser& textureBrowser ){
	TextureBrowser_heightChanged( textureBrowser );
	textureBrowser.m_originInvalid = true;

	g_activeShadersChangedCallbacks();
}

struct TextureBrowser_ShowScrollbar {
	static void Export(const TextureBrowser &self, const Callback<void(bool)> &returnz) {
		returnz(self.m_showTextureScrollbar);
	}

	static void Import(TextureBrowser &self, bool value) {
		self.m_showTextureScrollbar = value;
		if (self.m_texture_scroll) {
			self.m_texture_scroll.visible(self.m_showTextureScrollbar);
			TextureBrowser_updateScroll(self);
		}
	}
};


/*
   ==============
   TextureBrowser_ShowDirectory
   relies on texture_directory global for the directory to use
   1) Load the shaders for the given directory
   2) Scan the remaining texture, load them and assign them a default shader (the "noshader" shader)
   NOTE: when writing a texture plugin, or some texture extensions, this function may need to be overriden, and made
   available through the IShaders interface
   NOTE: for texture window layout:
   all shaders are stored with alphabetical order after load
   previously loaded and displayed stuff is hidden, only in-use and newly loaded is shown
   ( the GL textures are not flushed though)
   ==============
 */

bool endswith( const char *haystack, const char *needle ){
	size_t lh = strlen( haystack );
	size_t ln = strlen( needle );
	if ( lh < ln ) {
		return false;
	}
	return !memcmp( haystack + ( lh - ln ), needle, ln );
}

bool texture_name_ignore( const char* name ){
	StringOutputStream strTemp( string_length( name ) );
	strTemp << LowerCase( name );

	return
		endswith( strTemp.c_str(), ".specular" ) ||
		endswith( strTemp.c_str(), ".glow" ) ||
		endswith( strTemp.c_str(), ".bump" ) ||
		endswith( strTemp.c_str(), ".diffuse" ) ||
		endswith( strTemp.c_str(), ".blend" ) ||
		endswith( strTemp.c_str(), ".alpha" ) ||
		endswith( strTemp.c_str(), "_alpha" ) ||
		/* Quetoo */
		endswith( strTemp.c_str(), "_h" ) ||
		endswith( strTemp.c_str(), "_local" ) ||
		endswith( strTemp.c_str(), "_nm" ) ||
		endswith( strTemp.c_str(), "_s" ) ||
		/* DarkPlaces */
		endswith( strTemp.c_str(), "_bump" ) ||
		endswith( strTemp.c_str(), "_glow" ) ||
		endswith( strTemp.c_str(), "_gloss" ) ||
		endswith( strTemp.c_str(), "_luma" ) ||
		endswith( strTemp.c_str(), "_norm" ) ||
		endswith( strTemp.c_str(), "_pants" ) ||
		endswith( strTemp.c_str(), "_shirt" ) ||
		endswith( strTemp.c_str(), "_reflect" ) ||
		/* Unvanquished */
		endswith( strTemp.c_str(), "_d" ) ||
		endswith( strTemp.c_str(), "_n" ) ||
		endswith( strTemp.c_str(), "_p" ) ||
		endswith( strTemp.c_str(), "_g" ) ||
		endswith( strTemp.c_str(), "_a" ) ||
		0;
}

class LoadShaderVisitor : public Archive::Visitor
{
public:
void visit( const char* name ){
	IShader* shader = QERApp_Shader_ForName( CopiedString( StringRange( name, path_get_filename_base_end( name ) ) ).c_str() );
	shader->DecRef();
	shader->setWadName( g_TextureBrowser_currentDirectory.c_str() );
}
};

void TextureBrowser_SetHideUnused( TextureBrowser& textureBrowser, bool hideUnused );

ui::Widget g_page_textures{ui::null};

void TextureBrowser_toggleShow(){
	GroupDialog_showPage( g_page_textures );
}


void TextureBrowser_updateTitle(){
	GroupDialog_updatePageTitle( g_page_textures );
}


class TextureCategoryLoadShader
{
const char* m_directory;
std::size_t& m_count;
public:
using func = void(const char *);

TextureCategoryLoadShader( const char* directory, std::size_t& count )
	: m_directory( directory ), m_count( count ){
	m_count = 0;
}

void operator()( const char* name ) const {
	if ( shader_equal_prefix( name, "textures/" )
		 && shader_equal_prefix( name + string_length( "textures/" ), m_directory ) ) {
		++m_count;
		// request the shader, this will load the texture if needed
		// this Shader_ForName call is a kind of hack
		IShader *pFoo = QERApp_Shader_ForName( name );
		pFoo->DecRef();
	}
}
};

void TextureDirectory_loadTexture( const char* directory, const char* texture ){
	// Doom3-like dds/ prefix (used by DarkPlaces).
	// When we list dds/textures/ folder,
	// store the texture names without dds/ prefix.
	if ( !strncmp( "dds/", directory, 4 ) )
	{
		directory = &directory[ 4 ];
	}

	StringOutputStream name( 256 );
	name << directory << StringRange( texture, path_get_filename_base_end( texture ) );

	if ( texture_name_ignore( name.c_str() ) ) {
		return;
	}

	if ( !shader_valid( name.c_str() ) ) {
		globalOutputStream() << "Skipping invalid texture name: [" << name.c_str() << "]\n";
		return;
	}

	// if a texture is already in use to represent a shader, ignore it
	IShader* shader = QERApp_Shader_ForName( name.c_str() );
	shader->DecRef();
}

typedef ConstPointerCaller<char, void(const char*), TextureDirectory_loadTexture> TextureDirectoryLoadTextureCaller;

class LoadTexturesByTypeVisitor : public ImageModules::Visitor
{
const char* m_dirstring;
public:
LoadTexturesByTypeVisitor( const char* dirstring )
	: m_dirstring( dirstring ){
}

void visit( const char* minor, const _QERPlugImageTable& table ) const {
	GlobalFileSystem().forEachFile( m_dirstring, minor, TextureDirectoryLoadTextureCaller( m_dirstring ) );
}
};

void TextureBrowser_ShowDirectory( TextureBrowser& textureBrowser, const char* directory ){
	if ( TextureBrowser_showWads() ) {
		g_TextureBrowser_currentDirectory = directory;
		TextureBrowser_heightChanged( textureBrowser );

		Archive* archive = GlobalFileSystem().getArchive( directory );
		if ( archive != nullptr )
		{
		LoadShaderVisitor visitor;
		archive->forEachFile( Archive::VisitorFunc( visitor, Archive::eFiles, 0 ), "textures/" );

			// Doom3-like dds/ prefix (used by DarkPlaces).
			archive->forEachFile( Archive::VisitorFunc( visitor, Archive::eFiles, 0 ), "dds/textures/" );
		}
		else if ( extension_equal_i( path_get_extension( directory ), "wad" ) )
		{
			globalErrorStream() << "Failed to load " << directory << "\n";
		}
	}
	else
	{
		g_TextureBrowser_currentDirectory = directory;
		TextureBrowser_heightChanged( textureBrowser );

		std::size_t shaders_count;
		GlobalShaderSystem().foreachShaderName(makeCallback( TextureCategoryLoadShader( directory, shaders_count ) ) );
		globalOutputStream() << "Showing " << Unsigned( shaders_count ) << " shaders.\n";

		if ( g_pGameDescription->mGameType != "doom3" ) {
			// load remaining texture files

			StringOutputStream dirstring( 64 );
			dirstring << "textures/" << directory;

			{
				LoadTexturesByTypeVisitor visitor( dirstring.c_str() );
				Radiant_getImageModules().foreachModule( visitor );
			}

			// Doom3-like dds/ prefix (used by DarkPlaces).
			dirstring.clear();
			dirstring << "dds/textures/" << directory;

			{
				LoadTexturesByTypeVisitor visitor( dirstring.c_str() );
				Radiant_getImageModules().foreachModule( visitor );
			}
		}
	}

	// we'll display the newly loaded textures + all the ones already in use
	TextureBrowser_SetHideUnused( textureBrowser, false );

	TextureBrowser_updateTitle();
}

void TextureBrowser_ShowTagSearchResult( TextureBrowser& textureBrowser, const char* directory ){
	g_TextureBrowser_currentDirectory = directory;
	TextureBrowser_heightChanged( textureBrowser );

	std::size_t shaders_count;
	GlobalShaderSystem().foreachShaderName(makeCallback( TextureCategoryLoadShader( directory, shaders_count ) ) );
	globalOutputStream() << "Showing " << Unsigned( shaders_count ) << " shaders.\n";

	if ( g_pGameDescription->mGameType != "doom3" ) {
		// load remaining texture files
		StringOutputStream dirstring( 64 );
		dirstring << "textures/" << directory;

		{
			LoadTexturesByTypeVisitor visitor( dirstring.c_str() );
			Radiant_getImageModules().foreachModule( visitor );
		}

		// Doom3-like dds/ prefix (used by DarkPlaces).
		dirstring.clear();
		dirstring << "dds/textures/" << directory;

		{
			LoadTexturesByTypeVisitor visitor( dirstring.c_str() );
			Radiant_getImageModules().foreachModule( visitor );
		}
	}

	// we'll display the newly loaded textures + all the ones already in use
	TextureBrowser_SetHideUnused( textureBrowser, false );
}


bool TextureBrowser_hideUnused();

void TextureBrowser_hideUnusedExport( const Callback<void(bool)> & importer ){
	importer( TextureBrowser_hideUnused() );
}

typedef FreeCaller<void(const Callback<void(bool)> &), TextureBrowser_hideUnusedExport> TextureBrowserHideUnusedExport;

void TextureBrowser_showShadersExport( const Callback<void(bool)> & importer ){
	importer( GlobalTextureBrowser().m_showShaders );
}

typedef FreeCaller<void(const Callback<void(bool)> &), TextureBrowser_showShadersExport> TextureBrowserShowShadersExport;

void TextureBrowser_showTexturesExport( const Callback<void(bool)> & importer ){
	importer( GlobalTextureBrowser().m_showTextures );
}

typedef FreeCaller<void(const Callback<void(bool)> &), TextureBrowser_showTexturesExport> TextureBrowserShowTexturesExport;

void TextureBrowser_showShaderlistOnly( const Callback<void(bool)> & importer ){
	importer( g_TextureBrowser_shaderlistOnly );
}

typedef FreeCaller<void(const Callback<void(bool)> &), TextureBrowser_showShaderlistOnly> TextureBrowserShowShaderlistOnlyExport;

void TextureBrowser_fixedSize( const Callback<void(bool)> & importer ){
	importer( g_TextureBrowser_fixedSize );
}

typedef FreeCaller<void(const Callback<void(bool)> &), TextureBrowser_fixedSize> TextureBrowser_FixedSizeExport;

void TextureBrowser_filterMissing( const Callback<void(bool)> & importer ){
	importer( g_TextureBrowser_filterMissing );
}

typedef FreeCaller<void(const Callback<void(bool)> &), TextureBrowser_filterMissing> TextureBrowser_filterMissingExport;

void TextureBrowser_filterFallback( const Callback<void(bool)> & importer ){
	importer( g_TextureBrowser_filterFallback );
}

typedef FreeCaller<void(const Callback<void(bool)> &), TextureBrowser_filterFallback> TextureBrowser_filterFallbackExport;

void TextureBrowser_enableAlpha( const Callback<void(bool)> & importer ){
	importer( g_TextureBrowser_enableAlpha );
}

typedef FreeCaller<void(const Callback<void(bool)> &), TextureBrowser_enableAlpha> TextureBrowser_enableAlphaExport;

void TextureBrowser_SetHideUnused( TextureBrowser& textureBrowser, bool hideUnused ){
	if ( hideUnused ) {
		textureBrowser.m_hideUnused = true;
	}
	else
	{
		textureBrowser.m_hideUnused = false;
	}

	textureBrowser.m_hideunused_item.update();

	TextureBrowser_heightChanged( textureBrowser );
	textureBrowser.m_originInvalid = true;
}

void TextureBrowser_ShowStartupShaders( TextureBrowser& textureBrowser ){
	if ( textureBrowser.m_startupShaders == STARTUPSHADERS_COMMON ) {
		TextureBrowser_ShowDirectory( textureBrowser, TextureBrowser_getCommonShadersDir() );
	}
}


//++timo NOTE: this is a mix of Shader module stuff and texture explorer
// it might need to be split in parts or moved out .. dunno
// scroll origin so the specified texture is completely on screen
// if current texture is not displayed, nothing is changed
void TextureBrowser_Focus( TextureBrowser& textureBrowser, const char* name ){
	TextureLayout layout;
	// scroll origin so the texture is completely on screen
	Texture_StartPos( layout );

	for ( QERApp_ActiveShaders_IteratorBegin(); !QERApp_ActiveShaders_IteratorAtEnd(); QERApp_ActiveShaders_IteratorIncrement() )
	{
		IShader* shader = QERApp_ActiveShaders_IteratorCurrent();

		if ( !Texture_IsShown( shader, textureBrowser.m_showShaders, textureBrowser.m_showTextures, textureBrowser.m_hideUnused ) ) {
			continue;
		}

		int x, y;
		Texture_NextPos( textureBrowser, layout, shader->getTexture(), &x, &y );
		qtexture_t* q = shader->getTexture();
		if ( !q ) {
			break;
		}

		// we have found when texdef->name and the shader name match
		// NOTE: as everywhere else for our comparisons, we are not case sensitive
		if ( shader_equal( name, shader->getName() ) ) {
			//int textureHeight = (int)( q->height * ( (float)textureBrowser.m_textureScale / 100 ) ) + 2 * TextureBrowser_fontHeight( textureBrowser );
			int textureWidth, textureHeight;
			textureBrowser.getTextureWH( q, textureWidth, textureHeight );
			textureHeight += 2 * TextureBrowser_fontHeight( textureBrowser );


			int originy = TextureBrowser_getOriginY( textureBrowser );
			if ( y > originy ) {
				originy = y + 4;
			}

			if ( y - textureHeight < originy - textureBrowser.height ) {
				originy = ( y - textureHeight ) + textureBrowser.height;
			}

			TextureBrowser_setOriginY( textureBrowser, originy );
			return;
		}
	}
}

IShader* Texture_At( TextureBrowser& textureBrowser, int mx, int my ){
	my += TextureBrowser_getOriginY( textureBrowser ) - textureBrowser.height;

	TextureLayout layout;
	Texture_StartPos( layout );
	for ( QERApp_ActiveShaders_IteratorBegin(); !QERApp_ActiveShaders_IteratorAtEnd(); QERApp_ActiveShaders_IteratorIncrement() )
	{
		IShader* shader = QERApp_ActiveShaders_IteratorCurrent();

		if ( !Texture_IsShown( shader, textureBrowser.m_showShaders, textureBrowser.m_showTextures, textureBrowser.m_hideUnused ) ) {
			continue;
		}

		int x, y;
		Texture_NextPos( textureBrowser, layout, shader->getTexture(), &x, &y );
		qtexture_t  *q = shader->getTexture();
		if ( !q ) {
			break;
		}

		int nWidth, nHeight;
		textureBrowser.getTextureWH( q, nWidth, nHeight );
		if ( mx > x && mx - x < nWidth
			 && my < y && y - my < nHeight + TextureBrowser_fontHeight( textureBrowser ) ) {
			return shader;
		}
	}

	return 0;
}

/*
   ==============
   SelectTexture

   By mouse click
   ==============
 */
void SelectTexture( TextureBrowser& textureBrowser, int mx, int my, bool bShift ){
	IShader* shader = Texture_At( textureBrowser, mx, my );
	if ( shader != 0 ) {
		if ( bShift ) {
			if ( shader->IsDefault() ) {
				globalOutputStream() << "ERROR: " << shader->getName() << " is not a shader, it's a texture.\n";
			}
			else{
				ViewShader( shader->getShaderFileName(), shader->getName() );
			}
		}
		else
		{
			TextureBrowser_SetSelectedShader( textureBrowser, shader->getName() );
			TextureBrowser_textureSelected( shader->getName() );

			if ( !FindTextureDialog_isOpen() && !textureBrowser.m_rmbSelected ) {
				UndoableCommand undo( "textureNameSetSelected" );
				Select_SetShader( shader->getName() );
			}
		}
	}
}

/*
   ============================================================================

   MOUSE ACTIONS

   ============================================================================
 */

void TextureBrowser_trackingDelta( int x, int y, unsigned int state, void* data ){
	TextureBrowser& textureBrowser = *reinterpret_cast<TextureBrowser*>( data );
	if ( y != 0 ) {
		int scale = 1;

		if ( state & GDK_SHIFT_MASK ) {
			scale = 4;
		}

		int originy = TextureBrowser_getOriginY( textureBrowser );
		originy += y * scale;
		TextureBrowser_setOriginY( textureBrowser, originy );
	}
}

void TextureBrowser_Tracking_MouseUp( TextureBrowser& textureBrowser ){
	textureBrowser.m_move_started = false;
	/* NetRadiantCustom did this instead:
	textureBrowser.m_freezePointer.unfreeze_pointer( textureBrowser.m_gl_widget ); */

	textureBrowser.m_freezePointer.unfreeze_pointer( textureBrowser.m_gl_widget );
}

void TextureBrowser_Tracking_MouseDown( TextureBrowser& textureBrowser ){
	if( textureBrowser.m_move_started ){
		TextureBrowser_Tracking_MouseUp( textureBrowser );
	}
	textureBrowser.m_move_started = true;
	/* NetRadiantCustom did this instead:
	textureBrowser.m_freezePointer.freeze_pointer( textureBrowser.m_parent, textureBrowser.m_gl_widget, TextureBrowser_trackingDelta, &textureBrowser ); */
	textureBrowser.m_freezePointer.freeze_pointer( textureBrowser.m_gl_widget, TextureBrowser_trackingDelta, &textureBrowser );
}

void TextureBrowser_Selection_MouseDown( TextureBrowser& textureBrowser, guint32 flags, int pointx, int pointy ){
	SelectTexture( textureBrowser, pointx, textureBrowser.height - 1 - pointy, ( flags & GDK_SHIFT_MASK ) != 0 );
}

/*
   ============================================================================

   DRAWING

   ============================================================================
 */

/*
   ============
   Texture_Draw
   TTimo: relying on the shaders list to display the textures
   we must query all qtexture_t* to manage and display through the IShaders interface
   this allows a plugin to completely override the texture system
   ============
 */
void Texture_Draw( TextureBrowser& textureBrowser ){
	int originy = TextureBrowser_getOriginY( textureBrowser );

	glClearColor( textureBrowser.color_textureback[0],
				  textureBrowser.color_textureback[1],
				  textureBrowser.color_textureback[2],
				  0 );

	glViewport( 0, 0, textureBrowser.width, textureBrowser.height );
	glMatrixMode( GL_PROJECTION );
	glLoadIdentity();

	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	glDisable( GL_DEPTH_TEST );

	//glDisable( GL_BLEND );
	if ( g_TextureBrowser_enableAlpha ) {
		glEnable( GL_BLEND );
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else {
		glDisable( GL_BLEND );
	}

	glOrtho( 0, textureBrowser.width, originy - textureBrowser.height, originy, -100, 100 );
	glEnable( GL_TEXTURE_2D );

	glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );

	int last_y = 0, last_height = 0;

	TextureLayout layout;
	Texture_StartPos( layout );
	for ( QERApp_ActiveShaders_IteratorBegin(); !QERApp_ActiveShaders_IteratorAtEnd(); QERApp_ActiveShaders_IteratorIncrement() )
	{
		IShader* shader = QERApp_ActiveShaders_IteratorCurrent();

		if ( !Texture_IsShown( shader, textureBrowser.m_showShaders, textureBrowser.m_showTextures, textureBrowser.m_hideUnused ) ) {
			continue;
		}

		int x, y;
		Texture_NextPos( textureBrowser, layout, shader->getTexture(), &x, &y );
		qtexture_t *q = shader->getTexture();
		if ( !q ) {
			break;
		}

		int nWidth, nHeight;
		textureBrowser.getTextureWH( q, nWidth, nHeight );

		if ( y != last_y ) {
			last_y = y;
			last_height = 0;
		}
		last_height = std::max( nHeight, last_height );

		// Is this texture visible?
		if ( ( y - nHeight - TextureBrowser_fontHeight( textureBrowser ) < originy )
			 && ( y > originy - textureBrowser.height ) ) {
			// borders rules:
			// if it's the current texture, draw a thick red line, else:
			// shaders have a white border, simple textures don't
			// if !texture_showinuse: (some textures displayed may not be in use)
			// draw an additional square around with 0.5 1 0.5 color
			glLineWidth( 1 );
			const float xf = (float)x;
			const float yf = (float)( y - TextureBrowser_fontHeight( textureBrowser ) );
			float xfMax = xf + 1.5 + nWidth;
			float xfMin = xf - 1.5;
			float yfMax = yf + 1.5;
			float yfMin = yf - nHeight - 1.5;

			//selected texture
			if ( shader_equal( TextureBrowser_GetSelectedShader( textureBrowser ), shader->getName() ) ) {
				glLineWidth( 2 );
				if ( textureBrowser.m_rmbSelected ) {
					glColor3f( 0,0,1 );
				}
				else {
					glColor3f( 1,0,0 );
				}
				xfMax += .5;
				xfMin -= .5;
				yfMax += .5;
				yfMin -= .5;
				glDisable( GL_TEXTURE_2D );
				glBegin( GL_LINE_LOOP );
				glVertex2f( xfMin ,yfMax );
				glVertex2f( xfMin ,yfMin );
				glVertex2f( xfMax ,yfMin );
				glVertex2f( xfMax ,yfMax );
				glEnd();
				glEnable( GL_TEXTURE_2D );
			}
			// highlight in-use textures
			else if ( !textureBrowser.m_hideUnused && shader->IsInUse() ) {
				glColor3f( 0.5,1,0.5 );
				glDisable( GL_TEXTURE_2D );
				glBegin( GL_LINE_LOOP );
				glVertex2f( xfMin ,yfMax );
				glVertex2f( xfMin ,yfMin );
				glVertex2f( xfMax ,yfMin );
				glVertex2f( xfMax ,yfMax );
				glEnd();
				glEnable( GL_TEXTURE_2D );
			}
			// shader white border:
			else if ( !shader->IsDefault() ) {
				glColor3f( 1, 1, 1 );
				glDisable( GL_TEXTURE_2D );
				glBegin( GL_LINE_LOOP );
				glVertex2f( xfMin ,yfMax );
				glVertex2f( xfMin ,yfMin );
				glVertex2f( xfMax ,yfMin );
				glVertex2f( xfMax ,yfMax );
				glEnd();
				glEnable( GL_TEXTURE_2D );
			}

			// shader stipple:
			if ( !shader->IsDefault() ) {
				glEnable( GL_LINE_STIPPLE );
				glLineStipple( 1, 0xF000 );
				glDisable( GL_TEXTURE_2D );
				glBegin( GL_LINE_LOOP );
				glColor3f( 0, 0, 0 );
				glVertex2f( xfMin ,yfMax );
				glVertex2f( xfMin ,yfMin );
				glVertex2f( xfMax ,yfMin );
				glVertex2f( xfMax ,yfMax );
				glEnd();
				glDisable( GL_LINE_STIPPLE );
				glEnable( GL_TEXTURE_2D );
			}

			// draw checkerboard for transparent textures
 			if ( g_TextureBrowser_enableAlpha )
			{
				glDisable( GL_TEXTURE_2D );
				glBegin( GL_QUADS );
				int font_height = TextureBrowser_fontHeight( textureBrowser );
				for ( int i = 0; i < nHeight; i += 8 )
					for ( int j = 0; j < nWidth; j += 8 )
					{
						unsigned char color = (i + j) / 8 % 2 ? 0x66 : 0x99;
						glColor3ub( color, color, color );
						int left = j;
						int right = std::min(j+8, nWidth);
						int top = i;
						int bottom = std::min(i+8, nHeight);
						glVertex2i(x + right, y - nHeight - font_height + top);
						glVertex2i(x + left,  y - nHeight - font_height + top);
						glVertex2i(x + left,  y - nHeight - font_height + bottom);
						glVertex2i(x + right, y - nHeight - font_height + bottom);
					}
				glEnd();
				glEnable( GL_TEXTURE_2D );
			}

			// Draw the texture
			glBindTexture( GL_TEXTURE_2D, q->texture_number );
			GlobalOpenGL_debugAssertNoErrors();
			glColor3f( 1,1,1 );
			glBegin( GL_QUADS );
			glTexCoord2i( 0,0 );
			glVertex2i( x,y - TextureBrowser_fontHeight( textureBrowser ) );
			glTexCoord2i( 1,0 );
			glVertex2i( x + nWidth,y - TextureBrowser_fontHeight( textureBrowser ) );
			glTexCoord2i( 1,1 );
			glVertex2i( x + nWidth,y - TextureBrowser_fontHeight( textureBrowser ) - nHeight );
			glTexCoord2i( 0,1 );
			glVertex2i( x,y - TextureBrowser_fontHeight( textureBrowser ) - nHeight );
			glEnd();

			// draw the texture name
			glDisable( GL_TEXTURE_2D );
			glColor3f( 1,1,1 );

			glRasterPos2i( x, y - TextureBrowser_fontHeight( textureBrowser ) + 2 );//+5

			// don't draw the directory name
			const char* name = shader->getName();
			name += strlen( name );
			while ( name != shader->getName() && *( name - 1 ) != '/' && *( name - 1 ) != '\\' )
				name--;

			GlobalOpenGL().drawString( name );
			glEnable( GL_TEXTURE_2D );
		}

		//int totalHeight = abs(y) + last_height + TextureBrowser_fontHeight(textureBrowser) + 4;
	}


	// reset the current texture
	glBindTexture( GL_TEXTURE_2D, 0 );
	//qglFinish();
}

void TextureBrowser_queueDraw( TextureBrowser& textureBrowser ){
	if ( textureBrowser.m_gl_widget ) {
		gtk_widget_queue_draw( textureBrowser.m_gl_widget );
	}
}


void TextureBrowser_setScale( TextureBrowser& textureBrowser, std::size_t scale ){
	textureBrowser.m_textureScale = scale;

	textureBrowser.m_heightChanged = true;
	textureBrowser.m_originInvalid = true;
	g_activeShadersChangedCallbacks();

	TextureBrowser_queueDraw( textureBrowser );
}

void TextureBrowser_setUniformSize( TextureBrowser& textureBrowser, std::size_t scale ){
	textureBrowser.m_uniformTextureSize = scale;

	textureBrowser.m_heightChanged = true;
	textureBrowser.m_originInvalid = true;
	g_activeShadersChangedCallbacks();

	TextureBrowser_queueDraw( textureBrowser );
}

void TextureBrowser_setUniformMinSize( TextureBrowser& textureBrowser, std::size_t scale ){
	textureBrowser.m_uniformTextureMinSize = scale;

	textureBrowser.m_heightChanged = true;
	textureBrowser.m_originInvalid = true;
	g_activeShadersChangedCallbacks();

	TextureBrowser_queueDraw( textureBrowser );
}

void TextureBrowser_MouseWheel( TextureBrowser& textureBrowser, bool bUp ){
	int originy = TextureBrowser_getOriginY( textureBrowser );

	if ( bUp ) {
		originy += int(textureBrowser.m_mouseWheelScrollIncrement);
	}
	else
	{
		originy -= int(textureBrowser.m_mouseWheelScrollIncrement);
	}

	TextureBrowser_setOriginY( textureBrowser, originy );
}

XmlTagBuilder TagBuilder;

enum
{
	TAG_COLUMN,
	N_COLUMNS
};

void BuildStoreAssignedTags( ui::ListStore store, const char* shader, TextureBrowser* textureBrowser ){
	GtkTreeIter iter;

	store.clear();

	std::vector<CopiedString> assigned_tags;
	TagBuilder.GetShaderTags( shader, assigned_tags );

	for ( size_t i = 0; i < assigned_tags.size(); i++ )
	{
		store.append(TAG_COLUMN, assigned_tags[i].c_str());
	}
}

void BuildStoreAvailableTags(   ui::ListStore storeAvailable,
								ui::ListStore storeAssigned,
								const std::set<CopiedString>& allTags,
								TextureBrowser* textureBrowser ){
	GtkTreeIter iterAssigned;
	GtkTreeIter iterAvailable;
	std::set<CopiedString>::const_iterator iterAll;
	gchar* tag_assigned;

	storeAvailable.clear();

	bool row = gtk_tree_model_get_iter_first(storeAssigned, &iterAssigned ) != 0;

	if ( !row ) { // does the shader have tags assigned?
		for ( iterAll = allTags.begin(); iterAll != allTags.end(); ++iterAll )
		{
			storeAvailable.append(TAG_COLUMN, (*iterAll).c_str());
		}
	}
	else
	{
		while ( row ) // available tags = all tags - assigned tags
		{
			gtk_tree_model_get(storeAssigned, &iterAssigned, TAG_COLUMN, &tag_assigned, -1 );

			for ( iterAll = allTags.begin(); iterAll != allTags.end(); ++iterAll )
			{
				if ( strcmp( (char*)tag_assigned, ( *iterAll ).c_str() ) != 0 ) {
					storeAvailable.append(TAG_COLUMN, (*iterAll).c_str());
				}
				else
				{
					row = gtk_tree_model_iter_next(storeAssigned, &iterAssigned ) != 0;

					if ( row ) {
						gtk_tree_model_get(storeAssigned, &iterAssigned, TAG_COLUMN, &tag_assigned, -1 );
					}
				}
			}
		}
	}
}

gboolean TextureBrowser_button_press( ui::Widget widget, GdkEventButton* event, TextureBrowser* textureBrowser ){
	if ( event->type == GDK_BUTTON_PRESS ) {
		if ( event->button == 3 ) {
			if ( textureBrowser->m_tags ) {
				textureBrowser->m_rmbSelected = true;
				TextureBrowser_Selection_MouseDown( *textureBrowser, event->state, static_cast<int>( event->x ), static_cast<int>( event->y ) );

				BuildStoreAssignedTags( textureBrowser->m_assigned_store, textureBrowser->shader.c_str(), textureBrowser );
				BuildStoreAvailableTags( textureBrowser->m_available_store, textureBrowser->m_assigned_store, textureBrowser->m_all_tags, textureBrowser );
				textureBrowser->m_heightChanged = true;
				textureBrowser->m_tag_frame.show();

                ui::process();

				TextureBrowser_Focus( *textureBrowser, textureBrowser->shader.c_str() );
			}
			else
			{
				TextureBrowser_Tracking_MouseDown( *textureBrowser );
			}
		}
		else if ( event->button == 1 ) {
			TextureBrowser_Selection_MouseDown( *textureBrowser, event->state, static_cast<int>( event->x ), static_cast<int>( event->y ) );

			if ( textureBrowser->m_tags ) {
				textureBrowser->m_rmbSelected = false;
				textureBrowser->m_tag_frame.hide();
			}
		}
	}
	else if ( event->type == GDK_2BUTTON_PRESS && event->button == 1 ) {
		#define GARUX_DISABLE_2BUTTON
		#ifndef GARUX_DISABLE_2BUTTON
		CopiedString texName = textureBrowser->shader;
		const char* sh = textureBrowser->shader.c_str();
		char* dir = strrchr( sh, '/' );
		if( dir != NULL ){
			*(dir + 1) = '\0';
			dir = strchr( sh, '/' );
			if( dir != NULL ){
				dir++;
				if( *dir != '\0'){
					ScopeDisableScreenUpdates disableScreenUpdates( dir, "Loading Textures" );
					TextureBrowser_ShowDirectory( *textureBrowser, dir );
					TextureBrowser_Focus( *textureBrowser, textureBrowser->shader.c_str() );
					TextureBrowser_queueDraw( *textureBrowser );
				}
			}
		}
		#endif
	}
	else if ( event->type == GDK_2BUTTON_PRESS && event->button == 3 ) {
		ScopeDisableScreenUpdates disableScreenUpdates( TextureBrowser_getCommonShadersDir(), "Loading Textures" );
		TextureBrowser_ShowDirectory( *textureBrowser, TextureBrowser_getCommonShadersDir() );
		TextureBrowser_queueDraw( *textureBrowser );
	}
	return FALSE;
}

gboolean TextureBrowser_button_release( ui::Widget widget, GdkEventButton* event, TextureBrowser* textureBrowser ){
	if ( event->type == GDK_BUTTON_RELEASE ) {
		if ( event->button == 3 ) {
			if ( !textureBrowser->m_tags ) {
				TextureBrowser_Tracking_MouseUp( *textureBrowser );
			}
		}
	}
	return FALSE;
}

gboolean TextureBrowser_motion( ui::Widget widget, GdkEventMotion *event, TextureBrowser* textureBrowser ){
	return FALSE;
}

gboolean TextureBrowser_scroll( ui::Widget widget, GdkEventScroll* event, TextureBrowser* textureBrowser ){
	if ( event->direction == GDK_SCROLL_UP ) {
		TextureBrowser_MouseWheel( *textureBrowser, true );
	}
	else if ( event->direction == GDK_SCROLL_DOWN ) {
		TextureBrowser_MouseWheel( *textureBrowser, false );
	}
	return FALSE;
}

void TextureBrowser_scrollChanged( void* data, gdouble value ){
	//globalOutputStream() << "vertical scroll\n";
	TextureBrowser_setOriginY( *reinterpret_cast<TextureBrowser*>( data ), -(int)value );
}

static void TextureBrowser_verticalScroll(ui::Adjustment adjustment, TextureBrowser* textureBrowser ){
	textureBrowser->m_scrollAdjustment.value_changed( gtk_adjustment_get_value(adjustment) );
}

void TextureBrowser_updateScroll( TextureBrowser& textureBrowser ){
	if ( textureBrowser.m_showTextureScrollbar ) {
		int totalHeight = TextureBrowser_TotalHeight( textureBrowser );

		totalHeight = std::max( totalHeight, textureBrowser.height );

        auto vadjustment = gtk_range_get_adjustment( GTK_RANGE( textureBrowser.m_texture_scroll ) );

		gtk_adjustment_set_value(vadjustment, -TextureBrowser_getOriginY( textureBrowser ));
		gtk_adjustment_set_page_size(vadjustment, textureBrowser.height);
		gtk_adjustment_set_page_increment(vadjustment, textureBrowser.height / 2);
		gtk_adjustment_set_step_increment(vadjustment, 20);
		gtk_adjustment_set_lower(vadjustment, 0);
		gtk_adjustment_set_upper(vadjustment, totalHeight);

		g_signal_emit_by_name( G_OBJECT( vadjustment ), "changed" );
	}
}

gboolean TextureBrowser_size_allocate( ui::Widget widget, GtkAllocation* allocation, TextureBrowser* textureBrowser ){
	textureBrowser->width = allocation->width;
	textureBrowser->height = allocation->height;
	TextureBrowser_heightChanged( *textureBrowser );
	textureBrowser->m_originInvalid = true;
	TextureBrowser_queueDraw( *textureBrowser );
	return FALSE;
}

void TextureBrowser_redraw( TextureBrowser* textureBrowser ){
	if ( glwidget_make_current( textureBrowser->m_gl_widget ) != FALSE ) {
		GlobalOpenGL_debugAssertNoErrors();
		TextureBrowser_evaluateHeight( *textureBrowser );
		Texture_Draw( *textureBrowser );
		GlobalOpenGL_debugAssertNoErrors();
		glwidget_swap_buffers( textureBrowser->m_gl_widget );
	}
}

gboolean TextureBrowser_expose( ui::Widget widget, GdkEventExpose* event, TextureBrowser* textureBrowser ){
	TextureBrowser_redraw( textureBrowser );
	return FALSE;
}

TextureBrowser& GlobalTextureBrowser(){
	static TextureBrowser textureBrowser;
	return textureBrowser;
}

bool TextureBrowser_hideUnused(){
	return GlobalTextureBrowser().m_hideUnused;
}

void TextureBrowser_ToggleHideUnused(){
	TextureBrowser &textureBrowser = GlobalTextureBrowser();
	if ( textureBrowser.m_hideUnused ) {
		TextureBrowser_SetHideUnused( textureBrowser, false );
	}
	else
	{
		TextureBrowser_SetHideUnused( textureBrowser, true );
	}
}

const char* TextureGroups_transformDirName( const char* dirName, StringOutputStream *archiveName )
{
	if ( TextureBrowser_showWads() ) {
		archiveName->clear();
		*archiveName << StringRange( path_get_filename_start( dirName ), path_get_filename_base_end( dirName ) ) \
			<< "." << path_get_extension( dirName );
		return archiveName->c_str();
	}
	return dirName;
}

void TextureGroups_constructTreeModel( TextureGroups groups, ui::TreeStore store ){
	// put the information from the old textures menu into a treeview
	GtkTreeIter iter, child;

	TextureGroups::const_iterator i = groups.begin();
	while ( i != groups.end() )
	{
		StringOutputStream archiveName;
		StringOutputStream nextArchiveName;
		const char* dirName = TextureGroups_transformDirName( ( *i ).c_str(), &archiveName );

		const char* firstUnderscore = strchr( dirName, '_' );
		StringRange dirRoot( dirName, ( firstUnderscore == 0 ) ? dirName : firstUnderscore + 1 );

		TextureGroups::const_iterator next = i;
		++next;

		if ( firstUnderscore != 0
			 && next != groups.end()
			 && string_equal_start( TextureGroups_transformDirName( ( *next ).c_str(), &nextArchiveName ), dirRoot ) ) {
			gtk_tree_store_append( store, &iter, NULL );
			gtk_tree_store_set( store, &iter, 0, CopiedString( StringRange( dirName, firstUnderscore ) ).c_str(), -1 );

			// keep going...
			while ( i != groups.end() && string_equal_start( TextureGroups_transformDirName( ( *i ).c_str(), &nextArchiveName ), dirRoot ) )
			{
				gtk_tree_store_append( store, &child, &iter );
				gtk_tree_store_set( store, &child, 0, TextureGroups_transformDirName( ( *i ).c_str(), &nextArchiveName ), -1 );
				++i;
			}
		}
		else
		{
			gtk_tree_store_append( store, &iter, NULL );
			gtk_tree_store_set( store, &iter, 0, dirName, -1 );
			++i;
		}
	}
}

TextureGroups TextureGroups_constructTreeView(){
	TextureGroups groups;

	if ( TextureBrowser_showWads() ) {
		GlobalFileSystem().forEachArchive( TextureGroupsAddWadCaller( groups ) );
	}
	else
	{
		// scan texture dirs and pak files only if not restricting to shaderlist
		if ( g_pGameDescription->mGameType != "doom3" && !g_TextureBrowser_shaderlistOnly ) {
			GlobalFileSystem().forEachDirectory( "textures/", TextureGroupsAddDirectoryCaller( groups ) );
		}

		GlobalShaderSystem().foreachShaderName( TextureGroupsAddShaderCaller( groups ) );
	}

	return groups;
}

void TextureBrowser_constructTreeStore(){
	TextureGroups groups = TextureGroups_constructTreeView();
	auto store = ui::TreeStore::from(gtk_tree_store_new( 1, G_TYPE_STRING ));
	TextureGroups_constructTreeModel( groups, store );

	gtk_tree_view_set_model(GlobalTextureBrowser().m_treeViewTree, store);

	g_object_unref( G_OBJECT( store ) );
}

void TextureBrowser_constructTreeStoreTags(){
	TextureGroups groups;
	TextureBrowser &textureBrowser = GlobalTextureBrowser();
	auto store = ui::TreeStore::from(gtk_tree_store_new( 1, G_TYPE_STRING ));
	auto model = GlobalTextureBrowser().m_all_tags_list;

	gtk_tree_view_set_model(GlobalTextureBrowser().m_treeViewTags, model );

	g_object_unref( G_OBJECT( store ) );
}

void TreeView_onRowActivated( ui::TreeView treeview, ui::TreePath path, ui::TreeViewColumn col, gpointer userdata ){
	GtkTreeIter iter;

    auto model = gtk_tree_view_get_model(treeview );

	if ( gtk_tree_model_get_iter( model, &iter, path ) ) {
		gchar dirName[1024];

		gchar* buffer;
		gtk_tree_model_get( model, &iter, 0, &buffer, -1 );
		strcpy( dirName, buffer );
		g_free( buffer );

		GlobalTextureBrowser().m_searchedTags = false;

		if ( !TextureBrowser_showWads() ) {
			strcat( dirName, "/" );
		}

		ScopeDisableScreenUpdates disableScreenUpdates( dirName, "Loading Textures" );
		TextureBrowser_ShowDirectory( GlobalTextureBrowser(), dirName );
		TextureBrowser_queueDraw( GlobalTextureBrowser() );
		//deactivate, so SPACE and RETURN wont be broken for 2d
		gtk_window_set_focus( GTK_WINDOW( gtk_widget_get_toplevel( GTK_WIDGET( treeview ) ) ), NULL );
	}
}

void TextureBrowser_createTreeViewTree(){
	TextureBrowser &textureBrowser = GlobalTextureBrowser();
	gtk_tree_view_set_enable_search(textureBrowser.m_treeViewTree, FALSE );

	gtk_tree_view_set_headers_visible(textureBrowser.m_treeViewTree, FALSE );
	textureBrowser.m_treeViewTree.connect( "row-activated", (GCallback) TreeView_onRowActivated, NULL );

	auto renderer = ui::CellRendererText(ui::New);
	gtk_tree_view_insert_column_with_attributes(textureBrowser.m_treeViewTree, -1, "", renderer, "text", 0, NULL );

	TextureBrowser_constructTreeStore();
}

void TextureBrowser_addTag();

void TextureBrowser_renameTag();

void TextureBrowser_deleteTag();

void TextureBrowser_createContextMenu( ui::Widget treeview, GdkEventButton *event ){
	ui::Widget menu = ui::Menu(ui::New);

	ui::Widget menuitem = ui::MenuItem( "Add tag" );
	menuitem.connect( "activate", (GCallback)TextureBrowser_addTag, treeview );
	gtk_menu_shell_append( GTK_MENU_SHELL( menu ), menuitem );

	menuitem = ui::MenuItem( "Rename tag" );
	menuitem.connect( "activate", (GCallback)TextureBrowser_renameTag, treeview );
	gtk_menu_shell_append( GTK_MENU_SHELL( menu ), menuitem );

	menuitem = ui::MenuItem( "Delete tag" );
	menuitem.connect( "activate", (GCallback)TextureBrowser_deleteTag, treeview );
	gtk_menu_shell_append( GTK_MENU_SHELL( menu ), menuitem );

	gtk_widget_show_all( menu );

	gtk_menu_popup( GTK_MENU( menu ), NULL, NULL, NULL, NULL,
					( event != NULL ) ? event->button : 0,
					gdk_event_get_time( (GdkEvent*)event ) );
}

gboolean TreeViewTags_onButtonPressed( ui::TreeView treeview, GdkEventButton *event ){
	if ( event->type == GDK_BUTTON_PRESS && event->button == 3 ) {
		GtkTreePath *path;
        auto selection = gtk_tree_view_get_selection(treeview );

		if ( gtk_tree_view_get_path_at_pos(treeview, event->x, event->y, &path, NULL, NULL, NULL ) ) {
			gtk_tree_selection_unselect_all( selection );
			gtk_tree_selection_select_path( selection, path );
			gtk_tree_path_free( path );
		}

		TextureBrowser_createContextMenu( treeview, event );
		return TRUE;
	}
	return FALSE;
}

void TextureBrowser_createTreeViewTags(){
	TextureBrowser &textureBrowser = GlobalTextureBrowser();
	textureBrowser.m_treeViewTags = ui::TreeView(ui::New);
	gtk_tree_view_set_enable_search(textureBrowser.m_treeViewTags, FALSE );

	textureBrowser.m_treeViewTags.connect( "button-press-event", (GCallback)TreeViewTags_onButtonPressed, NULL );

	gtk_tree_view_set_headers_visible(textureBrowser.m_treeViewTags, FALSE );

	auto renderer = ui::CellRendererText(ui::New);
	gtk_tree_view_insert_column_with_attributes(textureBrowser.m_treeViewTags, -1, "", renderer, "text", 0, NULL );

	TextureBrowser_constructTreeStoreTags();
}

ui::MenuItem TextureBrowser_constructViewMenu( ui::Menu menu ){
	TextureBrowser &textureBrowser = GlobalTextureBrowser();
	ui::MenuItem textures_menu_item = ui::MenuItem(new_sub_menu_item_with_mnemonic( "_View" ));

	if ( g_Layout_enableDetachableMenus.m_value ) {
		menu_tearoff( menu );
	}

	create_check_menu_item_with_mnemonic( menu, "Hide _Unused", "ShowInUse" );
	if ( string_empty( g_pGameDescription->getKeyValue( "show_wads" ) ) ) {
		create_check_menu_item_with_mnemonic( menu, "Hide Image Missing", "FilterMissing" );
	}

	// hide notex and shadernotex on texture browser: no one wants to apply them
	create_check_menu_item_with_mnemonic( menu, "Hide Fallback", "FilterFallback" );

	menu_separator( menu );


	// we always want to show shaders but don't want a "Show Shaders" menu for doom3 and .wad file games
	if ( g_pGameDescription->mGameType == "doom3" || TextureBrowser_showWads() ) {
		textureBrowser.m_showShaders = true;
	}
	else
	{
		create_check_menu_item_with_mnemonic( menu, "Show shaders", "ToggleShowShaders" );
		create_check_menu_item_with_mnemonic( menu, "Show textures", "ToggleShowTextures" );
		menu_separator( menu );
	}

	if ( textureBrowser.m_tags ) {
		create_menu_item_with_mnemonic( menu, "Show Untagged", "ShowUntagged" );
	}
	if ( g_pGameDescription->mGameType != "doom3" && string_empty( g_pGameDescription->getKeyValue( "show_wads" ) ) ) {
		create_check_menu_item_with_mnemonic( menu, "ShaderList Only", "ToggleShowShaderlistOnly" );
	}

	menu_separator( menu );
	create_check_menu_item_with_mnemonic( menu, "Fixed Size", "FixedSize" );
	create_check_menu_item_with_mnemonic( menu, "Transparency", "EnableAlpha" );

	if ( string_empty( g_pGameDescription->getKeyValue( "show_wads" ) ) ) {
		menu_separator( menu );
		textureBrowser.m_shader_info_item = ui::Widget(create_menu_item_with_mnemonic( menu, "Shader Info", "ShaderInfo"  ));
		gtk_widget_set_sensitive( textureBrowser.m_shader_info_item, FALSE );
	}


	return textures_menu_item;
}

void Popup_View_Menu( GtkWidget *widget, GtkMenu *menu ){
	gtk_menu_popup( menu, NULL, NULL, NULL, NULL, 1, gtk_get_current_event_time() );
}

ui::MenuItem TextureBrowser_constructToolsMenu( ui::Menu menu ){
	ui::MenuItem textures_menu_item = ui::MenuItem(new_sub_menu_item_with_mnemonic( "_Tools" ));

	if ( g_Layout_enableDetachableMenus.m_value ) {
		menu_tearoff( menu );
	}

	create_menu_item_with_mnemonic( menu, "Flush & Reload Shaders", "RefreshShaders" );
	create_menu_item_with_mnemonic( menu, "Find / Replace...", "FindReplaceTextures" );

	return textures_menu_item;
}

ui::MenuItem TextureBrowser_constructTagsMenu( ui::Menu menu ){
	ui::MenuItem textures_menu_item = ui::MenuItem(new_sub_menu_item_with_mnemonic( "T_ags" ));

	if ( g_Layout_enableDetachableMenus.m_value ) {
		menu_tearoff( menu );
	}

	create_menu_item_with_mnemonic( menu, "Add tag", "AddTag" );
	create_menu_item_with_mnemonic( menu, "Rename tag", "RenameTag" );
	create_menu_item_with_mnemonic( menu, "Delete tag", "DeleteTag" );
	menu_separator( menu );
	create_menu_item_with_mnemonic( menu, "Copy tags from selected", "CopyTag" );
	create_menu_item_with_mnemonic( menu, "Paste tags to selected", "PasteTag" );

	return textures_menu_item;
}

gboolean TextureBrowser_tagMoveHelper( ui::TreeModel model, ui::TreePath path, GtkTreeIter* iter, GSList** selected ){
	g_assert( selected != NULL );

    auto rowref = gtk_tree_row_reference_new( model, path );
	*selected = g_slist_append( *selected, rowref );

	return FALSE;
}

void TextureBrowser_assignTags(){
	GSList* selected = NULL;
	GSList* node;
	gchar* tag_assigned;
	TextureBrowser &textureBrowser = GlobalTextureBrowser();

	auto selection = gtk_tree_view_get_selection(textureBrowser.m_available_tree );

	gtk_tree_selection_selected_foreach( selection, (GtkTreeSelectionForeachFunc)TextureBrowser_tagMoveHelper, &selected );

	if ( selected != NULL ) {
		for ( node = selected; node != NULL; node = node->next )
		{
            auto path = gtk_tree_row_reference_get_path( (GtkTreeRowReference*)node->data );

			if ( path ) {
				GtkTreeIter iter;

				if ( gtk_tree_model_get_iter(textureBrowser.m_available_store, &iter, path ) ) {
					gtk_tree_model_get(textureBrowser.m_available_store, &iter, TAG_COLUMN, &tag_assigned, -1 );
					if ( !TagBuilder.CheckShaderTag( textureBrowser.shader.c_str() ) ) {
						// create a custom shader/texture entry
						IShader* ishader = QERApp_Shader_ForName( textureBrowser.shader.c_str() );
						CopiedString filename = ishader->getShaderFileName();

						if ( filename.empty() ) {
							// it's a texture
							TagBuilder.AddShaderNode( textureBrowser.shader.c_str(), CUSTOM, TEXTURE );
						}
						else {
							// it's a shader
							TagBuilder.AddShaderNode( textureBrowser.shader.c_str(), CUSTOM, SHADER );
						}
						ishader->DecRef();
					}
					TagBuilder.AddShaderTag( textureBrowser.shader.c_str(), (char*)tag_assigned, TAG );

					gtk_list_store_remove( textureBrowser.m_available_store, &iter );
					textureBrowser.m_assigned_store.append(TAG_COLUMN, tag_assigned);
				}
			}
		}

		g_slist_foreach( selected, (GFunc)gtk_tree_row_reference_free, NULL );

		// Save changes
		TagBuilder.SaveXmlDoc();
	}
	g_slist_free( selected );
}

void TextureBrowser_removeTags(){
	GSList* selected = NULL;
	GSList* node;
	gchar* tag;
	TextureBrowser &textureBrowser = GlobalTextureBrowser();

	auto selection = gtk_tree_view_get_selection(textureBrowser.m_assigned_tree );

	gtk_tree_selection_selected_foreach( selection, (GtkTreeSelectionForeachFunc)TextureBrowser_tagMoveHelper, &selected );

	if ( selected != NULL ) {
		for ( node = selected; node != NULL; node = node->next )
		{
            auto path = gtk_tree_row_reference_get_path( (GtkTreeRowReference*)node->data );

			if ( path ) {
				GtkTreeIter iter;

				if ( gtk_tree_model_get_iter(textureBrowser.m_assigned_store, &iter, path ) ) {
					gtk_tree_model_get(textureBrowser.m_assigned_store, &iter, TAG_COLUMN, &tag, -1 );
					TagBuilder.DeleteShaderTag( textureBrowser.shader.c_str(), tag );
					gtk_list_store_remove( textureBrowser.m_assigned_store, &iter );
				}
			}
		}

		g_slist_foreach( selected, (GFunc)gtk_tree_row_reference_free, NULL );

		// Update the "available tags list"
		BuildStoreAvailableTags( textureBrowser.m_available_store, textureBrowser.m_assigned_store, textureBrowser.m_all_tags, &textureBrowser );

		// Save changes
		TagBuilder.SaveXmlDoc();
	}
	g_slist_free( selected );
}

void TextureBrowser_buildTagList(){
	TextureBrowser &textureBrowser = GlobalTextureBrowser();
	textureBrowser.m_all_tags_list.clear();

	std::set<CopiedString>::iterator iter;

	for ( iter = textureBrowser.m_all_tags.begin(); iter != textureBrowser.m_all_tags.end(); ++iter )
	{
		textureBrowser.m_all_tags_list.append(TAG_COLUMN, (*iter).c_str());
	}
}

void TextureBrowser_searchTags(){
	GSList* selected = NULL;
	GSList* node;
	gchar* tag;
	char buffer[256];
	char tags_searched[256];
	TextureBrowser &textureBrowser = GlobalTextureBrowser();

	auto selection = gtk_tree_view_get_selection(textureBrowser.m_treeViewTags );

	gtk_tree_selection_selected_foreach( selection, (GtkTreeSelectionForeachFunc)TextureBrowser_tagMoveHelper, &selected );

	if ( selected != NULL ) {
		strcpy( buffer, "/root/*/*[tag='" );
		strcpy( tags_searched, "[TAGS] " );

		for ( node = selected; node != NULL; node = node->next )
		{
            auto path = gtk_tree_row_reference_get_path( (GtkTreeRowReference*)node->data );

			if ( path ) {
				GtkTreeIter iter;

				if ( gtk_tree_model_get_iter(textureBrowser.m_all_tags_list, &iter, path ) ) {
					gtk_tree_model_get(textureBrowser.m_all_tags_list, &iter, TAG_COLUMN, &tag, -1 );

					strcat( buffer, tag );
					strcat( tags_searched, tag );
					if ( node != g_slist_last( node ) ) {
						strcat( buffer, "' and tag='" );
						strcat( tags_searched, ", " );
					}
				}
			}
		}

		strcat( buffer, "']" );

		g_slist_foreach( selected, (GFunc)gtk_tree_row_reference_free, NULL );

		textureBrowser.m_found_shaders.clear(); // delete old list
		TagBuilder.TagSearch( buffer, textureBrowser.m_found_shaders );

		if ( !textureBrowser.m_found_shaders.empty() ) { // found something
			size_t shaders_found = textureBrowser.m_found_shaders.size();

			globalOutputStream() << "Found " << (unsigned int)shaders_found << " textures and shaders with " << tags_searched << "\n";
			ScopeDisableScreenUpdates disableScreenUpdates( "Searching...", "Loading Textures" );

			std::set<CopiedString>::iterator iter;

			for ( iter = textureBrowser.m_found_shaders.begin(); iter != textureBrowser.m_found_shaders.end(); iter++ )
			{
				std::string path = ( *iter ).c_str();
				size_t pos = path.find_last_of( "/", path.size() );
				std::string name = path.substr( pos + 1, path.size() );
				path = path.substr( 0, pos + 1 );
				TextureDirectory_loadTexture( path.c_str(), name.c_str() );
			}
		}
		textureBrowser.m_searchedTags = true;
		g_TextureBrowser_currentDirectory = tags_searched;

		textureBrowser.m_nTotalHeight = 0;
		TextureBrowser_setOriginY( textureBrowser, 0 );
		TextureBrowser_heightChanged( textureBrowser );
		TextureBrowser_updateTitle();
	}
	g_slist_free( selected );
}

void TextureBrowser_toggleSearchButton(){
	TextureBrowser &textureBrowser = GlobalTextureBrowser();
	gint page = gtk_notebook_get_current_page( GTK_NOTEBOOK( textureBrowser.m_tag_notebook ) );

	if ( page == 0 ) { // tag page
		gtk_widget_show_all( textureBrowser.m_search_button );
	}
	else {
		textureBrowser.m_search_button.hide();
	}
}

void TextureBrowser_constructTagNotebook(){
	TextureBrowser &textureBrowser = GlobalTextureBrowser();
	textureBrowser.m_tag_notebook = ui::Widget::from(gtk_notebook_new());
	ui::Widget labelTags = ui::Label( "Tags" );
	ui::Widget labelTextures = ui::Label( "Textures" );

	gtk_notebook_append_page( GTK_NOTEBOOK( textureBrowser.m_tag_notebook ), textureBrowser.m_scr_win_tree, labelTextures );
	gtk_notebook_append_page( GTK_NOTEBOOK( textureBrowser.m_tag_notebook ), textureBrowser.m_scr_win_tags, labelTags );

	textureBrowser.m_tag_notebook.connect( "switch-page", G_CALLBACK( TextureBrowser_toggleSearchButton ), NULL );

	gtk_widget_show_all( textureBrowser.m_tag_notebook );
}

void TextureBrowser_constructSearchButton(){
	auto image = ui::Widget::from(gtk_image_new_from_stock( GTK_STOCK_FIND, GTK_ICON_SIZE_SMALL_TOOLBAR ));
	TextureBrowser &textureBrowser = GlobalTextureBrowser();
	textureBrowser.m_search_button = ui::Button(ui::New);
	textureBrowser.m_search_button.connect( "clicked", G_CALLBACK( TextureBrowser_searchTags ), NULL );
	gtk_widget_set_tooltip_text(textureBrowser.m_search_button, "Search with selected tags");
	textureBrowser.m_search_button.add(image);
}

void TextureBrowser_checkTagFile(){
	const char SHADERTAG_FILE[] = "shadertags.xml";
	CopiedString default_filename, rc_filename;
	StringOutputStream stream( 256 );
	TextureBrowser &textureBrowser = GlobalTextureBrowser();

	stream << LocalRcPath_get();
	stream << SHADERTAG_FILE;
	rc_filename = stream.c_str();

	if ( file_exists( rc_filename.c_str() ) ) {
		textureBrowser.m_tags = TagBuilder.OpenXmlDoc( rc_filename.c_str() );

		if ( textureBrowser.m_tags ) {
			globalOutputStream() << "Loading tag file " << rc_filename.c_str() << ".\n";
		}
	}
	else
	{
		// load default tagfile
		stream.clear();
		stream << g_pGameDescription->mGameToolsPath.c_str();
		stream << SHADERTAG_FILE;
		default_filename = stream.c_str();

		if ( file_exists( default_filename.c_str() ) ) {
			textureBrowser.m_tags = TagBuilder.OpenXmlDoc( default_filename.c_str(), rc_filename.c_str() );

			if ( textureBrowser.m_tags ) {
				globalOutputStream() << "Loading default tag file " << default_filename.c_str() << ".\n";
			}
		}
		else
		{
			globalOutputStream() << "Unable to find default tag file " << default_filename.c_str() << ". No tag support. Plugins -> ShaderPlug -> Create tag file: to start using tags\n";
		}
	}
}

void TextureBrowser_SetNotex(){
	IShader* notex = QERApp_Shader_ForName( DEFAULT_NOTEX_NAME );
	IShader* shadernotex = QERApp_Shader_ForName( DEFAULT_SHADERNOTEX_NAME );

	g_notex = notex->getTexture()->name;

	g_shadernotex = shadernotex->getTexture()->name;

	notex->DecRef();
	shadernotex->DecRef();
}

static bool isGLWidgetConstructed = false;
static bool isWindowConstructed = false;

void TextureBrowser_constructGLWidget(){
	TextureBrowser &textureBrowser = GlobalTextureBrowser();
	textureBrowser.m_gl_widget = glwidget_new( FALSE );
	g_object_ref( textureBrowser.m_gl_widget._handle );

	gtk_widget_set_events( textureBrowser.m_gl_widget, GDK_DESTROY | GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_SCROLL_MASK );
	gtk_widget_set_can_focus( textureBrowser.m_gl_widget, true );

	textureBrowser.m_sizeHandler = textureBrowser.m_gl_widget.connect( "size_allocate", G_CALLBACK( TextureBrowser_size_allocate ), &textureBrowser );
	textureBrowser.m_exposeHandler = textureBrowser.m_gl_widget.on_render( G_CALLBACK( TextureBrowser_expose ), &textureBrowser );

	textureBrowser.m_gl_widget.connect( "button_press_event", G_CALLBACK( TextureBrowser_button_press ), &textureBrowser );
	textureBrowser.m_gl_widget.connect( "button_release_event", G_CALLBACK( TextureBrowser_button_release ), &textureBrowser );
	textureBrowser.m_gl_widget.connect( "motion_notify_event", G_CALLBACK( TextureBrowser_motion ), &textureBrowser );
	textureBrowser.m_gl_widget.connect( "scroll_event", G_CALLBACK( TextureBrowser_scroll ), &textureBrowser );

#ifdef WORKAROUND_MACOS_GTK2_GLWIDGET
	textureBrowser.m_hframe.pack_start( textureBrowser.m_gl_widget, TRUE, TRUE, 0 );
#else // !WORKAROUND_MACOS_GTK2_GLWIDGET
	textureBrowser.m_frame.pack_start( textureBrowser.m_gl_widget, TRUE, TRUE, 0 );
#endif // !WORKAROUND_MACOS_GTK2_GLWIDGET

	textureBrowser.m_gl_widget.show();

	isGLWidgetConstructed = true;
}

ui::Widget TextureBrowser_constructWindow( ui::Window toplevel ){
	// The gl_widget and the tag assignment frame should be packed into a GtkVPaned with the slider
	// position stored in local.pref. gtk_paned_get_position() and gtk_paned_set_position() don't
	// seem to work in gtk 2.4 and the arrow buttons don't handle GTK_FILL, so here's another thing
	// for the "once-the-gtk-libs-are-updated-TODO-list" :x
	TextureBrowser &textureBrowser = GlobalTextureBrowser();

	TextureBrowser_checkTagFile();
	TextureBrowser_SetNotex();

	GlobalShaderSystem().setActiveShadersChangedNotify( ReferenceCaller<TextureBrowser, void(), TextureBrowser_activeShadersChanged>( textureBrowser ) );

	textureBrowser.m_parent = toplevel;

	auto table = ui::Table(3, 3, FALSE);
	auto vbox = ui::VBox(FALSE, 0);
	table.attach(vbox, {0, 1, 1, 3}, {GTK_FILL, GTK_FILL});
	vbox.show();

	// ui::Widget menu_bar{ui::null};
	auto toolbar = ui::Toolbar::from( gtk_toolbar_new() );

	{ // menu bar
		// menu_bar = ui::Widget::from(gtk_menu_bar_new());
		auto menu_view = ui::Menu(ui::New);
		// auto view_item = TextureBrowser_constructViewMenu( menu_view );
		TextureBrowser_constructViewMenu( menu_view );
		gtk_menu_set_title( menu_view, "View" );
		// gtk_menu_item_set_submenu( GTK_MENU_ITEM( view_item ), menu_view );
		// gtk_menu_shell_append( GTK_MENU_SHELL( menu_bar ), view_item );

		//gtk_table_attach( GTK_TABLE( table ), GTK_WIDGET( toolbar ), 0, 1, 0, 1, GTK_FILL, GTK_FILL, 0, 0 );
		gtk_box_pack_start( GTK_BOX( vbox ), GTK_WIDGET( toolbar ), FALSE, FALSE, 0 );

		//view menu button
		{
			auto button = toolbar_append_button( toolbar, "View", "texbro_view.png" );
			button.dimensions( 22, 22 );
			button.connect( "clicked", G_CALLBACK( Popup_View_Menu ), menu_view );

			//to show detached menu over floating tex bro
			gtk_menu_attach_to_widget( GTK_MENU( menu_view ), GTK_WIDGET( button ), NULL );
		}
		{
			auto button = toolbar_append_button( toolbar, "Find / Replace...", "texbro_gtk-find-and-replace.png", "FindReplaceTextures" );
			button.dimensions( 22, 22 );
		}
		{
			auto button = toolbar_append_button( toolbar, "Flush & Reload Shaders", "texbro_refresh.png", "RefreshShaders" );
			button.dimensions( 22, 22 );
		}
		toolbar.show();

/*
		auto menu_tools = ui::Menu(ui::New);
		auto tools_item = TextureBrowser_constructToolsMenu( menu_tools );
		gtk_menu_item_set_submenu( GTK_MENU_ITEM( tools_item ), menu_tools );
		gtk_menu_shell_append( GTK_MENU_SHELL( menu_bar ), tools_item );
*/
		// table.attach(menu_bar, {0, 3, 0, 1}, {GTK_FILL, GTK_SHRINK});
		// menu_bar.show();
	}
	{ // Texture TreeView
		textureBrowser.m_scr_win_tree = ui::ScrolledWindow(ui::New);
		gtk_container_set_border_width( GTK_CONTAINER( textureBrowser.m_scr_win_tree ), 0 );

		// vertical only scrolling for treeview
		gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( textureBrowser.m_scr_win_tree ), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS );

		textureBrowser.m_scr_win_tree.show();

		TextureBrowser_createTreeViewTree();

		gtk_scrolled_window_add_with_viewport( GTK_SCROLLED_WINDOW( textureBrowser.m_scr_win_tree ), textureBrowser.m_treeViewTree  );
		textureBrowser.m_treeViewTree.show();
	}
	{ // gl_widget scrollbar
		auto w = ui::Widget::from(gtk_vscrollbar_new( ui::Adjustment( 0,0,0,1,1,0 ) ));
		table.attach(w, {2, 3, 1, 2}, {GTK_SHRINK, GTK_FILL});
		w.show();
		textureBrowser.m_texture_scroll = w;

		auto vadjustment = ui::Adjustment::from(gtk_range_get_adjustment( GTK_RANGE( textureBrowser.m_texture_scroll ) ));
		vadjustment.connect( "value_changed", G_CALLBACK( TextureBrowser_verticalScroll ), &textureBrowser );

		textureBrowser.m_texture_scroll.visible(textureBrowser.m_showTextureScrollbar);
	}
	{ // gl_widget
#ifdef WORKAROUND_MACOS_GTK2_GLWIDGET
		textureBrowser.m_vframe = ui::VBox( FALSE, 0 );
		table.attach(textureBrowser.m_vframe, {1, 2, 1, 2});

		textureBrowser.m_vfiller = ui::VBox( FALSE, 0 );
		textureBrowser.m_vframe.pack_start( textureBrowser.m_vfiller, FALSE, FALSE, 0 );

		textureBrowser.m_hframe = ui::HBox( FALSE, 0 );
		textureBrowser.m_vframe.pack_start( textureBrowser.m_hframe, TRUE, TRUE, 0 );

		textureBrowser.m_hfiller = ui::HBox( FALSE, 0 );
		textureBrowser.m_hframe.pack_start( textureBrowser.m_hfiller, FALSE, FALSE, 0 );

		textureBrowser.m_vframe.show();
		textureBrowser.m_vfiller.show();
		textureBrowser.m_hframe.show(),
		textureBrowser.m_hfiller.show();
#else // !WORKAROUND_MACOS_GTK2_GLWIDGET
		textureBrowser.m_frame = ui::VBox( FALSE, 0 );
		table.attach(textureBrowser.m_frame, {1, 2, 1, 2});
		textureBrowser.m_frame.show();
#endif // !WORKAROUND_MACOS_GTK2_GLWIDGET

		TextureBrowser_constructGLWidget();
	}

	// tag stuff
	if ( textureBrowser.m_tags ) {
		{ // fill tag GtkListStore
			textureBrowser.m_all_tags_list = ui::ListStore::from(gtk_list_store_new( N_COLUMNS, G_TYPE_STRING ));
            auto sortable = GTK_TREE_SORTABLE( textureBrowser.m_all_tags_list );
			gtk_tree_sortable_set_sort_column_id( sortable, TAG_COLUMN, GTK_SORT_ASCENDING );

			TagBuilder.GetAllTags( textureBrowser.m_all_tags );
			TextureBrowser_buildTagList();
		}
		{ // tag menu bar
			auto menu_tags = ui::Menu(ui::New);
			// auto tags_item = TextureBrowser_constructTagsMenu( menu_tags );
			TextureBrowser_constructTagsMenu( menu_tags );
			// gtk_menu_item_set_submenu( GTK_MENU_ITEM( tags_item ), menu_tags );
			// gtk_menu_shell_append( GTK_MENU_SHELL( menu_bar ), tags_item );

			auto button = toolbar_append_button( toolbar, "Tags", "texbro_tags.png" );
			button.dimensions( 22, 22 );
			button.connect( "clicked", G_CALLBACK( Popup_View_Menu ), menu_tags );
		}
		{ // Tag TreeView
			textureBrowser.m_scr_win_tags = ui::ScrolledWindow(ui::New);
			gtk_container_set_border_width( GTK_CONTAINER( textureBrowser.m_scr_win_tags ), 0 );

			// vertical only scrolling for treeview
			gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( textureBrowser.m_scr_win_tags ), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS );

			TextureBrowser_createTreeViewTags();

            auto selection = gtk_tree_view_get_selection(textureBrowser.m_treeViewTags );
			gtk_tree_selection_set_mode( selection, GTK_SELECTION_MULTIPLE );

			gtk_scrolled_window_add_with_viewport( GTK_SCROLLED_WINDOW( textureBrowser.m_scr_win_tags ), textureBrowser.m_treeViewTags  );
			textureBrowser.m_treeViewTags.show();
		}
		{ // Texture/Tag notebook
			TextureBrowser_constructTagNotebook();
			vbox.pack_start( textureBrowser.m_tag_notebook, TRUE, TRUE, 0 );
		}
		{ // Tag search button
			TextureBrowser_constructSearchButton();
			vbox.pack_end(textureBrowser.m_search_button, FALSE, FALSE, 0);
		}
		auto frame_table = ui::Table(3, 3, FALSE);
		{ // Tag frame

			textureBrowser.m_tag_frame = ui::Frame( "Tag assignment" );
			gtk_frame_set_label_align( GTK_FRAME( textureBrowser.m_tag_frame ), 0.5, 0.5 );
			gtk_frame_set_shadow_type( GTK_FRAME( textureBrowser.m_tag_frame ), GTK_SHADOW_NONE );

			table.attach(textureBrowser.m_tag_frame, {1, 3, 2, 3}, {GTK_FILL, GTK_SHRINK});

			frame_table.show();

			textureBrowser.m_tag_frame.add(frame_table);
		}
		{ // assigned tag list
			ui::Widget scrolled_win = ui::ScrolledWindow(ui::New);
			gtk_container_set_border_width( GTK_CONTAINER( scrolled_win ), 0 );
			gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( scrolled_win ), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS );

			textureBrowser.m_assigned_store = ui::ListStore::from(gtk_list_store_new( N_COLUMNS, G_TYPE_STRING ));

            auto sortable = GTK_TREE_SORTABLE( textureBrowser.m_assigned_store );
			gtk_tree_sortable_set_sort_column_id( sortable, TAG_COLUMN, GTK_SORT_ASCENDING );

			auto renderer = ui::CellRendererText(ui::New);

			textureBrowser.m_assigned_tree = ui::TreeView(ui::TreeModel::from(textureBrowser.m_assigned_store._handle));
			textureBrowser.m_assigned_store.unref();
			textureBrowser.m_assigned_tree.connect( "row-activated", (GCallback) TextureBrowser_removeTags, NULL );
			gtk_tree_view_set_headers_visible(textureBrowser.m_assigned_tree, FALSE );

            auto selection = gtk_tree_view_get_selection(textureBrowser.m_assigned_tree );
			gtk_tree_selection_set_mode( selection, GTK_SELECTION_MULTIPLE );

            auto column = ui::TreeViewColumn( "", renderer, {{"text", TAG_COLUMN}} );
			gtk_tree_view_append_column(textureBrowser.m_assigned_tree, column );
			textureBrowser.m_assigned_tree.show();

			scrolled_win.show();
			gtk_scrolled_window_add_with_viewport( GTK_SCROLLED_WINDOW( scrolled_win ), textureBrowser.m_assigned_tree  );

			frame_table.attach(scrolled_win, {0, 1, 1, 3}, {GTK_FILL, GTK_FILL});
		}
		{ // available tag list
			ui::Widget scrolled_win = ui::ScrolledWindow(ui::New);
			gtk_container_set_border_width( GTK_CONTAINER( scrolled_win ), 0 );
			gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( scrolled_win ), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS );

			textureBrowser.m_available_store = ui::ListStore::from(gtk_list_store_new( N_COLUMNS, G_TYPE_STRING ));
            auto sortable = GTK_TREE_SORTABLE( textureBrowser.m_available_store );
			gtk_tree_sortable_set_sort_column_id( sortable, TAG_COLUMN, GTK_SORT_ASCENDING );

			auto renderer = ui::CellRendererText(ui::New);

			textureBrowser.m_available_tree = ui::TreeView(ui::TreeModel::from(textureBrowser.m_available_store._handle));
			textureBrowser.m_available_store.unref();
			textureBrowser.m_available_tree.connect( "row-activated", (GCallback) TextureBrowser_assignTags, NULL );
			gtk_tree_view_set_headers_visible(textureBrowser.m_available_tree, FALSE );

            auto selection = gtk_tree_view_get_selection(textureBrowser.m_available_tree );
			gtk_tree_selection_set_mode( selection, GTK_SELECTION_MULTIPLE );

            auto column = ui::TreeViewColumn( "", renderer, {{"text", TAG_COLUMN}} );
			gtk_tree_view_append_column(textureBrowser.m_available_tree, column );
			textureBrowser.m_available_tree.show();

			scrolled_win.show();
			gtk_scrolled_window_add_with_viewport( GTK_SCROLLED_WINDOW( scrolled_win ), textureBrowser.m_available_tree  );

			frame_table.attach(scrolled_win, {2, 3, 1, 3}, {GTK_FILL, GTK_FILL});
		}
		{ // tag arrow buttons
			auto m_btn_left = ui::Button(ui::New);
			auto m_btn_right = ui::Button(ui::New);
			auto m_arrow_left = ui::Widget::from(gtk_arrow_new( GTK_ARROW_LEFT, GTK_SHADOW_OUT ));
			auto m_arrow_right = ui::Widget::from(gtk_arrow_new( GTK_ARROW_RIGHT, GTK_SHADOW_OUT ));
			m_btn_left.add(m_arrow_left);
			m_btn_right.add(m_arrow_right);

			// workaround. the size of the tag frame depends of the requested size of the arrow buttons.
			m_arrow_left.dimensions(-1, 68);
			m_arrow_right.dimensions(-1, 68);

			frame_table.attach(m_btn_left, {1, 2, 1, 2}, {GTK_SHRINK, GTK_EXPAND});
			frame_table.attach(m_btn_right, {1, 2, 2, 3}, {GTK_SHRINK, GTK_EXPAND});

			m_btn_left.connect( "clicked", G_CALLBACK( TextureBrowser_assignTags ), NULL );
			m_btn_right.connect( "clicked", G_CALLBACK( TextureBrowser_removeTags ), NULL );

			m_btn_left.show();
			m_btn_right.show();
			m_arrow_left.show();
			m_arrow_right.show();
		}
		{ // tag fram labels
			ui::Widget m_lbl_assigned = ui::Label( "Assigned" );
			ui::Widget m_lbl_unassigned = ui::Label( "Available" );

			frame_table.attach(m_lbl_assigned, {0, 1, 0, 1}, {GTK_EXPAND, GTK_SHRINK});
			frame_table.attach(m_lbl_unassigned, {2, 3, 0, 1}, {GTK_EXPAND, GTK_SHRINK});

			m_lbl_assigned.show();
			m_lbl_unassigned.show();
		}
	}
	else { // no tag support, show the texture tree only
		vbox.pack_start( textureBrowser.m_scr_win_tree, TRUE, TRUE, 0 );
	}

	// TODO do we need this?
	//gtk_container_set_focus_chain(GTK_CONTAINER(hbox_table), NULL);

	isWindowConstructed = true;

	return table;
}

void TextureBrowser_destroyGLWidget(){
	TextureBrowser &textureBrowser = GlobalTextureBrowser();
	if ( isGLWidgetConstructed )
	{
		g_signal_handler_disconnect( G_OBJECT( textureBrowser.m_gl_widget ), textureBrowser.m_sizeHandler );
		g_signal_handler_disconnect( G_OBJECT( textureBrowser.m_gl_widget ), textureBrowser.m_exposeHandler );

#ifdef WORKAROUND_MACOS_GTK2_GLWIDGET
		textureBrowser.m_hframe.remove( textureBrowser.m_gl_widget );
#else // !WORKAROUND_MACOS_GTK2_GLWIDGET
		textureBrowser.m_frame.remove( textureBrowser.m_gl_widget );
#endif // !WORKAROUND_MACOS_GTK2_GLWIDGET

		textureBrowser.m_gl_widget.unref();

		isGLWidgetConstructed = false;
	}
}

void TextureBrowser_destroyWindow(){
	GlobalShaderSystem().setActiveShadersChangedNotify( Callback<void()>() );

	TextureBrowser_destroyGLWidget();
}

#ifdef WORKAROUND_MACOS_GTK2_GLWIDGET
/* workaround for gtkglext on gtk 2 issue: OpenGL texture viewport being drawn over the other pages */
/* this is very ugly: force the resizing of the viewport to a single bottom line by forcing the
 * resizing of the gl widget by expanding some empty boxes, so the widget area size is reduced
 * while covered by another page, so the texture viewport is still rendered over the other page
 * but does not annoy the user that much because it's just a line on the bottom that may even
 * be printed over existing bottom frame or very close to it. */
void TextureBrowser_showGLWidget(){
	TextureBrowser &textureBrowser = GlobalTextureBrowser();
	if ( isWindowConstructed && isGLWidgetConstructed )
	{
		textureBrowser.m_vframe.set_child_packing( textureBrowser.m_vfiller, FALSE, FALSE, 0, ui::Packing::START );
		textureBrowser.m_vframe.set_child_packing( textureBrowser.m_hframe, TRUE, TRUE, 0, ui::Packing::START );
		textureBrowser.m_vframe.set_child_packing( textureBrowser.m_hfiller, FALSE, FALSE, 0, ui::Packing::START );
		textureBrowser.m_vframe.set_child_packing( textureBrowser.m_gl_widget, TRUE, TRUE, 0, ui::Packing::START );

		textureBrowser.m_gl_widget.show();
}
}

void TextureBrowser_hideGLWidget(){
	TextureBrowser &textureBrowser = GlobalTextureBrowser();
	if ( isWindowConstructed && isGLWidgetConstructed )
	{
		textureBrowser.m_vframe.set_child_packing( textureBrowser.m_vfiller, TRUE, TRUE, 0, ui::Packing::START);
		textureBrowser.m_vframe.set_child_packing( textureBrowser.m_hframe, FALSE, FALSE, 0, ui::Packing::END );
		textureBrowser.m_vframe.set_child_packing( textureBrowser.m_hfiller, TRUE, TRUE, 0, ui::Packing::START);
		textureBrowser.m_vframe.set_child_packing( textureBrowser.m_gl_widget, FALSE, FALSE, 0, ui::Packing::END );

		// The hack needs the GL widget to not be hidden to work,
		// so resizing it triggers the redraw of it with the new size.
		// GlobalTextureBrowser().m_gl_widget.hide();

		// Trigger the redraw.
		TextureBrowser_redraw( &GlobalTextureBrowser() );
		ui::process();
	}
}
#endif // WORKAROUND_MACOS_GTK2_GLWIDGET

const Vector3& TextureBrowser_getBackgroundColour( TextureBrowser& textureBrowser ){
	return textureBrowser.color_textureback;
}

void TextureBrowser_setBackgroundColour( TextureBrowser& textureBrowser, const Vector3& colour ){
	textureBrowser.color_textureback = colour;
	TextureBrowser_queueDraw( textureBrowser );
}

void TextureBrowser_selectionHelper( ui::TreeModel model, ui::TreePath path, GtkTreeIter* iter, GSList** selected ){
	g_assert( selected != NULL );

	gchar* name;
	gtk_tree_model_get( model, iter, TAG_COLUMN, &name, -1 );
	*selected = g_slist_append( *selected, name );
}

void TextureBrowser_shaderInfo(){
	const char* name = TextureBrowser_GetSelectedShader( GlobalTextureBrowser() );
	IShader* shader = QERApp_Shader_ForName( name );

	DoShaderInfoDlg( name, shader->getShaderFileName(), "Shader Info" );

	shader->DecRef();
}

void TextureBrowser_addTag(){
	CopiedString tag;
	TextureBrowser &textureBrowser = GlobalTextureBrowser();

	EMessageBoxReturn result = DoShaderTagDlg( &tag, "Add shader tag" );

	if ( result == eIDOK && !tag.empty() ) {
		GtkTreeIter iter;
		textureBrowser.m_all_tags.insert( tag.c_str() );
		gtk_list_store_append( textureBrowser.m_available_store, &iter );
		gtk_list_store_set( textureBrowser.m_available_store, &iter, TAG_COLUMN, tag.c_str(), -1 );

		// Select the currently added tag in the available list
        auto selection = gtk_tree_view_get_selection(textureBrowser.m_available_tree );
		gtk_tree_selection_select_iter( selection, &iter );

		textureBrowser.m_all_tags_list.append(TAG_COLUMN, tag.c_str());
	}
}

void TextureBrowser_renameTag(){
	/* WORKAROUND: The tag treeview is set to GTK_SELECTION_MULTIPLE. Because
	   gtk_tree_selection_get_selected() doesn't work with GTK_SELECTION_MULTIPLE,
	   we need to count the number of selected rows first and use
	   gtk_tree_selection_selected_foreach() then to go through the list of selected
	   rows (which always containins a single row).
	 */

	GSList* selected = NULL;
	TextureBrowser &textureBrowser = GlobalTextureBrowser();

	auto selection = gtk_tree_view_get_selection(textureBrowser.m_treeViewTags );
	gtk_tree_selection_selected_foreach( selection, GtkTreeSelectionForeachFunc( TextureBrowser_selectionHelper ), &selected );

	if ( g_slist_length( selected ) == 1 ) { // we only rename a single tag
		CopiedString newTag;
		EMessageBoxReturn result = DoShaderTagDlg( &newTag, "Rename shader tag" );

		if ( result == eIDOK && !newTag.empty() ) {
			GtkTreeIter iterList;
			gchar* rowTag;
			gchar* oldTag = (char*)selected->data;

			bool row = gtk_tree_model_get_iter_first(textureBrowser.m_all_tags_list, &iterList ) != 0;

			while ( row )
			{
				gtk_tree_model_get(textureBrowser.m_all_tags_list, &iterList, TAG_COLUMN, &rowTag, -1 );

				if ( strcmp( rowTag, oldTag ) == 0 ) {
					gtk_list_store_set( textureBrowser.m_all_tags_list, &iterList, TAG_COLUMN, newTag.c_str(), -1 );
				}
				row = gtk_tree_model_iter_next(textureBrowser.m_all_tags_list, &iterList ) != 0;
			}

			TagBuilder.RenameShaderTag( oldTag, newTag.c_str() );

			textureBrowser.m_all_tags.erase( (CopiedString)oldTag );
			textureBrowser.m_all_tags.insert( newTag );

			BuildStoreAssignedTags( textureBrowser.m_assigned_store, textureBrowser.shader.c_str(), &textureBrowser );
			BuildStoreAvailableTags( textureBrowser.m_available_store, textureBrowser.m_assigned_store, textureBrowser.m_all_tags, &textureBrowser );
		}
	}
	else
	{
		ui::alert( textureBrowser.m_parent, "Select a single tag for renaming." );
	}
}

void TextureBrowser_deleteTag(){
	GSList* selected = NULL;
	TextureBrowser &textureBrowser = GlobalTextureBrowser();

	auto selection = gtk_tree_view_get_selection(textureBrowser.m_treeViewTags );
	gtk_tree_selection_selected_foreach( selection, GtkTreeSelectionForeachFunc( TextureBrowser_selectionHelper ), &selected );

	if ( g_slist_length( selected ) == 1 ) { // we only delete a single tag
		auto result = ui::alert( textureBrowser.m_parent, "Are you sure you want to delete the selected tag?", "Delete Tag", ui::alert_type::YESNO, ui::alert_icon::Question );

		if ( result == ui::alert_response::YES ) {
			GtkTreeIter iterSelected;
			gchar *rowTag;

			gchar* tagSelected = (char*)selected->data;

			bool row = gtk_tree_model_get_iter_first(textureBrowser.m_all_tags_list, &iterSelected ) != 0;

			while ( row )
			{
				gtk_tree_model_get(textureBrowser.m_all_tags_list, &iterSelected, TAG_COLUMN, &rowTag, -1 );

				if ( strcmp( rowTag, tagSelected ) == 0 ) {
					gtk_list_store_remove( textureBrowser.m_all_tags_list, &iterSelected );
					break;
				}
				row = gtk_tree_model_iter_next(textureBrowser.m_all_tags_list, &iterSelected ) != 0;
			}

			TagBuilder.DeleteTag( tagSelected );
			textureBrowser.m_all_tags.erase( (CopiedString)tagSelected );

			BuildStoreAssignedTags( textureBrowser.m_assigned_store, textureBrowser.shader.c_str(), &textureBrowser );
			BuildStoreAvailableTags( textureBrowser.m_available_store, textureBrowser.m_assigned_store, textureBrowser.m_all_tags, &textureBrowser );
		}
	}
	else {
		ui::alert( textureBrowser.m_parent, "Select a single tag for deletion." );
	}
}

void TextureBrowser_copyTag(){
	TextureBrowser &textureBrowser = GlobalTextureBrowser();
	textureBrowser.m_copied_tags.clear();
	TagBuilder.GetShaderTags( textureBrowser.shader.c_str(), textureBrowser.m_copied_tags );
}

void TextureBrowser_pasteTag(){
	TextureBrowser &textureBrowser = GlobalTextureBrowser();
	IShader* ishader = QERApp_Shader_ForName( textureBrowser.shader.c_str() );
	CopiedString shader = textureBrowser.shader.c_str();

	if ( !TagBuilder.CheckShaderTag( shader.c_str() ) ) {
		CopiedString shaderFile = ishader->getShaderFileName();
		if ( shaderFile.empty() ) {
			// it's a texture
			TagBuilder.AddShaderNode( shader.c_str(), CUSTOM, TEXTURE );
		}
		else
		{
			// it's a shader
			TagBuilder.AddShaderNode( shader.c_str(), CUSTOM, SHADER );
		}

		for ( size_t i = 0; i < textureBrowser.m_copied_tags.size(); ++i )
		{
			TagBuilder.AddShaderTag( shader.c_str(), textureBrowser.m_copied_tags[i].c_str(), TAG );
		}
	}
	else
	{
		for ( size_t i = 0; i < textureBrowser.m_copied_tags.size(); ++i )
		{
			if ( !TagBuilder.CheckShaderTag( shader.c_str(), textureBrowser.m_copied_tags[i].c_str() ) ) {
				// the tag doesn't exist - let's add it
				TagBuilder.AddShaderTag( shader.c_str(), textureBrowser.m_copied_tags[i].c_str(), TAG );
			}
		}
	}

	ishader->DecRef();

	TagBuilder.SaveXmlDoc();
	BuildStoreAssignedTags( textureBrowser.m_assigned_store, shader.c_str(), &textureBrowser );
	BuildStoreAvailableTags( textureBrowser.m_available_store, textureBrowser.m_assigned_store, textureBrowser.m_all_tags, &textureBrowser );
}

void TextureBrowser_RefreshShaders(){
	TextureBrowser &textureBrowser = GlobalTextureBrowser();

	/* When shaders are refreshed, forces reloading the textures as well.
	Previously it would at best only display shaders, at worst mess up some textured objects. */

    auto selection = gtk_tree_view_get_selection(GlobalTextureBrowser().m_treeViewTree);
	GtkTreeModel* model = NULL;
	GtkTreeIter iter;
	if ( gtk_tree_selection_get_selected (selection, &model, &iter) )
	{
		gchar dirName[1024];

		gchar* buffer;
		gtk_tree_model_get( model, &iter, 0, &buffer, -1 );
		strcpy( dirName, buffer );
		g_free( buffer );
		if ( !TextureBrowser_showWads() ) {
			strcat( dirName, "/" );
		}

		ScopeDisableScreenUpdates disableScreenUpdates( "Processing...", "Loading Shaders" );
		GlobalShaderSystem().refresh();
		/* texturebrowser tree update on vfs restart */
		TextureBrowser_constructTreeStore();
		UpdateAllWindows();

		TextureBrowser_ShowDirectory( GlobalTextureBrowser(), dirName );
		TextureBrowser_queueDraw( GlobalTextureBrowser() );
	}

	else{
		ScopeDisableScreenUpdates disableScreenUpdates( "Processing...", "Loading Shaders" );
		GlobalShaderSystem().refresh();
		/* texturebrowser tree update on vfs restart */
		TextureBrowser_constructTreeStore();
		UpdateAllWindows();
	}
}

void TextureBrowser_ToggleShowShaders(){
	GlobalTextureBrowser().m_showShaders ^= 1;
	GlobalTextureBrowser().m_showshaders_item.update();

	GlobalTextureBrowser().m_heightChanged = true;
	GlobalTextureBrowser().m_originInvalid = true;
	g_activeShadersChangedCallbacks();

	TextureBrowser_queueDraw( GlobalTextureBrowser() );
}

void TextureBrowser_ToggleShowTextures(){
	GlobalTextureBrowser().m_showTextures ^= 1;
	GlobalTextureBrowser().m_showtextures_item.update();

	GlobalTextureBrowser().m_heightChanged = true;
	GlobalTextureBrowser().m_originInvalid = true;
	g_activeShadersChangedCallbacks();

	TextureBrowser_queueDraw( GlobalTextureBrowser() );
}

void TextureBrowser_ToggleShowShaderListOnly(){
	g_TextureBrowser_shaderlistOnly ^= 1;
	GlobalTextureBrowser().m_showshaderlistonly_item.update();

	TextureBrowser_constructTreeStore();
}

void TextureBrowser_showAll(){
	g_TextureBrowser_currentDirectory = "";
	GlobalTextureBrowser().m_searchedTags = false;
//	TextureBrowser_SetHideUnused( GlobalTextureBrowser(), false );
	TextureBrowser_ToggleHideUnused();
	//TextureBrowser_heightChanged( GlobalTextureBrowser() );
	TextureBrowser_updateTitle();
}

void TextureBrowser_showUntagged(){
	TextureBrowser &textureBrowser = GlobalTextureBrowser();
	auto result = ui::alert( textureBrowser.m_parent, "WARNING! This function might need a lot of memory and time. Are you sure you want to use it?", "Show Untagged", ui::alert_type::YESNO, ui::alert_icon::Warning );

	if ( result == ui::alert_response::YES ) {
		textureBrowser.m_found_shaders.clear();
		TagBuilder.GetUntagged( textureBrowser.m_found_shaders );
		std::set<CopiedString>::iterator iter;

		ScopeDisableScreenUpdates disableScreenUpdates( "Searching untagged textures...", "Loading Textures" );

		for ( iter = textureBrowser.m_found_shaders.begin(); iter != textureBrowser.m_found_shaders.end(); iter++ )
		{
			std::string path = ( *iter ).c_str();
			size_t pos = path.find_last_of( "/", path.size() );
			std::string name = path.substr( pos + 1, path.size() );
			path = path.substr( 0, pos + 1 );
			TextureDirectory_loadTexture( path.c_str(), name.c_str() );
			globalErrorStream() << path.c_str() << name.c_str() << "\n";
		}

		g_TextureBrowser_currentDirectory = "Untagged";
		TextureBrowser_queueDraw( GlobalTextureBrowser() );
		TextureBrowser_heightChanged( textureBrowser );
		TextureBrowser_updateTitle();
	}
}

void TextureBrowser_FixedSize(){
	g_TextureBrowser_fixedSize ^= 1;
	GlobalTextureBrowser().m_fixedsize_item.update();
	TextureBrowser_activeShadersChanged( GlobalTextureBrowser() );
}

void TextureBrowser_FilterMissing(){
	g_TextureBrowser_filterMissing ^= 1;
	GlobalTextureBrowser().m_filternotex_item.update();
	TextureBrowser_activeShadersChanged( GlobalTextureBrowser() );
	TextureBrowser_RefreshShaders();
}

void TextureBrowser_FilterFallback(){
	g_TextureBrowser_filterFallback ^= 1;
	GlobalTextureBrowser().m_hidenotex_item.update();
	TextureBrowser_activeShadersChanged( GlobalTextureBrowser() );
	TextureBrowser_RefreshShaders();
}

void TextureBrowser_EnableAlpha(){
	g_TextureBrowser_enableAlpha ^= 1;
	GlobalTextureBrowser().m_enablealpha_item.update();
	TextureBrowser_activeShadersChanged( GlobalTextureBrowser() );
}

void TextureBrowser_exportTitle( const Callback<void(const char *)> & importer ){
	StringOutputStream buffer( 64 );
	buffer << "Textures: ";
	if ( !string_empty( g_TextureBrowser_currentDirectory.c_str() ) ) {
		buffer << g_TextureBrowser_currentDirectory.c_str();
	}
	else
	{
		buffer << "all";
	}
	importer( buffer.c_str() );
}

struct TextureScale {
	static void Export(const TextureBrowser &self, const Callback<void(int)> &returnz) {
		switch (self.m_textureScale) {
			case 10:
				returnz(0);
				break;
			case 25:
				returnz(1);
				break;
			case 50:
				returnz(2);
				break;
			case 100:
				returnz(3);
				break;
			case 200:
				returnz(4);
				break;
		}
	}

	static void Import(TextureBrowser &self, int value) {
		switch (value) {
			case 0:
				TextureBrowser_setScale(self, 10);
				break;
			case 1:
				TextureBrowser_setScale(self, 25);
				break;
			case 2:
				TextureBrowser_setScale(self, 50);
				break;
			case 3:
				TextureBrowser_setScale(self, 100);
				break;
			case 4:
				TextureBrowser_setScale(self, 200);
				break;
		}
	}
};

struct UniformTextureSize {
	static void Export(const TextureBrowser &self, const Callback<void(int)> &returnz) {
		returnz(GlobalTextureBrowser().m_uniformTextureSize);
	}

	static void Import(TextureBrowser &self, int value) {
		if (value > 16)
			TextureBrowser_setUniformSize(self, value);
	}
};

struct UniformTextureMinSize {
	static void Export(const TextureBrowser &self, const Callback<void(int)> &returnz) {
		returnz(GlobalTextureBrowser().m_uniformTextureMinSize);
	}

	static void Import(TextureBrowser &self, int value) {
		if (value > 16)
			TextureBrowser_setUniformSize(self, value);
	}
};

void TextureBrowser_constructPreferences( PreferencesPage& page ){
	page.appendCheckBox(
		"", "Texture scrollbar",
		make_property<TextureBrowser_ShowScrollbar>(GlobalTextureBrowser())
		);
	{
		const char* texture_scale[] = { "10%", "25%", "50%", "100%", "200%" };
		page.appendCombo(
			"Texture Thumbnail Scale",
			STRING_ARRAY_RANGE( texture_scale ),
			make_property<TextureScale>(GlobalTextureBrowser())
			);
	}
	page.appendSpinner( "Thumbnails Max Size", GlobalTextureBrowser().m_uniformTextureSize, GlobalTextureBrowser().m_uniformTextureSize, 16, 8192 );
	page.appendSpinner( "Thumbnails Min Size", GlobalTextureBrowser().m_uniformTextureMinSize, GlobalTextureBrowser().m_uniformTextureMinSize, 16, 8192 );
	page.appendEntry( "Mousewheel Increment", GlobalTextureBrowser().m_mouseWheelScrollIncrement );
	{
		const char* startup_shaders[] = { "None", TextureBrowser_getCommonShadersName() };
		page.appendCombo( "Load Shaders at Startup", reinterpret_cast<int&>( GlobalTextureBrowser().m_startupShaders ), STRING_ARRAY_RANGE( startup_shaders ) );
	}
}

void TextureBrowser_constructPage( PreferenceGroup& group ){
	PreferencesPage page( group.createPage( "Texture Browser", "Texture Browser Preferences" ) );
	TextureBrowser_constructPreferences( page );
}

void TextureBrowser_registerPreferencesPage(){
	PreferencesDialog_addSettingsPage( makeCallbackF(TextureBrowser_constructPage) );
}


#include "preferencesystem.h"
#include "stringio.h"


void TextureClipboard_textureSelected( const char* shader );

void TextureBrowser_Construct(){
	TextureBrowser &textureBrowser = GlobalTextureBrowser();

	GlobalCommands_insert( "ShaderInfo", makeCallbackF(TextureBrowser_shaderInfo) );
	GlobalCommands_insert( "ShowUntagged", makeCallbackF(TextureBrowser_showUntagged) );
	GlobalCommands_insert( "AddTag", makeCallbackF(TextureBrowser_addTag) );
	GlobalCommands_insert( "RenameTag", makeCallbackF(TextureBrowser_renameTag) );
	GlobalCommands_insert( "DeleteTag", makeCallbackF(TextureBrowser_deleteTag) );
	GlobalCommands_insert( "CopyTag", makeCallbackF(TextureBrowser_copyTag) );
	GlobalCommands_insert( "PasteTag", makeCallbackF(TextureBrowser_pasteTag) );
	GlobalCommands_insert( "RefreshShaders", makeCallbackF(VFS_Refresh) );
	GlobalToggles_insert( "ShowInUse", makeCallbackF(TextureBrowser_ToggleHideUnused), ToggleItem::AddCallbackCaller( textureBrowser.m_hideunused_item ), Accelerator( 'U' ) );
	GlobalCommands_insert( "ShowAllTextures", makeCallbackF(TextureBrowser_showAll), Accelerator( 'A', (GdkModifierType)GDK_CONTROL_MASK ) );
	GlobalCommands_insert( "ToggleTextures", makeCallbackF(TextureBrowser_toggleShow), Accelerator( 'T' ) );
	GlobalToggles_insert( "ToggleShowShaders", makeCallbackF(TextureBrowser_ToggleShowShaders), ToggleItem::AddCallbackCaller( GlobalTextureBrowser().m_showshaders_item ) );
	GlobalToggles_insert( "ToggleShowTextures", makeCallbackF(TextureBrowser_ToggleShowTextures), ToggleItem::AddCallbackCaller( GlobalTextureBrowser().m_showtextures_item ) );
	GlobalToggles_insert( "ToggleShowShaderlistOnly", makeCallbackF(TextureBrowser_ToggleShowShaderListOnly),
 ToggleItem::AddCallbackCaller( GlobalTextureBrowser().m_showshaderlistonly_item ) );
	GlobalToggles_insert( "FixedSize", makeCallbackF(TextureBrowser_FixedSize), ToggleItem::AddCallbackCaller( GlobalTextureBrowser().m_fixedsize_item ) );
	GlobalToggles_insert( "FilterMissing", makeCallbackF(TextureBrowser_FilterMissing), ToggleItem::AddCallbackCaller( GlobalTextureBrowser().m_filternotex_item ) );
	GlobalToggles_insert( "FilterFallback", makeCallbackF(TextureBrowser_FilterFallback), ToggleItem::AddCallbackCaller( GlobalTextureBrowser().m_hidenotex_item ) );
	GlobalToggles_insert( "EnableAlpha", makeCallbackF(TextureBrowser_EnableAlpha), ToggleItem::AddCallbackCaller( GlobalTextureBrowser().m_enablealpha_item ) );

	GlobalPreferenceSystem().registerPreference( "TextureScale", make_property_string<TextureScale>(textureBrowser) );
	GlobalPreferenceSystem().registerPreference( "UniformTextureSize", make_property_string<UniformTextureSize>(textureBrowser) );
	GlobalPreferenceSystem().registerPreference( "UniformTextureMinSize", make_property_string<UniformTextureMinSize>(textureBrowser) );
	GlobalPreferenceSystem().registerPreference( "TextureScrollbar", make_property_string<TextureBrowser_ShowScrollbar>(textureBrowser));
	GlobalPreferenceSystem().registerPreference( "ShowShaders", make_property_string( textureBrowser.m_showShaders ) );
	GlobalPreferenceSystem().registerPreference( "ShowTextures", make_property_string( GlobalTextureBrowser().m_showTextures ) );
	GlobalPreferenceSystem().registerPreference( "ShowShaderlistOnly", make_property_string( g_TextureBrowser_shaderlistOnly ) );
	GlobalPreferenceSystem().registerPreference( "FixedSize", make_property_string( g_TextureBrowser_fixedSize ) );
	GlobalPreferenceSystem().registerPreference( "FilterMissing", make_property_string( g_TextureBrowser_filterMissing ) );
	GlobalPreferenceSystem().registerPreference( "EnableAlpha", make_property_string( g_TextureBrowser_enableAlpha ) );
	GlobalPreferenceSystem().registerPreference( "LoadShaders", make_property_string( reinterpret_cast<int&>( textureBrowser.m_startupShaders ) ) );
	GlobalPreferenceSystem().registerPreference( "WheelMouseInc", make_property_string( textureBrowser.m_mouseWheelScrollIncrement ) );
	GlobalPreferenceSystem().registerPreference( "SI_Colors0", make_property_string( textureBrowser.color_textureback ) );

	textureBrowser.shader = texdef_name_default();

	Textures_setModeChangedNotify( ReferenceCaller<TextureBrowser, void(), TextureBrowser_queueDraw>( textureBrowser ) );

	TextureBrowser_registerPreferencesPage();

	GlobalShaderSystem().attach( g_ShadersObserver );

	TextureBrowser_textureSelected = TextureClipboard_textureSelected;
}

void TextureBrowser_Destroy(){
	GlobalShaderSystem().detach( g_ShadersObserver );

	Textures_setModeChangedNotify( Callback<void()>() );
}

ui::Widget TextureBrowser_getGLWidget(){
	return GlobalTextureBrowser().m_gl_widget;
}

#if WORKAROUND_WINDOWS_GTK2_GLWIDGET
ui::GLArea TextureBrowser_getGLWidget(){
	return GlobalTextureBrowser().m_gl_widget;
}
#endif // WORKAROUND_WINDOWS_GTK2_GLWIDGET
