#include "stdafx.h"

#include "imgui.h"
#include "imgui_handler.h"

#ifndef IMGUI_DISABLE

#include "gfwl_assets.h"
#include "H2MOD/Modules/Achievements/Achievements.h"

/*
* LAN edition: the original Games for Windows - LIVE achievement pop-up.
*
* Recreated from the player's xlive.dll: the visual scr_Notification in SKIN\skin.xur (the scene XAM\notify.xur hosts
* it) with its images, the NotifyPopup.wav sound, and its timeline (named frames TransTo 0, loop 81, EndTransTo 194
* -> loop, TransFrom 195, EndTransFrom 247; XUI timelines run at 60 frames a second). The text is the original's
* "Achievement unlocked\n%sG - %s" (XAM\Strings.xus).
*/

/* constants */

enum
{
	k_toast_frames_per_second = 60,
	k_toast_frame_loop = 81,
	k_toast_frame_loop_end = 194,		// EndTransTo: goto loop
	k_toast_frame_out = 195,			// TransFrom
	k_toast_frame_out_end = 247,		// EndTransFrom
	k_toast_loops = 2,					// times through the loop (the trophy / Home key) before going out
	k_toast_sound_frame = 43,
};

// the visual's design size (scr_Notification)
static const float k_toast_width = 361.f;
static const float k_toast_height = 70.f;

enum e_toast_interpolation
{
	_toast_interpolation_linear = 0,
	_toast_interpolation_none,			// hold until the next key
	_toast_interpolation_ease,			// ease in 0, out 100, scale 50
};

/* structures */

struct s_toast_key
{
	uint16 frame;
	uint8 interpolation;
	bool show;
	float opacity;
	float scale_x;
	float scale_y;
};

struct s_toast_element
{
	float x, y, width, height;
	float pivot_x, pivot_y;
	const s_toast_key* keys;
	uint32 key_count;
};

/* keyframes (skin.xur, scr_Notification) */

// XuiNineGrid1: the white glass panel (toast_bg.png, margins 15)
static const s_toast_key k_toast_background_keys[] =
{
	{ 0, 1, false, 1.f, 1.f, 1.f },
	{ 33, 0, true, .3f, .15f, 1.f },
	{ 48, 0, true, 1.f, 1.05f, 1.f },
	{ 52, 1, true, 1.f, 1.f, 1.f },
	{ 81, 1, true, 1.f, 1.f, 1.f },
	{ 220, 0, true, 1.f, 1.f, 1.f },
	{ 229, 0, true, .3f, .17f, 1.f },
	{ 235, 0, false, 0.f, .03f, 1.f },
};

// border: the orange glass square (toast_frame.png, margins 5)
static const s_toast_key k_toast_border_keys[] =
{
	{ 0, 1, false, 1.f, 1.f, 1.f },
	{ 19, 2, true, 0.f, .1f, .1f },
	{ 33, 0, true, 1.f, 1.75f, 1.75f },
	{ 41, 1, true, 1.f, 1.f, 1.f },
	{ 219, 0, true, 1.f, 1.f, 1.f },
	{ 223, 2, true, 1.f, 1.f, 1.f },
	{ 235, 0, true, 1.f, 1.5f, 1.52f },
	{ 247, 0, false, 0.f, .1f, .1f },
};

// ImageKey: the Home key (homekey.png), alternating with the icon while the pop-up is up
static const s_toast_key k_toast_home_key_keys[] =
{
	{ 0, 1, false, 0.f, 1.f, 1.f },
	{ 75, 0, true, 0.f, 1.f, 1.f },
	{ 81, 0, true, 1.f, 1.f, 1.f },
	{ 91, 2, true, 1.f, 1.f, 1.f },
	{ 100, 0, true, 1.f, .85f, .85f },
	{ 109, 1, true, 1.f, 1.f, 1.f },
	{ 132, 0, true, 1.f, 1.f, 1.f },
	{ 140, 0, false, 0.f, 1.f, 1.f },
	{ 187, 0, true, 0.f, 1.f, 1.f },
	{ 194, 0, true, 1.f, 1.f, 1.f },
	{ 195, 0, true, 1.f, 1.f, 1.f },
	{ 208, 0, true, 1.f, 1.f, 1.f },
	{ 216, 0, true, 0.f, 1.f, 1.f },
};

// Image: the notification icon (the achievement trophy, XAM\Achievement.png)
static const s_toast_key k_toast_icon_keys[] =
{
	{ 0, 1, false, 0.f, 1.f, 1.f },
	{ 33, 0, true, 0.f, 1.5f, 1.5f },
	{ 41, 1, true, 1.f, 1.f, 1.f },
	{ 75, 0, true, 1.f, 1.f, 1.f },
	{ 81, 1, true, 0.f, 1.f, 1.f },
	{ 140, 0, true, 0.f, 1.f, 1.f },
	{ 150, 1, true, 1.f, 1.f, 1.f },
	{ 178, 0, true, 1.f, 1.f, 1.f },
	{ 187, 0, true, 0.f, 1.f, 1.f },
	{ 216, 0, true, 0.f, 1.f, 1.f },
	{ 223, 0, true, 1.f, 1.f, 1.f },
	{ 235, 1, false, 0.f, 1.25f, 1.25f },
	{ 247, 0, false, 0.f, 1.5f, 1.5f },
};

// TextPresenter
static const s_toast_key k_toast_text_keys[] =
{
	{ 0, 1, false, 0.f, 1.f, 1.f },
	{ 54, 0, true, 0.f, 1.f, 1.f },
	{ 60, 1, true, 1.f, 1.f, 1.f },
	{ 216, 0, true, 1.f, 1.f, 1.f },
	{ 223, 0, false, 0.f, 1.f, 1.f },
	{ 247, 0, false, 0.f, 1.f, 1.f },
};

// layout (position, size and pivot from skin.xur)
static const s_toast_element k_toast_background = { 0.f, 0.f, 361.f, 70.f, 0.f, 0.f, k_toast_background_keys, NUMBEROF(k_toast_background_keys) };
static const s_toast_element k_toast_border = { 8.f, 8.f, 54.f, 54.f, 27.f, 27.f, k_toast_border_keys, NUMBEROF(k_toast_border_keys) };
static const s_toast_element k_toast_home_key = { 13.f, 12.f, 45.f, 45.f, 22.5f, 22.5f, k_toast_home_key_keys, NUMBEROF(k_toast_home_key_keys) };
static const s_toast_element k_toast_icon = { 18.f, 17.f, 36.f, 36.f, 18.f, 18.f, k_toast_icon_keys, NUMBEROF(k_toast_icon_keys) };
static const s_toast_element k_toast_text = { 75.f, 0.f, 268.f, 70.f, 0.f, 0.f, k_toast_text_keys, NUMBEROF(k_toast_text_keys) };

// text colour 0xFF16436C
static const ImU32 k_toast_text_color = IM_COL32(0x16, 0x43, 0x6C, 0xFF);

/* globals */

static bool g_toast_active = false;
static double g_toast_start_time = 0.0;
static bool g_toast_sound_played = false;
static char g_toast_lines[2][160] = {};

/* private code */

// the frame for the time since the pop-up started: TransTo, the loop k_toast_loops times, then TransFrom
static bool toast_frame_get(double elapsed, float* out_frame)
{
	const float frame = (float)(elapsed * k_toast_frames_per_second);
	const float loop_length = (float)(k_toast_frame_loop_end - k_toast_frame_loop);
	const float loops_end = k_toast_frame_loop + loop_length * k_toast_loops;

	if (frame < k_toast_frame_loop)
	{
		*out_frame = frame;
	}
	else if (frame < loops_end)
	{
		*out_frame = k_toast_frame_loop + fmodf(frame - k_toast_frame_loop, loop_length);
	}
	else
	{
		*out_frame = k_toast_frame_out + (frame - loops_end);
		if (*out_frame > k_toast_frame_out_end)
		{
			return false;
		}
	}
	return true;
}

// the element's state at a frame: each key's interpolation applies until the next key; show steps
static s_toast_key toast_element_evaluate(const s_toast_element* element, float frame)
{
	const s_toast_key* keys = element->keys;
	uint32 index = 0;
	while (index + 1 < element->key_count && keys[index + 1].frame <= frame)
	{
		index++;
	}

	s_toast_key result = keys[index];
	if (index + 1 < element->key_count && frame >= keys[index].frame && keys[index].interpolation != _toast_interpolation_none)
	{
		const s_toast_key* next = &keys[index + 1];
		float t = (frame - keys[index].frame) / (float)(next->frame - keys[index].frame);
		t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
		if (keys[index].interpolation == _toast_interpolation_ease)
		{
			// ease out
			t = 1.f - (1.f - t) * (1.f - t);
		}
		result.opacity = keys[index].opacity + (next->opacity - keys[index].opacity) * t;
		result.scale_x = keys[index].scale_x + (next->scale_x - keys[index].scale_x) * t;
		result.scale_y = keys[index].scale_y + (next->scale_y - keys[index].scale_y) * t;
	}
	return result;
}

// the element's rectangle on screen after its scale about its pivot
static void toast_element_rectangle(const s_toast_element* element, const s_toast_key* state, ImVec2 origin, float scale, ImVec2* out_min, ImVec2* out_max)
{
	const float pivot_x = element->x + element->pivot_x;
	const float pivot_y = element->y + element->pivot_y;
	const float left = pivot_x + (element->x - pivot_x) * state->scale_x;
	const float top = pivot_y + (element->y - pivot_y) * state->scale_y;
	const float right = pivot_x + (element->x + element->width - pivot_x) * state->scale_x;
	const float bottom = pivot_y + (element->y + element->height - pivot_y) * state->scale_y;
	*out_min = ImVec2(origin.x + left * scale, origin.y + top * scale);
	*out_max = ImVec2(origin.x + right * scale, origin.y + bottom * scale);
	return;
}

static ImU32 toast_color(float opacity)
{
	const int32 alpha = (int32)(opacity * 255.f + .5f);
	return IM_COL32(255, 255, 255, alpha < 0 ? 0 : (alpha > 255 ? 255 : alpha));
}

static void toast_draw_image(ImDrawList* draw_list, const s_gfwl_texture* texture, ImVec2 minimum, ImVec2 maximum, float opacity)
{
	if (texture == NULL || opacity <= 0.f)
	{
		return;
	}
	draw_list->AddImage((ImTextureID)(uintptr_t)texture->texture, minimum, maximum, ImVec2(0, 0), ImVec2(1, 1), toast_color(opacity));
	return;
}

// a nine-grid (XuiNineGrid): the corners keep their size, the edges and middle stretch; drawn at the element's size,
// then scaled with it like the original
static void toast_draw_nine_grid(ImDrawList* draw_list, const s_gfwl_texture* texture, float margin,
	const s_toast_element* element, const s_toast_key* state, ImVec2 origin, float scale)
{
	if (texture == NULL || state->opacity <= 0.f)
	{
		return;
	}

	const float xs[4] = { element->x, element->x + margin, element->x + element->width - margin, element->x + element->width };
	const float ys[4] = { element->y, element->y + margin, element->y + element->height - margin, element->y + element->height };
	const float us[4] = { 0.f, margin / texture->width, 1.f - margin / texture->width, 1.f };
	const float vs[4] = { 0.f, margin / texture->height, 1.f - margin / texture->height, 1.f };
	const float pivot_x = element->x + element->pivot_x;
	const float pivot_y = element->y + element->pivot_y;
	const ImU32 color = toast_color(state->opacity);

	for (int32 row = 0; row < 3; row++)
	{
		for (int32 column = 0; column < 3; column++)
		{
			const ImVec2 minimum(
				origin.x + (pivot_x + (xs[column] - pivot_x) * state->scale_x) * scale,
				origin.y + (pivot_y + (ys[row] - pivot_y) * state->scale_y) * scale);
			const ImVec2 maximum(
				origin.x + (pivot_x + (xs[column + 1] - pivot_x) * state->scale_x) * scale,
				origin.y + (pivot_y + (ys[row + 1] - pivot_y) * state->scale_y) * scale);
			draw_list->AddImage((ImTextureID)(uintptr_t)texture->texture, minimum, maximum,
				ImVec2(us[column], vs[row]), ImVec2(us[column + 1], vs[row + 1]), color);
		}
	}
	return;
}

static void toast_start(int32 achievement_index)
{
	// "title|description"
	const char* string = achievements_get_string(achievement_index);
	const char* divider = strchr(string, '|');
	char title[128];
	size_t title_length = divider ? (size_t)(divider - string) : strlen(string);
	if (title_length >= sizeof(title))
	{
		title_length = sizeof(title) - 1;
	}
	csmemcpy(title, string, title_length);
	title[title_length] = '\0';

	// XAM\Strings.xus: "Achievement unlocked\n%sG - %s"
	sprintf_s(g_toast_lines[0], "Achievement unlocked");
	sprintf_s(g_toast_lines[1], "%uG - %s", ImGuiHandler::ImHomeMenu::GetAchievementGamerscore(achievement_index), title);

	g_toast_active = true;
	g_toast_sound_played = false;
	g_toast_start_time = ImGui::GetTime();
	return;
}

/* public code */

namespace ImGuiHandler
{
	namespace ImAchievementToast
	{
		void Render()
		{
			if (!g_toast_active)
			{
				const int32 next = achievements_popup_get_current_index();
				if (next == NONE || !gfwl_assets_available())
				{
					return;
				}
				toast_start(next);
			}

			float frame;
			if (!toast_frame_get(ImGui::GetTime() - g_toast_start_time, &frame))
			{
				g_toast_active = false;
				achievements_popup_pop();
				return;
			}

			if (!g_toast_sound_played && frame >= k_toast_sound_frame)
			{
				g_toast_sound_played = true;
				gfwl_assets_play_sound("SKIN", L"NotifyPopup.wav");
			}

			// bottom centre, scaled with the screen height (the 360-wide visual is about 450 pixels wide at 720p)
			const ImGuiIO& io = ImGui::GetIO();
			const float scale = io.DisplaySize.y / 576.f;
			const ImVec2 origin(
				(io.DisplaySize.x - k_toast_width * scale) * .5f,
				io.DisplaySize.y * .88f - k_toast_height * scale);
			ImDrawList* draw_list = ImGui::GetForegroundDrawList();

			const s_toast_key background = toast_element_evaluate(&k_toast_background, frame);
			if (background.show)
			{
				toast_draw_nine_grid(draw_list, gfwl_assets_get_texture("SKIN", L"toast_bg.png"), 15.f, &k_toast_background, &background, origin, scale);
			}

			const s_toast_key border = toast_element_evaluate(&k_toast_border, frame);
			if (border.show)
			{
				toast_draw_nine_grid(draw_list, gfwl_assets_get_texture("SKIN", L"toast_frame.png"), 5.f, &k_toast_border, &border, origin, scale);
			}

			ImVec2 minimum, maximum;
			const s_toast_key home_key = toast_element_evaluate(&k_toast_home_key, frame);
			if (home_key.show)
			{
				toast_element_rectangle(&k_toast_home_key, &home_key, origin, scale, &minimum, &maximum);
				toast_draw_image(draw_list, gfwl_assets_get_texture("SKIN", L"homekey.png"), minimum, maximum, home_key.opacity);
			}

			const s_toast_key icon = toast_element_evaluate(&k_toast_icon, frame);
			if (icon.show)
			{
				toast_element_rectangle(&k_toast_icon, &icon, origin, scale, &minimum, &maximum);
				toast_draw_image(draw_list, gfwl_assets_get_texture("XAM", L"Achievement.png"), minimum, maximum, icon.opacity);
			}

			const s_toast_key text = toast_element_evaluate(&k_toast_text, frame);
			if (text.show && text.opacity > 0.f)
			{
				ImFont* font = ImGuiHandler::GetGfwlFont();
				const float font_size = 15.f * scale;
				const float line_height = font_size * 1.2f;
				const float left = origin.x + k_toast_text.x * scale;
				const float top = origin.y + (k_toast_text.y + k_toast_text.height * .5f) * scale - line_height;
				const int32 alpha = (int32)(text.opacity * 255.f);
				const ImU32 color = (k_toast_text_color & ~IM_COL32_A_MASK) | ((ImU32)(alpha < 0 ? 0 : (alpha > 255 ? 255 : alpha)) << IM_COL32_A_SHIFT);
				const ImVec4 clip(left, origin.y, left + k_toast_text.width * scale, origin.y + k_toast_height * scale);
				draw_list->AddText(font, font_size, ImVec2(left, top), color, g_toast_lines[0], NULL, 0.f, &clip);
				draw_list->AddText(font, font_size, ImVec2(left, top + line_height), color, g_toast_lines[1], NULL, 0.f, &clip);
			}
			return;
		}
	}
}

#endif
