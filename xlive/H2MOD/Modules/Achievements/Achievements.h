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
void achievements_popup_pop(void);
