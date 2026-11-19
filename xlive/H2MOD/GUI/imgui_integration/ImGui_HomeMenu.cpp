#include "stdafx.h"

#include "imgui.h"
#include "imgui_handler.h"

#ifndef IMGUI_DISABLE

#include "achievements/achievement_list.h"
#include "game/game.h"
#include "interface/user_interface_controller.h"
#include "rasterizer/dx9/rasterizer_dx9_main.h"

#include "H2MOD/Modules/Achievements/Achievements.h"

/*
* LAN edition: an offline version of the Games for Windows - LIVE Guide that the Home key opened.
* It shows the achievement player's profile, gamerscore and the 41 achievements with their icons. The gamerscore values
* and icons come from the game's own title data (the SPAFILE resource in halo2.exe, an XDBF file); the names and
* descriptions come from the achievements module, which uses the same table.
*/

/* constants */

const char* k_home_menu_window_name = "home_menu";

enum
{
	k_xdbf_header_size = 24,
	k_xdbf_entry_size = 18,
	k_xdbf_free_entry_size = 8,

	k_xdbf_namespace_metadata = 1,
	k_xdbf_namespace_image = 2,

	k_xach_header_size = 14,
	k_xach_entry_size = 0x24,

	k_title_image_id = 0x8000,

	k_home_menu_icon_size = 64,
};

/* structures */

struct s_home_menu_achievement
{
	bool valid;
	uint32 gamerscore;
	uint32 image_id;
	PDIRECT3DTEXTURE9 icon;
};

/* globals */

static s_home_menu_achievement g_home_menu_achievements[k_achievement_type_count] = {};
static PDIRECT3DTEXTURE9 g_home_menu_title_image = NULL;
static const uint8* g_home_menu_title_data = NULL;
static uint32 g_home_menu_title_data_size = 0;
static bool g_home_menu_title_data_read = false;
static bool g_home_menu_textures_created = false;
static int g_home_menu_filter = 0;

/* private code */

static uint16 xdbf_read_uint16(const uint8* data)
{
	return (uint16)((data[0] << 8) | data[1]);
}

static uint32 xdbf_read_uint32(const uint8* data)
{
	return ((uint32)data[0] << 24) | ((uint32)data[1] << 16) | ((uint32)data[2] << 8) | (uint32)data[3];
}

static uint64 xdbf_read_uint64(const uint8* data)
{
	return ((uint64)xdbf_read_uint32(data) << 32) | xdbf_read_uint32(data + 4);
}

// entry index: namespace, id, data and its size (big-endian XDBF)
static bool xdbf_get_entry(int32 entry_index, uint16* out_namespace, uint64* out_id, const uint8** out_data, uint32* out_size)
{
	const uint8* xdbf = g_home_menu_title_data;
	const uint32 size = g_home_menu_title_data_size;
	if (xdbf == NULL || size < k_xdbf_header_size || csmemcmp(xdbf, "XDBF", 4) != 0)
	{
		return false;
	}

	const uint32 entry_table_length = xdbf_read_uint32(xdbf + 8);
	const uint32 entry_count = xdbf_read_uint32(xdbf + 12);
	const uint32 free_table_length = xdbf_read_uint32(xdbf + 16);
	const uint64 data_start = k_xdbf_header_size + (uint64)entry_table_length * k_xdbf_entry_size + (uint64)free_table_length * k_xdbf_free_entry_size;
	if (entry_index < 0 || (uint32)entry_index >= entry_count || entry_count > entry_table_length || data_start > size)
	{
		return false;
	}

	const uint8* entry = xdbf + k_xdbf_header_size + entry_index * k_xdbf_entry_size;
	const uint64 offset = data_start + xdbf_read_uint32(entry + 10);
	const uint32 length = xdbf_read_uint32(entry + 14);
	if (offset + length > size)
	{
		return false;
	}

	*out_namespace = xdbf_read_uint16(entry);
	*out_id = xdbf_read_uint64(entry + 2);
	*out_data = xdbf + offset;
	*out_size = length;
	return true;
}

static bool xdbf_find(uint16 entry_namespace, uint64 id, const uint8** out_data, uint32* out_size)
{
	uint16 found_namespace;
	uint64 found_id;
	for (int32 entry_index = 0; xdbf_get_entry(entry_index, &found_namespace, &found_id, out_data, out_size); entry_index++)
	{
		if (found_namespace == entry_namespace && found_id == id)
		{
			return true;
		}
	}
	return false;
}

static void home_menu_title_data_read(void)
{
	if (g_home_menu_title_data_read)
	{
		return;
	}
	g_home_menu_title_data_read = true;

	HMODULE module = GetModuleHandleW(NULL);
	HRSRC resource = FindResourceW(module, L"SPAFILE", MAKEINTRESOURCEW(10));	// RT_RCDATA
	HGLOBAL loaded_resource = resource ? LoadResource(module, resource) : NULL;
	if (loaded_resource == NULL)
	{
		return;
	}
	g_home_menu_title_data = (const uint8*)LockResource(loaded_resource);
	g_home_menu_title_data_size = SizeofResource(module, resource);

	// the achievement table (XACH): id, label, description, unachieved description, image id, gamerscore
	uint16 entry_namespace;
	uint64 id;
	const uint8* data;
	uint32 size;
	for (int32 entry_index = 0; xdbf_get_entry(entry_index, &entry_namespace, &id, &data, &size); entry_index++)
	{
		if (entry_namespace != k_xdbf_namespace_metadata || size < k_xach_header_size || csmemcmp(data, "XACH", 4) != 0)
		{
			continue;
		}

		const uint16 count = xdbf_read_uint16(data + 12);
		for (uint16 index = 0; index < count && (uint32)(k_xach_header_size + (index + 1) * k_xach_entry_size) <= size; index++)
		{
			const uint8* achievement = data + k_xach_header_size + index * k_xach_entry_size;
			const int32 achievement_index = (int32)xdbf_read_uint16(achievement) - 1;	// ids start at 1
			if (VALID_INDEX(achievement_index, k_achievement_type_count))
			{
				s_home_menu_achievement* entry = &g_home_menu_achievements[achievement_index];
				entry->valid = true;
				entry->image_id = xdbf_read_uint32(achievement + 8);
				entry->gamerscore = xdbf_read_uint16(achievement + 12);
			}
		}
		break;
	}
	return;
}

static PDIRECT3DTEXTURE9 home_menu_texture_create(uint64 image_id)
{
	const uint8* png;
	uint32 png_size;
	if (!xdbf_find(k_xdbf_namespace_image, image_id, &png, &png_size))
	{
		return NULL;
	}

	PDIRECT3DTEXTURE9 texture = NULL;
	const HRESULT result = D3DXCreateTextureFromFileInMemoryEx(rasterizer_dx9_device_get_interface(), png, png_size,
		D3DX_DEFAULT_NONPOW2, D3DX_DEFAULT_NONPOW2, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
		D3DX_FILTER_NONE, D3DX_FILTER_NONE, 0, NULL, NULL, &texture);
	return result == S_OK ? texture : NULL;
}

static void home_menu_textures_create(void)
{
	if (g_home_menu_textures_created)
	{
		return;
	}
	g_home_menu_textures_created = true;

	home_menu_title_data_read();
	g_home_menu_title_image = home_menu_texture_create(k_title_image_id);
	for (int32 index = 0; index < k_achievement_type_count; index++)
	{
		s_home_menu_achievement* entry = &g_home_menu_achievements[index];
		entry->icon = entry->valid ? home_menu_texture_create(entry->image_id) : NULL;
	}
	return;
}

static void home_menu_textures_release(void)
{
	if (g_home_menu_title_image)
	{
		g_home_menu_title_image->Release();
		g_home_menu_title_image = NULL;
	}
	for (int32 index = 0; index < k_achievement_type_count; index++)
	{
		if (g_home_menu_achievements[index].icon)
		{
			g_home_menu_achievements[index].icon->Release();
			g_home_menu_achievements[index].icon = NULL;
		}
	}
	g_home_menu_textures_created = false;
	return;
}

static ImTextureID home_menu_texture_id(PDIRECT3DTEXTURE9 texture)
{
	return (ImTextureID)(uintptr_t)texture;
}

static void home_menu_draw_achievement(int32 index, bool unlocked)
{
	const s_home_menu_achievement* entry = &g_home_menu_achievements[index];
	const ImVec2 icon_size((float)k_home_menu_icon_size, (float)k_home_menu_icon_size);

	// "title|description"
	char title[128];
	const char* string = achievements_get_string(index);
	const char* divider = strchr(string, '|');
	size_t title_length = divider ? (size_t)(divider - string) : strlen(string);
	if (title_length >= sizeof(title))
	{
		title_length = sizeof(title) - 1;
	}
	csmemcpy(title, string, title_length);
	title[title_length] = '\0';
	const char* description = divider ? divider + 1 : "";

	ImGui::PushID(index);

	// locked achievements: dimmed icon and text
	if (entry->icon)
	{
		const ImVec4 tint = unlocked ? ImVec4(1.f, 1.f, 1.f, 1.f) : ImVec4(0.35f, 0.35f, 0.35f, 0.75f);
		ImGui::Image(home_menu_texture_id(entry->icon), icon_size, ImVec2(0, 0), ImVec2(1, 1), tint);
	}
	else
	{
		ImGui::Dummy(icon_size);
	}

	ImGui::SameLine();
	ImGui::BeginGroup();
	if (!unlocked)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
	}

	ImGui::TextUnformatted(title);
	if (entry->valid)
	{
		char gamerscore[16];
		sprintf_s(gamerscore, "%u G", entry->gamerscore);
		ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - ImGui::CalcTextSize(gamerscore).x);
		ImGui::TextUnformatted(gamerscore);
	}
	ImGui::PushTextWrapPos(0.f);
	ImGui::TextUnformatted(description);
	ImGui::PopTextWrapPos();
	ImGui::TextUnformatted(unlocked ? "Unlocked" : "Locked");

	if (!unlocked)
	{
		ImGui::PopStyleColor();
	}
	ImGui::EndGroup();

	ImGui::PopID();
	ImGui::Separator();
	return;
}

/* public code */

namespace ImGuiHandler
{
	namespace ImHomeMenu
	{
		void Render(bool* p_open)
		{
			bool open = *p_open;
			home_menu_textures_create();

			const e_controller_index controller_index = achievements_get_player_controller();
			const uint64 unlocked = achievements_get_unlocked();

			int32 unlocked_count = 0;
			uint32 gamerscore = 0;
			uint32 gamerscore_total = 0;
			for (int32 index = 0; index < k_achievement_type_count; index++)
			{
				const bool achievement_unlocked = ((unlocked >> index) & 1) != 0;
				gamerscore_total += g_home_menu_achievements[index].gamerscore;
				if (achievement_unlocked)
				{
					unlocked_count++;
					gamerscore += g_home_menu_achievements[index].gamerscore;
				}
			}

			// the window title is the profile name, like the Guide's gamertag
			char player_name[64] = "No profile";
			if (controller_index != k_no_controller)
			{
				const wchar_t* profile_name = user_interface_controller_get_player_profile_name(controller_index);
				if (profile_name != NULL && profile_name[0] != L'\0')
				{
					WideCharToMultiByte(CP_UTF8, 0, profile_name, -1, player_name, sizeof(player_name), NULL, NULL);
				}
			}
			char window_title[96];
			sprintf_s(window_title, "%s###%s", player_name, k_home_menu_window_name);

			const ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(ImVec2(viewport->WorkSize.x * 0.5f, viewport->WorkSize.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
			ImGui::SetNextWindowSize(ImVec2(640, 620), ImGuiCond_Appearing);
			ImGui::SetNextWindowSizeConstraints(ImVec2(480, 400), ImVec2(1920, 1080));
			if (game_is_ui_shell())
			{
				ImGui::SetNextWindowBgAlpha(1);
			}

			if (ImGui::Begin(window_title, &open, ImGuiWindowFlags_NoCollapse))
			{
				// header: the game's tile, gamerscore and progress
				if (g_home_menu_title_image)
				{
					ImGui::Image(home_menu_texture_id(g_home_menu_title_image), ImVec2((float)k_home_menu_icon_size, (float)k_home_menu_icon_size));
					ImGui::SameLine();
				}
				ImGui::BeginGroup();
				ImGui::TextUnformatted("Halo 2");
				ImGui::Text("Gamerscore: %u / %u", gamerscore, gamerscore_total);
				ImGui::Text("Achievements: %d of %d unlocked", unlocked_count, (int32)k_achievement_type_count);
				ImGui::EndGroup();

				char progress[32];
				sprintf_s(progress, "%d%%", (unlocked_count * 100) / (int32)k_achievement_type_count);
				ImGui::ProgressBar((float)unlocked_count / (float)k_achievement_type_count, ImVec2(-1.f, 0.f), progress);
				if (controller_index == k_no_controller)
				{
					ImGui::TextDisabled("Sign in to a profile to save achievements.");
				}

				if (ImGui::CollapsingHeader("Diagnostics"))
				{
					char diagnostics[512];
					achievements_get_diagnostics(diagnostics, sizeof(diagnostics));
					ImGui::TextUnformatted(diagnostics);
				}

				ImGui::Separator();
				ImGui::RadioButton("All", &g_home_menu_filter, 0);
				ImGui::SameLine();
				ImGui::RadioButton("Unlocked", &g_home_menu_filter, 1);
				ImGui::SameLine();
				ImGui::RadioButton("Locked", &g_home_menu_filter, 2);
				ImGui::Separator();

				if (ImGui::BeginChild("achievements", ImVec2(0.f, 0.f), ImGuiChildFlags_None, ImGuiWindowFlags_AlwaysVerticalScrollbar))
				{
					for (int32 index = 0; index < k_achievement_type_count; index++)
					{
						const bool achievement_unlocked = ((unlocked >> index) & 1) != 0;
						if ((g_home_menu_filter == 1 && !achievement_unlocked) || (g_home_menu_filter == 2 && achievement_unlocked))
						{
							continue;
						}
						home_menu_draw_achievement(index, achievement_unlocked);
					}
				}
				ImGui::EndChild();

				// Escape closes it too
				if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyPressed(ImGuiKey_Escape, false))
				{
					open = false;
				}
			}
			ImGui::End();

			if (!open)
			{
				ImGuiHandler::ToggleWindow(k_home_menu_window_name);
			}
			return;
		}

		void Open()
		{
			g_home_menu_filter = 0;
			return;
		}

		void Close()
		{
			home_menu_textures_release();
			return;
		}
	}
}

#endif
