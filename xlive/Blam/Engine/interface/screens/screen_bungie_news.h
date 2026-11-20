#pragma once

/* classes */

// the Xbox Live menu (H2V replaced the Xbox "xbox_live_main_menu" screen with this one)
class c_screen_bungie_news
{
public:
	static void* __cdecl load(class c_screen_parameters* parameters);

	// LAN edition: "XBOX LIVE" header, LIVE stats on the left of the panel, bungie.net news on the right
	static void apply_patches(void);
	static void apply_patches_on_map_load(void);
};

// LAN edition: the lobby opened by the Xbox Live menu's Create New Game backs out to the Xbox Live menu (the game only
// went back there when signed in to Xbox Live, else to the sign-in screen)
bool screen_bungie_news_lobby_opened_from_menu(void);
void screen_bungie_news_lobby_opened_from_menu_clear(void);

// loads the Xbox Live menu after leaving the lobby, and clears the flag above
void screen_bungie_news_recover_from_lobby(void);
