#pragma once

/* enums */

/* TODO: figure out names */
enum e_font_id : int32
{
	_font_id_fixedsys_9 = 0,
	_font_id_1,
	_font_id_2,
	_font_id_3,
	_font_id_4,
	_font_id_5,
	_font_id_6,
	_font_id_7,
	_font_id_8,
	_font_id_9,
	_font_id_10,
	_font_id_11,
	k_public_font_id_count
};

/* prototypes */

void __cdecl font_initialize(void);

void __cdecl font_idle(void);

int32 __cdecl font_get_line_height(e_font_id font_id);

// LAN edition: controller button glyph styles, each one a font table in maps\fonts
enum e_button_glyph_style : int32
{
	_button_glyph_style_original_xbox = 0,	// font_table_xbox.txt
	_button_glyph_style_xbox_360,			// font_table.txt
	_button_glyph_style_modern,				// font_table_modern.txt
	k_button_glyph_style_count
};

// before the fonts are loaded: use the font table for this style
void font_table_apply_patches(e_button_glyph_style style);

// the font table for the Button Glyphs setting, without ".txt" (font_table, font_table_xbox or font_table_modern)
const char* font_table_get_name(void);

// picks the font table without reloading (after the config is read, before the fonts are loaded)
void font_table_select_button_glyph_style(e_button_glyph_style style);

// switches the font table and reloads the fonts
void font_table_set_button_glyph_style(e_button_glyph_style style);
