#pragma once

#include "interface/user_interface_widget_list.h"
#include "interface/user_interface_widget_list_item.h"

/* macro defines */

// LAN edition: 7 (Achievements added); the main menu screen is allocated with room for the extra row and its tag's list
// shows 7 rows (c_main_menu_list::apply_instance_patches / apply_patches_on_map_load)
#define k_no_of_visible_items_for_main_menu 7

/* classes */

class c_main_menu_list : protected c_list_widget
{
protected:
	c_list_item_widget m_list_items[k_no_of_visible_items_for_main_menu];
	c_slot2<c_main_menu_list, s_event_record*, datum> m_slot;

	void handle_item_pressed_event(s_event_record* const& event, datum* pitem_index);
	void handle_item_campaign(s_event_record* const& event);
	void handle_item_xbox_live(s_event_record* const& event);
	void handle_item_splitscreen(s_event_record* const& event);
	void handle_item_system_link(s_event_record* const& event);
	void handle_item_achievements(s_event_record* const& event);
	void handle_item_settings(s_event_record* const& event);
	//void handle_item_guide(s_event_record* const& event);
	void handle_item_quit(s_event_record* const& event);

public:
	c_main_menu_list(int16 user_flags);
	
	void c_main_menu_list_ctor(int16 user_flags);
	static void apply_instance_patches();
	static void apply_patches_on_map_load();

	// c_main_menu_list virtual functions

	virtual ~c_main_menu_list() = default;
	virtual c_list_item_widget* get_list_items() override;
	virtual int32 get_list_items_count() override;
	virtual void update_list_items(c_list_item_widget* item, int32 skin_index) override;

};
ASSERT_STRUCT_SIZE(c_main_menu_list, 0x3E0 + 0x84);	// the original's 6 rows + 1


