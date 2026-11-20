#pragma once
#include "input/controllers.h"
#include "interface/user_interface.h"
#include "interface/user_interface_widget_list.h"
#include "interface/user_interface_widget_list_item.h"
#include "interface/user_interface_widget_window.h"

/*
* LAN edition: the Y menu (online_y_menu), Halo 2's Xbox Live players overlay with its Friends, Clan Members and Recent
* Players tabs, its collapsed tab ("Y FRIENDS") at the bottom left of the Xbox Live menu, and the X options dialogs of its
* Friends and Clan tabs (friends_options_dialog, clan_options_dialog).
*
* Halo 2 Vista removed the Y menu's code, but its layouts, bitmaps and strings are still in mainmenu.map
* (ui\screens\game_shell\online_y_menu). This rebuilds the screens around them: Y on the Xbox Live menu opens it, left
* and right change tabs, X opens a tab's options, Y or B closes it. There are no Xbox Live friends, clans or recent
* players offline, so its lists are left out and the tabs are empty.
*
* The tags were made for the Xbox's 640x480 screen: like the 4-way sign-in their positions are doubled for Halo 2
* Vista's screen, and their own (Xbox resolution) bitmaps are drawn at twice their size.
*/

class c_screen_y_menu : protected c_screen_widget
{
public:
	static void* __cdecl load(c_screen_parameters* parameters);

	// opens the Y menu over the current game shell screen
	static void open(void);

	// the Y menu tags fitted to Halo 2 Vista's screen, and the collapsed tab added to the Xbox Live menu (bungie_news)
	static void apply_patches_on_map_load(void);

	// the collapsed tab on the Xbox Live menu (each update): in the screen's bottom left corner at any resolution, at
	// twice its size, with its text
	static void update_collapsed_tab(c_screen_widget* xbox_live_menu, int32 pane_index);

	c_screen_y_menu(e_user_interface_channel_type channel_type, e_user_interface_render_window window_index, uint16 user_flags);

	virtual ~c_screen_y_menu() = default;
	virtual void update(void) override;
	virtual bool handle_event(s_event_record* event) override;
	virtual void initialize(c_screen_parameters* parameters) override;
	virtual const void* load_proc(void) const override;

private:
	// after a tab loads: its Xbox bitmaps at twice their size
	void scale_pane_bitmaps(void);
	// the gamertag in the tab's header ("<gamertag>'s Friends")
	void update_header(void);
};

// X on the Y menu's Friends or Clan tab: "Xbox Live Options" (Add Friend, View Your Stats, ...) or "Clan Options"
enum e_y_menu_options_type
{
	_y_menu_options_friends = 0,
	_y_menu_options_clan,
	k_y_menu_options_type_count
};

enum
{
	k_y_menu_options_maximum_visible_items = 7
};

class c_y_menu_options_list : public c_list_widget
{
protected:
	c_list_item_widget m_list_items[k_y_menu_options_maximum_visible_items];
	c_slot2<c_y_menu_options_list, s_event_record*, datum> m_slot;
	e_y_menu_options_type m_type;

	void handle_item_pressed_event(s_event_record* const& event, datum* item_index);

public:
	c_y_menu_options_list(uint16 user_flags, e_y_menu_options_type type);

	virtual ~c_y_menu_options_list() = default;
	virtual c_list_item_widget* get_list_items() override;
	virtual int32 get_list_items_count() override;
	virtual void update_list_items(c_list_item_widget* item, int32 skin_index) override;
};

class c_screen_y_menu_options : public c_screen_with_menu
{
protected:
	c_y_menu_options_list m_options_list;
	e_y_menu_options_type m_type;

	static void* load_type(e_y_menu_options_type type, c_screen_parameters* parameters);

public:
	static void* load_friends(c_screen_parameters* parameters);
	static void* load_clan(c_screen_parameters* parameters);
	static void open(e_y_menu_options_type type, e_controller_index controller);

	c_screen_y_menu_options(e_y_menu_options_type type, e_user_interface_channel_type channel_type, e_user_interface_render_window window_index, uint16 user_flags);

	virtual ~c_screen_y_menu_options() = default;
	virtual const void* load_proc() const override;
};
