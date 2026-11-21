#include "stdafx.h"
#include "screen_button_glyphs_setting.h"

#include "interface/user_interface.h"
#include "interface/user_interface_memory.h"
#include "interface/user_interface_screen_widget_definition.h"
#include "interface/user_interface_shared_globals.h"
#include "cache/cache_files.h"
#include "interface/user_interface_widget_text.h"
#include "memory/data.h"
#include "text/font_group.h"

#include "H2MOD/Modules/Shell/Config.h"

/* constants */

// uses the safe area menu tags, like the splitscreen menu
#define k_button_glyphs_menu_screen_id _screen_safe_area

static const char k_button_glyphs_edit_list_name[] = "button glyphs edit list";

static const wchar_t k_button_glyphs_header_string[] = L"Button Glyphs";
static const wchar_t k_button_glyphs_description_string[] = L"Choose the controller button icons shown in menus and button prompts.";

static const wchar_t* const k_button_glyphs_options_string[k_button_glyph_style_count] =
{
	L"Original Xbox",
	L"Xbox 360",
	L"Modern",
};

/* private code */

static e_button_glyph_style button_glyphs_get_style(void)
{
	return IN_RANGE(H2Config_button_glyphs, 0, k_button_glyph_style_count - 1) ? (e_button_glyph_style)H2Config_button_glyphs : _button_glyph_style_xbox_360;
}

// the thumbstick settings screen's controller picture (the thumbsticks bitmap: frame 0 the Xbox 360 controller, frame 1
// the original Xbox's Duke), as its tag has each of its bitmap blocks, so the picture can follow the glyph style
#define k_thumbstick_settings_screen_tag "ui\\screens\\game_shell\\settings_screen\\player_profile\\thumbstick_settings"
#define k_thumbsticks_bitmap_tag "ui\\screens\\game_shell\\settings_screen\\player_profile\\thumbsticks"
enum
{
	k_thumbsticks_duke_frame = 1,
	k_maximum_thumbstick_pictures = 32,
};

struct s_thumbstick_picture
{
	s_bitmap_block_reference* block;
	int16 initial_sprite_frame;
};
static s_thumbstick_picture g_thumbstick_pictures[k_maximum_thumbstick_pictures];
static int32 g_thumbstick_picture_count = 0;

/* public code */

void button_glyphs_controller_pictures_apply_on_map_load(void)
{
	g_thumbstick_picture_count = 0;
	const datum screen_index = tag_loaded(_tag_group_user_interface_screen_widget_definition, k_thumbstick_settings_screen_tag);
	const datum thumbsticks = tag_loaded(_tag_group_bitmap, k_thumbsticks_bitmap_tag);
	if (screen_index == NONE || thumbsticks == NONE)
	{
		return;
	}

	s_user_interface_screen_widget_definition* screen = user_interface_screen_widget_definition_get(screen_index);
	for (int32 pane_index = 0; pane_index < screen->panes.count; pane_index++)
	{
		s_window_pane_reference* pane = user_interface_widget_pane_get(&screen->panes, pane_index);
		for (int32 index = 0; index < pane->bitmap_blocks.count && g_thumbstick_picture_count < k_maximum_thumbstick_pictures; index++)
		{
			s_bitmap_block_reference* block = user_interface_widget_pane_get_bitmap(&pane->bitmap_blocks, index);
			if (block->bitmap_tag.index == thumbsticks)
			{
				g_thumbstick_pictures[g_thumbstick_picture_count].block = block;
				g_thumbstick_pictures[g_thumbstick_picture_count].initial_sprite_frame = block->initial_sprite_frame;
				g_thumbstick_picture_count++;
			}
		}
	}
	button_glyphs_controller_pictures_update();
	return;
}

void button_glyphs_controller_pictures_update(void)
{
	// the Original Xbox glyphs: the Duke on every pane; otherwise each pane's own picture
	const bool original_xbox = button_glyphs_get_style() == _button_glyph_style_original_xbox;
	for (int32 index = 0; index < g_thumbstick_picture_count; index++)
	{
		g_thumbstick_pictures[index].block->initial_sprite_frame = original_xbox ? (int16)k_thumbsticks_duke_frame : g_thumbstick_pictures[index].initial_sprite_frame;
	}
	return;
}

c_button_glyphs_edit_list::c_button_glyphs_edit_list(uint16 user_flags) :
	c_list_widget(user_flags),
	m_slot(this, &c_button_glyphs_edit_list::handle_item_pressed_event)
{
	m_list_data = ui_list_data_new(k_button_glyphs_edit_list_name, k_button_glyph_style_count, sizeof(datum));
	data_make_valid(m_list_data);

	for (int32 i = 0; i < m_list_data->maximum_count; ++i)
	{
		datum_new(m_list_data);
	}

	linker_type2.link(&m_slot);
	return;
}

c_list_item_widget* c_button_glyphs_edit_list::get_list_items()
{
	return m_list_items;
}

int32 c_button_glyphs_edit_list::get_list_items_count()
{
	return k_no_of_visible_items_for_button_glyphs;
}

void c_button_glyphs_edit_list::update_list_items(c_list_item_widget* item, int32 skin_index)
{
	ASSERT(item);

	c_text_widget* item_text = item->try_find_text_widget(_default_list_skin_text_main);
	const int32 item_index = DATUM_INDEX_TO_ABSOLUTE_INDEX(item->get_last_data_index());
	if (item_text && IN_RANGE(item_index, 0, k_button_glyph_style_count - 1))
	{
		item_text->set_text(k_button_glyphs_options_string[item_index]);
	}
	return;
}

void c_button_glyphs_edit_list::handle_item_pressed_event(s_event_record* const& event, datum* pitem_index)
{
	const int32 item_index = DATUM_INDEX_TO_ABSOLUTE_INDEX(*pitem_index);
	if (IN_RANGE(item_index, 0, k_button_glyph_style_count - 1) && item_index != H2Config_button_glyphs)
	{
		H2Config_button_glyphs = item_index;
		SaveH2Config();
		font_table_set_button_glyph_style((e_button_glyph_style)item_index);
		button_glyphs_controller_pictures_update();
	}

	user_interface_back_out_from_channel(this->get_parent_channel(), this->get_parent_render_window());
	return;
}


//
// c_screen_button_glyphs_menu class starts here
//


c_screen_button_glyphs_menu::c_screen_button_glyphs_menu(e_user_interface_channel_type channel_type, e_user_interface_render_window window_index, uint16 user_flags) :
	c_screen_with_menu(k_button_glyphs_menu_screen_id, channel_type, window_index, user_flags, &m_button_glyphs_edit_list),
	m_button_glyphs_edit_list(user_flags)
{
}

void c_screen_button_glyphs_menu::initialize(c_screen_parameters* parameters)
{
	c_screen_with_menu::initialize(parameters);

	c_text_widget* header = get_screen_header_text();
	c_text_widget* subheader = get_screen_subheader_text();

	if (header)
	{
		header->set_text(k_button_glyphs_header_string);
	}

	if (subheader)
	{
		subheader->set_text(k_button_glyphs_description_string);
	}
	return;
}

void c_screen_button_glyphs_menu::post_initialize()
{
	c_screen_with_menu::post_initialize();
	m_button_glyphs_edit_list.set_focused_item_index(button_glyphs_get_style());
	return;
}

const void* c_screen_button_glyphs_menu::load_proc() const
{
	return &c_screen_button_glyphs_menu::load;
}

void* c_screen_button_glyphs_menu::load(c_screen_parameters* parameters)
{
	c_screen_button_glyphs_menu* screen;

	void* pool = ui_pool_allocate_space(sizeof(c_screen_button_glyphs_menu), 0);
	if (pool)
	{
		screen = new (pool) c_screen_button_glyphs_menu(
			parameters->get_channel_type(),
			parameters->get_window_index(),
			parameters->get_user_flags()
		);

		screen->m_allocated = true;
		user_interface_register_screen_to_channel(screen, parameters);
	}
	else
	{
		screen = nullptr;
	}

	return screen;
}

const wchar_t* c_screen_button_glyphs_menu::get_header_string()
{
	return k_button_glyphs_header_string;
}

const wchar_t* c_screen_button_glyphs_menu::get_option_string()
{
	return k_button_glyphs_options_string[button_glyphs_get_style()];
}
