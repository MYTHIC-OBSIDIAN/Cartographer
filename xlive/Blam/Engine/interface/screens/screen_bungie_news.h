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
