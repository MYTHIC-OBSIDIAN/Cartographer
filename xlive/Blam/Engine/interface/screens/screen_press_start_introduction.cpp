#include "stdafx.h"
#include "screen_press_start_introduction.h"

#include "interface/user_interface.h"
#include "interface/user_interface_memory.h"
#include "interface/user_interface_widget_text.h"
#include "interface/user_interface_text.h"
#include "interface/screens/screen_cartographer_account_manager.h"
#include "H2MOD/Modules/Accounts/AccountLogin.h"

// typedef

typedef void(__cdecl* t_screen_press_start_introduction_open)(c_screen_parameters* parameters);
t_screen_press_start_introduction_open p_screen_press_start_introduction_open;

c_screen_press_start_introduction::c_screen_press_start_introduction(e_user_interface_channel_type channel_type, e_user_interface_render_window window_index, int16 user_flags) :
	c_screen_widget(_screen_press_start_intro, channel_type, window_index, user_flags),
	m_start_button(0, user_flags),
	m_slot(this, &c_screen_press_start_introduction::handle_item_pressed_event)
{
	this->m_has_input_saved = false;
}

// LAN edition: the start screen prompt "PRESS ANY KEY" (string start_screen_0) reads "PRESS START TO CONTINUE".
// Setting screen text block 1 only added a small line near the bottom, so replace the text wherever the prompt is shown:
// every widget on the screen whose text starts with "PRESS ANY KEY"
#define k_press_start_prompt L"PRESS START TO CONTINUE"
#define k_press_start_original_prompt L"PRESS ANY KEY"

static void press_start_prompt_replace(c_user_interface_widget* widget, const c_user_interface_widget* skip, int32 depth)
{
	for (; widget != NULL && depth < 16; widget = widget->get_next())
	{
		if (widget == skip)
		{
			continue;
		}

		c_user_interface_text* text = widget->get_interface();
		if (text != NULL)
		{
			const wchar_t* current = text->get_raw_string();
			if (current != NULL && _wcsnicmp(current, k_press_start_original_prompt, wcslen(k_press_start_original_prompt)) == 0)
			{
				text->set_text(k_press_start_prompt);
			}
		}
		press_start_prompt_replace(widget->get_children(), skip, depth + 1);
	}
}

void c_screen_press_start_introduction::update()
{
	INVOKE_TYPE(0x23F2FD, 0x0, void(__thiscall*)(c_screen_press_start_introduction*), this);

	// the widgets set their text up themselves, so check every update
	// (screen text block 1 is skipped: it is the hidden copy that showed up as small text near the bottom)
	const c_user_interface_widget* hidden_copy = (c_user_interface_widget*)this->try_find_screen_text(1);
	press_start_prompt_replace(this->get_children(), hidden_copy, 0);
}

bool c_screen_press_start_introduction::handle_event(s_event_record* event)
{
	return INVOKE_TYPE(0x23F0BA, 0x0, bool(__thiscall*)(c_screen_press_start_introduction*, s_event_record*), this, event);
}

CLASS_HOOK_DECLARE_LABEL(c_screen_press_start_introduction__initialize, c_screen_press_start_introduction::initialize);
void c_screen_press_start_introduction::initialize(c_screen_parameters* parameters)
{
	// LAN edition: stay signed in (offline) instead of signing out here. When nobody is signed in, the first press on
	// this screen only starts the sign-in (the LAN edition's offline sign-in finishes straight away, but the press that
	// started it is used up), so it took a second press to continue
	if (!XUserSignedIn(0))
	{
		cartographer_account_manager_open_list();
	}
	
	//orignal c_screen_press_start_introduction::initialize
	INVOKE_TYPE(0x23F180, 0x0, void(__thiscall*)(c_screen_press_start_introduction*, c_screen_parameters*), this, parameters);
}

void c_screen_press_start_introduction::sub_60EBC2(int32 a1)
{
	INVOKE_TYPE(0x23F011, 0x0, void(__thiscall*)(c_screen_press_start_introduction*, int32), this, a1);
}

const void* c_screen_press_start_introduction::load_proc(void) const
{
	return &c_screen_press_start_introduction::load;
}

void c_screen_press_start_introduction::handle_item_pressed_event(
	s_event_record* const& event,
	datum* pitem_index)
{
	m_saved_input = *event;
	m_has_input_saved = true;
	
	return;
}

void* c_screen_press_start_introduction::load(
	c_screen_parameters* parameters)
{
	//return p_screen_press_start_introduction_open(parameters);

	c_screen_press_start_introduction* screen;

	parameters->set_user_flag(2);

	void* pool = ui_pool_allocate_space(sizeof(c_screen_press_start_introduction), 0);
	if (pool)
	{
		screen = new (pool) c_screen_press_start_introduction(
			parameters->get_channel_type(),
			parameters->get_window_index(),
			parameters->get_user_flags()
		);

		screen->m_allocated = true;
		user_interface_register_screen_to_channel(screen, parameters);
	}
	else
	{
		screen = 0;
	}

	return screen;
}


void c_screen_press_start_introduction::apply_instance_patches()
{
	DETOUR_ATTACH(p_screen_press_start_introduction_open, Memory::GetAddress<t_screen_press_start_introduction_open>(0x23f6B7), c_screen_press_start_introduction::load);
}
