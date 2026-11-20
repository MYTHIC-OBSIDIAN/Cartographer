#include "stdafx.h"
#include "screen_y_menu.h"

#include "cache/cache_files.h"
#include "interface/user_interface.h"
#include "interface/user_interface_bitmap_block.h"
#include "interface/user_interface_widget_text.h"
#include "input/controllers.h"
#include "interface/user_interface_controller.h"
#include "interface/user_interface_memory.h"
#include "interface/user_interface_screen_widget_definition.h"
#include "interface/user_interface_shared_globals.h"
#include "tag_files/tag_loader/tag_injection.h"
#include "text/text_group.h"

/* constants */

#define k_y_menu_widget_tag "ui\\screens\\game_shell\\online_y_menu\\online_y_menu"
#define k_y_menu_collapsed_widget_tag "ui\\screens\\game_shell\\online_y_menu\\online_y_menu_collapsed"
#define k_y_menu_string_list_tag "ui\\screens\\game_shell\\online_y_menu\\online_y_menu"
#define k_bungie_news_widget_tag "ui\\screens\\game_shell\\bungie_news\\bungie_news"

enum
{
	// Friends, Clan Members, Recent Players
	k_y_menu_pane_count = 3,
	k_y_menu_maximum_panes = 6,
	k_keyboard_component_letter_y = 0x59,

	// Xbox (640x480) tags to Halo 2 Vista's screen
	k_y_menu_scale = 2,
	k_y_menu_xbox_background_left = -319,		// online_y_menu pane 0 bitmap 0 (y_menu_bkd) before scaling

	// the collapsed tab on the Xbox Live menu (1 unit = 1.21 pixels at 2576x1447: the screen's left edge is at -1064, and
	// the menu's bottom bar line at -527)
	k_y_menu_collapsed_tab_left = -1064,
	k_y_menu_collapsed_tab_top = -467,

	// online_y_menu panes: the "<gamertag>'s Friends" style header
	k_y_menu_header_text_index = 2,
	k_y_menu_maximum_bitmaps = 16,
};
static const real_vector2d k_y_menu_bitmap_scale = { 2.f, 2.f };

/* globals */

static string_id g_y_menu_collapsed_string = _string_id_invalid;
static bool g_y_menu_collapsed_text_loaded = false;
static wchar_t g_y_menu_collapsed_text[128] = L"";
static int32 g_y_menu_collapsed_text_index[16];			// per Xbox Live menu pane: the tab text's block index
static int32 g_y_menu_collapsed_bitmap_index[16];		// per Xbox Live menu pane: the tab bitmap's block index

/* public code */

void* __cdecl c_screen_y_menu::load(c_screen_parameters* parameters)
{
	c_screen_y_menu* screen = NULL;
	uint8* buffer = ui_pool_allocate_space(sizeof(c_screen_y_menu), 0);
	if (buffer != NULL)
	{
		screen = new (buffer) c_screen_y_menu(parameters->get_channel_type(), parameters->get_window_index(), parameters->get_user_flags());
		screen->m_allocated = true;
	}
	user_interface_register_screen_to_channel(screen, parameters);
	return screen;
}

void c_screen_y_menu::open(void)
{
	// a dialog over the screen it was opened from, which comes back when it closes
	c_screen_parameters parameters;
	parameters.initialize_default_user(
		user_interface_controller_get_signed_in_controllers_mask(),
		_user_interface_channel_type_gameshell_dialog,
		_window_4,
		c_screen_y_menu::load);
	parameters.execute_load_function();
	return;
}

c_screen_y_menu::c_screen_y_menu(e_user_interface_channel_type channel_type, e_user_interface_render_window window_index, uint16 user_flags) :
	c_screen_widget(_screen_y_menugameshell, channel_type, window_index, user_flags)
{
	return;
}

bool c_screen_y_menu::handle_event(s_event_record* event)
{
	// left and right: the next tab, wrapping round
	if (event->type == _user_interface_event_type_tab_left || event->type == _user_interface_event_type_tab_right)
	{
		int32 pane_index = this->m_pane_index + (event->type == _user_interface_event_type_tab_right ? 1 : -1);
		pane_index = (pane_index + k_y_menu_pane_count) % k_y_menu_pane_count;
		this->switch_panes(&pane_index);
		this->update_pane_widgets();
		return true;
	}

	// Y closes it like B
	if (event->type == _user_interface_event_type_gamepad_button_pressed && event->component == _controller_component_button_y)
	{
		s_event_record back = *event;
		back.component = _controller_component_button_b;
		return c_screen_widget::handle_event(&back);
	}
	if (event->type == _user_interface_event_type_keyboard_button_pressed && (int32)event->component == k_keyboard_component_letter_y)
	{
		s_event_record back = *event;
		back.component = _user_interface_keyboard_component_button_escape;
		return c_screen_widget::handle_event(&back);
	}

	return c_screen_widget::handle_event(event);
}

void c_screen_y_menu::initialize(c_screen_parameters* parameters)
{
	// the tag has a pane per tab and no tab view, so nothing is loaded here: switch_panes loads the first tab's texts and
	// bitmaps
	s_interface_expected_screen_layout layout;
	csmemset(&layout, 0, sizeof(layout));
	layout.panes_count = k_y_menu_pane_count;

	datum widget_datum = user_interface_get_screen_tag_index_by_id(this->m_screen_id);
	if (widget_datum == NONE)
	{
		widget_datum = tag_loaded(_tag_group_user_interface_screen_widget_definition, k_y_menu_widget_tag);
	}
	if (widget_datum != NONE)
	{
		this->verify_and_load_from_layout(widget_datum, &layout);
	}
	this->setup_children();

	int32 first_pane = 0;
	this->switch_panes(&first_pane);
	this->update_pane_widgets();
	return;
}

void c_screen_y_menu::update_pane_widgets(void)
{
	// the Xbox bitmaps are half the resolution of Halo 2 Vista's: drawn at twice their size
	for (int32 index = 0; index < k_y_menu_maximum_bitmaps; index++)
	{
		c_bitmap_widget* bitmap = this->try_find_bitmap_widget(index);
		if (bitmap != NULL)
		{
			bitmap->set_render_scale(&k_y_menu_bitmap_scale);
		}
	}

	// the header's gamertag placeholder ("<gamertag>'s Friends"): the signed-in profile's name
	c_text_widget* header = this->try_find_screen_text(k_y_menu_header_text_index);
	if (header != NULL)
	{
		const wchar_t* current = header->get_interface()->get_raw_string();
		const wchar_t* name = user_interface_controller_get_player_profile_name(first_controller());
		if (current != NULL && name != NULL)
		{
			wchar_t replaced[256];
			int32 length = 0;
			bool changed = false;
			for (const wchar_t* character = current; *character != L'\0' && length < (int32)NUMBEROF(replaced) - 1; character++)
			{
				const bool placeholder = *character < L' ' || (*character >= 0xE000 && *character <= 0xF8FF);
				if (placeholder && !changed)
				{
					for (const wchar_t* letter = name; *letter != L'\0' && length < (int32)NUMBEROF(replaced) - 1; letter++)
					{
						replaced[length++] = *letter;
					}
					changed = true;
				}
				else if (!placeholder)
				{
					replaced[length++] = *character;
				}
			}
			replaced[length] = L'\0';
			if (changed)
			{
				header->set_text(replaced);
			}
		}
	}
	return;
}

void c_screen_y_menu::scale_collapsed_tab(c_screen_widget* xbox_live_menu, int32 pane_index)
{
	if (!VALID_INDEX(pane_index, (int32)NUMBEROF(g_y_menu_collapsed_bitmap_index)) || g_y_menu_collapsed_bitmap_index[pane_index] == NONE)
	{
		return;
	}
	c_bitmap_widget* bitmap = xbox_live_menu->try_find_bitmap_widget(g_y_menu_collapsed_bitmap_index[pane_index]);
	if (bitmap != NULL)
	{
		bitmap->set_render_scale(&k_y_menu_bitmap_scale);
	}
	return;
}

const void* c_screen_y_menu::load_proc(void) const
{
	return c_screen_y_menu::load;
}

int32 c_screen_y_menu::collapsed_tab_text_index(int32 pane_index)
{
	return VALID_INDEX(pane_index, (int32)NUMBEROF(g_y_menu_collapsed_text_index)) ? g_y_menu_collapsed_text_index[pane_index] : NONE;
}

const wchar_t* c_screen_y_menu::collapsed_tab_text(void)
{
	if (!g_y_menu_collapsed_text_loaded && g_y_menu_collapsed_string != _string_id_invalid)
	{
		const datum strings = tag_loaded(_tag_group_multilingual_unicode_string_list, k_y_menu_string_list_tag);
		if (strings != NONE)
		{
			c_maximum_interface_text text;
			string_list_get_normal_string(strings, g_y_menu_collapsed_string, &text);
			wcsncpy_s(g_y_menu_collapsed_text, text.get_string(), _TRUNCATE);
		}
		g_y_menu_collapsed_text_loaded = true;
	}
	return g_y_menu_collapsed_text;
}

void c_screen_y_menu::apply_patches_on_map_load(void)
{
	g_y_menu_collapsed_string = _string_id_invalid;
	g_y_menu_collapsed_text_loaded = false;
	g_y_menu_collapsed_text[0] = L'\0';
	for (int32 index = 0; index < (int32)NUMBEROF(g_y_menu_collapsed_text_index); index++)
	{
		g_y_menu_collapsed_text_index[index] = NONE;
		g_y_menu_collapsed_bitmap_index[index] = NONE;
	}

	// the Y menu: its lists would need Xbox Live players, so its panes keep only their texts and bitmaps; it was made for
	// the Xbox's 640x480 screen, so like the 4-way sign-in its positions are doubled for Halo 2 Vista's screen (and its
	// bitmaps drawn at twice their size, see y_menu_scale_bitmaps)
	const datum y_menu_datum = tag_loaded(_tag_group_user_interface_screen_widget_definition, k_y_menu_widget_tag);
	if (y_menu_datum != NONE)
	{
		s_user_interface_screen_widget_definition* y_menu = user_interface_screen_widget_definition_get(y_menu_datum);
		// panes can share blocks: each is scaled once (and only the original Xbox tag, so never twice)
		int32 scaled_blocks[3 * k_y_menu_maximum_panes] = {};
		int32 scaled_count = 0;
		auto first_time = [&](int32 block) -> bool
		{
			for (int32 index = 0; index < scaled_count; index++)
			{
				if (scaled_blocks[index] == block)
				{
					return false;
				}
			}
			if (scaled_count < (int32)NUMBEROF(scaled_blocks))
			{
				scaled_blocks[scaled_count++] = block;
			}
			return true;
		};

		const bool original = y_menu->panes.count > 0
			&& user_interface_widget_pane_get(&y_menu->panes, 0)->bitmap_blocks.count > 0
			&& TAG_BLOCK_GET_ELEMENT(&user_interface_widget_pane_get(&y_menu->panes, 0)->bitmap_blocks, 0, s_bitmap_block_reference)->topleft.x == k_y_menu_xbox_background_left;
		for (int32 pane_index = 0; pane_index < y_menu->panes.count; pane_index++)
		{
			s_window_pane_reference* pane = user_interface_widget_pane_get(&y_menu->panes, pane_index);
			pane->list_block.count = 0;
			pane->buttons.count = 0;
			if (!original)
			{
				continue;
			}

			if (first_time(pane->text_blocks.data))
			{
				for (int32 index = 0; index < pane->text_blocks.count; index++)
				{
					rectangle2d_scale(&TAG_BLOCK_GET_ELEMENT(&pane->text_blocks, index, s_text_block_reference)->text_bounds, k_y_menu_scale);
				}
			}
			if (first_time(pane->bitmap_blocks.data))
			{
				for (int32 index = 0; index < pane->bitmap_blocks.count; index++)
				{
					point2d_scale(&TAG_BLOCK_GET_ELEMENT(&pane->bitmap_blocks, index, s_bitmap_block_reference)->topleft, k_y_menu_scale);
				}
			}
			if (first_time(pane->player_blocks.data))
			{
				for (int32 index = 0; index < pane->player_blocks.count; index++)
				{
					s_player_block_reference* players = TAG_BLOCK_GET_ELEMENT(&pane->player_blocks, index, s_player_block_reference);
					point2d_scale(&players->bottomleft, k_y_menu_scale);
					players->row_height = (int16)(players->row_height * k_y_menu_scale);
					players->column_width = (int16)(players->column_width * k_y_menu_scale);
				}
			}
		}
	}

	// the collapsed Y menu: its tab and text at the bottom left of every pane of the Xbox Live menu
	const datum collapsed_datum = tag_loaded(_tag_group_user_interface_screen_widget_definition, k_y_menu_collapsed_widget_tag);
	const datum bungie_news_datum = tag_loaded(_tag_group_user_interface_screen_widget_definition, k_bungie_news_widget_tag);
	if (collapsed_datum == NONE || bungie_news_datum == NONE)
	{
		return;
	}
	s_user_interface_screen_widget_definition* collapsed = user_interface_screen_widget_definition_get(collapsed_datum);
	if (collapsed->panes.count < 1)
	{
		return;
	}
	s_window_pane_reference* collapsed_pane = user_interface_widget_pane_get(&collapsed->panes, 0);
	if (collapsed_pane->bitmap_blocks.count < 1 || collapsed_pane->text_blocks.count < 1)
	{
		return;
	}
	// the Xbox tab sat in the bottom left corner of the 640x480 screen: here at the left edge of a 16:9 screen, its bottom
	// line along the Xbox Live menu's bottom bar, its text placed as on the Xbox (doubled)
	s_bitmap_block_reference tab = *TAG_BLOCK_GET_ELEMENT(&collapsed_pane->bitmap_blocks, 0, s_bitmap_block_reference);
	s_text_block_reference tab_text = *TAG_BLOCK_GET_ELEMENT(&collapsed_pane->text_blocks, 0, s_text_block_reference);
	g_y_menu_collapsed_string = tab_text.string;
	const point2d xbox_tab = tab.topleft;
	const rectangle2d xbox_text = tab_text.text_bounds;
	tab.topleft.x = k_y_menu_collapsed_tab_left;
	tab.topleft.y = k_y_menu_collapsed_tab_top;
	tab_text.text_bounds.top = (int16)(k_y_menu_collapsed_tab_top + (xbox_text.top - xbox_tab.y) * k_y_menu_scale);
	tab_text.text_bounds.bottom = (int16)(k_y_menu_collapsed_tab_top + (xbox_text.bottom - xbox_tab.y) * k_y_menu_scale);
	tab_text.text_bounds.left = (int16)(k_y_menu_collapsed_tab_left + (xbox_text.left - xbox_tab.x) * k_y_menu_scale);
	tab_text.text_bounds.right = (int16)(k_y_menu_collapsed_tab_left + (xbox_text.right - xbox_tab.x) * k_y_menu_scale);

	s_user_interface_screen_widget_definition* bungie_news = user_interface_screen_widget_definition_get(bungie_news_datum);
	for (int32 pane_index = 0; pane_index < bungie_news->panes.count; pane_index++)
	{
		// each pane gets its own copies of its blocks, with the tab and its text added at the end (drawn last)
		s_window_pane_reference* pane = user_interface_widget_pane_get(&bungie_news->panes, pane_index);

		const int32 bitmap_index = pane->bitmap_blocks.count;
		tag_injection_extend_block(&pane->bitmap_blocks, sizeof(s_bitmap_block_reference), 1);
		*TAG_BLOCK_GET_ELEMENT(&pane->bitmap_blocks, bitmap_index, s_bitmap_block_reference) = tab;
		if (pane_index < (int32)NUMBEROF(g_y_menu_collapsed_bitmap_index))
		{
			g_y_menu_collapsed_bitmap_index[pane_index] = bitmap_index;
		}

		const int32 text_index = pane->text_blocks.count;
		tag_injection_extend_block(&pane->text_blocks, sizeof(s_text_block_reference), 1);
		*TAG_BLOCK_GET_ELEMENT(&pane->text_blocks, text_index, s_text_block_reference) = tab_text;
		if (pane_index < (int32)NUMBEROF(g_y_menu_collapsed_text_index))
		{
			g_y_menu_collapsed_text_index[pane_index] = text_index;
		}
	}
	return;
}
