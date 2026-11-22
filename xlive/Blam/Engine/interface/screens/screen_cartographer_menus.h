#pragma once

// miscellaneous cartographer menus

#include "input/controllers.h"
#include "interface/user_interface_widget_list.h"
#include "interface/user_interface_widget_list_item.h"
#include "interface/user_interface_widget_window.h"

/* macro defines */

#define k_no_of_visible_items_for_cartographer_guide_list 4
#define k_no_of_visible_items_for_cartographer_credits_list 4
#define k_no_of_visible_items_for_cartographer_update_list 2
#define k_no_of_visible_items_for_cartographer_update_notice_list 2
#define k_no_of_visible_items_for_skulls_list 4
#define k_no_of_visible_items_for_nameplates_list 4
#define k_no_of_visible_items_for_rank_list 4

/* enums */

enum e_cartographer_update_status
{
	_cartographer_update_status_none = NONE,
	_cartographer_update_status_pending_check_for_update,
	_cartographer_update_status_checking_for_update,
	_cartographer_update_status_pending_download_update,
	_cartographer_update_status_downloading_update,
	_cartographer_update_status_pending_install_update,
	_cartographer_update_status_installing_update,

	k_cartographer_update_status_end,
};

/* classes */

class c_cartographer_guide_edit_list : public c_list_widget
{
protected:
	c_list_item_widget m_list_item_widgets[k_no_of_visible_items_for_cartographer_guide_list];
	c_slot2<c_cartographer_guide_edit_list, struct s_event_record*, int32> m_slot_2;

	void handle_item_pressed_event(struct s_event_record* const& event, int32* pitem_index);
public:
	c_cartographer_guide_edit_list(uint16 _flags);

	// c_cartographer_guide_edit_list virtual functions

	virtual ~c_cartographer_guide_edit_list() = default;
	virtual c_list_item_widget* get_list_items() override;
	virtual int32 get_list_items_count() override;
	virtual void update_list_items(c_list_item_widget* item, int32 skin_index) override;
};

class c_cartographer_guide_menu : public c_screen_with_menu
{
protected:
	c_cartographer_guide_edit_list m_guide_edit_list;
public:
	c_cartographer_guide_menu(e_user_interface_channel_type _ui_channel, e_user_interface_render_window _window_index, uint16 _flags);
	static void* load(c_screen_parameters* parameters);
	
	// c_cartographer_guide_menu virtual functions

	virtual ~c_cartographer_guide_menu() = default;
	virtual void initialize(c_screen_parameters* screen_parameters) override;
	virtual const void* load_proc(void) const override;
};


// credits

class c_cartographer_credits_edit_list : public c_list_widget
{
protected:
	c_list_item_widget m_list_item_widgets[k_no_of_visible_items_for_cartographer_credits_list];
	c_slot2<c_cartographer_credits_edit_list, struct s_event_record*, int32> m_slot_2;

	void handle_item_pressed_event(struct s_event_record* const& event, int32* pitem_index);

public:
	c_cartographer_credits_edit_list(uint16 _flags);

	// c_cartographer_credits_edit_list virtual functions

	virtual ~c_cartographer_credits_edit_list() = default;
	virtual c_list_item_widget* get_list_items() override;
	virtual int32 get_list_items_count() override;
	virtual void update_list_items(c_list_item_widget* item, int32 skin_index) override;
};

class c_cartographer_credits_menu : public c_screen_with_menu
{
	c_cartographer_credits_edit_list m_credits_edit_list;
public:
	c_cartographer_credits_menu(e_user_interface_channel_type _ui_channel, e_user_interface_render_window _window_index, uint16 _flags);
	static void* load(c_screen_parameters* parameters);

	// c_cartographer_credits_menu virtual functions

	virtual ~c_cartographer_credits_menu() = default;
	virtual const void* load_proc(void) const override;
	virtual void initialize(c_screen_parameters* screen_parameters) override;
};


// update

class c_cartographer_update_edit_list : public c_list_widget
{
protected:
	c_list_item_widget m_list_item_widgets[k_no_of_visible_items_for_cartographer_update_list];
	c_slot2<c_cartographer_update_edit_list, struct s_event_record*, int32> m_slot_2;
	int32 m_update_status;
	bool m_keep_screen_open;

	void handle_item_pressed_event(struct s_event_record* const& event, int32* pitem_index);

public:
	c_cartographer_update_edit_list(uint16 _flags);
	void set_status(e_cartographer_update_status status);
	void update_updater_status();

	// c_cartographer_update_edit_list virtual functions
	
	virtual ~c_cartographer_update_edit_list() = default;
	virtual void pre_destroy() override;
	virtual c_list_item_widget* get_list_items() override;
	virtual int32 get_list_items_count() override;
	virtual void update_list_items(c_list_item_widget* item, int32 skin_index) override;
};

class c_cartographer_update_menu : public c_screen_with_menu
{
protected:
	c_cartographer_update_edit_list m_update_edit_list;

public:
	c_cartographer_update_menu(e_user_interface_channel_type _ui_channel, e_user_interface_render_window _window_index, uint16 _flags);
	void set_update_status(e_cartographer_update_status status);
	static void* load(c_screen_parameters* parameters);

	// c_cartographer_update_menu virtual functions

	virtual ~c_cartographer_update_menu() = default;
	virtual void update() override;
	virtual const void* load_proc(void) const override;
	virtual void initialize(c_screen_parameters* screen_parameters) override;
};

// update notice

class c_cartographer_update_notice_edit_list : public c_list_widget
{
protected:
	c_list_item_widget m_list_item_widgets[k_no_of_visible_items_for_cartographer_update_notice_list];
	c_slot2<c_cartographer_update_notice_edit_list, struct  s_event_record*, int32> m_slot_2;

	void handle_item_pressed_event(struct s_event_record* const& event, int32* pitem_index);

public:
	c_cartographer_update_notice_edit_list(uint16 _flags);


	// c_cartographer_update_notice_edit_list virtual functions.

	virtual ~c_cartographer_update_notice_edit_list() = default;
	virtual c_list_item_widget* get_list_items() override;
	virtual int32 get_list_items_count() override;
	virtual void update_list_items(c_list_item_widget* item, int32 skin_index) override;
};

class c_cartographer_update_notice_menu : public c_screen_with_menu
{
protected:
	c_cartographer_update_notice_edit_list m_update_notice_edit_list;
public:
	c_cartographer_update_notice_menu(e_user_interface_channel_type _ui_channel, e_user_interface_render_window _window_index, uint16 _flags);
	static void* load(class c_screen_parameters* parameters);

	// c_cartographer_update_notice_menu virtual functions.

	virtual ~c_cartographer_update_notice_menu() = default;
	virtual const void* load_proc(void) const override;
	virtual void initialize(class c_screen_parameters* screen_parameters) override;
};

//public code

// LAN edition: Settings > Skulls, an on/off toggle for each skull

class c_skulls_edit_list : public c_list_widget
{
protected:
	c_list_item_widget m_list_item_widgets[k_no_of_visible_items_for_skulls_list];
	c_slot2<c_skulls_edit_list, struct s_event_record*, int32> m_slot_2;

	void handle_item_pressed_event(struct s_event_record* const& event, int32* pitem_index);
	void set_item_text(c_list_item_widget* item);
public:
	c_skulls_edit_list(uint16 _flags);

	// c_skulls_edit_list virtual functions

	virtual ~c_skulls_edit_list() = default;
	virtual c_list_item_widget* get_list_items() override;
	virtual int32 get_list_items_count() override;
	virtual void update_list_items(c_list_item_widget* item, int32 skin_index) override;
};

class c_skulls_menu : public c_screen_with_menu
{
protected:
	c_skulls_edit_list m_skulls_edit_list;
public:
	c_skulls_menu(e_user_interface_channel_type _ui_channel, e_user_interface_render_window _window_index, uint16 _flags);
	static void* load(c_screen_parameters* parameters);

	// c_skulls_menu virtual functions

	virtual ~c_skulls_menu() = default;
	virtual void initialize(c_screen_parameters* screen_parameters) override;
	virtual const void* load_proc(void) const override;
};

// LAN edition: nameplate picker, opened from Settings > Player Profile > Appearance

class c_nameplates_edit_list : public c_list_widget
{
protected:
	c_list_item_widget m_list_item_widgets[k_no_of_visible_items_for_nameplates_list];
	c_slot2<c_nameplates_edit_list, struct s_event_record*, int32> m_slot_2;
	e_controller_index m_controller_index;

	void handle_item_pressed_event(struct s_event_record* const& event, int32* pitem_index);
	void set_item_text(c_list_item_widget* item);
public:
	c_nameplates_edit_list(uint16 _flags);

	// c_nameplates_edit_list virtual functions

	virtual ~c_nameplates_edit_list() = default;
	virtual c_list_item_widget* get_list_items() override;
	virtual int32 get_list_items_count() override;
	virtual void update_list_items(c_list_item_widget* item, int32 skin_index) override;
};

class c_nameplates_menu : public c_screen_with_menu
{
protected:
	c_nameplates_edit_list m_nameplates_edit_list;
public:
	c_nameplates_menu(e_user_interface_channel_type _ui_channel, e_user_interface_render_window _window_index, uint16 _flags);
	static void* load(c_screen_parameters* parameters);

	// c_nameplates_menu virtual functions

	virtual ~c_nameplates_menu() = default;
	virtual void initialize(c_screen_parameters* screen_parameters) override;
	virtual const void* load_proc(void) const override;
};

// LAN edition: rank picker (Rank 1 to 50), opened from Settings > Player Profile > Rank

class c_rank_edit_list : public c_list_widget
{
protected:
	c_list_item_widget m_list_item_widgets[k_no_of_visible_items_for_rank_list];
	c_slot2<c_rank_edit_list, struct s_event_record*, int32> m_slot_2;
	e_controller_index m_controller_index;

	void handle_item_pressed_event(struct s_event_record* const& event, int32* pitem_index);
	void set_item_text(c_list_item_widget* item);
public:
	c_rank_edit_list(uint16 _flags);

	// c_rank_edit_list virtual functions

	virtual ~c_rank_edit_list() = default;
	virtual c_list_item_widget* get_list_items() override;
	virtual int32 get_list_items_count() override;
	virtual void update_list_items(c_list_item_widget* item, int32 skin_index) override;
};

class c_rank_menu : public c_screen_with_menu
{
protected:
	c_rank_edit_list m_rank_edit_list;
public:
	c_rank_menu(e_user_interface_channel_type _ui_channel, e_user_interface_render_window _window_index, uint16 _flags);
	static void* load(c_screen_parameters* parameters);

	// c_rank_menu virtual functions

	virtual ~c_rank_menu() = default;
	virtual void initialize(c_screen_parameters* screen_parameters) override;
	virtual const void* load_proc(void) const override;
};

// LAN edition: the Xbox's player profile options PC Halo 2 left out (Voice Output, Voice Masking, Online Status), opened
// from Settings > Player Profile: a choice saved in the edited player profile (bytes the PC game no longer uses)

#define k_maximum_profile_option_count 4

struct s_profile_option_menu_definition
{
	e_user_interface_screen_id screen_id;	// the Xbox's screen, whose tag the PC map still has
	uint32 profile_offset;					// in s_saved_game_player_profile
	int32 option_count;
	const wchar_t* options[k_maximum_profile_option_count];
	const wchar_t* header;
	const wchar_t* description;
};

class c_profile_option_list : public c_list_widget
{
protected:
	c_list_item_widget m_list_item_widgets[k_maximum_profile_option_count];
	c_slot2<c_profile_option_list, struct s_event_record*, int32> m_slot_2;
	const s_profile_option_menu_definition* m_definition;

	void handle_item_pressed_event(struct s_event_record* const& event, int32* pitem_index);
public:
	c_profile_option_list(uint16 _flags, const s_profile_option_menu_definition* definition);

	// c_profile_option_list virtual functions

	virtual ~c_profile_option_list() = default;
	virtual c_list_item_widget* get_list_items() override;
	virtual int32 get_list_items_count() override;
	virtual void update_list_items(c_list_item_widget* item, int32 skin_index) override;
};

class c_profile_option_menu : public c_screen_with_menu
{
protected:
	c_profile_option_list m_option_list;
	const s_profile_option_menu_definition* m_definition;

	static void* load_definition(c_screen_parameters* parameters, const s_profile_option_menu_definition* definition);
public:
	c_profile_option_menu(e_user_interface_channel_type _ui_channel, e_user_interface_render_window _window_index, uint16 _flags, const s_profile_option_menu_definition* definition);
	static void* load_voice_output(c_screen_parameters* parameters);
	static void* load_voice_masking(c_screen_parameters* parameters);
	static void* load_online_status(c_screen_parameters* parameters);

	// c_profile_option_menu virtual functions

	virtual ~c_profile_option_menu() = default;
	virtual void initialize(c_screen_parameters* screen_parameters) override;
	virtual void post_initialize() override;
	virtual const void* load_proc(void) const override;
};

// adds the Nameplate entry to the player profile edit screen and sends the chosen nameplate with the player's details
void nameplates_apply_patches(void);
void nameplates_apply_patches_on_map_load(void);

// the rank chosen in the Rank menu (0 = Rank 1) and the user role of the nameplate chosen in the Nameplate menu
int32 rank_get_selected(e_controller_index controller_index);
int8 nameplate_get_user_role(e_controller_index controller_index);

void* ui_load_cartographer_guide_menu();
void* ui_load_skulls_menu();
void* ui_load_cartographer_update_notice_menu();
void* ui_load_cartographer_credits_menu();
void* ui_load_cartographer_update_menu();
