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
// the original Xbox's Duke), so the picture can follow the glyph style. On the Duke the thumbsticks are elsewhere: each
// stick's arrows and labels move with it
#define k_thumbstick_settings_screen_tag "ui\\screens\\game_shell\\settings_screen\\player_profile\\thumbstick_settings"
#define k_thumbsticks_bitmap_tag "ui\\screens\\game_shell\\settings_screen\\player_profile\\thumbsticks"
#define k_thumbstick_arrows_bitmap_tag "ui\\screens\\game_shell\\settings_screen\\player_profile\\thumbstick_arrows"
enum
{
	k_thumbsticks_xbox_360_frame = 0,
	k_thumbsticks_duke_frame = 1,
	k_maximum_thumbstick_pictures = 16,
	k_maximum_thumbstick_moved_widgets = 128,

	// the panes' descriptions are above this; the stick labels below it
	k_thumbstick_label_maximum_top = 200,
};

// the Duke's sticks from the Xbox 360 controller's (the pictures share the same top left), in user interface units
static const point2d k_duke_left_stick_offset = { -16, -43 };
static const point2d k_duke_right_stick_offset = { 44, -79 };

struct s_thumbstick_picture
{
	s_bitmap_block_reference* block;
	int16 initial_sprite_frame;
};
static s_thumbstick_picture g_thumbstick_pictures[k_maximum_thumbstick_pictures];
static int32 g_thumbstick_picture_count = 0;

// a stick's arrows (bitmap) or label (text) on a pane with the Xbox 360 controller, and where its tag has it
struct s_thumbstick_moved_widget
{
	s_bitmap_block_reference* bitmap;
	s_text_block_reference* text;
	rectangle2d bounds;			// text: its bounds; bitmap: its top left in the first two
	bool right_stick;
};
static s_thumbstick_moved_widget g_thumbstick_moved_widgets[k_maximum_thumbstick_moved_widgets];
static int32 g_thumbstick_moved_widget_count = 0;

static void thumbstick_moved_widget_add(s_bitmap_block_reference* bitmap, s_text_block_reference* text, bool right_stick)
{
	if (g_thumbstick_moved_widget_count >= k_maximum_thumbstick_moved_widgets)
	{
		return;
	}
	s_thumbstick_moved_widget* widget = &g_thumbstick_moved_widgets[g_thumbstick_moved_widget_count++];
	widget->bitmap = bitmap;
	widget->text = text;
	widget->right_stick = right_stick;
	if (text != NULL)
	{
		widget->bounds = text->text_bounds;
	}
	else
	{
		widget->bounds = { bitmap->topleft.x, bitmap->topleft.y, 0, 0 };
	}
	return;
}

/* public code */

void button_glyphs_controller_pictures_apply_on_map_load(void)
{
	g_thumbstick_picture_count = 0;
	g_thumbstick_moved_widget_count = 0;
	const datum thumbstick_arrows = tag_loaded(_tag_group_bitmap, k_thumbstick_arrows_bitmap_tag);
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
		bool xbox_360_pane = false;
		for (int32 index = 0; index < pane->bitmap_blocks.count && g_thumbstick_picture_count < k_maximum_thumbstick_pictures; index++)
		{
			s_bitmap_block_reference* block = user_interface_widget_pane_get_bitmap(&pane->bitmap_blocks, index);
			if (block->bitmap_tag.index == thumbsticks)
			{
				g_thumbstick_pictures[g_thumbstick_picture_count].block = block;
				g_thumbstick_pictures[g_thumbstick_picture_count].initial_sprite_frame = block->initial_sprite_frame;
				g_thumbstick_picture_count++;
				xbox_360_pane = xbox_360_pane || block->initial_sprite_frame == k_thumbsticks_xbox_360_frame;
			}
		}

		// a pane with the Xbox 360 controller: its sticks' arrows (the left most the left stick's) and labels (left of
		// the screen's centre the left stick's), moved with the sticks on the Duke
		if (!xbox_360_pane || thumbstick_arrows == NONE)
		{
			continue;
		}
		s_bitmap_block_reference* arrows[2] = { NULL, NULL };
		for (int32 index = 0; index < pane->bitmap_blocks.count; index++)
		{
			s_bitmap_block_reference* block = user_interface_widget_pane_get_bitmap(&pane->bitmap_blocks, index);
			if (block->bitmap_tag.index != thumbstick_arrows)
			{
				continue;
			}
			if (arrows[0] == NULL)
			{
				arrows[0] = block;
			}
			else if (arrows[1] == NULL)
			{
				arrows[1] = block;
			}
		}
		if (arrows[0] != NULL && arrows[1] != NULL)
		{
			const bool first_is_left = arrows[0]->topleft.x <= arrows[1]->topleft.x;
			thumbstick_moved_widget_add(arrows[0], NULL, !first_is_left);
			thumbstick_moved_widget_add(arrows[1], NULL, first_is_left);
		}
		for (int32 index = 0; index < pane->text_blocks.count; index++)
		{
			s_text_block_reference* text = user_interface_widget_pane_get_text(&pane->text_blocks, index);
			if (text->text_bounds.top > k_thumbstick_label_maximum_top)
			{
				continue;
			}
			thumbstick_moved_widget_add(NULL, text, text->text_bounds.left + text->text_bounds.right > 0);
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
	for (int32 index = 0; index < g_thumbstick_moved_widget_count; index++)
	{
		const s_thumbstick_moved_widget* widget = &g_thumbstick_moved_widgets[index];
		const point2d offset = !original_xbox ? point2d{ 0, 0 } : widget->right_stick ? k_duke_right_stick_offset : k_duke_left_stick_offset;
		if (widget->text != NULL)
		{
			widget->text->text_bounds.top = (int16)(widget->bounds.top + offset.y);
			widget->text->text_bounds.bottom = (int16)(widget->bounds.bottom + offset.y);
			widget->text->text_bounds.left = (int16)(widget->bounds.left + offset.x);
			widget->text->text_bounds.right = (int16)(widget->bounds.right + offset.x);
		}
		else
		{
			widget->bitmap->topleft.x = (int16)(widget->bounds.v[0] + offset.x);
			widget->bitmap->topleft.y = (int16)(widget->bounds.v[1] + offset.y);
		}
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
