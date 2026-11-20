#pragma once
#include "interface/user_interface_widget_window.h"

/*
* LAN edition: the Y menu (online_y_menu), Halo 2's Xbox Live players overlay with its Friends, Clan Members and Recent
* Players tabs, and its collapsed tab ("Xbox Live Players") at the bottom left of the Xbox Live menu.
*
* Halo 2 Vista removed the Y menu's code, but its layouts, bitmaps and strings are still in mainmenu.map
* (ui\screens\game_shell\online_y_menu). This rebuilds the screen around them: Y on the Xbox Live menu opens it, left and
* right change tabs, Y or B closes it. There are no Xbox Live friends, clans or recent players offline, so its lists
* are left out and the tabs are empty.
*/

class c_screen_y_menu : protected c_screen_widget
{
public:
	static void* __cdecl load(c_screen_parameters* parameters);

	// opens the Y menu over the current game shell screen
	static void open(void);

	// the Y menu tag without its lists, and the collapsed tab added to the Xbox Live menu (bungie_news)
	static void apply_patches_on_map_load(void);

	// the collapsed tab's text for the Xbox Live menu ("Xbox Live Players"), empty when not found
	static const wchar_t* collapsed_tab_text(void);

	// the collapsed tab's text block index in an Xbox Live menu pane, NONE when it wasn't added
	static int32 collapsed_tab_text_index(int32 pane_index);

	c_screen_y_menu(e_user_interface_channel_type channel_type, e_user_interface_render_window window_index, uint16 user_flags);

	virtual ~c_screen_y_menu() = default;
	virtual bool handle_event(s_event_record* event) override;
	virtual void initialize(c_screen_parameters* parameters) override;
	virtual const void* load_proc(void) const override;
};
