#include "stdafx.h"
#include "wmv_playback.h"

/* public code */

void __cdecl wmv_update(void)
{
	INVOKE(0x3A125, 0x0, wmv_update);
	return;
}

bool __cdecl wmv_playback_in_progress(void)
{
	return INVOKE(0x39F09, 0x0, wmv_playback_in_progress);
}

void __cdecl wmv_render(void)
{
	INVOKE(0x3A264, 0x0, wmv_render);
	return;
}

// LAN edition: <halo2.exe folder>\movies\afk.wmv plays after 3 minutes without input on the main menu
// (not in a lobby); any key, mouse or controller input stops it, and the timer starts again afterwards

#include "game/game.h"
#include "networking/logic/network_life_cycle.h"



// halo2.exe+0x3A2B7: plays a movie (UTF-8 path) with the movie flags below
enum e_wmv_playback_flags : uint32
{
	_wmv_playback_flag_skip_with_button = FLAG(1),		// A/B/X/Y/Start/Back, Enter, Space or A key
	_wmv_playback_flag_hide_user_interface = FLAG(2),
	_wmv_playback_flag_mute_game_sounds = FLAG(7),
	_wmv_playback_flag_keep_playing = FLAG(12),			// without this (or 0x400) the movie is stopped on the next update
};

static void __cdecl wmv_play(const char* utf8_path, uint32 flags)
{
	INVOKE(0x3A2B7, 0x0, wmv_play, utf8_path, flags);
	return;
}

// stops the movie and restores what the flags changed
static void __cdecl wmv_stop(void)
{
	INVOKE(0x3A0E6, 0x0, wmv_stop);
	return;
}

constexpr uint32 k_afk_movie_idle_milliseconds = 3 * 60 * 1000;
constexpr uint32 k_afk_movie_input_grace_milliseconds = 1000;
constexpr SHORT k_afk_movie_thumbstick_deadzone = 12000;
constexpr BYTE k_afk_movie_trigger_threshold = 64;

typedef DWORD(WINAPI* t_xinput_get_state)(DWORD user_index, XINPUT_STATE* state);

static uint32 g_afk_movie_last_input_ticks = 0;
static uint32 g_afk_movie_started_ticks = 0;
static bool g_afk_movie_playing = false;
static bool g_afk_movie_missing = false;
static WORD g_afk_movie_last_buttons[XUSER_MAX_COUNT] = {};

static t_xinput_get_state afk_movie_xinput_get_state(void)
{
	static bool initialized = false;
	static t_xinput_get_state xinput_get_state = nullptr;
	if (!initialized)
	{
		initialized = true;
		const wchar_t* const modules[] = { L"xinput1_4.dll", L"xinput9_1_0.dll", L"xinput1_3.dll" };
		for (uint32 index = 0; index < NUMBEROF(modules) && xinput_get_state == nullptr; index++)
		{
			HMODULE module = GetModuleHandleW(modules[index]);
			if (module == NULL)
			{
				module = LoadLibraryW(modules[index]);
			}
			if (module != NULL)
			{
				xinput_get_state = (t_xinput_get_state)GetProcAddress(module, "XInputGetState");
			}
		}
	}
	return xinput_get_state;
}

// any controller held or moved since the last check
static bool afk_movie_controller_input(void)
{
	t_xinput_get_state xinput_get_state = afk_movie_xinput_get_state();
	if (xinput_get_state == nullptr)
	{
		return false;
	}

	bool input = false;
	for (DWORD user_index = 0; user_index < XUSER_MAX_COUNT; user_index++)
	{
		XINPUT_STATE state = {};
		if (xinput_get_state(user_index, &state) != ERROR_SUCCESS)
		{
			g_afk_movie_last_buttons[user_index] = 0;
			continue;
		}

		const XINPUT_GAMEPAD& pad = state.Gamepad;
		if (pad.wButtons != 0 || pad.wButtons != g_afk_movie_last_buttons[user_index]
			|| pad.bLeftTrigger > k_afk_movie_trigger_threshold || pad.bRightTrigger > k_afk_movie_trigger_threshold
			|| abs(pad.sThumbLX) > k_afk_movie_thumbstick_deadzone || abs(pad.sThumbLY) > k_afk_movie_thumbstick_deadzone
			|| abs(pad.sThumbRX) > k_afk_movie_thumbstick_deadzone || abs(pad.sThumbRY) > k_afk_movie_thumbstick_deadzone)
		{
			input = true;
		}
		g_afk_movie_last_buttons[user_index] = pad.wButtons;
	}
	return input;
}

// ticks (GetTickCount) of the latest keyboard, mouse or controller input
static uint32 afk_movie_last_input_ticks(uint32 now)
{
	if (afk_movie_controller_input())
	{
		g_afk_movie_last_input_ticks = now;
	}

	LASTINPUTINFO last_input = { sizeof(LASTINPUTINFO), 0 };
	if (GetLastInputInfo(&last_input) && (int32)(last_input.dwTime - g_afk_movie_last_input_ticks) > 0)
	{
		g_afk_movie_last_input_ticks = last_input.dwTime;
	}
	return g_afk_movie_last_input_ticks;
}

static bool afk_movie_get_path(char* utf8_path, uint32 size)
{
	wchar_t path[MAX_PATH];
	const DWORD length = GetModuleFileNameW(NULL, path, NUMBEROF(path));
	if (length == 0 || length >= NUMBEROF(path))
	{
		return false;
	}
	wchar_t* last_backslash = wcsrchr(path, L'\\');
	if (last_backslash == nullptr)
	{
		return false;
	}
	last_backslash[1] = L'\0';
	if (wcscat_s(path, NUMBEROF(path), L"movies\\afk.wmv") != 0)
	{
		return false;
	}

	const DWORD attributes = GetFileAttributesW(path);
	if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY))
	{
		return false;
	}

	return WideCharToMultiByte(CP_UTF8, 0, path, -1, utf8_path, (int)size, NULL, NULL) > 0;
}

void wmv_afk_movie_update(void)
{
	const uint32 now = GetTickCount();
	if (g_afk_movie_last_input_ticks == 0)
	{
		g_afk_movie_last_input_ticks = now;
	}
	const uint32 last_input = afk_movie_last_input_ticks(now);

	if (g_afk_movie_playing)
	{
		if (!wmv_playback_in_progress())
		{
			// finished or skipped: wait another 3 minutes
			g_afk_movie_playing = false;
			g_afk_movie_last_input_ticks = now;
		}
		else if (now - g_afk_movie_started_ticks > k_afk_movie_input_grace_milliseconds
			&& (int32)(last_input - g_afk_movie_started_ticks) > (int32)k_afk_movie_input_grace_milliseconds)
		{
			// any input stops it, not only the buttons the game checks
			wmv_stop();
			g_afk_movie_playing = false;
			g_afk_movie_last_input_ticks = now;
		}
		return;
	}

	const bool on_main_menu = game_is_ui_shell() && network_life_cycle_get_state() == _life_cycle_state_none;
	if (!on_main_menu || wmv_playback_in_progress())
	{
		g_afk_movie_last_input_ticks = now;
		return;
	}

	if (now - last_input < k_afk_movie_idle_milliseconds)
	{
		return;
	}

	char utf8_path[MAX_PATH * 3];
	if (!afk_movie_get_path(utf8_path, sizeof(utf8_path)))
	{
		if (!g_afk_movie_missing)
		{
			g_afk_movie_missing = true;
			error(_error_log, "movies\\afk.wmv not found next to halo2.exe, the idle movie is not played");
		}
		g_afk_movie_last_input_ticks = now;
		return;
	}
	g_afk_movie_missing = false;

	wmv_play(utf8_path, _wmv_playback_flag_skip_with_button | _wmv_playback_flag_hide_user_interface | _wmv_playback_flag_mute_game_sounds | _wmv_playback_flag_keep_playing);
	g_afk_movie_started_ticks = now;
	g_afk_movie_playing = wmv_playback_in_progress();
	if (!g_afk_movie_playing)
	{
		g_afk_movie_last_input_ticks = now;
	}
	return;
}
