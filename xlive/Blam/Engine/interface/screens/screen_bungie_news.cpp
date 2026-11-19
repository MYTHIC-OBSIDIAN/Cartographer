#include "stdafx.h"
#include "screen_bungie_news.h"

#include "cache/cache_files.h"
#include "interface/user_interface.h"
#include "interface/user_interface_widget_text.h"
#include "interface/user_interface_widget_window.h"
#include "interface/user_interface_screen_widget_definition.h"
#include "interface/user_interface_shared_globals.h"
#include "tag_files/tag_loader/tag_injection.h"

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

/* globals */

typedef void(__thiscall* screen_update_t)(c_screen_widget* screen);
static screen_update_t p_bungie_news_update = NULL;

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

static void __fastcall bungie_news_update_hook(c_screen_widget* screen, void*)
{
	p_bungie_news_update(screen);

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
