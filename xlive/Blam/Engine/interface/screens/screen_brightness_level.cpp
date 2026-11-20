#include "stdafx.h"
#include "screen_brightness_level.h"

#include "interface/user_interface.h"
#include "interface/user_interface_screen_widget_definition.h"

// LAN edition: the brightness level list one row (37 units) higher than its tag has it, for this screen only (the tag
// goes back as it was once the screen has loaded, the other screens that borrow the brightness_level dialog keep it)
static s_list_reference* brightness_level_list_get(void)
{
	const datum widget_datum = user_interface_get_screen_tag_index_by_id(_screen_brightness_level);
	if (widget_datum == NONE)
	{
		return NULL;
	}

	s_user_interface_screen_widget_definition* widget = user_interface_screen_widget_definition_get(widget_datum);
	if (widget->panes.count <= 0 || user_interface_widget_pane_get(&widget->panes, 0)->list_block.count <= 0)
	{
		return NULL;
	}
	return TAG_BLOCK_GET_ELEMENT(&user_interface_widget_pane_get(&widget->panes, 0)->list_block, 0, s_list_reference);
}

static void* brightness_level_load_raised(void* (__cdecl* load)(c_screen_parameters*), c_screen_parameters* parameters)
{
	const int16 k_brightness_level_list_raise = 37;
	s_list_reference* list = brightness_level_list_get();
	if (list != NULL)
	{
		list->bottom_left.y = (int16)(list->bottom_left.y + k_brightness_level_list_raise);
	}
	void* screen = load(parameters);
	if (list != NULL)
	{
		list->bottom_left.y = (int16)(list->bottom_left.y - k_brightness_level_list_raise);
	}
	return screen;
}

static void* __cdecl brightness_level_load_original(c_screen_parameters* parameters)
{
	return INVOKE(0x24925C, 0x0, brightness_level_load_original, parameters);
}

static void* __cdecl brightness_level_load_mp_original(c_screen_parameters* parameters)
{
	return INVOKE(0x258C8C, 0x0, brightness_level_load_mp_original, parameters);
}

void* __cdecl c_screen_brightness_level_menu::load(c_screen_parameters* parameters)
{
	return brightness_level_load_raised(brightness_level_load_original, parameters);
}

void* __cdecl c_screen_brightness_level_menu::load_mp(c_screen_parameters* parameters)
{
	return brightness_level_load_raised(brightness_level_load_mp_original, parameters);
}
