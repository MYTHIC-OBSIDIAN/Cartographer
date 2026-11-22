#include "stdafx.h"
#include "screen_cartographer_menus.h"

#include "cartographer/config/endpoints.h"
#include "cseries/language.h"
#include "game/cheats.h"
#include "game/players.h"
#include "input/controllers.h"
#include "saved_games/cartographer_player_profile/cartographer_player_profile.h"
#include "interface/user_interface.h"
#include "H2MOD/Modules/Shell/Config.h"
#include "bitmaps/bitmap_group.h"
#include "cache/cache_files.h"
#include "interface/user_interface_memory.h"
#include "interface/user_interface_screen_widget_definition.h"
#include "interface/user_interface_shared_globals.h"
#include "tag_files/tag_loader/tag_injection.h"
#include "memory/data.h"

#include "H2MOD/GUI/ImGui_Integration/imgui_handler.h"
#include "H2MOD/Modules/CustomMenu/CustomMenuGlobals.h"
#include "H2MOD/Modules/Updater/Updater.h"
#include "H2MOD/Utils/Utils.h"

/* macro defines */

#define k_cartographer_guide_list_name "cartographer guide list"
#define k_cartographer_credits_list_name "cartographer credits list"
#define k_cartographer_update_list_name "cartographer update list"
#define k_cartographer_update_notice_list_name "cartographer update notice list"

/* enums */

enum e_cartographer_guide_list_items : uint16
{
	_item_cartographer_guide_advanced_settings = 0,
	_item_cartographer_guide_website,
	_item_cartographer_guide_credits,
	_item_cartographer_guide_update,

	k_total_no_of_cartographer_guide_list_items,
};

enum e_cartographer_guide_string_table
{
	_cartographer_guide_text_none = NONE,
	_cartographer_guide_text_header,
	_cartographer_guide_text_subheader,

	k_cartographer_guide_button_label_index_start,
	_cartographer_guide_button_advanced_settings_text = k_cartographer_guide_button_label_index_start,
	_cartographer_guide_button_text_website,
	_cartographer_guide_button_text_credits,
	_cartographer_guide_button_text_update,

	k_cartographer_guide_end,
};

enum e_cartographer_credits_string_table
{
	_cartographer_credits_text_none = NONE,
	_cartographer_credits_text_header,
	_cartographer_credits_text_subheader,

	k_cartographer_credits_button_text_index,
	_cartographer_credits_text_button_1 = k_cartographer_credits_button_text_index,
	_cartographer_credits_text_button_2,
	_cartographer_credits_text_button_3,
	_cartographer_credits_text_button_4,
	_cartographer_credits_text_button_5,
	_cartographer_credits_text_button_6,
	_cartographer_credits_text_button_7,
	_cartographer_credits_text_button_8,
	_cartographer_credits_text_button_9,
	_cartographer_credits_text_button_10,
	_cartographer_credits_text_button_11,
	_cartographer_credits_text_button_12,
	_cartographer_credits_text_button_13,
	_cartographer_credits_text_button_14,
	_cartographer_credits_text_button_15,
	_cartographer_credits_text_button_16,

	k_cartographer_credits_end,
	k_total_no_of_cartographer_credits_list_items = k_cartographer_credits_end - k_cartographer_credits_button_text_index,
};

enum e_cartographer_update_list_items : uint16
{
	_item_cartographer_update_status = 0,
	_item_cartographer_update_cancel,

	k_total_no_of_cartographer_update_list_items,
};

enum e_cartographer_update_string_table
{
	_cartographer_update_text_none = NONE,
	_cartographer_update_text_header,
	_cartographer_update_text_subheader,

	k_cartographer_update_button_label_index_start,
	_cartographer_update_text_button_check_for_updates = k_cartographer_update_button_label_index_start,
	_cartographer_update_text_button_status_checking_for_updates,
	_cartographer_update_text_button_download_updates,
	_cartographer_update_text_button_status_downloading_update,
	_cartographer_update_text_button_install_update,
	_cartographer_update_text_button_status_installing_update,
	_cartographer_update_text_button_status_failed_to_run_updater_app,
	_cartographer_update_text_button_status_download_the_following,
	_cartographer_update_text_button_status_install_the_following,
	_cartographer_update_text_button_status_up_to_date,
	_cartographer_update_text_button_status_cancel,

	k_cartographer_update_text_end,
};

enum e_cartographer_update_notice_list_items : uint16
{
	_item_cartographer_update_notice_yes = 0,
	_item_cartographer_update_notice_no,

	k_total_no_of_cartographer_update_notice_list_items,
};

enum e_cartographer_update_notice_string_table
{
	_cartographer_update_notice_text_none = NONE,
	_cartographer_update_notice_text_header,
	_cartographer_update_notice_text_subheader,

	k_cartographer_update_notice_button_label_index_start,
	_cartographer_update_text_button_notice_yes = k_cartographer_update_notice_button_label_index_start,
	_cartographer_update_text_button_notice_no,

	k_cartographer_update_notice_end,
};

static void get_guide_label(int32 label_id, const wchar_t** out_label)
{
	const wchar_t* label_table[k_language_count][k_cartographer_guide_end] = {};

	label_table[_language_english][_cartographer_guide_text_header] = L"Project Cartographer Guide";
	label_table[_language_english][_cartographer_guide_text_subheader] = L"Press the %S Key to open this guide from anywhere.";
	label_table[_language_english][_cartographer_guide_button_advanced_settings_text] = L"Advanced Settings";
	label_table[_language_english][_cartographer_guide_button_text_website] = L"Website";
	label_table[_language_english][_cartographer_guide_button_text_credits] = L"Credits";
	label_table[_language_english][_cartographer_guide_button_text_update] = L"Update";

	*out_label = label_table[_language_english][label_id];
}

static void get_credits_label(int32 label_id, const wchar_t** out_label)
{
	const wchar_t* label_table[k_language_count][k_cartographer_credits_end] = {};

	label_table[_language_english][_cartographer_credits_text_header] = L"Credits";
	label_table[_language_english][_cartographer_credits_text_subheader] = L"Praise the Following:";
	label_table[_language_english][_cartographer_credits_text_button_1] = L"--- The Devs ---";
	label_table[_language_english][_cartographer_credits_text_button_2] = L"pnill";
	label_table[_language_english][_cartographer_credits_text_button_3] = L"Glitchy Scripts";
	label_table[_language_english][_cartographer_credits_text_button_4] = L"Himanshu01";
	label_table[_language_english][_cartographer_credits_text_button_5] = L"nukeulater";
	label_table[_language_english][_cartographer_credits_text_button_6] = L"Berthalamew";
	label_table[_language_english][_cartographer_credits_text_button_7] = L"Rude Yoshi";
	label_table[_language_english][_cartographer_credits_text_button_8] = L"num005";
	label_table[_language_english][_cartographer_credits_text_button_9] = L"Supersniper";
	label_table[_language_english][_cartographer_credits_text_button_10] = L"--- Content Creators ---";
	label_table[_language_english][_cartographer_credits_text_button_11] = L"General_101";
	label_table[_language_english][_cartographer_credits_text_button_12] = L"--- Additional 7hanks ---";
	label_table[_language_english][_cartographer_credits_text_button_13] = L"Project Cartographer Staff";
	label_table[_language_english][_cartographer_credits_text_button_14] = L"Dev Preview Members";
	label_table[_language_english][_cartographer_credits_text_button_15] = L"And the many many more";
	label_table[_language_english][_cartographer_credits_text_button_16] = L"from the Halo 2 Community!";

	*out_label = label_table[_language_english][label_id];
}

static void get_update_label(int32 label_id, const wchar_t** out_label)
{
	const wchar_t* label_table[k_language_count][k_cartographer_update_text_end] = {};

	label_table[_language_english][_cartographer_update_text_header] = L"Update!";
	label_table[_language_english][_cartographer_update_text_subheader] = L"Update Project Cartographer";
	label_table[_language_english][_cartographer_update_text_button_check_for_updates] = L"Check for Update";
	label_table[_language_english][_cartographer_update_text_button_status_checking_for_updates] = L"Checking For Update...";
	label_table[_language_english][_cartographer_update_text_button_download_updates] = L"Download Update";
	label_table[_language_english][_cartographer_update_text_button_status_downloading_update] = L"Downloading Update...";
	label_table[_language_english][_cartographer_update_text_button_install_update] = L"Install Update";
	label_table[_language_english][_cartographer_update_text_button_status_installing_update] = L"Installing Update...";
	label_table[_language_english][_cartographer_update_text_button_status_failed_to_run_updater_app] = L"Failed to run updater app!";
	label_table[_language_english][_cartographer_update_text_button_status_cancel] = L"Cancel";

	*out_label = label_table[_language_english][label_id];
}

static void get_update_notice_label(int32 label_id, const wchar_t** out_label)
{
	const wchar_t* label_table[k_language_count][k_cartographer_update_notice_end] = {};

	label_table[_language_english][_cartographer_update_notice_text_header] = L"Outdated Version!";
	label_table[_language_english][_cartographer_update_notice_text_subheader] = L"You are using an outdated version of Project Cartographer! Would you like to go install the latest version?";
	label_table[_language_english][_cartographer_update_text_button_notice_yes] = L"Yes";
	label_table[_language_english][_cartographer_update_text_button_notice_no] = L"No";

	*out_label = label_table[_language_english][label_id];
}


// guide

c_cartographer_guide_edit_list::c_cartographer_guide_edit_list(uint16 _flags) :
	c_list_widget(_flags),
	m_slot_2(this, &c_cartographer_guide_edit_list::handle_item_pressed_event)
{
	data_array* account_list_data = ui_list_data_new(k_cartographer_guide_list_name, k_total_no_of_cartographer_guide_list_items, sizeof(datum));
	this->m_list_data = account_list_data;

	data_make_valid(account_list_data);

	for (int32 i = 0; i < this->m_list_data->maximum_count; i++) {
		datum_new(this->m_list_data);
	}

	linker_type2.link(&this->m_slot_2);
}

c_list_item_widget* c_cartographer_guide_edit_list::get_list_items()
{
	return m_list_item_widgets;
}

int32 c_cartographer_guide_edit_list::get_list_items_count()
{
	return NUMBEROF(m_list_item_widgets);
}

void c_cartographer_guide_edit_list::update_list_items(c_list_item_widget* item, int32 skin_index)
{
	int16 list_item_index = DATUM_INDEX_TO_ABSOLUTE_INDEX(item->get_last_data_index());

	c_text_widget* text_widget = item->try_find_text_widget(_default_list_skin_text_main);
	if (text_widget)
	{
		const wchar_t* button_label = nullptr;
		get_guide_label(k_cartographer_guide_button_label_index_start + (int32)list_item_index, &button_label);

		ASSERT(button_label != nullptr);
		text_widget->set_text(button_label);
	}
}

void c_cartographer_guide_edit_list::handle_item_pressed_event(s_event_record* const& event, int32* pitem_index)
{
	int16 button_id = DATUM_INDEX_TO_ABSOLUTE_INDEX(*pitem_index);

	if (button_id == _item_cartographer_guide_advanced_settings) 
	{
#ifndef IMGUI_DISABLE
		ImGuiHandler::ImAdvancedSettings::set_controller_index(_controller0);
		ImGuiHandler::ToggleWindow(k_advanced_settings_window_name);
#endif
	}
	else if (button_id == _item_cartographer_guide_website) 
	{
		ShellExecuteA(NULL, "open", k_cartographer_url_https, NULL, NULL, SW_SHOWDEFAULT);
	}
	else if (button_id == _item_cartographer_guide_credits) 
	{
		ui_load_cartographer_credits_menu();
	}
	else if (button_id == _item_cartographer_guide_update) 
	{
		ui_load_cartographer_update_menu();
	}
}

c_cartographer_guide_menu::c_cartographer_guide_menu(e_user_interface_channel_type _ui_channel, e_user_interface_render_window _window_index, uint16 _flags) :
	c_screen_with_menu(_screen_brightness_level, _ui_channel, _window_index, _flags, &m_guide_edit_list),
	m_guide_edit_list(_flags)
{
}


void* c_cartographer_guide_menu::load(c_screen_parameters* parameters)
{
	c_cartographer_guide_menu* guide_menu = nullptr;
	uint8* ui_buffer = (uint8*)ui_pool_allocate_space(sizeof(c_cartographer_guide_menu), 0);

	if (ui_buffer) {
		guide_menu = new (ui_buffer) c_cartographer_guide_menu(
			parameters->get_channel_type(),
			parameters->get_window_index(),
			parameters->get_user_flags()
		);
		guide_menu->m_allocated = true;
	}

	user_interface_register_screen_to_channel(guide_menu, parameters);
	return guide_menu;
}

void c_cartographer_guide_menu::initialize(c_screen_parameters* screen_parameters)
{
	c_screen_with_menu::initialize(screen_parameters);

	//update header and subheader labels

	const wchar_t* header_text = NULL;
	const wchar_t* subheader_text = NULL;
	wchar_t text_buffer[512];

	get_guide_label(_cartographer_guide_text_header, &header_text);
	get_guide_label(_cartographer_guide_text_subheader, &subheader_text);

	ASSERT(header_text != nullptr && subheader_text != nullptr);

	m_header_text.set_text(header_text);
	c_text_widget* subheader_text_widget = get_screen_subheader_text();
	if (subheader_text_widget)
	{
		extern WPARAM H2Config_hotkeyIdGuide;
		
		c_static_string<64> vkstring;
		GetVKeyCodeString(H2Config_hotkeyIdGuide, &vkstring);
		swprintf(text_buffer, ARRAYSIZE(text_buffer), subheader_text, vkstring.get_string());
		subheader_text = text_buffer;

		subheader_text_widget->set_text(subheader_text);
	}
}

const void* c_cartographer_guide_menu::load_proc(void) const
{
	return c_cartographer_guide_menu::load;
}

// credits

c_cartographer_credits_edit_list::c_cartographer_credits_edit_list(uint16 _flags) :
	c_list_widget(_flags),
	m_slot_2(this, &c_cartographer_credits_edit_list::handle_item_pressed_event)
{
	data_array* account_list_data = ui_list_data_new(k_cartographer_credits_list_name, k_total_no_of_cartographer_credits_list_items, sizeof(datum));
	this->m_list_data = account_list_data;

	data_make_valid(account_list_data);

	for (int32 i = 0; i < this->m_list_data->maximum_count; i++) {
		datum_new(this->m_list_data);
	}

	linker_type2.link(&this->m_slot_2);
}

c_list_item_widget* c_cartographer_credits_edit_list::get_list_items()
{
	return m_list_item_widgets;
}

int32 c_cartographer_credits_edit_list::get_list_items_count()
{
	return NUMBEROF(m_list_item_widgets);
}

void c_cartographer_credits_edit_list::update_list_items(c_list_item_widget* item, int32 skin_index)
{
	datum list_item_index = DATUM_INDEX_TO_ABSOLUTE_INDEX(item->get_last_data_index());

	c_text_widget* text_widget = (c_text_widget*)item->try_find_text_widget(_default_list_skin_text_main);
	if (text_widget)
	{
		const wchar_t* button_label = NULL;
		get_credits_label(k_cartographer_credits_button_text_index + (int32)list_item_index, &button_label);

		ASSERT(button_label != nullptr);
		text_widget->set_text(button_label);
	}
}

void c_cartographer_credits_edit_list::handle_item_pressed_event(s_event_record* const& event, int32* a3)
{
	return;
}

c_cartographer_credits_menu::c_cartographer_credits_menu(e_user_interface_channel_type _ui_channel, e_user_interface_render_window _window_index, uint16 _flags) :
	c_screen_with_menu(_screen_brightness_level, _ui_channel, _window_index, _flags, &m_credits_edit_list),
	m_credits_edit_list(_flags)
{
}

void* c_cartographer_credits_menu::load(c_screen_parameters* parameters)
{
	c_cartographer_credits_menu* credits_menu = nullptr;
	uint8* ui_buffer = (uint8*)ui_pool_allocate_space(sizeof(c_cartographer_credits_menu), 0);

	if (ui_buffer) {
		credits_menu = new (ui_buffer) c_cartographer_credits_menu(
			parameters->get_channel_type(),
			parameters->get_window_index(),
			parameters->get_user_flags()
		);
		credits_menu->m_allocated = true;
	}

	user_interface_register_screen_to_channel(credits_menu, parameters);
	return credits_menu;
}

const void* c_cartographer_credits_menu::load_proc(void) const
{
	return c_cartographer_credits_menu::load;
}

void c_cartographer_credits_menu::initialize(c_screen_parameters* screen_parameters)
{
	c_screen_with_menu::initialize(screen_parameters);

	const wchar_t* header_text = NULL;
	const wchar_t* subheader_text = NULL;

	get_credits_label(_cartographer_credits_text_header, &header_text);
	get_credits_label(_cartographer_credits_text_subheader, &subheader_text);

	ASSERT(header_text != nullptr && subheader_text != nullptr);

	m_header_text.set_text(header_text);
	c_text_widget* subheader_text_widget = get_screen_subheader_text();
	if (subheader_text_widget)
	{
		subheader_text_widget->set_text(subheader_text);
	}
}

// update

c_cartographer_update_edit_list::c_cartographer_update_edit_list(uint16 _flags) :
	c_list_widget(_flags),
	m_slot_2(this, &c_cartographer_update_edit_list::handle_item_pressed_event)
{
	data_array* update_list_data = ui_list_data_new(k_cartographer_update_list_name, k_total_no_of_cartographer_update_list_items, sizeof(datum));
	this->m_list_data = update_list_data;

	data_make_valid(update_list_data);

	for (int32 i = 0; i < this->m_list_data->maximum_count; i++) {
		datum_new(this->m_list_data);
	}

	linker_type2.link(&this->m_slot_2);

	// ### TODO FIXME this is pretty hacked up, cleanup the system at some point
	m_keep_screen_open = true;
	m_update_status = _cartographer_update_status_pending_check_for_update;

	// check the updater state, and set the current state accordingly
	if (updater_has_files_to_download)
	{
		m_update_status = _cartographer_update_status_pending_download_update;
	}

	if (updater_has_files_to_install)
	{
		m_update_status = _cartographer_update_status_pending_install_update;
	}
}

void c_cartographer_update_edit_list::pre_destroy()
{
	if (m_keep_screen_open)
	{
		ui_load_cartographer_update_menu();
	}
}

c_list_item_widget* c_cartographer_update_edit_list::get_list_items()
{
	return m_list_item_widgets;
}

int32 c_cartographer_update_edit_list::get_list_items_count()
{
	return NUMBEROF(m_list_item_widgets);
}

void c_cartographer_update_edit_list::update_list_items(c_list_item_widget* item, int32 skin_index)
{
	int16 list_item_index = DATUM_INDEX_TO_ABSOLUTE_INDEX(item->get_last_data_index());

	c_text_widget* text_widget = (c_text_widget*)item->try_find_text_widget(_default_list_skin_text_main);
	if (text_widget)
	{
		const wchar_t* button_label = NULL;

		switch (list_item_index)
		{
		case _item_cartographer_update_status:
			get_update_label(k_cartographer_update_button_label_index_start + m_update_status, &button_label);
			text_widget->get_interface()->set_pulsating((m_update_status % 2) == 0);
			break;
		case _item_cartographer_update_cancel:
			get_update_label(_cartographer_update_text_button_status_cancel, &button_label);
			break;
		default:
			break;
		}

		ASSERT(button_label != nullptr);
		text_widget->set_text(button_label);
	}
}

void c_cartographer_update_edit_list::set_status(e_cartographer_update_status status)
{
}


void c_cartographer_update_edit_list::update_updater_status()
{
	if (updater_has_up_to_date)
	{
		m_update_status = _cartographer_update_status_pending_check_for_update;
	}

	if (m_update_status < _cartographer_update_status_pending_download_update && updater_has_files_to_download)
	{
		m_update_status = _cartographer_update_status_pending_download_update;
	}

	if (m_update_status < _cartographer_update_status_pending_install_update && updater_has_files_to_install)
	{
		m_update_status = _cartographer_update_status_pending_install_update;
	}
}


void c_cartographer_update_edit_list::handle_item_pressed_event(s_event_record* const& event, int32* pitem_index)
{
	int32 button_id = DATUM_INDEX_TO_ABSOLUTE_INDEX(*pitem_index);

	e_user_interface_render_window	parent_render_window = this->get_parent_render_window();
	e_user_interface_channel_type	parent_screen_ui_channel = this->get_parent_channel();

	if (button_id == _item_cartographer_update_status)
	{
		switch (m_update_status)
		{
		case _cartographer_update_status_pending_check_for_update:
			GSDownloadCheck();
			m_update_status = _cartographer_update_status_checking_for_update;
			break;
		case _cartographer_update_status_pending_download_update:
			if (updater_has_files_to_download) {
				GSDownloadDL();
				m_update_status = _cartographer_update_status_downloading_update;
			}
			break;
		case _cartographer_update_status_pending_install_update:
			if (updater_has_files_to_install) {
				GSDownloadInstall();
				m_update_status = _cartographer_update_status_installing_update;
			}
			break;

		case _cartographer_update_status_checking_for_update:
		case _cartographer_update_status_downloading_update:
		default:
			break;
		}
	}
	else if (button_id == _item_cartographer_update_cancel)
	{
		GSDownloadCancel();
		m_keep_screen_open = false;

		user_interface_back_out_from_channel(parent_screen_ui_channel, parent_render_window);
	}

	return;
}


c_cartographer_update_menu::c_cartographer_update_menu(e_user_interface_channel_type _ui_channel, e_user_interface_render_window _window_index, uint16 _flags) :
	c_screen_with_menu(_screen_brightness_level, _ui_channel, _window_index, _flags, &m_update_edit_list),
	m_update_edit_list(_flags)
{
	GSDownloadInit();
}

void c_cartographer_update_menu::update()
{
	m_update_edit_list.update_updater_status();
	c_screen_with_menu::update();
}

const void* c_cartographer_update_menu::load_proc(void) const
{
	return c_cartographer_update_menu::load;
}

void c_cartographer_update_menu::initialize(c_screen_parameters* screen_parameters)
{
	c_screen_with_menu::initialize(screen_parameters);

	const wchar_t* header_text = nullptr;
	const wchar_t* subheader_text = nullptr;

	get_update_label(_cartographer_update_text_header, &header_text);
	get_update_label(_cartographer_update_text_subheader, &subheader_text);

	ASSERT(header_text != nullptr && subheader_text != nullptr);

	m_header_text.set_text(header_text);
	c_text_widget* subheader_text_widget = get_screen_subheader_text();
	if (subheader_text_widget)
	{
		subheader_text_widget->set_text(subheader_text);
	}
}

void c_cartographer_update_menu::set_update_status(e_cartographer_update_status status)
{
	m_update_edit_list.set_status(status);
}

void* c_cartographer_update_menu::load(c_screen_parameters* parameters)
{
	c_cartographer_update_menu* update_menu = nullptr;
	uint8* ui_buffer = (uint8*)ui_pool_allocate_space(sizeof(c_cartographer_update_menu), 0);

	if (ui_buffer) {
		update_menu = new (ui_buffer) c_cartographer_update_menu(
			parameters->get_channel_type(),
			parameters->get_window_index(),
			parameters->get_user_flags()
		);
		update_menu->m_allocated = true;
	}

	user_interface_register_screen_to_channel(update_menu, parameters);
	return update_menu;
}


// update notice

c_cartographer_update_notice_edit_list::c_cartographer_update_notice_edit_list(uint16 _flags) :
	c_list_widget(_flags),
	m_slot_2(this, &c_cartographer_update_notice_edit_list::handle_item_pressed_event)
{
	data_array* account_list_data = ui_list_data_new(k_cartographer_update_notice_list_name, k_total_no_of_cartographer_update_notice_list_items, sizeof(datum));
	this->m_list_data = account_list_data;

	data_make_valid(account_list_data);

	for (int32 i = 0; i < this->m_list_data->maximum_count; i++) {
		datum_new(this->m_list_data);
	}

	linker_type2.link(&this->m_slot_2);
}

c_list_item_widget* c_cartographer_update_notice_edit_list::get_list_items()
{
	return m_list_item_widgets;
}

int32 c_cartographer_update_notice_edit_list::get_list_items_count()
{
	return NUMBEROF(m_list_item_widgets);
}

void c_cartographer_update_notice_edit_list::update_list_items(c_list_item_widget* item, int32 skin_index)
{
	int16 list_item_index = DATUM_INDEX_TO_ABSOLUTE_INDEX(item->get_last_data_index());

	c_text_widget* text_widget = (c_text_widget*)item->try_find_text_widget(_default_list_skin_text_main);
	if (text_widget)
	{
		const wchar_t* button_label = NULL;
		get_update_notice_label(k_cartographer_update_notice_button_label_index_start + (int32)list_item_index, &button_label);

		ASSERT(button_label != NULL);
		text_widget->set_text(button_label);
	}
}

void c_cartographer_update_notice_edit_list::handle_item_pressed_event(s_event_record* const& event, int32* pitem_index)
{
	int16 button_id = DATUM_INDEX_TO_ABSOLUTE_INDEX(*pitem_index);

	e_user_interface_render_window	parent_render_window = this->get_parent_render_window();
	e_user_interface_channel_type	parent_screen_ui_channel = this->get_parent_channel();

	if (button_id == _item_cartographer_update_notice_yes) 
	{
		ui_load_cartographer_update_menu();
	}
	else if (button_id == _item_cartographer_update_notice_no)
	{
		user_interface_back_out_from_channel(parent_screen_ui_channel, parent_render_window);
	}

	return;
}


c_cartographer_update_notice_menu::c_cartographer_update_notice_menu(e_user_interface_channel_type _ui_channel, e_user_interface_render_window _window_index, uint16 _flags) :
	c_screen_with_menu(_screen_brightness_level, _ui_channel, _window_index, _flags, &m_update_notice_edit_list),
	m_update_notice_edit_list(_flags)
{
}

const void* c_cartographer_update_notice_menu::load_proc(void) const
{
	return c_cartographer_update_notice_menu::load;
}

void c_cartographer_update_notice_menu::initialize(c_screen_parameters* screen_parameters)
{
	c_screen_with_menu::initialize(screen_parameters);

	const wchar_t* header_text = NULL;
	const wchar_t* subheader_text = NULL;

	get_update_notice_label(_cartographer_update_notice_text_header, &header_text);
	get_update_notice_label(_cartographer_update_notice_text_subheader, &subheader_text);

	ASSERT(header_text != NULL && subheader_text != NULL);

	m_header_text.set_text(header_text);
	c_text_widget* subheader_text_widget = get_screen_subheader_text();
	if (subheader_text_widget)
	{
		subheader_text_widget->set_text(subheader_text);
	}
}

void* c_cartographer_update_notice_menu::load(c_screen_parameters* parameters)
{
	c_cartographer_update_notice_menu* update_notice_menu = nullptr;
	uint8* ui_buffer = (uint8*)ui_pool_allocate_space(sizeof(c_cartographer_update_notice_menu), 0);

	if (ui_buffer) {
		update_notice_menu = new (ui_buffer) c_cartographer_update_notice_menu(
			parameters->get_channel_type(),
			parameters->get_window_index(),
			parameters->get_user_flags()
		);
		update_notice_menu->m_allocated = true;
	}

	user_interface_register_screen_to_channel(update_notice_menu, parameters);
	return update_notice_menu;
}

// skulls (LAN edition)

#define k_skulls_list_name "skulls list"

struct s_skulls_list_entry
{
	e_skull_type skull;
	const wchar_t* name;
};

// listed A-Z; Cow Bell is left out as it was never finished and does nothing
static const s_skulls_list_entry k_skulls_list_entries[] =
{
	{ _skull_type_anger, L"Anger" },
	{ _skull_type_assassians, L"Assassins" },
	{ _skull_type_black_eye, L"Black Eye" },
	{ _skull_type_blind, L"Blind" },
	{ _skull_type_catch, L"Catch" },
	{ _skull_type_envy, L"Envy" },
	{ _skull_type_famine, L"Famine" },
	{ _skull_type_ghost, L"Ghost" },
	{ _skull_type_grunt_birthday_party, L"Grunt Birthday Party" },
	{ _skull_type_iwhbyd, L"IWHBYD" },
	{ _skull_type_iron, L"Iron" },
	{ _skull_type_mythic, L"Mythic" },
	{ _skull_type_sputnik, L"Sputnik" },
	{ _skull_type_thunderstorm, L"Thunderstorm" },
	{ _skull_type_whuppopotamus, L"Whuppopotamus" },
};

c_skulls_edit_list::c_skulls_edit_list(uint16 _flags) :
	c_list_widget(_flags),
	m_slot_2(this, &c_skulls_edit_list::handle_item_pressed_event)
{
	data_array* skulls_list_data = ui_list_data_new(k_skulls_list_name, NUMBEROF(k_skulls_list_entries), sizeof(datum));
	this->m_list_data = skulls_list_data;

	data_make_valid(skulls_list_data);

	for (int32 i = 0; i < this->m_list_data->maximum_count; i++) {
		datum_new(this->m_list_data);
	}

	linker_type2.link(&this->m_slot_2);
}

c_list_item_widget* c_skulls_edit_list::get_list_items()
{
	return m_list_item_widgets;
}

int32 c_skulls_edit_list::get_list_items_count()
{
	return NUMBEROF(m_list_item_widgets);
}

void c_skulls_edit_list::set_item_text(c_list_item_widget* item)
{
	const datum data_index = item->get_last_data_index();
	if (data_index == NONE)
	{
		return;
	}

	const int16 list_item_index = DATUM_INDEX_TO_ABSOLUTE_INDEX(data_index);
	if (list_item_index < 0 || list_item_index >= (int16)NUMBEROF(k_skulls_list_entries))
	{
		return;
	}

	c_text_widget* text_widget = item->try_find_text_widget(_default_list_skin_text_main);
	if (text_widget)
	{
		const s_skulls_list_entry* entry = &k_skulls_list_entries[list_item_index];
		wchar_t label[64];
		swprintf(label, NUMBEROF(label), L"%s: %s", entry->name, g_cheats_skull_enabled[entry->skull] ? L"On" : L"Off");
		text_widget->set_text(label);
	}
}

void c_skulls_edit_list::update_list_items(c_list_item_widget* item, int32 skin_index)
{
	set_item_text(item);
}

void c_skulls_edit_list::handle_item_pressed_event(s_event_record* const& event, int32* pitem_index)
{
	const int16 list_item_index = DATUM_INDEX_TO_ABSOLUTE_INDEX(*pitem_index);
	if (list_item_index < 0 || list_item_index >= (int16)NUMBEROF(k_skulls_list_entries))
	{
		return;
	}

	const e_skull_type skull = k_skulls_list_entries[list_item_index].skull;
	g_cheats_skull_enabled[skull] = !g_cheats_skull_enabled[skull];

	// refresh the label of the pressed item straight away
	for (int32 i = 0; i < (int32)NUMBEROF(m_list_item_widgets); i++)
	{
		if (m_list_item_widgets[i].get_last_data_index() == *pitem_index)
		{
			set_item_text(&m_list_item_widgets[i]);
		}
	}
}

c_skulls_menu::c_skulls_menu(e_user_interface_channel_type _ui_channel, e_user_interface_render_window _window_index, uint16 _flags) :
	c_screen_with_menu(_screen_brightness_level, _ui_channel, _window_index, _flags, &m_skulls_edit_list),
	m_skulls_edit_list(_flags)
{
}

void* c_skulls_menu::load(c_screen_parameters* parameters)
{
	c_skulls_menu* skulls_menu = nullptr;
	uint8* ui_buffer = (uint8*)ui_pool_allocate_space(sizeof(c_skulls_menu), 0);

	if (ui_buffer) {
		skulls_menu = new (ui_buffer) c_skulls_menu(
			parameters->get_channel_type(),
			parameters->get_window_index(),
			parameters->get_user_flags()
		);
		skulls_menu->m_allocated = true;
	}

	user_interface_register_screen_to_channel(skulls_menu, parameters);
	return skulls_menu;
}

void c_skulls_menu::initialize(c_screen_parameters* screen_parameters)
{
	// LAN edition: the list one row (37 units) higher than the brightness_level dialog it borrows has it, for this
	// screen only (the tag goes back as it was once the list is made)
	s_list_reference* list = NULL;
	const datum widget_datum = user_interface_get_screen_tag_index_by_id(this->m_screen_id);
	if (widget_datum != NONE)
	{
		s_user_interface_screen_widget_definition* widget = user_interface_screen_widget_definition_get(widget_datum);
		if (widget->panes.count > 0 && user_interface_widget_pane_get(&widget->panes, 0)->list_block.count > 0)
		{
			list = TAG_BLOCK_GET_ELEMENT(&user_interface_widget_pane_get(&widget->panes, 0)->list_block, 0, s_list_reference);
		}
	}
	const int16 k_skulls_list_raise = 37;
	if (list != NULL)
	{
		list->bottom_left.y = (int16)(list->bottom_left.y + k_skulls_list_raise);
	}
	c_screen_with_menu::initialize(screen_parameters);
	if (list != NULL)
	{
		list->bottom_left.y = (int16)(list->bottom_left.y - k_skulls_list_raise);
	}

	m_header_text.set_text(L"Skulls");
	c_text_widget* subheader_text_widget = get_screen_subheader_text();
	if (subheader_text_widget)
	{
		subheader_text_widget->set_text(L"Turn campaign modifiers on or off.");
	}
}

const void* c_skulls_menu::load_proc(void) const
{
	return c_skulls_menu::load;
}

// nameplates (LAN edition)

#define k_nameplates_list_name "nameplates list"

// lobby name row backgrounds in mainmenu.map (ui\screens\game_shell\pregame_lobby\item_background_lobby), by index
static const wchar_t* const k_nameplate_names[] =
{
	L"Default",
	L"Bungie.net",
	L"Bungie.net Staff",
	L"Seventh Column",
	L"Stars",
	L"Bungie",
	L"Bungie Stars",
};

static_assert(NUMBEROF(k_nameplate_names) == k_cartographer_profile_nameplate_count, "nameplate count");

// the nameplate is saved in the player profile (cartographer profile v4) of the controller
static int32 nameplate_get_selected(e_controller_index controller_index)
{
	if (controller_index < 0 || controller_index >= k_number_of_controllers)
	{
		return 0;
	}
	const int32 nameplate = cartographer_player_profile_get_by_controller_index(controller_index)->nameplate;
	return nameplate < (int32)NUMBEROF(k_nameplate_names) ? nameplate : 0;
}

// the controller the screen was opened for (lowest bit of the user flags)
static e_controller_index nameplate_controller_from_user_flags(uint16 user_flags)
{
	for (int32 controller = 0; controller < k_number_of_controllers; controller++)
	{
		if (user_flags & FLAG(controller))
		{
			return (e_controller_index)controller;
		}
	}
	return k_no_controller;
}

c_nameplates_edit_list::c_nameplates_edit_list(uint16 _flags) :
	c_list_widget(_flags),
	m_slot_2(this, &c_nameplates_edit_list::handle_item_pressed_event),
	m_controller_index(nameplate_controller_from_user_flags(_flags))
{
	data_array* nameplates_list_data = ui_list_data_new(k_nameplates_list_name, NUMBEROF(k_nameplate_names), sizeof(datum));
	this->m_list_data = nameplates_list_data;

	data_make_valid(nameplates_list_data);

	for (int32 i = 0; i < this->m_list_data->maximum_count; i++) {
		datum_new(this->m_list_data);
	}

	linker_type2.link(&this->m_slot_2);
}

c_list_item_widget* c_nameplates_edit_list::get_list_items()
{
	return m_list_item_widgets;
}

int32 c_nameplates_edit_list::get_list_items_count()
{
	return NUMBEROF(m_list_item_widgets);
}

void c_nameplates_edit_list::set_item_text(c_list_item_widget* item)
{
	const datum data_index = item->get_last_data_index();
	if (data_index == NONE)
	{
		return;
	}

	const int16 list_item_index = DATUM_INDEX_TO_ABSOLUTE_INDEX(data_index);
	if (list_item_index < 0 || list_item_index >= (int16)NUMBEROF(k_nameplate_names))
	{
		return;
	}

	c_text_widget* text_widget = item->try_find_text_widget(_default_list_skin_text_main);
	if (text_widget)
	{
		wchar_t label[64];
		swprintf(label, NUMBEROF(label), L"%s%s", k_nameplate_names[list_item_index], list_item_index == nameplate_get_selected(m_controller_index) ? L" (Selected)" : L"");
		text_widget->set_text(label);
	}
}

void c_nameplates_edit_list::update_list_items(c_list_item_widget* item, int32 skin_index)
{
	set_item_text(item);
}

void c_nameplates_edit_list::handle_item_pressed_event(s_event_record* const& event, int32* pitem_index)
{
	const int16 list_item_index = DATUM_INDEX_TO_ABSOLUTE_INDEX(*pitem_index);
	if (list_item_index < 0 || list_item_index >= (int16)NUMBEROF(k_nameplate_names))
	{
		return;
	}

	const e_controller_index controller_index = (e_controller_index)event->controller;
	if (controller_index < 0 || controller_index >= k_number_of_controllers)
	{
		return;
	}
	m_controller_index = controller_index;
	cartographer_player_profile_get_by_controller_index(controller_index)->nameplate = (uint8)list_item_index;
	cartographer_player_profile_save(controller_index);

	// send the new details so the lobby shows the nameplate straight away
	INVOKE_TYPE(0x206A97, 0x0, void(__cdecl*)(e_controller_index), event->controller);

	for (int32 i = 0; i < (int32)NUMBEROF(m_list_item_widgets); i++)
	{
		set_item_text(&m_list_item_widgets[i]);
	}
}

c_nameplates_menu::c_nameplates_menu(e_user_interface_channel_type _ui_channel, e_user_interface_render_window _window_index, uint16 _flags) :
	c_screen_with_menu(_screen_brightness_level, _ui_channel, _window_index, _flags, &m_nameplates_edit_list),
	m_nameplates_edit_list(_flags)
{
}

void* c_nameplates_menu::load(c_screen_parameters* parameters)
{
	c_nameplates_menu* nameplates_menu = nullptr;
	uint8* ui_buffer = (uint8*)ui_pool_allocate_space(sizeof(c_nameplates_menu), 0);

	if (ui_buffer) {
		nameplates_menu = new (ui_buffer) c_nameplates_menu(
			parameters->get_channel_type(),
			parameters->get_window_index(),
			parameters->get_user_flags()
		);
		nameplates_menu->m_allocated = true;
	}

	user_interface_register_screen_to_channel(nameplates_menu, parameters);
	return nameplates_menu;
}

void c_nameplates_menu::initialize(c_screen_parameters* screen_parameters)
{
	c_screen_with_menu::initialize(screen_parameters);

	m_header_text.set_text(L"Nameplate");
	c_text_widget* subheader_text_widget = get_screen_subheader_text();
	if (subheader_text_widget)
	{
		subheader_text_widget->set_text(L"Choose the background shown behind your name in the lobby.");
	}
}

const void* c_nameplates_menu::load_proc(void) const
{
	return c_nameplates_menu::load;
}

// rank (LAN edition): the rank icon shown next to the player's name in the lobby, chosen like the nameplate

#define k_rank_list_name "rank list"

// the rank is saved in the player profile (cartographer profile v4) of the controller, as rank - 1
int32 rank_get_selected(e_controller_index controller_index)
{
	if (controller_index < 0 || controller_index >= k_number_of_controllers)
	{
		return 0;
	}
	const int32 rank = cartographer_player_profile_get_by_controller_index(controller_index)->rank;
	return rank < k_cartographer_profile_rank_count ? rank : 0;
}

c_rank_edit_list::c_rank_edit_list(uint16 _flags) :
	c_list_widget(_flags),
	m_slot_2(this, &c_rank_edit_list::handle_item_pressed_event),
	m_controller_index(nameplate_controller_from_user_flags(_flags))
{
	data_array* rank_list_data = ui_list_data_new(k_rank_list_name, k_cartographer_profile_rank_count, sizeof(datum));
	this->m_list_data = rank_list_data;

	data_make_valid(rank_list_data);

	for (int32 i = 0; i < this->m_list_data->maximum_count; i++) {
		datum_new(this->m_list_data);
	}

	linker_type2.link(&this->m_slot_2);
}

c_list_item_widget* c_rank_edit_list::get_list_items()
{
	return m_list_item_widgets;
}

int32 c_rank_edit_list::get_list_items_count()
{
	return NUMBEROF(m_list_item_widgets);
}

void c_rank_edit_list::set_item_text(c_list_item_widget* item)
{
	const datum data_index = item->get_last_data_index();
	if (data_index == NONE)
	{
		return;
	}

	const int16 list_item_index = DATUM_INDEX_TO_ABSOLUTE_INDEX(data_index);
	if (list_item_index < 0 || list_item_index >= k_cartographer_profile_rank_count)
	{
		return;
	}

	c_text_widget* text_widget = item->try_find_text_widget(_default_list_skin_text_main);
	if (text_widget)
	{
		wchar_t label[64];
		swprintf(label, NUMBEROF(label), L"Rank %d%s", list_item_index + 1, list_item_index == rank_get_selected(m_controller_index) ? L" (Selected)" : L"");
		text_widget->set_text(label);
	}
}

void c_rank_edit_list::update_list_items(c_list_item_widget* item, int32 skin_index)
{
	set_item_text(item);
}

void c_rank_edit_list::handle_item_pressed_event(s_event_record* const& event, int32* pitem_index)
{
	const int16 list_item_index = DATUM_INDEX_TO_ABSOLUTE_INDEX(*pitem_index);
	if (list_item_index < 0 || list_item_index >= k_cartographer_profile_rank_count)
	{
		return;
	}

	const e_controller_index controller_index = (e_controller_index)event->controller;
	if (controller_index < 0 || controller_index >= k_number_of_controllers)
	{
		return;
	}
	m_controller_index = controller_index;
	cartographer_player_profile_get_by_controller_index(controller_index)->rank = (uint8)list_item_index;
	cartographer_player_profile_save(controller_index);

	// send the new details so the lobby shows the rank straight away
	INVOKE_TYPE(0x206A97, 0x0, void(__cdecl*)(e_controller_index), event->controller);

	for (int32 i = 0; i < (int32)NUMBEROF(m_list_item_widgets); i++)
	{
		set_item_text(&m_list_item_widgets[i]);
	}
}

c_rank_menu::c_rank_menu(e_user_interface_channel_type _ui_channel, e_user_interface_render_window _window_index, uint16 _flags) :
	c_screen_with_menu(_screen_brightness_level, _ui_channel, _window_index, _flags, &m_rank_edit_list),
	m_rank_edit_list(_flags)
{
}

void* c_rank_menu::load(c_screen_parameters* parameters)
{
	c_rank_menu* rank_menu = nullptr;
	uint8* ui_buffer = (uint8*)ui_pool_allocate_space(sizeof(c_rank_menu), 0);

	if (ui_buffer) {
		rank_menu = new (ui_buffer) c_rank_menu(
			parameters->get_channel_type(),
			parameters->get_window_index(),
			parameters->get_user_flags()
		);
		rank_menu->m_allocated = true;
	}

	user_interface_register_screen_to_channel(rank_menu, parameters);
	return rank_menu;
}

void c_rank_menu::initialize(c_screen_parameters* screen_parameters)
{
	c_screen_with_menu::initialize(screen_parameters);

	m_header_text.set_text(L"Rank");
	c_text_widget* subheader_text_widget = get_screen_subheader_text();
	if (subheader_text_widget)
	{
		subheader_text_widget->set_text(L"Display your rank in the lobby.");
	}
}

const void* c_rank_menu::load_proc(void) const
{
	return c_rank_menu::load;
}

// the Xbox's player profile options (screens whose tags the PC map kept): each a choice saved in a byte of the edited player
// profile that the PC game doesn't use (s_saved_game_player_profile, after the subtitle setting at 0x1179)
// the Xbox's screens as the Xbox shows them (its code sets the options and the Voice Masking and Online Status descriptions)
static const s_profile_option_menu_definition k_profile_voice_output_menu =
{
	_screen_pp_voice_thru_tv, 0x117A, 4,
	{ L"Mixed", L"Speakers Only", L"Xbox Communicator Only", L"Disable Voice" },
	{ 0, 1, 2, 3 },
	L"VOICE OUTPUT",
	L"Team chat plays on the Xbox Communicator, proximity on the speakers. You can override this.",
};
static const s_profile_option_menu_definition k_profile_voice_masking_menu =
{
	_screen_pp_choose_voice_mask, 0x117B, 2,
	{ L"Disabled", L"Enabled" },
	{ 0, 1 },
	L"VOICE MASKING",
	L"When enabled this option will mask your voice to other players.",
};
static const s_profile_option_menu_definition k_profile_online_status_menu =
{
	_screen_pp_appear_offline, 0x117C, 2,
	{ L"Appear Offline", L"Appear Online" },
	{ 1, 0 },	// appear offline
	L"ONLINE STATUS",
	L"This will make you appear as if you're not signed in to Xbox Live.",
};

static uint8* profile_option_get(const s_profile_option_menu_definition* definition)
{
	uint8* profile = (uint8*)user_interface_globals_get_edit_player_profile();
	return profile != nullptr ? profile + definition->profile_offset : nullptr;
}

c_profile_option_list::c_profile_option_list(uint16 _flags, const s_profile_option_menu_definition* definition) :
	c_list_widget(_flags),
	m_slot_2(this, &c_profile_option_list::handle_item_pressed_event),
	m_definition(definition)
{
	data_array* list_data = ui_list_data_new("profile option list", definition->option_count, sizeof(datum));
	this->m_list_data = list_data;
	data_make_valid(list_data);
	for (int32 i = 0; i < this->m_list_data->maximum_count; i++)
	{
		datum_new(this->m_list_data);
	}
	linker_type2.link(&this->m_slot_2);
}

c_list_item_widget* c_profile_option_list::get_list_items()
{
	return m_list_item_widgets;
}

int32 c_profile_option_list::get_list_items_count()
{
	return m_definition->option_count;
}

void c_profile_option_list::update_list_items(c_list_item_widget* item, int32 skin_index)
{
	const datum data_index = item->get_last_data_index();
	if (data_index == NONE)
	{
		return;
	}
	const int16 option = DATUM_INDEX_TO_ABSOLUTE_INDEX(data_index);
	c_text_widget* text_widget = item->try_find_text_widget(_default_list_skin_text_main);
	if (text_widget != nullptr && option >= 0 && option < m_definition->option_count)
	{
		text_widget->set_text(m_definition->options[option]);
	}
}

void c_profile_option_list::handle_item_pressed_event(s_event_record* const& event, int32* pitem_index)
{
	const int16 option = DATUM_INDEX_TO_ABSOLUTE_INDEX(*pitem_index);
	uint8* value = profile_option_get(m_definition);
	if (value != nullptr && option >= 0 && option < m_definition->option_count)
	{
		// kept with the edited profile, saved when the player profile screen is left
		*value = m_definition->values[option];
	}
	user_interface_back_out_from_channel(this->get_parent_channel(), this->get_parent_render_window());
}

c_profile_option_menu::c_profile_option_menu(e_user_interface_channel_type _ui_channel, e_user_interface_render_window _window_index, uint16 _flags, const s_profile_option_menu_definition* definition) :
	c_screen_with_menu(definition->screen_id, _ui_channel, _window_index, _flags, &m_option_list),
	m_option_list(_flags, definition),
	m_definition(definition)
{
}

void* c_profile_option_menu::load_definition(c_screen_parameters* parameters, const s_profile_option_menu_definition* definition)
{
	c_profile_option_menu* menu = nullptr;
	void* pool = ui_pool_allocate_space(sizeof(c_profile_option_menu), 0);
	if (pool != nullptr)
	{
		menu = new (pool) c_profile_option_menu(parameters->get_channel_type(), parameters->get_window_index(), parameters->get_user_flags(), definition);
		menu->m_allocated = true;
		user_interface_register_screen_to_channel(menu, parameters);
	}
	return menu;
}

void* c_profile_option_menu::load_voice_output(c_screen_parameters* parameters)
{
	return load_definition(parameters, &k_profile_voice_output_menu);
}

void* c_profile_option_menu::load_voice_masking(c_screen_parameters* parameters)
{
	return load_definition(parameters, &k_profile_voice_masking_menu);
}

void* c_profile_option_menu::load_online_status(c_screen_parameters* parameters)
{
	return load_definition(parameters, &k_profile_online_status_menu);
}

void c_profile_option_menu::initialize(c_screen_parameters* screen_parameters)
{
	c_screen_with_menu::initialize(screen_parameters);

	c_text_widget* header = get_screen_header_text();
	if (header != nullptr)
	{
		header->set_text(m_definition->header);
	}
	c_text_widget* subheader = get_screen_subheader_text();
	if (subheader != nullptr)
	{
		subheader->set_text(m_definition->description);
	}
}

void c_profile_option_menu::post_initialize()
{
	c_screen_with_menu::post_initialize();

	const uint8* value = profile_option_get(m_definition);
	for (int32 option = 0; value != nullptr && option < m_definition->option_count; option++)
	{
		if (m_definition->values[option] == *value)
		{
			m_option_list.set_focused_item_index(option);
		}
	}
}

const void* c_profile_option_menu::load_proc(void) const
{
	return m_definition == &k_profile_voice_output_menu ? (const void*)c_profile_option_menu::load_voice_output
		: m_definition == &k_profile_voice_masking_menu ? (const void*)c_profile_option_menu::load_voice_masking
		: (const void*)c_profile_option_menu::load_online_status;
}

// player profile edit list (halo2.exe c_player_profile_edit_list): Nameplate and Rank added under Appearance, and the
// Xbox's Voice Output, Voice Masking, Online Status and Subtitles after the game's own items. One pane per item
enum e_profile_edit_list_item : int16
{
	_profile_edit_item_appearance,		// the game's item 0
	_profile_edit_item_nameplate,
	_profile_edit_item_rank,
	_profile_edit_item_controller,		// the game's item 1
	_profile_edit_item_keyboard,		// the game's item 2
	_profile_edit_item_voice_output,
	_profile_edit_item_voice_masking,
	_profile_edit_item_online_status,
	_profile_edit_item_subtitles,
	k_profile_edit_list_data_count
};
#define k_profile_edit_list_game_item_count 3
#define k_profile_edit_screen_widget_tag "ui\\screens\\game_shell\\settings_screen\\player_profile\\edit_profile_menu"

// the game's item for a list item (NONE for the LAN edition's)
static int16 profile_edit_list_game_item(int16 item_index)
{
	switch (item_index)
	{
	case _profile_edit_item_appearance: return 0;
	case _profile_edit_item_controller: return 1;
	case _profile_edit_item_keyboard: return 2;
	default: return NONE;
	}
}

static const wchar_t* profile_edit_list_item_name(int16 item_index)
{
	switch (item_index)
	{
	case _profile_edit_item_nameplate: return L"Nameplate";
	case _profile_edit_item_rank: return L"Rank";
	case _profile_edit_item_voice_output: return L"Voice Output";
	case _profile_edit_item_voice_masking: return L"Voice Masking";
	case _profile_edit_item_online_status: return L"Online Status";
	case _profile_edit_item_subtitles: return L"Subtitles";
	default: return L"";
	}
}

// the lobby shows nameplate bitmap (user role - 1), role 0 and 1 both show the default one
int8 nameplate_get_user_role(e_controller_index controller_index)
{
	const int32 nameplate = nameplate_get_selected(controller_index);
	return (int8)(nameplate == 0 ? 0 : nameplate + 1);
}

static void __fastcall profile_edit_list_update_list_items(void* list, void* edx, void* item, int32 skin_index)
{
	int16* item_data_index = (int16*)((uint8*)item + 0x70);
	const int16 item_index = *item_data_index;
	const int16 game_item_index = profile_edit_list_game_item(item_index);

	if (game_item_index == NONE)
	{
		void* text = INVOKE_TYPE(0x211973, 0x0, void*(__thiscall*)(void*, int32), item, 0);
		if (text != nullptr)
		{
			INVOKE_TYPE(0x21BAA9, 0x0, void(__thiscall*)(void*, const wchar_t*), text, profile_edit_list_item_name(item_index));
		}
		return;
	}

	// the game's own items are further down the list than they were
	*item_data_index = game_item_index;
	INVOKE_TYPE(0x24C6AC, 0x0, void(__thiscall*)(void*, void*, int32), list, item, skin_index);
	*item_data_index = item_index;
}

static void __fastcall profile_edit_list_handle_item_pressed(void* list, void* edx, s_event_record** event, datum* item_index)
{
	if (event == nullptr || *event == nullptr || item_index == nullptr || *item_index == NONE)
	{
		return;
	}

	const int16 list_item_index = DATUM_INDEX_TO_ABSOLUTE_INDEX(*item_index);
	const int16 game_item_index = profile_edit_list_game_item(list_item_index);
	if (game_item_index != NONE)
	{
		data_array* data = *(data_array**)((uint8*)list + 0x70);
		const uint16 identifier = *(uint16*)((uint8*)data->data + data->size * game_item_index);
		datum game_item_datum = DATUM_INDEX_NEW(game_item_index, identifier);
		INVOKE_TYPE(0x24CD82, 0x0, void(__thiscall*)(void*, s_event_record**, datum*), list, event, &game_item_datum);
		return;
	}

	// same call the game uses to open the profile sub menus
	void* load = nullptr;
	switch (list_item_index)
	{
	case _profile_edit_item_nameplate: load = (void*)c_nameplates_menu::load; break;
	case _profile_edit_item_rank: load = (void*)c_rank_menu::load; break;
	case _profile_edit_item_voice_output: load = (void*)c_profile_option_menu::load_voice_output; break;
	case _profile_edit_item_voice_masking: load = (void*)c_profile_option_menu::load_voice_masking; break;
	case _profile_edit_item_online_status: load = (void*)c_profile_option_menu::load_online_status; break;
	// the game's subtitles screen (c_screen_subtitle_display, still used by another settings list), which sets the
	// edited profile's subtitles
	case _profile_edit_item_subtitles: load = Memory::GetAddress<void*>(0x2273DC); break;
	}
	if (load != nullptr)
	{
		INVOKE_TYPE(0x20C851, 0x0, void(__cdecl*)(uint32, void*), FLAG((*event)->controller), load);
	}
}

// the list has room for 3 item widgets (screen + 0xA60 + 0xB0): 5 item widgets placed after the screen object (which is
// allocated bigger for them) show 5 items at a time, the list scrolling through the rest
#define k_profile_edit_screen_size 0xCB8
#define k_profile_edit_list_offset 0xA60
#define k_profile_edit_list_item_count 5
#define k_profile_edit_list_item_spacing 46
#define k_list_item_widget_size 0x84

static void* profile_edit_list_get_extra_item_widgets(void* list)
{
	return (uint8*)list + (k_profile_edit_screen_size - k_profile_edit_list_offset);
}

static void* __fastcall profile_edit_list_get_list_items(void* list, void* edx)
{
	return profile_edit_list_get_extra_item_widgets(list);
}

static int32 __fastcall profile_edit_list_get_list_items_count(void* list, void* edx)
{
	return k_profile_edit_list_item_count;
}

// c_player_profile_edit_list constructor, called by the screen constructor
static void* __fastcall profile_edit_list_constructor(void* list, void* edx, int32 user_flags)
{
	void* result = INVOKE_TYPE(0x24CDF7, 0x0, void*(__thiscall*)(void*, int32), list, user_flags);
	// eh vector constructor iterator (c_list_item_widget constructor / destructor)
	INVOKE_TYPE(0x28870B, 0x0, void(__stdcall*)(void*, uint32, int32, void*, void*),
		profile_edit_list_get_extra_item_widgets(list), k_list_item_widget_size, k_profile_edit_list_item_count,
		Memory::GetAddressRelative<void*>(0x61FFC9), Memory::GetAddressRelative<void*>(0x40A551));
	return result;
}

// c_player_profile_edit_list destructor, called by the screen destructor
static void __fastcall profile_edit_list_destructor(void* list, void* edx)
{
	// eh vector destructor iterator
	INVOKE_TYPE(0x2886A8, 0x0, void(__stdcall*)(void*, uint32, int32, void*),
		profile_edit_list_get_extra_item_widgets(list), k_list_item_widget_size, k_profile_edit_list_item_count,
		Memory::GetAddressRelative<void*>(0x40A551));
	INVOKE_TYPE(0x23B7CA, 0x0, void(__thiscall*)(void*), list);
}

// the Xbox's panes for its items (the Xbox mainmenu.map's edit_profile_menu panes 2 to 5): its picture (edit_profile bitmap:
// the Xbox's speakers, added by the map build, and the headset, the Xbox Live figure and the television the PC map kept),
// description and callouts, with their animations. The places are the Xbox's at the PC pictures' size: the PC Appearance
// pane draws the helmet 2.3 times the Xbox's size (512x512 at 1.15 scale for 256x256) from (-480, 370) where the Xbox
// draws it from (-186, 152), so an Xbox (x, y) is (-480 + 2.3 (x + 186), 370 + 2.3 (y - 152))
struct s_profile_edit_pane_callout
{
	const wchar_t* text;
	rectangle2d bounds;
	int16 animation_index;
};
struct s_profile_edit_pane_texts
{
	int16 item;
	int16 picture_frame;
	point2d picture_topleft;
	const wchar_t* description;
	int16 description_animation_index;
	s_profile_edit_pane_callout callouts[2];
};
static const s_profile_edit_pane_texts k_profile_edit_pane_texts[] =
{
	{
		// the Xbox's top left (-190, 154); callout (48, -196, 0, -86)
		_profile_edit_item_voice_output, 7, { -489, 375 },
		L"Play multiplayer voice through your headset or through your system speakers.", 14,
		{
			{ L"If toast always lands butter-side down, and cats always land on their feet, what happens if you strap toast on the back of a cat --and drop it?", { 131, -503, 20, -250 }, 5 },
			{ L"", { 0, 0, 0, 0 }, 14 },
		},
	},
	{
		// the Xbox's top left (-190, 154); callouts (94, -192, 40, -125) and (46, -53, 7, 26)
		_profile_edit_item_voice_masking, 3, { -489, 375 },
		L"Disguise your voice in multiplayer games with an anonymous voice mask.", 7,
		{
			{ L"People will think you are a psycho arguing with himself in the middle of your living room.", { 237, -494, 112, -340 }, 14 },
			{ L"Disguise your voice so your boss will never know it is you who smacked him down.", { 126, -174, 36, 8 }, 16 },
		},
	},
	{
		// the Xbox's top left (-186, 152); callouts (102, -38, 0, -12) and (80, 0, 0, 35)
		_profile_edit_item_online_status, 4, { -480, 370 },
		L"Set your status to appear offline when you want to be left alone.", 2,
		{
			{ L"Online\r\nOffline\r\nVoice\r\nMuted\r\nRank\r\nStatus\r\nMessage\r\nRequest\r\nInvite\r\nParty\r\nClan", { 255, -140, 20, -80 }, 14 },
			{ L"Appear Offline. When you call in sick on November 9, no one needs to know your little secret.", { 204, -52, 20, 28 }, 10 },
		},
	},
	{
		// the Xbox's top left (-186, 152); callouts (133, -191, 15, -165) and (80, 0, 0, 35)
		_profile_edit_item_subtitles, 5, { -480, 370 },
		L"Control whether or not subtitles are shown during cinematics.", 9,
		{
			{ L"The only water stuff item!!! soffish will be woken up by water and look so alive!! As bath-toy is also very fun.", { 326, -492, 55, -432 }, 6 },
			{ L"This soffish do not eat. Real fishes might hurt human body. soffish is 100% harmless.", { 204, -52, 20, 28 }, 14 },
		},
	},
};
#define k_profile_edit_speakers_frame 7
#define k_profile_edit_appearance_picture_frame 1

static const s_profile_edit_pane_texts* profile_edit_pane_texts_get(int16 pane_index)
{
	for (int32 index = 0; index < (int32)NUMBEROF(k_profile_edit_pane_texts); index++)
	{
		if (k_profile_edit_pane_texts[index].item == pane_index)
		{
			return &k_profile_edit_pane_texts[index];
		}
	}
	return nullptr;
}

// c_screen_player_profile_edit::post_initialize_button_keys, runs on every pane switch: texts of the Nameplate pane
static void __fastcall player_profile_edit_post_initialize_button_keys(c_screen_widget* screen, void* edx)
{
	INVOKE_TYPE(0x210B4E, 0x0, void(__thiscall*)(c_screen_widget*), screen);

	const int16 pane_index = *(int16*)((uint8*)screen + 0x9F8);
	const s_profile_edit_pane_texts* texts = profile_edit_pane_texts_get(pane_index);
	if (texts != nullptr)
	{
		const wchar_t* strings[] = { texts->description, texts->callouts[0].text, texts->callouts[1].text };
		for (int32 index = 0; index < (int32)NUMBEROF(strings); index++)
		{
			c_text_widget* text = screen->try_find_screen_text(index);
			if (text != nullptr)
			{
				text->set_text(strings[index]);
			}
		}
	}
	else if (pane_index == _profile_edit_item_nameplate)
	{
		c_text_widget* description = screen->try_find_screen_text(0);
		if (description != nullptr)
		{
			description->set_text(L"Choose the background shown behind your name in the lobby.");
		}
		// callout 1 sits beside the Seventh Column logo (bounds set in nameplates_apply_patches_on_map_load), callout 2 is unused
		c_text_widget* logo_callout = screen->try_find_screen_text(1);
		if (logo_callout != nullptr)
		{
			logo_callout->set_text(L"The Seventh Column -\r\nTru7h. Carnage. Bungie");
		}
		c_text_widget* unused_callout = screen->try_find_screen_text(2);
		if (unused_callout != nullptr)
		{
			unused_callout->set_text(L"");
		}
	}
	else if (pane_index == _profile_edit_item_rank)
	{
		c_text_widget* description = screen->try_find_screen_text(0);
		if (description != nullptr)
		{
			description->set_text(L"Display your rank in the lobby.");
		}
		// two callouts: one at the top right of the Marathon symbol, the other under it on the right
		// (bounds set in nameplates_apply_patches_on_map_load)
		c_text_widget* symbol_callout = screen->try_find_screen_text(1);
		if (symbol_callout != nullptr)
		{
			symbol_callout->set_text(L"Somewhere in the heavens...");
		}
		c_text_widget* second_callout = screen->try_find_screen_text(2);
		if (second_callout != nullptr)
		{
			second_callout->set_text(L"they are waiting.");
		}
	}
}

// local player details: the bungie user role (nameplate) used to come from the Live account
static void __cdecl user_interface_controller_get_user_properties_nameplate(e_controller_index controller_index, s_player_identifier* controller_user_identifier, s_player_configuration* properties)
{
	INVOKE_TYPE(0x20696F, 0x0, void(__cdecl*)(e_controller_index, s_player_identifier*, s_player_configuration*), controller_index, controller_user_identifier, properties);
	if (properties != nullptr)
	{
		properties->bungie_user_role = nameplate_get_user_role(controller_index);

		// the lobby shows rank icon (displayed skill) through ui_shared_globals: skill 0 = Rank 1 ... 49 = Rank 50
		const int8 rank = (int8)rank_get_selected(controller_index);
		properties->player_displayed_skill = rank;
		properties->player_overall_skill = rank;
	}
}

void nameplates_apply_patches(void)
{
	// player profile edit list: 9 items instead of 3 (push 3 -> push 9 for the list data count)
	WriteValue<uint8>(Memory::GetAddressRelative(0x64CE7C), k_profile_edit_list_data_count);
	WritePointer(Memory::GetAddressRelative(0x7D894C) + 24 * sizeof(void*), (void*)profile_edit_list_update_list_items);
	WritePointer(Memory::GetAddressRelative(0x64CE75), (void*)profile_edit_list_handle_item_pressed);
	WritePointer(Memory::GetAddressRelative(0x7D3134) + 24 * sizeof(void*), (void*)player_profile_edit_post_initialize_button_keys);

	// player profile edit list: 5 item widgets so all 5 items are visible
	WriteValue<uint32>(Memory::GetAddressRelative(0x63C6C6), k_profile_edit_screen_size + k_profile_edit_list_item_count * k_list_item_widget_size);
	WriteValue<uint32>(Memory::GetAddressRelative(0x64BFBB), k_profile_edit_screen_size + k_profile_edit_list_item_count * k_list_item_widget_size);
	PatchCall(Memory::GetAddressRelative(0x63BD5A), profile_edit_list_constructor);
	PatchCall(Memory::GetAddressRelative(0x63BDB3), profile_edit_list_destructor);
	WritePointer(Memory::GetAddressRelative(0x7D894C) + 22 * sizeof(void*), (void*)profile_edit_list_get_list_items);
	WritePointer(Memory::GetAddressRelative(0x7D894C) + 23 * sizeof(void*), (void*)profile_edit_list_get_list_items_count);

	// the session interface update (halo2.exe 0x5B2C20 loop) sets every local user's displayed and overall skill to -1
	// (they came from Xbox Live stats), which hid the rank chosen in the Rank menu: keep the skill from the player details
	NopFill(Memory::GetAddressRelative(0x5B2C29), 7);

	PatchCall(Memory::GetAddressRelative(0x606AD3), user_interface_controller_get_user_properties_nameplate);
	PatchCall(Memory::GetAddressRelative(0x61D3FB), user_interface_controller_get_user_properties_nameplate);

	// pregame lobby: role 4 was shown as role 1, show every nameplate as itself
	WriteValue<uint8>(Memory::GetAddressRelative(0x644483), 0xEB);
}

void nameplates_apply_patches_on_map_load(void)
{
	// edit profile screen: one pane per list item, add the Nameplate and Rank panes (copies of the Appearance pane) after Appearance
	const datum widget_datum = tag_loaded(_tag_group_user_interface_screen_widget_definition, k_profile_edit_screen_widget_tag);
	if (widget_datum == NONE)
	{
		return;
	}

	s_user_interface_screen_widget_definition* widget = user_interface_screen_widget_definition_get(widget_datum);
	if (widget->panes.count != 3)
	{
		return;
	}

	tag_injection_extend_block(&widget->panes, sizeof(s_window_pane_reference), k_profile_edit_list_data_count - k_profile_edit_list_game_item_count);
	*user_interface_widget_pane_get(&widget->panes, _profile_edit_item_keyboard) = *user_interface_widget_pane_get(&widget->panes, 2);
	*user_interface_widget_pane_get(&widget->panes, _profile_edit_item_controller) = *user_interface_widget_pane_get(&widget->panes, 1);

	// the Xbox's panes: copies of the Appearance pane with their own picture (in place of the helmet, picture block 4) and
	// callouts. The speakers are only there with the map build's picture (otherwise the headset)
	const datum edit_profile_bitmap = tag_loaded(_tag_group_bitmap, "ui\\screens\\game_shell\\settings_screen\\player_profile\\edit_profile");
	const bool have_speakers = edit_profile_bitmap != NONE && bitmap_group_get(edit_profile_bitmap)->bitmaps.count > k_profile_edit_speakers_frame;
	for (int32 index = 0; index < (int32)NUMBEROF(k_profile_edit_pane_texts); index++)
	{
		const s_profile_edit_pane_texts* texts = &k_profile_edit_pane_texts[index];
		s_window_pane_reference* pane = user_interface_widget_pane_get(&widget->panes, texts->item);
		*pane = *user_interface_widget_pane_get(&widget->panes, _profile_edit_item_appearance);
		tag_injection_extend_block(&pane->bitmap_blocks, sizeof(s_bitmap_block_reference), 0);
		for (int32 block_index = 0; block_index < pane->bitmap_blocks.count; block_index++)
		{
			s_bitmap_block_reference* block = TAG_BLOCK_GET_ELEMENT(&pane->bitmap_blocks, block_index, s_bitmap_block_reference);
			if (block->bitmap_tag.index == edit_profile_bitmap && block->initial_sprite_frame == k_profile_edit_appearance_picture_frame)
			{
				block->initial_sprite_frame = (texts->picture_frame == k_profile_edit_speakers_frame && !have_speakers) ? 3 : texts->picture_frame;
				block->topleft = texts->picture_topleft;
			}
		}
		tag_injection_extend_block(&pane->text_blocks, sizeof(s_text_block_reference), 0);
		if (pane->text_blocks.count > 2)
		{
			TAG_BLOCK_GET_ELEMENT(&pane->text_blocks, 0, s_text_block_reference)->animation_index = (e_animation_index)texts->description_animation_index;
			for (int32 callout = 0; callout < 2; callout++)
			{
				s_text_block_reference* text = TAG_BLOCK_GET_ELEMENT(&pane->text_blocks, 1 + callout, s_text_block_reference);
				text->text_bounds = texts->callouts[callout].bounds;
				text->animation_index = (e_animation_index)texts->callouts[callout].animation_index;
			}
		}
	}

	// the Rank pane is a copy of the Appearance pane with its own text blocks; its picture is the Marathon symbol added to
	// the LAN edition mainmenu.map (multiplayer49 extra bitmap 9, a 440x414 picture with the symbol in the 292x302 at the
	// top left, appended by digsite_apply_patches_on_map_load) in place of the helmet, with one callout placed like the
	// Nameplate logo's
	s_window_pane_reference* rank_pane = user_interface_widget_pane_get(&widget->panes, _profile_edit_item_rank);
	*rank_pane = *user_interface_widget_pane_get(&widget->panes, 0);
	tag_injection_extend_block(&rank_pane->text_blocks, sizeof(s_text_block_reference), 0);
	if (rank_pane->text_blocks.count > 2)
	{
		// the symbol is drawn from (-480, 370), about 336x347 (292x302 at 1.15 scale)
		TAG_BLOCK_GET_ELEMENT(&rank_pane->text_blocks, 1, s_text_block_reference)->text_bounds = { 335, -140, 285, 35 };
		// the second sits on the red line at the symbol's lower right (screen x 1062-1130 of 2576, y 655 of 1449)
		TAG_BLOCK_GET_ELEMENT(&rank_pane->text_blocks, 2, s_text_block_reference)->text_bounds = { 70, -187, 30, 13 };
	}
	const datum rank_picture_bitmap = tag_loaded(_tag_group_bitmap, "ui\\code_global_bitmaps\\multiplayer49");
	if (rank_picture_bitmap != NONE && rank_pane->bitmap_blocks.count > 4)
	{
		tag_injection_extend_block(&rank_pane->bitmap_blocks, sizeof(s_bitmap_block_reference), 0);
		s_bitmap_block_reference* picture = TAG_BLOCK_GET_ELEMENT(&rank_pane->bitmap_blocks, 4, s_bitmap_block_reference);
		picture->bitmap_tag.group = _tag_group_bitmap;
		picture->bitmap_tag.index = rank_picture_bitmap;
		picture->initial_sprite_frame = 9;
	}
	s_window_pane_reference* nameplate_pane = user_interface_widget_pane_get(&widget->panes, _profile_edit_item_nameplate);
	*nameplate_pane = *user_interface_widget_pane_get(&widget->panes, 0);

	// the Nameplate pane shows the Seventh Column logo added to the LAN edition mainmenu.map (mp_games bitmap 5, an unused
	// 8x8 placeholder in the stock map) in place of the Appearance helmet (edit_profile bitmap 1, picture block 4)
	const datum mp_games_bitmap = tag_loaded(_tag_group_bitmap, "ui\\screens\\game_shell\\mp_game_select\\mp_games");
	if (mp_games_bitmap != NONE && nameplate_pane->bitmap_blocks.count > 4)
	{
		// own copy of the bitmap blocks, so the Appearance pane keeps the helmet
		tag_injection_extend_block(&nameplate_pane->bitmap_blocks, sizeof(s_bitmap_block_reference), 0);
		s_bitmap_block_reference* picture = TAG_BLOCK_GET_ELEMENT(&nameplate_pane->bitmap_blocks, 4, s_bitmap_block_reference);
		picture->bitmap_tag.group = _tag_group_bitmap;
		picture->bitmap_tag.index = mp_games_bitmap;
		picture->initial_sprite_frame = 5;
	}

	// own copy of the text blocks, so the Appearance callouts stay where they are: the first callout moves to the
	// empty space at the top right of the logo (the logo is drawn from (-480, 370), about 292x302)
	if (nameplate_pane->text_blocks.count > 1)
	{
		tag_injection_extend_block(&nameplate_pane->text_blocks, sizeof(s_text_block_reference), 0);
		s_text_block_reference* logo_callout = TAG_BLOCK_GET_ELEMENT(&nameplate_pane->text_blocks, 1, s_text_block_reference);
		logo_callout->text_bounds = { 350, -165, 300, 10 };
	}

	// 5 visible list items, and the list moved up by one item so the last item (Keyboard/Mouse) sits where the 4th did
	// (default list skin items are 46 units apart: text bounds 38 to -8; y is up)
	// the panes are copies of each other and share their list blocks, so move each list reference once
	s_list_reference* moved_lists[16];
	int32 moved_list_count = 0;
	for (int32 pane_index = 0; pane_index < widget->panes.count; pane_index++)
	{
		s_window_pane_reference* pane = user_interface_widget_pane_get(&widget->panes, pane_index);
		for (int32 list_index = 0; list_index < pane->list_block.count; list_index++)
		{
			s_list_reference* list = TAG_BLOCK_GET_ELEMENT(&pane->list_block, list_index, s_list_reference);
			list->num_visible_items = k_profile_edit_list_item_count;

			bool already_moved = false;
			for (int32 i = 0; i < moved_list_count; i++)
			{
				already_moved |= moved_lists[i] == list;
			}
			if (!already_moved && moved_list_count < (int32)NUMBEROF(moved_lists))
			{
				list->bottom_left.y = (int16)(list->bottom_left.y + k_profile_edit_list_item_spacing);
				moved_lists[moved_list_count++] = list;
			}
		}
	}
}

void* ui_load_skulls_menu()
{
	return ui_custom_cartographer_load_menu(c_skulls_menu::load);
}

void* ui_load_cartographer_guide_menu()
{
	return ui_custom_cartographer_load_menu(c_cartographer_guide_menu::load);
}

void* ui_load_cartographer_update_notice_menu()
{
	return ui_custom_cartographer_load_menu(c_cartographer_update_notice_menu::load);
}

void* ui_load_cartographer_credits_menu()
{
	return ui_custom_cartographer_load_menu(c_cartographer_credits_menu::load);
}

void* ui_load_cartographer_update_menu()
{
	return ui_custom_cartographer_load_menu(c_cartographer_update_menu::load);
}


