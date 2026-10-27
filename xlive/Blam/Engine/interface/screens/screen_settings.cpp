#include "stdafx.h"
#include "screen_settings.h"

#include "screen_audio_settings.h"
#include "screen_video_settings.h"
#include "screen_about_dialog.h"
#include "screen_network_adapter.h"
#include "screen_variant_game_engine_type.h"
#include "screen_single_player_profile_select.h"

#include "bitmaps/bitmap_group.h"
#include "cache/cache_files.h"
#include "interface/user_interface.h"
#include "interface/user_interface_memory.h"
#include "interface/user_interface_networking.h"
#include "interface/user_interface_widget_text.h"
#include "interface//user_interface_bitmap_block.h"
#include "interface/user_interface_screen_widget_definition.h"
#include "interface/screens/screen_cartographer_menus.h"
#include "networking/network_event.h"
#include "networking/panorama/panorama_presence.h"


#include "tag_files/tag_loader/tag_injection.h"

/* macro defines */

#define k_settings_list_name "settings list"

/* enums */

enum e_settings_list_items : uint16
{
	//h2x
	//_item_player_profile,
	//_item_variant,

	//h2v
	_item_player_profile,
	_item_variant,
	_item_video,
	_item_audio,
	_item_network,
	_item_about,

	// LAN edition: Cartographer settings entry removed
	k_total_no_of_settings_list_items
};

enum e_settings_screen_panes
{
	_settings_pane_player_profile,
	_settings_pane_variant,
	_settings_pane_video,
	_settings_pane_audio,
	_settings_pane_network,
	_settings_pane_about,

	//custom addition
	_settings_pane_guide,
	k_total_no_of_settings_pane,

	k_total_no_of_settings_pane_orignal = 6,
	k_number_of_addition_panes_for_settings_screen = k_total_no_of_settings_pane- k_total_no_of_settings_pane_orignal
};

enum e_screen_pane_texts
{
	_pane_about_text_description = 0,
	_pane_about_text_flavor_text33,
	_pane_about_text_flavor_text34,
	k_screen_pane_about_texts_count,


	_pane_guide_text_description = 0,
	_pane_guide_text_flavor_text33,
	_pane_guide_text_flavor_text34,

	//custom addition
	_pane_guide_text_custom_flavor_text1,
	_pane_guide_text_custom_flavor_text2,
	k_screen_pane_guide_texts_count,

	k_total_no_of_guide_texts_orignal = 3,
	k_number_of_addition_texts_for_guide_pane = k_screen_pane_guide_texts_count - k_total_no_of_guide_texts_orignal
};

enum e_screen_pane_bitmaps
{
	_pane_about_bitmap_settings_screen_bitmap = 0,
	_pane_about_bitmap_ul_06,
	_pane_about_bitmap_br_03,
	k_screen_pane_about_bitmaps_count,

	_pane_guide_bitmap_settings_screen_bitmap = 0,
	_pane_guide_bitmap_ul_06,
	_pane_guide_bitmap_br_03,
	k_screen_pane_guide_bitmaps_count,
};

enum e_settings_cartographer_bitmap_type
{
	_settings_cartographer_bitmap_type_default = 0,
	k_number_of_cartographer_variant_bitmap_types,
};

/* globals */

datum cartographer_bitmap_datum = NONE;

c_settings_list::c_settings_list(int16 user_flags):
	c_list_widget(user_flags),
	m_slot(this, &c_settings_list::handle_item_pressed_event)
{
	//never actually set true?
	field_464 = false;

	//we dont need s_list_item_datum here as no of list items remain same
	m_list_data = ui_list_data_new(k_settings_list_name, k_total_no_of_settings_list_items, sizeof(datum));
	data_make_valid(m_list_data);

	for (int32 i = 0; i < m_list_data->maximum_count; ++i)
	{
		datum_new(m_list_data);
	}

	linker_type2.link(&m_slot);
}


c_list_item_widget* c_settings_list::get_list_items()
{
	return m_list_items;
}

int32 c_settings_list::get_list_items_count()
{
	//return INVOKE_TYPE(0x21E9BF, 0x0, int(__thiscall*)(c_settings_list*), this);
	// LAN edition: original count (6, +1 when field_464 is set), as halo2.exe 0x21E9BF
	return (field_464 != false) + 6;
}

void c_settings_list::update_list_items(c_list_item_widget* item, int32 skin_index)
{
	//return INVOKE_TYPE(0x21E9CE, 0x0, void(__thiscall*)(c_settings_list*, c_list_widget*, int), this, item, skin_index);

	ASSERT(item);
	c_text_widget* item_text = item->try_find_text_widget(_default_list_skin_text_main);

	if (item_text)
	{
		switch (DATUM_INDEX_TO_ABSOLUTE_INDEX(item->get_last_data_index()))
		{
		case _item_player_profile:
			item_text->set_text_from_string_id(_string_id_settings_player_profile);
			break;
		case _item_variant:
			item_text->set_text_from_string_id(_string_id_settings_variant);
			break;
		case _item_video:
			item_text->set_text_from_string_id(_string_id_settings_video);
			break;
		case _item_audio:
			item_text->set_text_from_string_id(_string_id_settings_audio);
			break;
		case _item_network:
			item_text->set_text_from_string_id(_string_id_settings_network);
			break;
		case _item_about:
			item_text->set_text_from_string_id(_string_id_settings_about);
			break;
		}
	}
}

void c_settings_list::handle_item_pressed_event(s_event_record* const& event, datum* pitem_index)
{
	//return INVOKE_TYPE(0x21F70A, 0x0, void(__thiscall*)(c_settings_list*, s_event_record**, long*), this, pevent, pitem_index);

	int16 item_absolute_index = DATUM_INDEX_TO_ABSOLUTE_INDEX(*pitem_index);

	switch (item_absolute_index)
	{
	case _item_player_profile:
		handle_item_player_profile(event);
		break;
	case _item_variant:
		handle_item_variant(event);
		break;
	case _item_video:
		handle_item_video(event);
		break;
	case _item_audio:
		handle_item_audio(event);
		break;
	case _item_network:
		handle_item_network(event);
		break;
	case _item_about:
		handle_item_about(event);
		break;
	}
}

void c_settings_list::handle_item_player_profile(
	s_event_record* const& event)
{
	c_screen_parameters params;

	params.initialize_default_user(FLAG(event->controller), _user_interface_channel_type_gameshell_screen, _window_4, c_screen_single_player_profile_select_fancy::load);
	params.execute_load_function();
	return;
}

void c_settings_list::handle_item_variant(
	s_event_record* const& event)
{
	c_screen_parameters params;

	params.initialize_default_user(FLAG(event->controller), _user_interface_channel_type_gameshell_screen, _window_4, c_screen_game_engine_category::load_for_settings);
	params.execute_load_function();
	return;
}

void c_settings_list::handle_item_video(
	s_event_record* const& event)
{
	c_screen_parameters params;

	params.initialize_default_user(FLAG(event->controller), _user_interface_channel_type_gameshell_screen, _window_4, c_screen_video_settings::load);
	params.execute_load_function();
	return;
}

void c_settings_list::handle_item_audio(
	s_event_record* const& event)
{
	c_screen_parameters params;
	
	params.initialize_default_user(FLAG(event->controller), _user_interface_channel_type_gameshell_screen, _window_4, c_screen_audio_settings::load);
	params.execute_load_function();
	return;
}

void c_settings_list::handle_item_network(
	s_event_record* const& event)
{
	c_screen_parameters params;

	params.initialize_default_user(FLAG(event->controller), _user_interface_channel_type_gameshell_dialog, _window_4, c_screen_network_adapter_menu::load);
	params.execute_load_function();
	return;
}

void c_settings_list::handle_item_about(s_event_record* const& event)
{
	c_screen_parameters params;

	params.initialize_default_user(FLAG(event->controller), _user_interface_channel_type_gameshell_dialog, _window_4, c_screen_about_dialog_menu::load);
	params.execute_load_function();
	return;
}

void c_settings_list::handle_item_guide(s_event_record* const& event)
{
	ui_load_cartographer_guide_menu();
	return;
}


//
// c_screen_settings class starts here
// 


c_screen_settings::c_screen_settings(e_user_interface_channel_type channel_type, e_user_interface_render_window window_index, int16 user_flags) :
	c_screen_with_menu(_screen_settings, channel_type, window_index, user_flags, &m_settings_list),
	m_settings_list(user_flags)
{
	user_interface_globals_commit_edit_profile_changes();
	user_interface_squad_clear_match_playlist();
}


void c_screen_settings::post_initialize_button_keys()
{
	// LAN edition: Cartographer guide pane removed
	c_screen_widget::post_initialize_button_keys();
}


const void* c_screen_settings::load_proc(void) const
{
	return &c_screen_settings::load;
}

void* c_screen_settings::load(c_screen_parameters* parameters)
{
	//return INVOKE(0xB071, 0x0, c_screen_settings::load, parameters);

	c_screen_settings* screen;

	void* pool = ui_pool_allocate_space(sizeof(c_screen_settings), 0);
	if (pool)
	{
		screen = new (pool) c_screen_settings(
			parameters->get_channel_type(),
			parameters->get_window_index(),
			parameters->get_user_flags()
		);

		screen->m_allocated = true;

		panorama_presence_set(_context_presence_settings);
		user_interface_register_screen_to_channel(screen, parameters);
	}
	else
	{
		screen = 0;
	}

	return screen;
}

void c_screen_settings::apply_patches_on_map_load()
{
	// LAN edition: no extra Cartographer pane is added to the settings screen tag
	return;
}
