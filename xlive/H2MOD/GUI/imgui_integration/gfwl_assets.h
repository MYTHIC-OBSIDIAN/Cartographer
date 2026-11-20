#pragma once

/*
* LAN edition: the Games for Windows - LIVE interface files (images, sounds) from Microsoft's own xlive.dll.
*
* Cartographer replaces xlive.dll in the game folder, so the original's achievement pop-up and Guide are gone. Their
* files are still in the real xlive.dll installed with Games for Windows - LIVE: its XUIZ packages (resource type
* "RT_RCDATA": SKIN, XAM, SHRDRES, GP, HUD, ...). They are read from the player's copy at run time, never shipped:
* - xlive_gfwl.dll in the game folder (a copy of it), else
* - xlive.dll in the system folder (C:\Windows\SysWOW64 on 64-bit Windows)
*/

struct s_gfwl_texture
{
	PDIRECT3DTEXTURE9 texture;
	uint32 width;
	uint32 height;
};

// loads the packages on first use; false when no Games for Windows - LIVE xlive.dll was found
bool gfwl_assets_available(void);

// a file from a package, e.g. ("SKIN", L"toast_bg.png"); names are case insensitive
bool gfwl_assets_get_file(const char* package, const wchar_t* name, const uint8** out_data, uint32* out_size);

// a texture made from a package's PNG (created on first use, NULL when missing)
const s_gfwl_texture* gfwl_assets_get_texture(const char* package, const wchar_t* name);

// plays a package's WAV (asynchronous)
void gfwl_assets_play_sound(const char* package, const wchar_t* name);

// releases the textures (device reset); they are created again on next use
void gfwl_assets_release_textures(void);

// a dynamic default pool texture from PNG data (the game's Direct3D 9Ex device can't fill a plain one), NULL on failure
PDIRECT3DTEXTURE9 imgui_texture_create_from_png(const uint8* png, uint32 png_size, uint32* out_width, uint32* out_height);
