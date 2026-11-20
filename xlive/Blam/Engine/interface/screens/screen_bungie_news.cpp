#include "stdafx.h"
#include "screen_bungie_news.h"

#include "cache/cache_files.h"
#include "interface/user_interface.h"
#include "interface/user_interface_widget_text.h"
#include "interface/user_interface_widget_window.h"
#include "interface/user_interface_screen_widget_definition.h"
#include "interface/user_interface_shared_globals.h"
#include "tag_files/tag_loader/tag_injection.h"
#include "interface/user_interface_widget_list.h"
#include "interface/user_interface_widget_list_item.h"
#include "H2MOD/GUI/imgui_integration/imgui_handler.h"

/* constants */

// LAN edition: the Xbox Live menu (ui\screens\game_shell\bungie_news\bungie_news, screen 0xF7, vtable halo2.exe 0x79BF84)
// The bottom panel showed "LIVE" with LIVE Stats on the right and the Guide help on the left. Like the Xbox version's
// Xbox Live menu it now shows the bungie.net logo with "Latest news:" and Bungie's last news message on the right,
// with the LIVE stats moved to the left in place of the Guide help.

#define k_bungie_news_widget_tag "ui\\screens\\game_shell\\bungie_news\\bungie_news"
#define k_bungie_news_logo_bitmap_tag "ui\\code_global_bitmaps\\multiplayer49"
#define k_bungie_news_logo_bitmap_index 8	// extra bitmap in the LAN edition mainmenu.map, logo at the top left of 440x414

enum e_bungie_news_text
{
	_bungie_news_text_help = 0,
	_bungie_news_text_panel_header,		// "LIVE" -> "Latest news:"
	_bungie_news_text_live_upsell,
	_bungie_news_text_live_stats_header,
	_bungie_news_text_live_stats_first = _bungie_news_text_live_stats_header,
	_bungie_news_text_live_stats_last = 7,
	_bungie_news_text_guide_help,		// Guide help -> the news message
	k_bungie_news_text_count
};

#define k_bungie_news_header L"XBOX LIVE"
#define k_bungie_news_latest_news L"Latest news:"
#define k_bungie_news_message L"They say all good things must come to an end. Well, we say that totally sucks! Thanks for treating our old gal right for all these years. And thanks a million for playing. <3, Bungie."

// LAN edition: the list (halo2.exe list class at screen +0xA60, vtable 0x7D0544) was Quickmatch, Custom Match, Browse
// Games, Create New Game (item / pane 0-3). Now it is Quickmatch, Custom Match, Create New Game, Achievements: Create
// New Game moves up (its pane with it) and Browse Games' place opens the achievements (the Home menu)
enum e_xbox_live_list_item
{
	_xbox_live_list_item_quickmatch = 0,
	_xbox_live_list_item_custom_match,
	_xbox_live_list_item_create_new_game,		// the original's item 3
	_xbox_live_list_item_achievements,			// the original's item 2, Browse Games
	k_xbox_live_list_item_count
};

#define k_xbox_live_list_original_browse_games 2
#define k_xbox_live_list_original_create_new_game 3
#define k_xbox_live_list_item_data_index_offset 0x70	// c_list_item_widget: the item's data index (low 16 bits: absolute index)
#define k_xbox_live_achievements_text L"Achievements"
#define k_xbox_live_achievements_description L"View your Achievements."

/* globals */

typedef void(__thiscall* screen_update_t)(c_screen_widget* screen);
static screen_update_t p_bungie_news_update = NULL;

typedef void(__thiscall* t_xbox_live_list_update_list_items)(c_list_widget* list, c_list_item_widget* item, int32 skin_index);
static t_xbox_live_list_update_list_items p_xbox_live_list_update_list_items = NULL;

typedef void(__thiscall* t_xbox_live_list_handle_item_pressed)(c_list_widget* list, s_event_record* const* event, datum* item_index);
static t_xbox_live_list_handle_item_pressed p_xbox_live_list_handle_item_pressed = NULL;

/* private code */

static void bungie_news_set_text(c_text_widget* text, const wchar_t* value)
{
	if (text == NULL)
	{
		return;
	}
	const wchar_t* current = text->get_interface()->get_raw_string();
	if (current == NULL || wcscmp(current, value) != 0)
	{
		text->set_text(value);
	}
}

// c_screen_widget's current pane (protected)
struct c_screen_widget_pane_access : public c_screen_widget
{
	static int16 pane_index(const c_screen_widget* screen)
	{
		return static_cast<const c_screen_widget_pane_access*>(screen)->m_pane_index;
	}
};

static int16* xbox_live_list_item_index(c_list_item_widget* item)
{
	return (int16*)((uint8*)item + k_xbox_live_list_item_data_index_offset);
}

// the list's item text: the original's for its items (with their own strings), "Achievements" for Browse Games' place
static void __fastcall xbox_live_list_update_list_items_hook(c_list_widget* list, void*, c_list_item_widget* item, int32 skin_index)
{
	int16* index = xbox_live_list_item_index(item);
	switch (*index)
	{
	case _xbox_live_list_item_create_new_game:
		*index = k_xbox_live_list_original_create_new_game;
		p_xbox_live_list_update_list_items(list, item, skin_index);
		*index = _xbox_live_list_item_create_new_game;
		break;
	case _xbox_live_list_item_achievements:
	{
		c_text_widget* text = item->try_find_text_widget(0);
		if (text != NULL)
		{
			text->set_text(k_xbox_live_achievements_text);
		}
		break;
	}
	default:
		p_xbox_live_list_update_list_items(list, item, skin_index);
		break;
	}
	return;
}

static void __fastcall xbox_live_list_handle_item_pressed_hook(c_list_widget* list, void*, s_event_record* const* event, datum* item_index)
{
	const int16 index = DATUM_INDEX_TO_ABSOLUTE_INDEX(*item_index);
	if (index == _xbox_live_list_item_achievements)
	{
#ifndef IMGUI_DISABLE
		if (!ImGuiHandler::IsWindowActive(k_home_menu_window_name))
		{
			ImGuiHandler::ImHomeMenu::Toggle();
		}
#endif
		return;
	}
	if (index == _xbox_live_list_item_create_new_game)
	{
		datum create_new_game = (*item_index & ~0xFFFF) | k_xbox_live_list_original_create_new_game;
		p_xbox_live_list_handle_item_pressed(list, event, &create_new_game);
		return;
	}
	p_xbox_live_list_handle_item_pressed(list, event, item_index);
	return;
}

static void __fastcall bungie_news_update_hook(c_screen_widget* screen, void*)
{
	p_bungie_news_update(screen);

	// the Achievements item's description (its pane is Browse Games')
	if (c_screen_widget_pane_access::pane_index(screen) == _xbox_live_list_item_achievements)
	{
		bungie_news_set_text(screen->try_find_screen_text(_bungie_news_text_help), k_xbox_live_achievements_description);
	}

	// only set when different, so the texts aren't rebuilt every frame
	bungie_news_set_text(screen->get_screen_header_text(), k_bungie_news_header);
	bungie_news_set_text(screen->try_find_screen_text(_bungie_news_text_panel_header), k_bungie_news_latest_news);
	bungie_news_set_text(screen->try_find_screen_text(_bungie_news_text_guide_help), k_bungie_news_message);
}

/* public code */

void* __cdecl c_screen_bungie_news::load(c_screen_parameters* parameters)
{
	return INVOKE(0xB101, 0x0, c_screen_bungie_news::load, parameters);
}

void c_screen_bungie_news::apply_patches(void)
{
	// vtable slot 3: update
	void** update_slot = (void**)Memory::GetAddressRelative(0x79BF84 + 3 * sizeof(void*));
	p_bungie_news_update = (screen_update_t)*update_slot;
	WritePointer((uint32)update_slot, (void*)bungie_news_update_hook);

	// the list: vtable slot 24 (update_list_items), and the item pressed handler its constructor (0x61FDDC) puts in its
	// slot (mov [edi+0x14], 0x61F958)
	void** list_update_slot = (void**)Memory::GetAddressRelative(0x7D0544 + 24 * sizeof(void*));
	p_xbox_live_list_update_list_items = (t_xbox_live_list_update_list_items)*list_update_slot;
	WritePointer((uint32)list_update_slot, (void*)xbox_live_list_update_list_items_hook);

	p_xbox_live_list_handle_item_pressed = (t_xbox_live_list_handle_item_pressed)Memory::GetAddressRelative(0x61F958);
	WriteValue<uint32>(Memory::GetAddressRelative(0x61FE5A), (uint32)xbox_live_list_handle_item_pressed_hook);
}

void c_screen_bungie_news::apply_patches_on_map_load(void)
{
	const datum widget_datum = tag_loaded(_tag_group_user_interface_screen_widget_definition, k_bungie_news_widget_tag);
	if (widget_datum == NONE)
	{
		return;
	}
	s_user_interface_screen_widget_definition* widget = user_interface_screen_widget_definition_get(widget_datum);
	const datum logo_bitmap = tag_loaded(_tag_group_bitmap, k_bungie_news_logo_bitmap_tag);

	// Create New Game moved up a place in the list: its pane (the original's 3) swaps with Browse Games' (2), which is
	// now the Achievements item's
	if (widget->panes.count > k_xbox_live_list_original_create_new_game)
	{
		s_window_pane_reference* browse_games = user_interface_widget_pane_get(&widget->panes, k_xbox_live_list_original_browse_games);
		s_window_pane_reference* create_new_game = user_interface_widget_pane_get(&widget->panes, k_xbox_live_list_original_create_new_game);
		const s_window_pane_reference swap = *browse_games;
		*browse_games = *create_new_game;
		*create_new_game = swap;
	}

	// every pane (one per list item) has the same panel; their text blocks may be shared, so move each block once
	void* done_text_blocks[8] = {};
	for (int32 pane_index = 0; pane_index < widget->panes.count && pane_index < (int32)NUMBEROF(done_text_blocks); pane_index++)
	{
		s_window_pane_reference* pane = user_interface_widget_pane_get(&widget->panes, pane_index);

		bool text_done = false;
		for (int32 i = 0; i < pane_index; i++) { text_done |= done_text_blocks[i] == (void*)pane->text_blocks.data; }
		done_text_blocks[pane_index] = (void*)pane->text_blocks.data;
		if (!text_done && pane->text_blocks.count >= k_bungie_news_text_count)
		{
			// LIVE stats: the left column, where the Guide help was (same rows)
			for (int32 text_index = _bungie_news_text_live_stats_first; text_index <= _bungie_news_text_live_stats_last; text_index++)
			{
				s_text_block_reference* stat = TAG_BLOCK_GET_ELEMENT(&pane->text_blocks, text_index, s_text_block_reference);
				stat->text_bounds.left = -570;
				stat->text_bounds.right = -40;
			}

			// "Latest news:" beside the bungie.net logo, at the top of the right column
			TAG_BLOCK_GET_ELEMENT(&pane->text_blocks, _bungie_news_text_panel_header, s_text_block_reference)->text_bounds = { -60, 270, -100, 560 };

			// the news message under them, wrapping well inside the panel's right edge (five lines)
			TAG_BLOCK_GET_ELEMENT(&pane->text_blocks, _bungie_news_text_guide_help, s_text_block_reference)->text_bounds = { -115, -10, -380, 490 };
		}

		if (logo_bitmap != NONE && pane->bitmap_blocks.count > 0)
		{
			// bungie.net logo: each pane gets its own copy of the bitmap blocks plus a copy of the panel background block
			// (bitmap 0) showing the logo picture, at the top left of the right column (drawn last, over the panel)
			const int32 logo_index = pane->bitmap_blocks.count;
			tag_injection_extend_block(&pane->bitmap_blocks, sizeof(s_bitmap_block_reference), 1);
			s_bitmap_block_reference* logo = TAG_BLOCK_GET_ELEMENT(&pane->bitmap_blocks, logo_index, s_bitmap_block_reference);
			*logo = *TAG_BLOCK_GET_ELEMENT(&pane->bitmap_blocks, 0, s_bitmap_block_reference);
			logo->bitmap_tag.group = _tag_group_bitmap;
			logo->bitmap_tag.index = logo_bitmap;
			logo->initial_sprite_frame = k_bungie_news_logo_bitmap_index;
			logo->topleft.x = -15;
			logo->topleft.y = -52;
		}
	}
}
