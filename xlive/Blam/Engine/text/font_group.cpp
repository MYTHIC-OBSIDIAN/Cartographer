#include "stdafx.h"
#include "font_group.h"

/* public code */

void __cdecl font_initialize(void)
{
	INVOKE(0x31DFF, 0x0, font_initialize);	// Not used on dedi
	return;
}

void __cdecl font_idle(void)
{
	INVOKE(0x31B5C, 0x0, font_idle);
	return;
}

int32 __cdecl font_get_line_height(e_font_id font_id)
{
	return INVOKE(0x31865, 0x0, font_get_line_height, font_id);
}

// LAN edition: the controller button glyphs come from the fonts listed in maps\fonts\font_table*.txt,
// so the Button Glyphs video setting picks the font table the game loads

static const char* const k_font_table_names[k_button_glyph_style_count] =
{
	"font_table_xbox",		// Original Xbox
	"font_table",			// Xbox 360 (the game's own)
	"font_table_modern",	// Modern
};

static void __cdecl font_character_cache_dispose(void)
{
	INVOKE(0x8D63C, 0x0, font_character_cache_dispose);
	return;
}

// halo2.exe+0x31B97 builds "<name>[_<language>].txt" from this name (pushed at +0x31BCE and +0x31CA3)
static char g_font_table_name[32] = "font_table";

static bool font_table_file_exists(const char* name)
{
	char path[MAX_PATH];
	if (sprintf_s(path, NUMBEROF(path), "maps\\fonts\\%s.txt", name) < 0)
	{
		return false;
	}
	const DWORD attributes = GetFileAttributesA(path);
	return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

static void font_table_set_name(e_button_glyph_style style)
{
	const char* name = k_font_table_names[_button_glyph_style_xbox_360];
	if (IN_RANGE(style, 0, k_button_glyph_style_count - 1) && font_table_file_exists(k_font_table_names[style]))
	{
		name = k_font_table_names[style];
	}
	else if (style != _button_glyph_style_xbox_360)
	{
		error(_error_log, "maps\\fonts\\%s.txt not found, using font_table.txt", IN_RANGE(style, 0, k_button_glyph_style_count - 1) ? k_font_table_names[style] : "?");
	}
	strncpy_s(g_font_table_name, NUMBEROF(g_font_table_name), name, _TRUNCATE);

	// the game remembers whether the language specific table exists: check again for the new name
	*Memory::GetAddress<int32*>(0x412708) = NONE;
	return;
}

void font_table_apply_patches(e_button_glyph_style style)
{
	font_table_set_name(style);
	WriteValue<uint32>(Memory::GetAddress(0x31BCF), (uint32)g_font_table_name);
	WriteValue<uint32>(Memory::GetAddress(0x31CA4), (uint32)g_font_table_name);
	return;
}

const char* font_table_get_name(void)
{
	return g_font_table_name;
}

void font_table_select_button_glyph_style(e_button_glyph_style style)
{
	font_table_set_name(style);
	return;
}

void font_table_set_button_glyph_style(e_button_glyph_style style)
{
	font_table_set_name(style);

	// reload the fonts: free the character cache (halo2.exe+0x8D63C, rebuilt by font_initialize) so the glyphs
	// already drawn with the old fonts are drawn again, then load the fonts from the new table
	font_character_cache_dispose();
	*Memory::GetAddress<uint8*>(0x47E7C0) = 0;
	font_initialize();
	return;
}
