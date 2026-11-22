#include "stdafx.h"
#include "screen_controller_settings_menu.h"

#include "input/input_abstraction.h"
#include "interface/user_interface.h"
#include "interface/user_interface_widget_list.h"
#include "interface/user_interface_widget_list_item.h"
#include "interface/user_interface_widget_text.h"
#include "saved_games/player_profile.h"


/* enums */

enum e_controller_list_items : uint16
{
	_item_thumbstick_settings,
	_item_button_settings,
	_item_look_sensitivity,
	_item_invert_look,
	_item_auto_center,
	_item_vibration,
	_item_restore,
	k_total_no_of_controller_list_items
};


CLASS_HOOK_DECLARE_LABEL(c_controller_settings_edit_list_update_list_items, c_controller_settings_edit_list::update_list_items);
void c_controller_settings_edit_list::update_list_items(c_list_item_widget* item, int32 skin_index)
{
	//INVOKE_TYPE(0x2551EC, 0x0, void(__thiscall*)(c_controller_settings_edit_list*, c_list_item_widget*, int32), this, item, skin_index);

	ASSERT(item);
	s_saved_game_player_profile* player_profile = user_interface_globals_get_edit_player_profile();
	c_text_widget* header_text = item->try_find_text_widget(_settings_list_skin_text_header);
	c_text_widget* value_text = item->try_find_text_widget(_settings_list_skin_text_value);

	string_id header_string = _string_id_invalid;
	string_id value_string = _string_id_empty_string;
	switch (DATUM_INDEX_TO_ABSOLUTE_INDEX(item->get_last_data_index()))
	{
			case _item_thumbstick_settings:
				header_string = _string_id_thumbstick_settings;
				value_string = _string_id_empty_string;
				switch (player_profile->input_preferences.controller_thumbstick_layout)
				{
				case _joystick_preset_default:
					value_string = _string_id_default;
					break;
				case _joystick_preset_south_paw:
					value_string = _string_id_southpaw;
					break;
				case _joystick_preset_legacy:
					value_string = _string_id_legacy;
					break;
				case _joystick_preset_legacy_south_paw:
					value_string = _string_id_legacy_southpaw;
					break;
				}
				break;
			case _item_button_settings:
				header_string = _string_id_button_settings;
				value_string = _string_id_empty_string;
				switch (player_profile->input_preferences.controller_button_layout)
				{
				case _button_preset_default:
				case _button_preset_jumpy:	// LAN edition: no Jumpy layout (the default one is used)
					value_string = _string_id_default;
					break;
				case _button_preset_south_paw:
					value_string = _string_id_south_paw;
					break;
				case _button_preset_boxer:
					value_string = _string_id_boxer;
					break;
				case _button_preset_green_thumb:
					value_string = _string_id_green_thumb;
					break;
				}
				break;
			case _item_look_sensitivity:
				header_string = _string_id_look_sensitivity;
				value_string = _string_id_empty_string;

				switch (PIN(player_profile->input_preferences.controller_sensitivity, 1, 10))
				{
				case 1:
					value_string = _string_id_look_sensitivity_1;
					break;
				case 2:
					value_string = _string_id_look_sensitivity_2;
					break;
				case 3:
					value_string = _string_id_look_sensitivity_3;
					break;
				case 4:
					value_string = _string_id_look_sensitivity_4;
					break;
				case 5:
					value_string = _string_id_look_sensitivity_5;
					break;
				case 6:
					value_string = _string_id_look_sensitivity_6;
					break;
				case 7:
					value_string = _string_id_look_sensitivity_7;
					break;
				case 8:
					value_string = _string_id_look_sensitivity_8;
					break;
				case 9:
					value_string = _string_id_look_sensitivity_9;
					break;
				case 10:
					value_string = _string_id_look_sensitivity_10;
					break;				
				}
				break;
			case _item_invert_look:
				header_string = _string_id_invert_look;
				value_string = player_profile->input_preferences.flags.test(_saved_game_profile_input_preference_bit_controller_look_inversion) ? _string_id_enable : _string_id_disable;
				break;
			case _item_auto_center:
				header_string = _string_id_auto_center;
				value_string = player_profile->input_preferences.flags.test(_saved_game_profile_input_preference_bit_controller_auto_look_centering) ? _string_id_enable : _string_id_disable;
				break;
			case _item_vibration:
				header_string = _string_id_vibration;
				value_string = player_profile->input_preferences.flags.test(_saved_game_profile_input_preference_bit_vibration_disabled) ? _string_id_off : _string_id_on;
				break;
			case _item_restore:
				header_string = _string_id_restore_controller_defaults;
				break;
			default:
				header_string = _string_id_invalid;
				value_string = _string_id_empty_string;
	}

	if (header_text)
	{
		header_text->set_text_from_string_id(header_string);
	}

	if (value_text)
	{
		value_text->set_text_from_string_id(value_string);
	}
	return;
}

__declspec(naked) void jmp_c_controller_settings_edit_list_update_list_items()
{
	CLASS_HOOK_JMP(c_controller_settings_edit_list_update_list_items, c_controller_settings_edit_list::update_list_items);
}

void c_controller_settings_edit_list::apply_instance_patches()
{
	//Replace orignal vtable function with ours
	WritePointer(Memory::GetAddress(0x3DA594), jmp_c_controller_settings_edit_list_update_list_items);
}
