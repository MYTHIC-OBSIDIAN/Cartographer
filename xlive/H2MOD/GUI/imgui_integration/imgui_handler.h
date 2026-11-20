#pragma once

#ifndef IMGUI_DISABLE

#include "input/controllers.h"

struct ImFont;
#include "saved_games/cartographer_player_profile/cartographer_player_profile.h"

/* constants */

extern const char* k_advanced_settings_window_name;
extern const char* k_weapon_offsets_window_name;
extern const char* k_motd_window_name;
extern const char* k_debug_overlay_window_name;
extern const char* k_message_box_window_name;
extern const char* k_home_menu_window_name;

/* enums */

enum e_network_stats_display_type : int32
{
	_network_stats_display_none,
	_network_stats_display_basic,
	_network_stats_display_complete,
};

enum e_imgui_window_type
{
	_imgui_window_weapon_offsets = 0,
	_imgui_window_motd = 1,
	_imgui_window_message_box = 2,
	_imgui_window_advanced_settings = 3,
	_imgui_window_home_menu = 4,
#ifdef TERMINAL_ENABLED
	_imgui_window_console,
#endif
	k_imgui_window_type_count
};

/* globals */

extern s_saved_game_cartographer_player_profile* g_advanced_settings_current_cartographer_profile;
extern e_controller_index g_advanced_settings_current_controller_index;

namespace ImGuiHandler
{
	extern int g_network_stats_overlay;

	enum s_imgui_images
	{
		patch_notes
	};

	enum e_imgui_aspect_ratio : byte
	{
		four_three,
		sixten_nine
	};

	bool ImGuiShouldHandleInput();
	void ImGuiToggleInput(bool state);
	void SetGameInputState(bool enable);
	bool CanDrawImgui();
	void DrawImgui();
	
	bool IsWindowActive(const char* name);
	void ToggleWindow(const char* name);

	void Initalize(HWND hWnd);
	float WidthPercentage(float percent);
	void TextVerticalPad(const char* label);
	bool LoadTextureFromFile(const wchar_t* filename, s_imgui_images image, int* out_width, int* out_height);
	PDIRECT3DTEXTURE9 GetTexture(s_imgui_images image);
	void release_motd_texture();
	e_imgui_aspect_ratio GetAspectRatio(const real_point2d* display_size);
	namespace ImMOTD {
		void Render(bool* p_open);
		void Open();
		void Close();
	}
	namespace ImAdvancedSettings
	{
		void Render(bool* p_open);
		void set_controller_index(e_controller_index controller_index);
		void Open();
		void Close();
	}
	namespace ImMessageBox
	{
		void Render(bool* p_open);
		void SetMessage(const char* message);
		void Open();
		void Close();
	}
	// LAN edition: offline Home (Guide) menu with the achievements, opened with the Home key
	namespace ImHomeMenu
	{
		void Render(bool* p_open);
		void Open();
		void Close();
		// from the game's title data (halo2.exe SPAFILE), 0 when it can't be read
		uint32 GetAchievementGamerscore(int32 achievement_index);
		void ReleaseTextures();
	}
	// LAN edition: the original Games for Windows - LIVE achievement pop-up (drawn every frame while one is queued)
	namespace ImAchievementToast
	{
		void Render();
	}
	// Segoe UI (the Games for Windows - LIVE interface font) when Windows has it, else the default font
	ImFont* GetGfwlFont();
	namespace WeaponOffsets
	{
		void Render(bool* p_open);
		void Open();
		void Close();
	}
}

#endif
