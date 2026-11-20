#include "stdafx.h"
#include "Achievements.h"

#include "cartographer/config/endpoints.h"
#include "networking/network_event.h"

#include "achievements/achievement_list.h"
#include "achievements/achievement_manager.h"
#include "interface/user_interface_guide.h"
#include "saved_games/saved_game_files.h"
#include "saved_games/saved_game_files_async_windows.h"
#include "H2MOD/Modules/Accounts/Accounts.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"

#include <deque>
#include <intrin.h>

/* constants */

static const char k_cartographer_unlock_url[] = k_cartographer_url_https"/achievement-api/unlock.php";
static const char k_cartographer_achivement_list_url[] = k_cartographer_url_https"/achievement-api/achievement_list.php?xuid=";

bool g_achievement_list[42];

using namespace rapidjson;


std::unordered_map<std::string, bool> AchievementMap;

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
	((std::string*)userp)->append((char*)contents, size * nmemb);
	return size * nmemb;
}

void AchievementUnlock(unsigned long long xuid, int achievement_id, XOVERLAPPED* pOverlapped)
{
	event(_event_status, "h2mod:achievements: - Unlocking achievement ID: %d", achievement_id);

	CURL *curl;
	CURLcode res;
	std::string readBuffer;


	curl = curl_interface_init_no_verify();
	if (curl) {
		rapidjson::Document document;
		document.SetObject();


		Value token(kStringType);
		token.SetString(H2CurrentAccountLoginToken, document.GetAllocator());
		document.AddMember("token", token, document.GetAllocator());
		document.AddMember("id", achievement_id, document.GetAllocator());
		document.AddMember("xuid", Value().SetUint64(xuid), document.GetAllocator());


		StringBuffer buffer;
		Writer<StringBuffer> writer(buffer);
		document.Accept(writer);

		curl_easy_setopt(curl, CURLOPT_URL, k_cartographer_unlock_url);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
		curl_easy_setopt(curl, CURLOPT_POST, 1L);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, buffer.GetString());
		res = curl_easy_perform(curl);
		curl_easy_cleanup(curl);
	}

	return;
}

// TODO: add extra verification for json returned from dedi
void GetAchievements(unsigned long long xuid)
{
	CURL *curl;
	CURLcode res;
	std::string readBuffer;

	curl = curl_interface_init_no_verify();
	if (curl)
	{
		c_static_string<512> server_url(k_cartographer_achivement_list_url);
		server_url.append(std::to_string(xuid).c_str());
		curl_easy_setopt(curl, CURLOPT_URL, server_url.get_string());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
		res = curl_easy_perform(curl);
		curl_easy_cleanup(curl);

		rapidjson::Document document;
		document.Parse(readBuffer.c_str());

		csmemset(g_achievement_list, false, sizeof(g_achievement_list));
		if (!document.HasParseError() && document.HasMember("achievements"))
		{
			for (rapidjson::Value& achievement : document["achievements"].GetArray())
			{
				// TODO: remove string conversion once master is updated and just do achievement.GetUint() 
				if (achievement.IsString() || achievement.IsUint())
				{
					const uint32 id = achievement.IsString() ? (uint32)std::stoll(achievement.GetString()) : achievement.GetUint();
					if (VALID_INDEX(id, NUMBEROF(g_achievement_list)))
					{
						g_achievement_list[id] = true;
					}
					else
					{
						error(_error_log, "invalid achievement index provided: %d", id);
					}
				}
				else
				{
					error(_error_log, "invalid data type in achievements array");
				}
			}
		}
	}

	return;
}

/* LAN edition: local achievements */

/* constants */

// "title|description", by e_achievement_type: the names and descriptions from the game's Games for Windows - LIVE
// title data (the XDBF achievement table embedded in halo2.exe)
static char const* k_achievement_strings[] =
{
	"Cairo Station|Complete Cairo Station at Normal, Heroic, or Legendary.",
	"Outskirts|Complete Outskirts at Normal, Heroic, or Legendary.",
	"Metropolis|Complete Metropolis at Normal, Heroic, or Legendary.",
	"The Arbiter|Complete The Arbiter at Normal, Heroic, or Legendary.",
	"Oracle|Complete The Oracle at Normal, Heroic, or Legendary.",
	"Delta Halo|Complete Delta Halo at Normal, Heroic, or Legendary.",
	"Regret|Complete Regret at Normal, Heroic, or Legendary.",
	"Sacred Icon|Complete Sacred Icon at Normal, Heroic, or Legendary.",
	"Quarantine Zone|Complete Quarantine Zone at Normal, Heroic, or Legendary.",
	"Gravemind|Complete Gravemind at Normal, Heroic, or Legendary.",
	"Uprising|Complete Uprising at Normal, Heroic, or Legendary.",
	"High Charity|Complete High Charity at Normal, Heroic, or Legendary.",
	"The Great Journey|Complete The Great Journey at Normal, Heroic, or Legendary.",
	"Warrior|Complete the game on Normal difficulty.",
	"Hero|Complete the game on Heroic difficulty.",
	"Legend|Complete the game on Legendary difficulty.",
	"King of the Scarab|Acquire the Scarab gun on the Metropolis level, any difficulty.",
	"Silent but Deadly|Kill 7 opponents from behind in a row without being spotted, any level, any difficulty.",
	"Demon|Complete any campaign level without dying once on Normal, Heroic, or Legendary.",
	"Go Ape Shiv|Kill an enraged, berserk Brute by melee, any level, any difficulty.",
	"Stick It|Stick an opponent with a plasma grenade and blow them up.",
	"Counterpoint|Kill the sword carrier.",
	"Carjacking|Steal an occupied vehicle from an opponent and take it over.",
	"Violent Cartographer|Play every available default map and stay in each game until it's over.",
	"Rainman|Play every available game variant, with at least 3 other players, to the end of each game.",
	"Double Kill|Kill 2 opponents within 4 seconds.",
	"Triple Kill|Kill 3 opponents within 4 seconds.",
	"Killtacular|Kill 4 opponents within 4 seconds.",
	"Killing Spree|Kill 5 opponents in a row in the same game, without dying.",
	"Running Riot|Kill 10 opponents in a row in the same game, without dying.",
	"Sniper Kill|Kill an opponent with the sniper rifle or the beam rifle.",
	"Roadkill|Run over, hit and kill an opponent with a vehicle.",
	"Bonecracker|Hit and kill an opponent with a melee attack.",
	"Assassin|Melee an opponent from behind.",
	"Skewer Stopper|Kill the sword carrier after they kill 5 or more times in a row with the sword.",
	"Vigilante|Stop a Killing Spree of another player.",
	"Air Traffic Controller|Blow up the Banshee in flight while manned, with grenades or the rocket launcher.",
	"Decorated Soldier|Get awarded at least 8 different medals in one non-team game.",
	"Ninja|In a non-team game, kill 5 people by melee, from behind.",
	"Flaming Ninja|Kill an opponent that has the Ninja achievement.",
	"Hired Gun|Kill an opponent who has the Legend achievement."
};
static_assert(NUMBEROF(k_achievement_strings) == k_achievement_type_count, "one string per achievement");

enum
{
	k_achievements_save_signature = 'achv',
	k_achievements_save_version = 1,
};

static const wchar_t k_achievements_bin_name[] = L"cartographer_achievements";
static const uint64 k_achievements_all_mask = (1ull << k_achievement_type_count) - 1;

// unlocked for everyone: they need to know other players' achievements (Ninja / Legend), which came from Xbox Live
static const uint64 k_achievements_default_unlocked_mask =
	(1ull << _achievement_type_flaming_ninja) |
	(1ull << _achievement_type_hired_gun);

/* structures */

// cartographer_achievements.bin, saved next to the player profile (like cartographer_profile.bin)
struct s_saved_game_achievements
{
	uint32 signature;
	uint32 version;
	uint64 unlocked;	// bit n: e_achievement_type n unlocked
};
static_assert(sizeof(s_saved_game_achievements) == 16, "achievements save size changed");

struct s_achievements_profile
{
	int32 enumerated_file_index;	// NONE when no profile is signed in on the controller
	uint64 unlocked;
};

/* globals */

static s_achievements_profile g_achievements_profiles[k_number_of_controllers];
// unlocked while no profile is signed in: kept until the game closes
static uint64 g_achievements_unsaved = k_achievements_default_unlocked_mask;
static std::deque<int32> g_achievement_popups;				// achievement indices, oldest first

/* private code */

// the game has a single achievement player (the Live user on Xbox / Games for Windows): the profile signed in on the
// lowest controller
static s_achievements_profile* achievements_profile_get_primary(void)
{
	for (uint32 index = 0; index < k_number_of_controllers; index++)
	{
		if (g_achievements_profiles[index].enumerated_file_index != NONE)
		{
			return &g_achievements_profiles[index];
		}
	}
	return NULL;
}

// the list the game reads back through XUserCreateAchievementEnumerator (indexed by achievement id = type + 1)
static void achievements_list_refresh(void)
{
	const s_achievements_profile* profile = achievements_profile_get_primary();
	const uint64 unlocked = profile ? profile->unlocked : g_achievements_unsaved;

	csmemset(g_achievement_list, 0, sizeof(g_achievement_list));
	for (int32 index = 0; index < k_achievement_type_count; index++)
	{
		g_achievement_list[index + 1] = ((unlocked >> index) & 1) != 0;
	}
	return;
}

static void achievements_profile_save(const s_achievements_profile* profile)
{
	if (profile->enumerated_file_index == NONE || ENUMERATED_INDEX_IS_DEFAULT_SAVE(profile->enumerated_file_index))
	{
		return;
	}

	s_saved_game_achievements save = {};
	save.signature = k_achievements_save_signature;
	save.version = k_achievements_save_version;
	save.unlocked = profile->unlocked;
	saved_games_async_helper_write_saved_game_bin(k_achievements_bin_name, profile->enumerated_file_index, (int8*)&save, sizeof(save));
	return;
}

static void achievements_unlock(int32 achievement_index)
{
	if (!VALID_INDEX(achievement_index, k_achievement_type_count))
	{
		return;
	}

	s_achievements_profile* profile = achievements_profile_get_primary();
	uint64* unlocked = profile ? &profile->unlocked : &g_achievements_unsaved;
	const uint64 flag = 1ull << achievement_index;
	if ((*unlocked & flag) != 0)
	{
		// already unlocked: no second pop-up
		return;
	}

	*unlocked |= flag;
	if (profile)
	{
		achievements_profile_save(profile);
	}
	achievements_list_refresh();

	g_achievement_popups.push_back(achievement_index);
	event(_event_status, "achievements: unlocked %d (%s)", achievement_index + 1, k_achievement_strings[achievement_index]);
	return;
}

// The achievement code in halo2.exe (manager 0x44A619-0x44B553, Live upload 0x48C6A5-0x48C99D, trackers
// 0x4C11B4-0x4C1883) is tamper protected: the game doesn't start when it is patched. So instead of patching it, this
// hooks the guide state manager getter it calls (halo2.exe 0x609674, not protected) and answers by caller:
// - from the Live check 0x48C6A5 called by the achievement manager's game start functions: a guide state that reads
//   "signed in to Live", so achievements are earned (and their progress saved in checkpoints) for an offline user
// - from 0x48C726 (queue an achievement for the Live upload, called when an achievement's conditions are met): unlock it
//   locally; the real guide state is returned, so nothing is queued for upload

typedef void* (__cdecl* t_user_interface_guide_state_manager_get)(void);
static t_user_interface_guide_state_manager_get p_user_interface_guide_state_manager_get = NULL;

// enough of a c_user_interface_guide_state_manager for the Live check (m_sign_in_state at +8)
static uint32 g_achievements_live_guide_state[4] = { 0, 0, eXUserSigninState_SignedInToLive, 0 };

static uintptr_t g_live_check_return = 0;				// in 0x48C6A5, after its call to 0x609674
static uintptr_t g_live_check_callers[3] = {};			// manager game start functions, after their calls to 0x48C6A5
static uintptr_t g_queue_achievement_return = 0;		// in 0x48C726, after its call to 0x609674
static uintptr_t g_queue_achievement_caller = 0;		// in the manager's update 0x44A66E, after its call to 0x48C726

// what the hooks have seen, shown under Diagnostics in the Home menu
static uint32 g_achievements_live_checks[3] = {};		// game start Live checks answered "signed in", by caller
static uint32 g_achievements_queued = 0;				// achievements the game reported as earned
static uint32 g_achievements_multiplayer_enabled = 0;	// multiplayer trackers enabled after the Gold check

static void* __cdecl achievements_guide_state_manager_get_hook(void)
{
	// stack at entry: [0] our return address, [1...] the caller's stack
	uintptr_t* stack = (uintptr_t*)_AddressOfReturnAddress();

	if (stack[0] == g_live_check_return)
	{
		// 0x48C6A5 pushes nothing before the call: [1] is its return address
		for (uint32 index = 0; index < NUMBEROF(g_live_check_callers); index++)
		{
			if (stack[1] == g_live_check_callers[index])
			{
				c_user_interface_guide_state_manager* guide = (c_user_interface_guide_state_manager*)p_user_interface_guide_state_manager_get();
				if (guide->m_sign_in_state != eXUserSigninState_NotSignedIn)
				{
					g_achievements_live_checks[index]++;
					if (index == 1)
					{
						// saved game resumed (0x44B220, pushes ebx and esi before the call: [2] esi, [3] ebx, [4] its
						// return address, [5] its argument): the argument says whether achievements were on when the game
						// was saved; saves from before the LAN edition had achievements always say no, so say yes
						*(uint8*)&stack[5] = 1;
					}
					return g_achievements_live_guide_state;
				}
				break;
			}
		}
	}
	else if (stack[0] == g_queue_achievement_return && stack[2] == g_queue_achievement_caller)
	{
		// 0x48C726 pushes esi before the call: [1] esi, [2] its return address, [3] the achievement index
		g_achievements_queued++;
		achievements_unlock((int32)stack[3]);
	}

	return p_user_interface_guide_state_manager_get();
}

// achievement manager layout (halo2.exe): +1 earning achievements, +4 "signed in to Live", +8 the 41 trackers,
// +0xB0 a flag passed to the trackers when they are enabled
// tracker: +0 vtable (slot 0: enable(arg, flag)), +4 enabled, +5 achieved, +6 multiplayer (needs Gold)
enum
{
	k_achievement_manager_earning_offset = 0x1,
	k_achievement_manager_signed_in_offset = 0x4,
	k_achievement_manager_trackers_offset = 0x8,
	k_achievement_manager_enable_flag_offset = 0xB0,

	k_achievement_tracker_enabled_offset = 0x4,
	k_achievement_tracker_achieved_offset = 0x5,
	k_achievement_tracker_multiplayer_offset = 0x6,
};

typedef void(__thiscall* t_achievement_tracker_enable)(void* tracker, uint32 arg, uint32 flag);

static uint8* achievements_manager_data(void)
{
	return (uint8*)achievement_manager_get();
}

static uint8* achievements_manager_tracker(uint8* manager, int32 index)
{
	return ((uint8**)(manager + k_achievement_manager_trackers_offset))[index];
}

// halo2.exe 0x47BFD1, called by the achievement manager's enable function (0x44A6F9) right after it enables the
// trackers. Multiplayer trackers are only enabled there for Gold members (an encrypted guide state check, 0x61A995):
// enable the ones it left off, the same way it enables the others
typedef int32(__cdecl* t_achievements_post_enable)(void);
static t_achievements_post_enable p_achievements_post_enable = NULL;
static uintptr_t g_achievements_post_enable_return = 0;		// in 0x44A6F9, after its call to 0x47BFD1

static int32 __cdecl achievements_post_enable_hook(void)
{
	// stack at entry: [0] return address, [1] esi, [2] ebp, [3] ebx, [4] edi (pushed by 0x44A6F9), [5] its return
	// address, [6] its argument
	const uintptr_t* stack = (const uintptr_t*)_AddressOfReturnAddress();
	if (stack[0] == g_achievements_post_enable_return)
	{
		uint8* manager = achievements_manager_data();
		if (manager != NULL && manager[k_achievement_manager_earning_offset] != 0)
		{
			for (int32 index = 0; index < k_achievement_type_count; index++)
			{
				uint8* tracker = achievements_manager_tracker(manager, index);
				if (tracker != NULL && tracker[k_achievement_tracker_multiplayer_offset] != 0 && tracker[k_achievement_tracker_enabled_offset] == 0)
				{
					const t_achievement_tracker_enable enable = (t_achievement_tracker_enable)(*(void***)tracker)[0];
					enable(tracker, (uint32)stack[6], manager[k_achievement_manager_enable_flag_offset]);
					tracker[k_achievement_tracker_achieved_offset] = 0;
					g_achievements_multiplayer_enabled++;
				}
			}
		}
	}
	return p_achievements_post_enable();
}

/* public code */

void achievements_apply_patches(void)
{
	for (uint32 index = 0; index < k_number_of_controllers; index++)
	{
		g_achievements_profiles[index].enumerated_file_index = NONE;
		g_achievements_profiles[index].unlocked = 0;
	}

	g_live_check_return = Memory::GetAddressRelative(0x48C6B4);
	g_live_check_callers[0] = Memory::GetAddressRelative(0x44B1E7);	// campaign level chosen
	g_live_check_callers[1] = Memory::GetAddressRelative(0x44B233);	// game start (with eligibility)
	g_live_check_callers[2] = Memory::GetAddressRelative(0x44B452);	// game start
	g_queue_achievement_return = Memory::GetAddressRelative(0x48C738);
	g_queue_achievement_caller = Memory::GetAddressRelative(0x44A6B4);

	g_achievements_post_enable_return = Memory::GetAddressRelative(0x44A782);

	// (runs inside H2MOD::Initialize's detour transaction)
	DETOUR_ATTACH(p_user_interface_guide_state_manager_get, Memory::GetAddress<t_user_interface_guide_state_manager_get>(0x209674), achievements_guide_state_manager_get_hook);
	DETOUR_ATTACH(p_achievements_post_enable, Memory::GetAddress<t_achievements_post_enable>(0x7BFD1), achievements_post_enable_hook);
	return;
}

void achievements_profile_sign_in(e_controller_index controller_index, uint32 enumerated_file_index)
{
	if (!VALID_INDEX(controller_index, k_number_of_controllers))
	{
		return;
	}

	// profiles are signed in again on every map change: only read the save when the profile changed
	s_achievements_profile* profile = &g_achievements_profiles[controller_index];
	if (profile->enumerated_file_index != (int32)enumerated_file_index)
	{
		profile->enumerated_file_index = (int32)enumerated_file_index;
		profile->unlocked = 0;

		if (!ENUMERATED_INDEX_IS_DEFAULT_SAVE(enumerated_file_index))
		{
			s_saved_game_achievements save = {};
			if (saved_games_async_helper_read_saved_game_bin(k_achievements_bin_name, enumerated_file_index, (int8*)&save, sizeof(save))
				&& save.signature == k_achievements_save_signature
				&& save.version == k_achievements_save_version)
			{
				profile->unlocked = save.unlocked & k_achievements_all_mask;
			}
		}
		profile->unlocked |= k_achievements_default_unlocked_mask;
	}

	achievements_list_refresh();
	return;
}

void achievements_profile_sign_out(e_controller_index controller_index)
{
	if (!VALID_INDEX(controller_index, k_number_of_controllers))
	{
		return;
	}

	g_achievements_profiles[controller_index].enumerated_file_index = NONE;
	g_achievements_profiles[controller_index].unlocked = 0;
	achievements_list_refresh();
	return;
}

const char* achievements_popup_get_current(void)
{
	return g_achievement_popups.empty() ? NULL : k_achievement_strings[g_achievement_popups.front()];
}

int32 achievements_popup_get_current_index(void)
{
	return g_achievement_popups.empty() ? NONE : g_achievement_popups.front();
}

void achievements_popup_pop(void)
{
	if (!g_achievement_popups.empty())
	{
		g_achievement_popups.pop_front();
	}
	return;
}

e_controller_index achievements_get_player_controller(void)
{
	const s_achievements_profile* profile = achievements_profile_get_primary();
	return profile ? (e_controller_index)(profile - g_achievements_profiles) : k_no_controller;
}

uint64 achievements_get_unlocked(void)
{
	const s_achievements_profile* profile = achievements_profile_get_primary();
	return profile ? profile->unlocked : g_achievements_unsaved;
}

const char* achievements_get_string(int32 achievement_index)
{
	return VALID_INDEX(achievement_index, k_achievement_type_count) ? k_achievement_strings[achievement_index] : "";
}

void achievements_get_diagnostics(char* buffer, uint32 buffer_size)
{
	uint8* manager = achievements_manager_data();
	int32 enabled = 0;
	int32 multiplayer = 0;
	int32 multiplayer_enabled = 0;
	int32 achieved = 0;
	if (manager != NULL)
	{
		for (int32 index = 0; index < k_achievement_type_count; index++)
		{
			const uint8* tracker = achievements_manager_tracker(manager, index);
			if (tracker == NULL)
			{
				continue;
			}
			const bool tracker_enabled = tracker[k_achievement_tracker_enabled_offset] != 0;
			enabled += tracker_enabled ? 1 : 0;
			achieved += tracker[k_achievement_tracker_achieved_offset] != 0 ? 1 : 0;
			if (tracker[k_achievement_tracker_multiplayer_offset] != 0)
			{
				multiplayer++;
				multiplayer_enabled += tracker_enabled ? 1 : 0;
			}
		}
	}

	_snprintf_s(buffer, buffer_size, _TRUNCATE,
		"manager %s, earning %d, signed in %d\n"
		"trackers enabled %d of %d, multiplayer enabled %d of %d, completed %d\n"
		"checks: level chosen %u, saved game %u, game start %u\n"
		"earned reports %u, multiplayer enabled by fix %u",
		manager ? "found" : "missing",
		manager ? manager[k_achievement_manager_earning_offset] : -1,
		manager ? manager[k_achievement_manager_signed_in_offset] : -1,
		enabled, (int32)k_achievement_type_count, multiplayer_enabled, multiplayer, achieved,
		g_achievements_live_checks[0], g_achievements_live_checks[1], g_achievements_live_checks[2],
		g_achievements_queued, g_achievements_multiplayer_enabled);
	return;
}
