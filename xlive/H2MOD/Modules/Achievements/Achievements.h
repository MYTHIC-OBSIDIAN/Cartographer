#pragma once
#include "input/controllers.h"

void AchievementUnlock(unsigned long long xuid, int achievement_id, XOVERLAPPED* pOverlapped);
void GetAchievements(unsigned long long xuid);

extern bool g_achievement_list[42];
extern std::unordered_map<std::string, bool> AchievementMap;

// LAN edition: achievements are earned offline and saved per player profile (cartographer_achievements.bin next to
// the profile), with an "Achievement Unlocked" pop-up for each new one
void achievements_apply_patches(void);
void achievements_profile_sign_in(e_controller_index controller_index, uint32 enumerated_file_index);
void achievements_profile_sign_out(e_controller_index controller_index);

// pop-up queue, oldest first: "title|description", or NULL when empty
const char* achievements_popup_get_current(void);
int32 achievements_popup_get_current_index(void);		// NONE when empty
void achievements_popup_pop(void);

// for the Home menu: the achievement player's controller (k_no_controller when no profile is signed in), their unlocked
// achievements (bit n = e_achievement_type n) and "title|description" for an achievement
e_controller_index achievements_get_player_controller(void);
uint64 achievements_get_unlocked(void);
const char* achievements_get_string(int32 achievement_index);

// what the achievement hooks have seen (Home menu > Diagnostics)
void achievements_get_diagnostics(char* buffer, uint32 buffer_size);
