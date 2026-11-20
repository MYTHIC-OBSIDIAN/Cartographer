#include "stdafx.h"
#include "screen_y_menu.h"

#include "cache/cache_files.h"
#include "interface/hud.h"
#include "interface/user_interface_bitmap_block.h"
#include "interface/user_interface_controller.h"
#include "interface/user_interface_memory.h"
#include "interface/user_interface_screen_widget_definition.h"
#include "interface/user_interface_shared_globals.h"
#include "interface/user_interface_widget_text.h"
#include "memory/data.h"
#include "rasterizer/rasterizer_globals.h"
#include "render/render.h"
#include "tag_files/tag_loader/tag_injection.h"
#include "text/text_group.h"

/* constants */

#define k_y_menu_widget_tag "ui\\screens\\game_shell\\online_y_menu\\online_y_menu"
#define k_y_menu_collapsed_widget_tag "ui\\screens\\game_shell\\online_y_menu\\online_y_menu_collapsed"
#define k_y_menu_friends_options_widget_tag "ui\\screens\\game_shell\\online_y_menu\\friends_options_dialog"
#define k_y_menu_clan_options_widget_tag "ui\\screens\\game_shell\\online_y_menu\\clan_options_dialog"
#define k_y_menu_string_list_tag "ui\\screens\\game_shell\\online_y_menu\\online_y_menu"
#define k_bungie_news_widget_tag "ui\\screens\\game_shell\\bungie_news\\bungie_news"
#define k_rank_icons_bitmap_tag "ui\\global_bitmaps\\rank_icons"

// the Y menu's own (Xbox resolution) bitmaps; the others (ui\global_bitmaps) are Halo 2 Vista's
static const char* const k_y_menu_xbox_bitmap_tags[] =
{
	"ui\\screens\\game_shell\\online_y_menu\\y_menu_bkd",
	"ui\\screens\\game_shell\\online_y_menu\\tabs",
	"ui\\screens\\game_shell\\online_y_menu\\y_menu_compact",
};

enum
{
	// Friends, Clan Members, Recent Players
	k_y_menu_pane_count = 3,
	k_y_menu_friends_pane = 0,
	k_y_menu_clan_pane = 1,
	k_y_menu_maximum_panes = 6,
	k_y_menu_maximum_bitmaps = 32,
	k_y_menu_maximum_xbox_live_menu_panes = 16,
	k_keyboard_component_letter_x = 0x58,
	k_keyboard_component_letter_y = 0x59,

	// Xbox (640x480) tags to Halo 2 Vista's screen
	k_y_menu_scale = 2,
	k_y_menu_xbox_background_left = -319,		// online_y_menu pane 0 bitmap 0 (y_menu_bkd) before scaling
	k_y_menu_xbox_dialog_left = -144,			// the options dialogs' bitmap 0 (ul_07) before scaling

	// online_y_menu panes: the "<gamertag>'s Friends" style header
	k_y_menu_header_text_index = 2,
	// online_y_menu panes: the collapsed tab's "<Y> FRIENDS" (title font), and the text block added for its glyph
	k_y_menu_pane_tab_text_index = 3,
	k_y_menu_pane_text_count = 8,
	k_y_menu_pane_tab_glyph_text_index = k_y_menu_pane_text_count,

	// the collapsed tab (y_menu_compact) is 60 pixels high, drawn at twice its size
	k_y_menu_collapsed_tab_height = 60 * k_y_menu_scale,

	// "<Y> FRIENDS": the controller glyph is drawn in the body text font (whose glyphs change with the Button Glyphs
	// setting), the label after it in the title font
	k_y_menu_tab_glyph_width = 40,
};
static const real_vector2d k_y_menu_xbox_bitmap_scale = { 2.f, 2.f };

// the options dialogs' items (their strings are in the Y menu's string list, without string ids for these)
static const wchar_t* const k_y_menu_friends_options[] =
{
	L"Add Friend",
	L"View Your Stats",
	L"Message All Friends",
	L"Invite Online Friends",
};
static const wchar_t* const k_y_menu_clan_options[] =
{
	L"Add Member",
	L"View Clan Stats",
	L"Message Clan",
	L"Invite Online Members",
	L"Leave Clan",
	L"Create Clan",
};
static const char k_y_menu_options_list_name[] = "y menu options list";

/* globals */

static string_id g_y_menu_collapsed_string = _string_id_invalid;
static bool g_y_menu_collapsed_text_loaded = false;
static wchar_t g_y_menu_collapsed_text[128] = L"";
// per Xbox Live menu pane: the collapsed tab's bitmap and text block indices
static int32 g_y_menu_collapsed_bitmap_index[k_y_menu_maximum_xbox_live_menu_panes];
static int32 g_y_menu_collapsed_text_index[k_y_menu_maximum_xbox_live_menu_panes];
static int32 g_y_menu_collapsed_glyph_index[k_y_menu_maximum_xbox_live_menu_panes];
// the collapsed tab's text, relative to the tab's top left
static rectangle2d g_y_menu_collapsed_text_offset = {};
// per Y menu pane: its Xbox bitmaps' block indices
static uint32 g_y_menu_xbox_bitmaps[k_y_menu_maximum_panes];

/* private code */

static bool y_menu_character_is_glyph(wchar_t character)
{
	return character >= 0xE000 && character <= 0xF8FF;
}

// "<glyph> LABEL": the glyph in its own text (body text font), the label without it; nothing when already split
static void y_menu_split_glyph(c_text_widget* label, c_text_widget* glyph, const wchar_t* full_text)
{
	if (label == NULL || glyph == NULL || full_text == NULL || !y_menu_character_is_glyph(full_text[0]))
	{
		return;
	}
	const wchar_t glyph_text[2] = { full_text[0], L'\0' };
	const wchar_t* label_text = full_text + 1;
	while (*label_text == L' ')
	{
		label_text++;
	}
	glyph->set_text(glyph_text);
	label->set_text(label_text);
	return;
}

// a text block's copy for the glyph of "<glyph> LABEL": the body text font, as wide as the glyph; the label moves right
static s_text_block_reference y_menu_glyph_text_block(s_text_block_reference* label)
{
	s_text_block_reference glyph = *label;
	glyph.custom_font = custom_font_body_text;
	glyph.text_bounds.right = (int16)(glyph.text_bounds.left + k_y_menu_tab_glyph_width + 20);
	label->text_bounds.left = (int16)(label->text_bounds.left + k_y_menu_tab_glyph_width);
	return glyph;
}

static bool y_menu_bitmap_is_xbox(datum bitmap)
{
	for (int32 index = 0; index < (int32)NUMBEROF(k_y_menu_xbox_bitmap_tags); index++)
	{
		if (bitmap != NONE && bitmap == tag_loaded(_tag_group_bitmap, k_y_menu_xbox_bitmap_tags[index]))
		{
			return true;
		}
	}
	return false;
}

// the screen's left and bottom edges in user interface units (the origin is the screen's centre, y up)
static bool y_menu_screen_edges(int16* out_left, int16* out_bottom)
{
	const real32 scale = *get_ui_scale();
	const window_bound* bounds = get_user_window_bounds(0);
	real32 width = 0.f;
	real32 height = 0.f;
	if (bounds != NULL)
	{
		width = (real32)bounds->rasterizer_camera.viewport_bounds.right;
		height = (real32)bounds->rasterizer_camera.window_bounds.bottom;
	}
	if (width <= 0.f || height <= 0.f)
	{
		const s_rasterizer_globals* rasterizer_globals = rasterizer_globals_get();
		width = (real32)rasterizer_globals->resolution_x;
		height = (real32)rasterizer_globals->resolution_y;
	}
	if (scale <= 0.f || width <= 0.f || height <= 0.f)
	{
		return false;
	}
	*out_left = (int16)floorf(-width * .5f / scale);
	*out_bottom = (int16)floorf(-height * .5f / scale);
	return true;
}

static void y_menu_widget_move_to(c_user_interface_widget* widget, int16 left, int16 top)
{
	rectangle2d bounds;
	widget->get_bounds(&bounds);
	if (bounds.left == left && bounds.top == top)
	{
		return;
	}
	const int16 dx = (int16)(left - bounds.left);
	const int16 dy = (int16)(top - bounds.top);
	bounds.left = (int16)(bounds.left + dx);
	bounds.right = (int16)(bounds.right + dx);
	bounds.top = (int16)(bounds.top + dy);
	bounds.bottom = (int16)(bounds.bottom + dy);
	widget->set_bounds(&bounds);
	return;
}

static const wchar_t* y_menu_collapsed_tab_text(void)
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

static void y_menu_set_text(c_text_widget* text, const wchar_t* value)
{
	if (text == NULL || value == NULL)
	{
		return;
	}
	const wchar_t* current = text->get_interface()->get_raw_string();
	if (current == NULL || wcscmp(current, value) != 0)
	{
		text->set_text(value);
	}
	return;
}

// doubles a tag's positions (each block once: panes can share them)
static void y_menu_scale_widget_tag(s_user_interface_screen_widget_definition* widget, bool remove_lists)
{
	int32 scaled_blocks[4 * k_y_menu_maximum_panes] = {};
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

	for (int32 pane_index = 0; pane_index < widget->panes.count; pane_index++)
	{
		s_window_pane_reference* pane = user_interface_widget_pane_get(&widget->panes, pane_index);
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
		if (!remove_lists && first_time(pane->list_block.data))
		{
			for (int32 index = 0; index < pane->list_block.count; index++)
			{
				point2d_scale(&TAG_BLOCK_GET_ELEMENT(&pane->list_block, index, s_list_reference)->bottom_left, k_y_menu_scale);
			}
		}
		if (remove_lists)
		{
			pane->list_block.count = 0;
			pane->buttons.count = 0;
		}
	}
	return;
}

/* public code: the Y menu */

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

void c_screen_y_menu::update(void)
{
	c_screen_widget::update();
	this->update_header();

	c_text_widget* tab_label = this->try_find_screen_text(k_y_menu_pane_tab_text_index);
	if (tab_label != NULL)
	{
		y_menu_split_glyph(tab_label, this->try_find_screen_text(k_y_menu_pane_tab_glyph_text_index), tab_label->get_interface()->get_raw_string());
	}
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
		this->scale_pane_bitmaps();
		this->update_header();
		return true;
	}

	const bool gamepad = event->type == _user_interface_event_type_gamepad_button_pressed;
	const bool keyboard = event->type == _user_interface_event_type_keyboard_button_pressed;

	// X: the Friends or Clan tab's options
	if ((gamepad && event->component == _controller_component_button_x) || (keyboard && (int32)event->component == k_keyboard_component_letter_x))
	{
		if (this->m_pane_index == k_y_menu_friends_pane || this->m_pane_index == k_y_menu_clan_pane)
		{
			c_screen_y_menu_options::open(this->m_pane_index == k_y_menu_clan_pane ? _y_menu_options_clan : _y_menu_options_friends, event->controller);
			return true;
		}
	}

	// Y closes it like B
	if (gamepad && event->component == _controller_component_button_y)
	{
		s_event_record back = *event;
		back.component = _controller_component_button_b;
		return c_screen_widget::handle_event(&back);
	}
	if (keyboard && (int32)event->component == k_keyboard_component_letter_y)
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
	this->scale_pane_bitmaps();
	this->update_header();
	return;
}

const void* c_screen_y_menu::load_proc(void) const
{
	return c_screen_y_menu::load;
}

void c_screen_y_menu::scale_pane_bitmaps(void)
{
	const uint32 xbox_bitmaps = VALID_INDEX(this->m_pane_index, k_y_menu_maximum_panes) ? g_y_menu_xbox_bitmaps[this->m_pane_index] : 0;
	for (int32 index = 0; index < k_y_menu_maximum_bitmaps; index++)
	{
		if (TEST_BIT(xbox_bitmaps, index))
		{
			c_bitmap_widget* bitmap = this->try_find_bitmap_widget(index);
			if (bitmap != NULL)
			{
				bitmap->set_render_scale(&k_y_menu_xbox_bitmap_scale);
			}
		}
	}
	return;
}

void c_screen_y_menu::update_header(void)
{
	// the header's gamertag placeholder (U+E420 in "\xE420's Friends"): the signed-in profile's name
	c_text_widget* header = this->try_find_screen_text(k_y_menu_header_text_index);
	if (header == NULL)
	{
		return;
	}
	const wchar_t* current = header->get_interface()->get_raw_string();
	const wchar_t* name = user_interface_controller_get_player_profile_name(first_controller());
	if (current == NULL || name == NULL)
	{
		return;
	}

	wchar_t replaced[256];
	int32 length = 0;
	bool changed = false;
	for (const wchar_t* character = current; *character != L'\0' && length < (int32)NUMBEROF(replaced) - 1; character++)
	{
		const bool placeholder = *character >= 0xE000 && *character <= 0xF8FF;
		if (!placeholder)
		{
			replaced[length++] = *character;
			continue;
		}
		for (const wchar_t* letter = name; *letter != L'\0' && length < (int32)NUMBEROF(replaced) - 1; letter++)
		{
			replaced[length++] = *letter;
		}
		changed = true;
	}
	replaced[length] = L'\0';
	if (changed)
	{
		header->set_text(replaced);
	}
	return;
}

void c_screen_y_menu::update_collapsed_tab(c_screen_widget* xbox_live_menu, int32 pane_index)
{
	if (!VALID_INDEX(pane_index, k_y_menu_maximum_xbox_live_menu_panes) || g_y_menu_collapsed_bitmap_index[pane_index] == NONE)
	{
		return;
	}

	int16 left;
	int16 bottom;
	const bool edges = y_menu_screen_edges(&left, &bottom);
	const int16 top = (int16)(bottom + k_y_menu_collapsed_tab_height);

	c_bitmap_widget* tab = xbox_live_menu->try_find_bitmap_widget(g_y_menu_collapsed_bitmap_index[pane_index]);
	if (tab != NULL)
	{
		tab->set_render_scale(&k_y_menu_xbox_bitmap_scale);
		if (edges)
		{
			y_menu_widget_move_to(tab, left, top);
		}
	}

	c_text_widget* text = xbox_live_menu->try_find_screen_text(g_y_menu_collapsed_text_index[pane_index]);
	c_text_widget* glyph = g_y_menu_collapsed_glyph_index[pane_index] != NONE ? xbox_live_menu->try_find_screen_text(g_y_menu_collapsed_glyph_index[pane_index]) : NULL;
	if (text != NULL)
	{
		const wchar_t* full_text = y_menu_collapsed_tab_text();
		if (glyph != NULL && y_menu_character_is_glyph(full_text[0]))
		{
			const wchar_t* current = glyph->get_interface()->get_raw_string();
			if (current == NULL || current[0] != full_text[0] || current[1] != L'\0')
			{
				y_menu_split_glyph(text, glyph, full_text);
			}
		}
		else
		{
			y_menu_set_text(text, full_text);
		}
		if (edges)
		{
			y_menu_widget_move_to(text, (int16)(left + g_y_menu_collapsed_text_offset.left + (glyph != NULL ? k_y_menu_tab_glyph_width : 0)), (int16)(top + g_y_menu_collapsed_text_offset.top));
			if (glyph != NULL)
			{
				y_menu_widget_move_to(glyph, (int16)(left + g_y_menu_collapsed_text_offset.left), (int16)(top + g_y_menu_collapsed_text_offset.top));
			}
		}
	}
	return;
}

void c_screen_y_menu::apply_patches_on_map_load(void)
{
	g_y_menu_collapsed_string = _string_id_invalid;
	g_y_menu_collapsed_text_loaded = false;
	g_y_menu_collapsed_text[0] = L'\0';
	for (int32 index = 0; index < k_y_menu_maximum_xbox_live_menu_panes; index++)
	{
		g_y_menu_collapsed_bitmap_index[index] = NONE;
		g_y_menu_collapsed_text_index[index] = NONE;
		g_y_menu_collapsed_glyph_index[index] = NONE;
	}
	csmemset(g_y_menu_xbox_bitmaps, 0, sizeof(g_y_menu_xbox_bitmaps));

	// the Y menu: its lists and player blocks would need Xbox Live players and the Level column's rank icon a selected
	// player, so they're left out
	const datum y_menu_datum = tag_loaded(_tag_group_user_interface_screen_widget_definition, k_y_menu_widget_tag);
	if (y_menu_datum != NONE)
	{
		s_user_interface_screen_widget_definition* y_menu = user_interface_screen_widget_definition_get(y_menu_datum);
		const bool original = y_menu->panes.count > 0
			&& user_interface_widget_pane_get(&y_menu->panes, 0)->bitmap_blocks.count > 0
			&& TAG_BLOCK_GET_ELEMENT(&user_interface_widget_pane_get(&y_menu->panes, 0)->bitmap_blocks, 0, s_bitmap_block_reference)->topleft.x == k_y_menu_xbox_background_left;
		if (original)
		{
			y_menu_scale_widget_tag(y_menu, true);
		}

		const datum rank_icons = tag_loaded(_tag_group_bitmap, k_rank_icons_bitmap_tag);
		for (int32 pane_index = 0; pane_index < y_menu->panes.count; pane_index++)
		{
			s_window_pane_reference* pane = user_interface_widget_pane_get(&y_menu->panes, pane_index);
			pane->list_block.count = 0;
			pane->buttons.count = 0;
			pane->player_blocks.count = 0;

			// the tab's "<Y> FRIENDS": its glyph in a text block of its own (each pane gets its own copy of its blocks)
			if (pane->text_blocks.count == k_y_menu_pane_text_count)
			{
				tag_injection_extend_block(&pane->text_blocks, sizeof(s_text_block_reference), 1);
				s_text_block_reference* label = TAG_BLOCK_GET_ELEMENT(&pane->text_blocks, k_y_menu_pane_tab_text_index, s_text_block_reference);
				*TAG_BLOCK_GET_ELEMENT(&pane->text_blocks, k_y_menu_pane_tab_glyph_text_index, s_text_block_reference) = y_menu_glyph_text_block(label);
			}
			for (int32 index = 0; index < pane->bitmap_blocks.count; index++)
			{
				s_bitmap_block_reference* bitmap = TAG_BLOCK_GET_ELEMENT(&pane->bitmap_blocks, index, s_bitmap_block_reference);
				if (rank_icons != NONE && bitmap->bitmap_tag.index == rank_icons)
				{
					bitmap->bitmap_tag.index = NONE;
				}
				if (pane_index < k_y_menu_maximum_panes && index < k_y_menu_maximum_bitmaps && y_menu_bitmap_is_xbox(bitmap->bitmap_tag.index))
				{
					g_y_menu_xbox_bitmaps[pane_index] |= FLAG(index);
				}
			}
		}
	}

	// the options dialogs: their bitmaps (the dialog frame) are Halo 2 Vista's, so only their positions are doubled
	const char* const options_tags[] = { k_y_menu_friends_options_widget_tag, k_y_menu_clan_options_widget_tag };
	for (int32 index = 0; index < (int32)NUMBEROF(options_tags); index++)
	{
		const datum options_datum = tag_loaded(_tag_group_user_interface_screen_widget_definition, options_tags[index]);
		if (options_datum == NONE)
		{
			continue;
		}
		s_user_interface_screen_widget_definition* options = user_interface_screen_widget_definition_get(options_datum);
		const bool original = options->panes.count > 0
			&& user_interface_widget_pane_get(&options->panes, 0)->bitmap_blocks.count > 0
			&& TAG_BLOCK_GET_ELEMENT(&user_interface_widget_pane_get(&options->panes, 0)->bitmap_blocks, 0, s_bitmap_block_reference)->topleft.x == k_y_menu_xbox_dialog_left;
		if (original)
		{
			y_menu_scale_widget_tag(options, false);
		}
	}

	// the collapsed Y menu: its tab and text at the bottom left of every pane of the Xbox Live menu (placed in the
	// screen's corner each update, see update_collapsed_tab)
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
	s_bitmap_block_reference tab = *TAG_BLOCK_GET_ELEMENT(&collapsed_pane->bitmap_blocks, 0, s_bitmap_block_reference);
	s_text_block_reference tab_text = *TAG_BLOCK_GET_ELEMENT(&collapsed_pane->text_blocks, 0, s_text_block_reference);
	g_y_menu_collapsed_string = tab_text.string;

	// the text where the Xbox had it relative to the tab (doubled); the tab starts at its Xbox place doubled, the corner
	// of a 640x480 screen, until update_collapsed_tab moves both
	const point2d xbox_tab = tab.topleft;
	const rectangle2d xbox_text = tab_text.text_bounds;
	g_y_menu_collapsed_text_offset.top = (int16)((xbox_text.top - xbox_tab.y) * k_y_menu_scale);
	g_y_menu_collapsed_text_offset.bottom = (int16)((xbox_text.bottom - xbox_tab.y) * k_y_menu_scale);
	g_y_menu_collapsed_text_offset.left = (int16)((xbox_text.left - xbox_tab.x) * k_y_menu_scale);
	g_y_menu_collapsed_text_offset.right = (int16)((xbox_text.right - xbox_tab.x) * k_y_menu_scale);
	point2d_scale(&tab.topleft, k_y_menu_scale);
	tab_text.text_bounds.top = (int16)(tab.topleft.y + g_y_menu_collapsed_text_offset.top);
	tab_text.text_bounds.bottom = (int16)(tab.topleft.y + g_y_menu_collapsed_text_offset.bottom);
	tab_text.text_bounds.left = (int16)(tab.topleft.x + g_y_menu_collapsed_text_offset.left);
	tab_text.text_bounds.right = (int16)(tab.topleft.x + g_y_menu_collapsed_text_offset.right);

	s_user_interface_screen_widget_definition* bungie_news = user_interface_screen_widget_definition_get(bungie_news_datum);
	for (int32 pane_index = 0; pane_index < bungie_news->panes.count && pane_index < k_y_menu_maximum_xbox_live_menu_panes; pane_index++)
	{
		// each pane gets its own copies of its blocks, with the tab and its text added at the end (drawn last)
		s_window_pane_reference* pane = user_interface_widget_pane_get(&bungie_news->panes, pane_index);

		const int32 bitmap_index = pane->bitmap_blocks.count;
		tag_injection_extend_block(&pane->bitmap_blocks, sizeof(s_bitmap_block_reference), 1);
		*TAG_BLOCK_GET_ELEMENT(&pane->bitmap_blocks, bitmap_index, s_bitmap_block_reference) = tab;
		g_y_menu_collapsed_bitmap_index[pane_index] = bitmap_index;

		s_text_block_reference label = tab_text;
		const s_text_block_reference glyph = y_menu_glyph_text_block(&label);
		const int32 text_index = pane->text_blocks.count;
		tag_injection_extend_block(&pane->text_blocks, sizeof(s_text_block_reference), 2);
		*TAG_BLOCK_GET_ELEMENT(&pane->text_blocks, text_index, s_text_block_reference) = label;
		*TAG_BLOCK_GET_ELEMENT(&pane->text_blocks, text_index + 1, s_text_block_reference) = glyph;
		g_y_menu_collapsed_text_index[pane_index] = text_index;
		g_y_menu_collapsed_glyph_index[pane_index] = text_index + 1;
	}
	return;
}

/* public code: the options dialogs */

c_y_menu_options_list::c_y_menu_options_list(uint16 user_flags, e_y_menu_options_type type) :
	c_list_widget(user_flags),
	m_slot(this, &c_y_menu_options_list::handle_item_pressed_event),
	m_type(type)
{
	const uint32 item_count = type == _y_menu_options_clan ? NUMBEROF(k_y_menu_clan_options) : NUMBEROF(k_y_menu_friends_options);
	m_list_data = ui_list_data_new(k_y_menu_options_list_name, item_count, sizeof(datum));
	data_make_valid(m_list_data);
	for (int32 index = 0; index < m_list_data->maximum_count; index++)
	{
		datum_new(m_list_data);
	}
	linker_type2.link(&m_slot);
}

c_list_item_widget* c_y_menu_options_list::get_list_items()
{
	return m_list_items;
}

int32 c_y_menu_options_list::get_list_items_count()
{
	// the tags' visible items: 7 (friends_options_dialog), 5 (clan_options_dialog)
	return m_type == _y_menu_options_clan ? 5 : k_y_menu_options_maximum_visible_items;
}

void c_y_menu_options_list::update_list_items(c_list_item_widget* item, int32 skin_index)
{
	c_text_widget* text = item->try_find_text_widget(_default_list_skin_text_main);
	if (text == NULL)
	{
		return;
	}
	const int32 index = DATUM_INDEX_TO_ABSOLUTE_INDEX(item->get_last_data_index());
	if (m_type == _y_menu_options_clan)
	{
		text->set_text(VALID_INDEX(index, (int32)NUMBEROF(k_y_menu_clan_options)) ? k_y_menu_clan_options[index] : L"");
	}
	else
	{
		text->set_text(VALID_INDEX(index, (int32)NUMBEROF(k_y_menu_friends_options)) ? k_y_menu_friends_options[index] : L"");
	}
	return;
}

void c_y_menu_options_list::handle_item_pressed_event(s_event_record* const& event, datum* item_index)
{
	// offline there are no friends or clans to act on: back to the Y menu
	user_interface_back_out_from_channel(this->get_parent_channel(), this->get_parent_render_window());
	return;
}

c_screen_y_menu_options::c_screen_y_menu_options(e_y_menu_options_type type, e_user_interface_channel_type channel_type, e_user_interface_render_window window_index, uint16 user_flags) :
	c_screen_with_menu(type == _y_menu_options_clan ? _screen_clan_options_dialog : _screen_friends_options_dialog, channel_type, window_index, user_flags, &m_options_list),
	m_options_list(user_flags, type),
	m_type(type)
{
	return;
}

void* c_screen_y_menu_options::load_type(e_y_menu_options_type type, c_screen_parameters* parameters)
{
	c_screen_y_menu_options* screen = NULL;
	void* pool = ui_pool_allocate_space(sizeof(c_screen_y_menu_options), 0);
	if (pool != NULL)
	{
		screen = new (pool) c_screen_y_menu_options(type, parameters->get_channel_type(), parameters->get_window_index(), parameters->get_user_flags());
		screen->m_allocated = true;
		user_interface_register_screen_to_channel(screen, parameters);
	}
	return screen;
}

void* c_screen_y_menu_options::load_friends(c_screen_parameters* parameters)
{
	return c_screen_y_menu_options::load_type(_y_menu_options_friends, parameters);
}

void* c_screen_y_menu_options::load_clan(c_screen_parameters* parameters)
{
	return c_screen_y_menu_options::load_type(_y_menu_options_clan, parameters);
}

void c_screen_y_menu_options::open(e_y_menu_options_type type, e_controller_index controller)
{
	c_screen_parameters parameters;
	parameters.initialize_default_user(
		VALID_INDEX(controller, k_number_of_controllers) ? (uint16)FLAG(controller) : user_interface_controller_get_signed_in_controllers_mask(),
		_user_interface_channel_type_gameshell_dialog,
		_window_4,
		type == _y_menu_options_clan ? c_screen_y_menu_options::load_clan : c_screen_y_menu_options::load_friends);
	parameters.execute_load_function();
	return;
}

const void* c_screen_y_menu_options::load_proc() const
{
	return m_type == _y_menu_options_clan ? (const void*)c_screen_y_menu_options::load_clan : (const void*)c_screen_y_menu_options::load_friends;
}
