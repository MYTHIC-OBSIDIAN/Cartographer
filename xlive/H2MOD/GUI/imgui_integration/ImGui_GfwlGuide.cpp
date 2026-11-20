#include "stdafx.h"

#include "imgui.h"
#include "imgui_handler.h"

#ifndef IMGUI_DISABLE

#include "gfwl_assets.h"
#include "achievements/achievement_list.h"
#include "interface/user_interface_controller.h"
#include "H2MOD/Modules/Achievements/Achievements.h"

/*
* LAN edition: the original Games for Windows - LIVE Guide's achievement screens, opened with the Home key.
*
* Recreated from the player's xlive.dll (see gfwl_assets.h), on the Guide's 1024x768 canvas:
* - XAM\hudbkgnd.xur: the darkened backdrop, the glass panel (screen.png), the top bar (header_frame.png, the Games for
*   Windows LIVE logo, the clock) and the controller bar, with their ClosedToFull / FullToClosed timelines and the
*   HUD_open / HUD_close sounds. Scenes are hosted at (50, 0).
* - GP\821-2_GameAchievStatsMe.xur: the achievement list (the btn_Achievements rows of SKIN\skin.xur)
* - GP\828_AchievDetails.xur: one achievement's details
* Layout, images, text colours and sizes are the scenes' own. Text is the original's (GP strings): "Unlocked %s",
* "Locked", "Achievements sorted by date" / "...by gamerscore", "Change Sort Order", "Achievement Description".
*/

/* constants */

static const float k_guide_canvas_width = 1024.f;
static const float k_guide_canvas_height = 768.f;
static const float k_guide_scene_x = 50.f;						// AppHostElementId
static const float k_guide_point_to_pixel = 1.55f;				// XUI point size to canvas pixels (Segoe UI)

enum
{
	k_guide_frames_per_second = 60,
	k_guide_list_visible_rows = 8,
	k_guide_controller_repeat_delay_ms = 400,
	k_guide_controller_repeat_ms = 100,
};

// hudbkgnd.xur timeline frames
static const float k_guide_open_start = 136.f;		// ClosedToFull
static const float k_guide_open_end = 165.f;
static const float k_guide_close_start = 166.f;		// FullToClosed
static const float k_guide_close_end = 210.f;
static const float k_guide_close_sound = 181.f;

// text colours (ARGB in the skin)
static const ImU32 k_guide_text_blue = IM_COL32(0x16, 0x43, 0x6C, 0xFF);
static const ImU32 k_guide_text_navy = IM_COL32(0x05, 0x20, 0x38, 0xFF);
static const ImU32 k_guide_text_gray = IM_COL32(0x60, 0x60, 0x60, 0xFF);
static const ImU32 k_guide_text_white = IM_COL32(0xFF, 0xFF, 0xFF, 0xFF);

enum e_guide_state
{
	_guide_state_closed = 0,
	_guide_state_opening,
	_guide_state_open,
	_guide_state_closing,
};

enum e_guide_screen
{
	_guide_screen_list = 0,
	_guide_screen_details,
};

enum e_guide_sort
{
	_guide_sort_date = 0,
	_guide_sort_gamerscore,
};

enum e_guide_align
{
	_guide_align_left = 0,
	_guide_align_right,
	_guide_align_center,
};

/* globals */

static e_guide_state g_guide_state = _guide_state_closed;
static double g_guide_state_time = 0.0;
static bool g_guide_close_sound_played = false;
static e_guide_screen g_guide_screen = _guide_screen_list;
static e_guide_sort g_guide_sort = _guide_sort_date;
static int32 g_guide_selection = 0;		// position in the sorted list
static int32 g_guide_scroll = 0;		// first visible row

static ImDrawList* g_guide_draw_list = NULL;
static ImVec2 g_guide_origin;
static float g_guide_scale = 1.f;
static float g_guide_alpha = 1.f;

// controller
typedef DWORD(WINAPI* t_guide_xinput_get_state)(DWORD, XINPUT_STATE*);
static WORD g_guide_controller_buttons[XUSER_MAX_COUNT] = {};
static ULONGLONG g_guide_controller_repeat_time = 0;
static int32 g_guide_controller_repeat_direction = 0;

/* private code */

static ImVec2 guide_point(float x, float y)
{
	return ImVec2(g_guide_origin.x + x * g_guide_scale, g_guide_origin.y + y * g_guide_scale);
}

static ImU32 guide_color(ImU32 color, float opacity)
{
	const float alpha = ((color >> IM_COL32_A_SHIFT) & 0xFF) * opacity * g_guide_alpha;
	const int32 value = (int32)(alpha + .5f);
	return (color & ~IM_COL32_A_MASK) | ((ImU32)(value < 0 ? 0 : (value > 255 ? 255 : value)) << IM_COL32_A_SHIFT);
}

static ImTextureID guide_texture_id(PDIRECT3DTEXTURE9 texture)
{
	return (ImTextureID)(uintptr_t)texture;
}

// an XuiImage: the texture at its own size from the element's top left, clipped to the element
static void guide_image(const char* package, const wchar_t* name, float x, float y, float width, float height)
{
	const s_gfwl_texture* texture = gfwl_assets_get_texture(package, name);
	if (texture == NULL)
	{
		return;
	}
	g_guide_draw_list->PushClipRect(guide_point(x, y), guide_point(x + width, y + height), true);
	g_guide_draw_list->AddImage(guide_texture_id(texture->texture), guide_point(x, y),
		guide_point(x + texture->width, y + texture->height), ImVec2(0, 0), ImVec2(1, 1), guide_color(IM_COL32_WHITE, 1.f));
	g_guide_draw_list->PopClipRect();
	return;
}

// an image presenter: the texture stretched to the element
static void guide_image_stretched(PDIRECT3DTEXTURE9 texture, float x, float y, float width, float height)
{
	if (texture == NULL)
	{
		return;
	}
	g_guide_draw_list->AddImage(guide_texture_id(texture), guide_point(x, y), guide_point(x + width, y + height),
		ImVec2(0, 0), ImVec2(1, 1), guide_color(IM_COL32_WHITE, 1.f));
	return;
}

static void guide_image_stretched(const char* package, const wchar_t* name, float x, float y, float width, float height)
{
	const s_gfwl_texture* texture = gfwl_assets_get_texture(package, name);
	if (texture != NULL)
	{
		guide_image_stretched(texture->texture, x, y, width, height);
	}
	return;
}

// an XuiNineGrid
static void guide_nine_grid(const char* package, const wchar_t* name, float x, float y, float width, float height, float margin)
{
	const s_gfwl_texture* texture = gfwl_assets_get_texture(package, name);
	if (texture == NULL)
	{
		return;
	}
	const float xs[4] = { x, x + margin, x + width - margin, x + width };
	const float ys[4] = { y, y + margin, y + height - margin, y + height };
	const float us[4] = { 0.f, margin / texture->width, 1.f - margin / texture->width, 1.f };
	const float vs[4] = { 0.f, margin / texture->height, 1.f - margin / texture->height, 1.f };
	for (int32 row = 0; row < 3; row++)
	{
		for (int32 column = 0; column < 3; column++)
		{
			g_guide_draw_list->AddImage(guide_texture_id(texture->texture), guide_point(xs[column], ys[row]), guide_point(xs[column + 1], ys[row + 1]),
				ImVec2(us[column], vs[row]), ImVec2(us[column + 1], vs[row + 1]), guide_color(IM_COL32_WHITE, 1.f));
		}
	}
	return;
}

// an XuiTextPresenter: single line text centred vertically, or wrapped text from the top
static void guide_text(const char* text, float x, float y, float width, float height, float point_size, ImU32 color,
	e_guide_align align = _guide_align_left, bool wrap = false)
{
	ImFont* font = ImGuiHandler::GetGfwlFont();
	const float size = point_size * k_guide_point_to_pixel * g_guide_scale;
	const float wrap_width = wrap ? width * g_guide_scale : 0.f;
	const ImVec2 text_size = font->CalcTextSizeA(size, FLT_MAX, wrap_width, text);

	ImVec2 position = guide_point(x, y);
	if (!wrap)
	{
		position.y += (height * g_guide_scale - text_size.y) * .5f;
		if (align == _guide_align_right)
		{
			position.x += width * g_guide_scale - text_size.x;
		}
		else if (align == _guide_align_center)
		{
			position.x += (width * g_guide_scale - text_size.x) * .5f;
		}
	}
	const ImVec4 clip(guide_point(x, y).x, guide_point(x, y).y, guide_point(x + width, y + height).x, guide_point(x + width, y + height).y);
	g_guide_draw_list->AddText(font, size, position, guide_color(color, 1.f), text, NULL, wrap_width, &clip);
	return;
}

static bool guide_point_inside(ImVec2 point, float x, float y, float width, float height)
{
	const ImVec2 minimum = guide_point(x, y);
	const ImVec2 maximum = guide_point(x + width, y + height);
	return point.x >= minimum.x && point.x < maximum.x && point.y >= minimum.y && point.y < maximum.y;
}

static float guide_ease(float t)
{
	return 1.f - (1.f - t) * (1.f - t);
}

// a value moving from one key to the next (eased or linear), held outside them
static float guide_key(float frame, float frame_from, float from, float frame_to, float to, bool ease)
{
	if (frame <= frame_from)
	{
		return from;
	}
	if (frame >= frame_to)
	{
		return to;
	}
	float t = (frame - frame_from) / (frame_to - frame_from);
	return from + (to - from) * (ease ? guide_ease(t) : t);
}

// the hudbkgnd.xur frame now
static float guide_frame(void)
{
	const float elapsed = (float)((ImGui::GetTime() - g_guide_state_time) * k_guide_frames_per_second);
	switch (g_guide_state)
	{
	case _guide_state_opening:
		return k_guide_open_start + elapsed;
	case _guide_state_closing:
		return k_guide_close_start + elapsed;
	default:
		return k_guide_open_end;
	}
}

static void guide_state_set(e_guide_state state)
{
	g_guide_state = state;
	g_guide_state_time = ImGui::GetTime();
	return;
}

static void guide_sound(const char* package, const wchar_t* name)
{
	gfwl_assets_play_sound(package, name);
	return;
}

// the achievements in the chosen order: by date (most recent first, then ones with no date, then locked ones in list
// order) or by gamerscore (highest first)
static void guide_order_get(int32 order[k_achievement_type_count])
{
	const uint64 unlocked = achievements_get_unlocked();
	for (int32 index = 0; index < k_achievement_type_count; index++)
	{
		order[index] = index;
	}

	auto before = [unlocked](int32 a, int32 b) -> bool
	{
		if (g_guide_sort == _guide_sort_gamerscore)
		{
			const uint32 score_a = ImGuiHandler::ImHomeMenu::GetAchievementGamerscore(a);
			const uint32 score_b = ImGuiHandler::ImHomeMenu::GetAchievementGamerscore(b);
			return score_a != score_b ? score_a > score_b : a < b;
		}
		const bool unlocked_a = ((unlocked >> a) & 1) != 0;
		const bool unlocked_b = ((unlocked >> b) & 1) != 0;
		if (unlocked_a != unlocked_b)
		{
			return unlocked_a;
		}
		const uint64 time_a = unlocked_a ? achievements_get_unlock_time(a) : 0;
		const uint64 time_b = unlocked_b ? achievements_get_unlock_time(b) : 0;
		return time_a != time_b ? time_a > time_b : a < b;
	};

	// insertion sort (41 items)
	for (int32 i = 1; i < k_achievement_type_count; i++)
	{
		const int32 value = order[i];
		int32 j = i - 1;
		while (j >= 0 && before(value, order[j]))
		{
			order[j + 1] = order[j];
			j--;
		}
		order[j + 1] = value;
	}
	return;
}

// "title|description"
static void guide_achievement_text(int32 index, char* title, size_t title_size, const char** description)
{
	const char* string = achievements_get_string(index);
	const char* divider = strchr(string, '|');
	size_t length = divider ? (size_t)(divider - string) : strlen(string);
	if (length >= title_size)
	{
		length = title_size - 1;
	}
	csmemcpy(title, string, length);
	title[length] = '\0';
	*description = divider ? divider + 1 : "";
	return;
}

// "Unlocked <date>", "Unlocked" when the date isn't known, or "Locked"
static void guide_unlock_text(int32 index, bool unlocked, char* text, size_t text_size)
{
	if (!unlocked)
	{
		strcpy_s(text, text_size, "Locked");
		return;
	}

	const uint64 time = achievements_get_unlock_time(index);
	FILETIME utc = { (DWORD)time, (DWORD)(time >> 32) };
	FILETIME local;
	SYSTEMTIME system_time;
	wchar_t date[64];
	char date_utf8[128];
	if (time != 0
		&& FileTimeToLocalFileTime(&utc, &local)
		&& FileTimeToSystemTime(&local, &system_time)
		&& GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, DATE_SHORTDATE, &system_time, NULL, date, (int)NUMBEROF(date), NULL) != 0
		&& WideCharToMultiByte(CP_UTF8, 0, date, -1, date_utf8, sizeof(date_utf8), NULL, NULL) != 0)
	{
		sprintf_s(text, text_size, "Unlocked %s", date_utf8);
	}
	else
	{
		strcpy_s(text, text_size, "Unlocked");
	}
	return;
}

static void guide_selection_set(int32 selection)
{
	selection = selection < 0 ? 0 : (selection >= k_achievement_type_count ? k_achievement_type_count - 1 : selection);
	if (selection != g_guide_selection)
	{
		g_guide_selection = selection;
		guide_sound("SHRDRES", L"btn_Focus.wav");
	}
	if (g_guide_selection < g_guide_scroll)
	{
		g_guide_scroll = g_guide_selection;
	}
	else if (g_guide_selection >= g_guide_scroll + k_guide_list_visible_rows)
	{
		g_guide_scroll = g_guide_selection - k_guide_list_visible_rows + 1;
	}
	return;
}

static void guide_close(void)
{
	if (g_guide_state == _guide_state_opening || g_guide_state == _guide_state_open)
	{
		g_guide_close_sound_played = false;
		guide_state_set(_guide_state_closing);
	}
	return;
}

// A, B, Y and up / down from the keyboard, the mouse wheel and the controllers
struct s_guide_input
{
	bool accept;
	bool back;
	bool sort;
	int32 move;
};

static t_guide_xinput_get_state guide_xinput_get_state(void)
{
	static t_guide_xinput_get_state xinput_get_state = NULL;
	static bool looked_up = false;
	if (!looked_up)
	{
		looked_up = true;
		const wchar_t* const modules[] = { L"xinput1_4.dll", L"xinput9_1_0.dll", L"xinput1_3.dll" };
		for (uint32 index = 0; index < NUMBEROF(modules) && xinput_get_state == NULL; index++)
		{
			HMODULE module = GetModuleHandleW(modules[index]);
			if (module == NULL)
			{
				module = LoadLibraryW(modules[index]);
			}
			if (module != NULL)
			{
				xinput_get_state = (t_guide_xinput_get_state)GetProcAddress(module, "XInputGetState");
			}
		}
	}
	return xinput_get_state;
}

static void guide_input_get(s_guide_input* input)
{
	*input = {};
	const ImGuiIO& io = ImGui::GetIO();

	input->accept = ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false) || ImGui::IsKeyPressed(ImGuiKey_Space, false);
	input->back = ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsKeyPressed(ImGuiKey_Backspace, false) || ImGui::IsMouseClicked(ImGuiMouseButton_Right);
	input->sort = ImGui::IsKeyPressed(ImGuiKey_Y, false);
	input->move += ImGui::IsKeyPressed(ImGuiKey_DownArrow, true) ? 1 : 0;
	input->move -= ImGui::IsKeyPressed(ImGuiKey_UpArrow, true) ? 1 : 0;
	input->move += ImGui::IsKeyPressed(ImGuiKey_PageDown, true) ? k_guide_list_visible_rows : 0;
	input->move -= ImGui::IsKeyPressed(ImGuiKey_PageUp, true) ? k_guide_list_visible_rows : 0;
	if (io.MouseWheel != 0.f)
	{
		input->move -= io.MouseWheel > 0.f ? 1 : -1;
	}

	t_guide_xinput_get_state xinput_get_state = guide_xinput_get_state();
	if (xinput_get_state == NULL)
	{
		return;
	}

	int32 direction = 0;
	for (DWORD user_index = 0; user_index < XUSER_MAX_COUNT; user_index++)
	{
		XINPUT_STATE state = {};
		if (xinput_get_state(user_index, &state) != ERROR_SUCCESS)
		{
			g_guide_controller_buttons[user_index] = 0;
			continue;
		}
		const WORD buttons = state.Gamepad.wButtons;
		const WORD pressed = buttons & ~g_guide_controller_buttons[user_index];
		g_guide_controller_buttons[user_index] = buttons;

		input->accept |= (pressed & XINPUT_GAMEPAD_A) != 0;
		input->back |= (pressed & XINPUT_GAMEPAD_B) != 0;
		input->sort |= (pressed & XINPUT_GAMEPAD_Y) != 0;

		if ((buttons & XINPUT_GAMEPAD_DPAD_DOWN) || state.Gamepad.sThumbLY < -16000)
		{
			direction = 1;
		}
		else if ((buttons & XINPUT_GAMEPAD_DPAD_UP) || state.Gamepad.sThumbLY > 16000)
		{
			direction = -1;
		}
	}

	// held up / down repeats
	const ULONGLONG now = GetTickCount64();
	if (direction == 0)
	{
		g_guide_controller_repeat_direction = 0;
	}
	else if (direction != g_guide_controller_repeat_direction)
	{
		g_guide_controller_repeat_direction = direction;
		g_guide_controller_repeat_time = now + k_guide_controller_repeat_delay_ms;
		input->move += direction;
	}
	else if (now >= g_guide_controller_repeat_time)
	{
		g_guide_controller_repeat_time = now + k_guide_controller_repeat_ms;
		input->move += direction;
	}
	return;
}

// the scene header ("Label_txt_scene_header"): white text on scene_header.png
static void guide_draw_scene_header(float scene_y, const char* text)
{
	guide_image("SKIN", L"scene_header.png", k_guide_scene_x + 4.f, scene_y + 37.f, 914.f, 64.f);
	guide_text(text, k_guide_scene_x + 5.f + 32.f, scene_y + 37.f + 18.f, 624.f, 22.f, 10.f, k_guide_text_white);
	return;
}

static void guide_draw_list(float scene_y, const s_guide_input* input)
{
	const float x = k_guide_scene_x;
	const uint64 unlocked = achievements_get_unlocked();
	int32 order[k_achievement_type_count];
	guide_order_get(order);

	int32 unlocked_count = 0;
	for (int32 index = 0; index < k_achievement_type_count; index++)
	{
		unlocked_count += ((unlocked >> index) & 1) != 0 ? 1 : 0;
	}

	guide_draw_scene_header(scene_y, "Halo 2");
	guide_image("SKIN", L"screen_full_basic.png", x + 6.f, scene_y + 83.f, 910.f, 450.f);

	char count[64];
	sprintf_s(count, "%d of %d Unlocked", unlocked_count, (int32)k_achievement_type_count);
	guide_text(count, x + 29.f, scene_y + 99.f, 380.f, 24.f, 11.f, k_guide_text_gray);
	guide_text(g_guide_sort == _guide_sort_date ? "Achievements sorted by date" : "Achievements sorted by gamerscore",
		x + 291.f, scene_y + 100.f, 354.f, 33.f, 10.f, k_guide_text_gray, _guide_align_right);

	// the Y button (btn_ListHeadPosLast)
	const ImVec2 mouse = ImGui::GetIO().MousePos;
	const bool sort_hovered = guide_point_inside(mouse, x + 652.f, scene_y + 94.f, 255.f, 34.f);
	guide_nine_grid("SKIN", sort_hovered ? L"btn_list_header_posE_focus.png" : L"btn_list_header_posE_normal.png", x + 652.f, scene_y + 94.f, 255.f, 34.f, 5.f);
	guide_image_stretched("SKIN", sort_hovered ? L"Ybutton_Focus.png" : L"Ybutton_Normal.png", x + 652.f + 3.f, scene_y + 94.f + 3.f, 28.f, 28.f);
	guide_text("Change Sort Order", x + 652.f + 36.f, scene_y + 94.f, 210.f, 34.f, 10.f, k_guide_text_blue);

	// rows (ListAchievements / btn_Achievements): 870x42, 43 apart, from (2, 2) in the 874x351 list at (17, 129)
	const float list_x = x + 17.f + 2.f;
	const float list_y = scene_y + 129.f + 2.f;
	int32 hovered = NONE;
	for (int32 row = 0; row < k_guide_list_visible_rows; row++)
	{
		const int32 position = g_guide_scroll + row;
		if (position >= k_achievement_type_count)
		{
			break;
		}
		const int32 index = order[position];
		const bool achievement_unlocked = ((unlocked >> index) & 1) != 0;
		const float row_y = list_y + row * 43.f;
		if (guide_point_inside(mouse, list_x, row_y, 870.f, 42.f))
		{
			hovered = position;
		}

		guide_image("SKIN", position == g_guide_selection ? L"btn_list_focus.png" : L"btn_list_normal.png", list_x, row_y, 870.f, 42.f);

		char title[128];
		const char* description;
		guide_achievement_text(index, title, sizeof(title), &description);
		guide_text(title, list_x + 20.f, row_y + 1.f, 430.f, 40.f, 12.f, k_guide_text_blue);

		char status[96];
		guide_unlock_text(index, achievement_unlocked, status, sizeof(status));
		guide_text(status, list_x + 480.f, row_y + 1.f, 200.f, 40.f, 10.f, k_guide_text_blue);

		char gamerscore[16];
		sprintf_s(gamerscore, "%u", ImGuiHandler::ImHomeMenu::GetAchievementGamerscore(index));
		guide_text(gamerscore, list_x + 685.f, row_y + 9.f, 63.f, 27.f, 12.f, k_guide_text_navy, _guide_align_right);
		guide_image_stretched("SHRDRES", L"GScore.png", list_x + 759.f, row_y + 5.f, 32.f, 32.f);

		if (achievement_unlocked)
		{
			guide_image_stretched(ImGuiHandler::ImHomeMenu::GetAchievementIcon(index), list_x + 801.f, row_y + 5.f, 32.f, 32.f);
		}
		else
		{
			guide_image_stretched("SHRDRES", L"unearnedAchievement.png", list_x + 801.f, row_y + 5.f, 32.f, 32.f);
		}
	}

	// scroll position (the list's scroll bar track, 16 wide at 874)
	if (k_achievement_type_count > k_guide_list_visible_rows)
	{
		const float track_y = scene_y + 129.f + 16.f;
		const float track_height = 318.f;
		const float thumb_height = track_height * k_guide_list_visible_rows / k_achievement_type_count;
		const float thumb_y = track_y + (track_height - thumb_height) * g_guide_scroll / (k_achievement_type_count - k_guide_list_visible_rows);
		g_guide_draw_list->AddRectFilled(guide_point(x + 17.f + 878.f, thumb_y), guide_point(x + 17.f + 886.f, thumb_y + thumb_height),
			guide_color(IM_COL32(0x16, 0x43, 0x6C, 0x90), 1.f), 3.f * g_guide_scale);
	}

	if (g_guide_state != _guide_state_open)
	{
		return;
	}

	// input
	if (ImGui::GetIO().MouseDelta.x != 0.f || ImGui::GetIO().MouseDelta.y != 0.f)
	{
		if (hovered != NONE)
		{
			guide_selection_set(hovered);
		}
	}
	if (input->move != 0)
	{
		guide_selection_set(g_guide_selection + input->move);
	}
	if (input->sort || (sort_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)))
	{
		g_guide_sort = g_guide_sort == _guide_sort_date ? _guide_sort_gamerscore : _guide_sort_date;
		guide_sound("SHRDRES", L"btn_Select.wav");
	}
	if (input->accept || (hovered != NONE && ImGui::IsMouseClicked(ImGuiMouseButton_Left)))
	{
		if (hovered != NONE && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			g_guide_selection = hovered;
		}
		g_guide_screen = _guide_screen_details;
		guide_sound("SHRDRES", L"btn_Select.wav");
	}
	else if (input->back)
	{
		guide_sound("SHRDRES", L"btn_Back.wav");
		guide_close();
	}
	return;
}

static void guide_draw_details(float scene_y, const s_guide_input* input)
{
	const float x = k_guide_scene_x;
	int32 order[k_achievement_type_count];
	guide_order_get(order);
	const int32 index = order[g_guide_selection];
	const bool unlocked = ((achievements_get_unlocked() >> index) & 1) != 0;

	char title[128];
	const char* description;
	guide_achievement_text(index, title, sizeof(title), &description);

	guide_draw_scene_header(scene_y, "Halo 2");
	guide_image("SKIN", L"screen_full_basic.png", x + 6.f, scene_y + 83.f, 910.f, 450.f);

	// title and lock state on the blue pane (metaTipBlueAchievements, text from (14, 22))
	guide_nine_grid("SKIN", L"meta_tip_blue.png", x + 28.f, scene_y + 142.f, 638.f, 86.f, 10.f);
	guide_text(title, x + 28.f + 14.f, scene_y + 142.f + 14.f, 638.f - 26.f, 34.f, 14.f, k_guide_text_white);
	char status[96];
	guide_unlock_text(index, unlocked, status, sizeof(status));
	guide_text(status, x + 44.f, scene_y + 194.f, 620.f, 35.f, 11.f, k_guide_text_white);

	// gamerscore pane (screen9GridBlue) with the G and the achievement's picture (btn_image64)
	guide_nine_grid("SKIN", L"meta_tip_blue.png", x + 673.f, scene_y + 142.f, 206.f, 86.f, 10.f);
	char gamerscore[16];
	sprintf_s(gamerscore, "%u", ImGuiHandler::ImHomeMenu::GetAchievementGamerscore(index));
	guide_text(gamerscore, x + 684.f, scene_y + 174.f, 65.f, 28.f, 12.f, k_guide_text_white, _guide_align_right);
	guide_image_stretched("SHRDRES", L"GScore.png", x + 758.f, scene_y + 170.f, 32.f, 32.f);
	guide_image("SKIN", L"btn_gamertile_normal.png", x + 796.f, scene_y + 146.f, 80.f, 80.f);
	if (unlocked)
	{
		guide_image_stretched(ImGuiHandler::ImHomeMenu::GetAchievementIcon(index), x + 796.f + 7.f, scene_y + 146.f + 7.f, 64.f, 64.f);
	}
	else
	{
		guide_image_stretched("SHRDRES", L"unearnedAchievement.png", x + 796.f + 7.f, scene_y + 146.f + 7.f, 64.f, 64.f);
	}

	guide_text("Achievement Description", x + 33.f, scene_y + 242.f, 665.f, 25.f, 10.f, k_guide_text_gray);
	guide_text(description, x + 32.f, scene_y + 270.f, 850.f, 190.f, 11.f, k_guide_text_gray, _guide_align_left, true);

	if (g_guide_state != _guide_state_open)
	{
		return;
	}

	// up / down move between achievements here too; B goes back to the list
	if (input->move != 0)
	{
		guide_selection_set(g_guide_selection + input->move);
	}
	if (input->back)
	{
		g_guide_screen = _guide_screen_list;
		guide_sound("SHRDRES", L"btn_Back.wav");
	}
	return;
}

/* public code */

namespace ImGuiHandler
{
	namespace ImGfwlGuide
	{
		bool Available()
		{
			return gfwl_assets_available();
		}

		void Open()
		{
			g_guide_screen = _guide_screen_list;
			g_guide_selection = 0;
			g_guide_scroll = 0;
			csmemset(g_guide_controller_buttons, 0xFF, sizeof(g_guide_controller_buttons));	// ignore buttons already held
			guide_state_set(_guide_state_opening);
			guide_sound("XAM", L"HUD_open.wav");
			return;
		}

		bool Toggle()
		{
			if (g_guide_state == _guide_state_opening || g_guide_state == _guide_state_open)
			{
				guide_close();
				return true;
			}
			return false;
		}

		// false once it has finished closing
		bool Render()
		{
			if (g_guide_state == _guide_state_closed)
			{
				return false;
			}

			float frame = guide_frame();
			if (g_guide_state == _guide_state_opening && frame >= k_guide_open_end)
			{
				guide_state_set(_guide_state_open);
				frame = k_guide_open_end;
			}
			if (g_guide_state == _guide_state_closing)
			{
				if (!g_guide_close_sound_played && frame >= k_guide_close_sound)
				{
					g_guide_close_sound_played = true;
					guide_sound("XAM", L"HUD_close.wav");
				}
				if (frame >= k_guide_close_end)
				{
					g_guide_state = _guide_state_closed;
					return false;
				}
			}

			// hudbkgnd.xur ClosedToFull (136-165) / FullToClosed (166-210)
			const bool closing = g_guide_state == _guide_state_closing;
			const float darken = closing ? guide_key(frame, 195.f, 1.f, 210.f, 0.f, false) : guide_key(frame, 136.f, 0.f, 150.f, 1.f, false);
			const float bar_y = closing ? guide_key(frame, 195.f, -1.f, 210.f, -44.f, true) : guide_key(frame, 136.f, -44.f, 150.f, -1.f, true);
			const float controller_y = closing ? guide_key(frame, 195.f, -2.f, 210.f, -45.f, true) : guide_key(frame, 136.f, -45.f, 150.f, -2.f, true);
			const float panel_y = closing ? guide_key(frame, 181.f, 12.f, 210.f, -771.f, true) : guide_key(frame, 136.f, -771.f, 165.f, 12.f, true);

			const ImGuiIO& io = ImGui::GetIO();
			g_guide_scale = io.DisplaySize.x / k_guide_canvas_width < io.DisplaySize.y / k_guide_canvas_height
				? io.DisplaySize.x / k_guide_canvas_width : io.DisplaySize.y / k_guide_canvas_height;
			g_guide_origin = ImVec2((io.DisplaySize.x - k_guide_canvas_width * g_guide_scale) * .5f, (io.DisplaySize.y - k_guide_canvas_height * g_guide_scale) * .5f);
			g_guide_alpha = 1.f;

			// a full screen window takes the input while the Guide is up
			ImGui::SetNextWindowPos(ImVec2(0.f, 0.f));
			ImGui::SetNextWindowSize(io.DisplaySize);
			ImGui::Begin("##gfwl_guide", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings
				| ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus);
			g_guide_draw_list = ImGui::GetWindowDrawList();

			// darken (a dark figure over the whole screen)
			g_guide_draw_list->AddRectFilled(ImVec2(0.f, 0.f), io.DisplaySize, guide_color(IM_COL32(0x0F, 0x0F, 0x0F, 0x80), darken));

			// the glass panel, and the scene on it (scenes are hosted at (50, 0) and move with the panel)
			guide_image("XAM", L"screen.png", 0.f, panel_y, 1024.f, 768.f);
			s_guide_input input;
			guide_input_get(&input);
			const float scene_y = panel_y - 12.f;
			if (g_guide_screen == _guide_screen_list)
			{
				guide_draw_list(scene_y, &input);
			}
			else
			{
				guide_draw_details(scene_y, &input);
			}

			// top bar: frame, Games for Windows LIVE logo, clock and close button
			guide_image("XAM", L"header_frame.png", 0.f, bar_y, 1024.f, 64.f);
			guide_image_stretched("XAM", L"header_logo.png", 421.f, bar_y + 2.f, 176.f, 43.f);
			guide_image("XAM", L"screen_time.png", 822.f, bar_y, 128.f, 32.f);
			wchar_t time_wide[32];
			char time_text[64];
			if (GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, TIME_NOSECONDS, NULL, NULL, time_wide, (int)NUMBEROF(time_wide)) != 0
				&& WideCharToMultiByte(CP_UTF8, 0, time_wide, -1, time_text, sizeof(time_text), NULL, NULL) != 0)
			{
				guide_text(time_text, 842.f, bar_y, 90.f, 22.f, 9.f, k_guide_text_blue, _guide_align_center);
			}
			const bool close_hovered = guide_point_inside(io.MousePos, 935.f, bar_y, 46.f, 18.f);
			guide_image_stretched("SKIN", L"closex.png", 935.f + 13.f, bar_y + 0.f, 19.f, 19.f);

			// controller bar
			guide_image("XAM", L"header_controller.png", 138.f, controller_y, 90.f, 38.f);

			ImGui::End();

			if (g_guide_state == _guide_state_open && close_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			{
				guide_sound("SHRDRES", L"btn_Back.wav");
				guide_close();
			}
			return true;
		}

		void Close()
		{
			g_guide_state = _guide_state_closed;
			return;
		}
	}
}

#endif
