#include "stdafx.h"
#include "SpecialEvents.h"
#include "SpecialEventHelpers.h"
#include "Events/Birthday.h"
#include "Events/Christmas.h"
#include "Events/Halloween.h"
#include "Events/Mook.h"
#include "Events/Paddy.h"

#include "game/game.h"
#include "networking/session/network_session.h"
#include "tag_files/tag_loader/tag_injection.h"

#include "H2MOD/GUI/imgui_integration/imgui_handler.h"
#include "H2MOD/Modules/Shell/Config.h"

const char k_cartographer_shared_missing_message[] =
	"Error: Cartographer Shared map content is missing. Try updating your game from the mainmenu.\r\n\r\n"
	"By going to Cartographer > Update.\r\n\r\n"
	"If that doesn't work reach out to us in #help on discord.";

// Enables event if the current date and time line up with an event time
e_special_event_type get_current_special_event()
{
	// LAN edition: holiday special events removed
	return _special_event_none;
}

void load_special_event()
{
	if (tag_injection_check_map_exists(k_events_map))
	{
		bool create_new_markers = true;
		switch (get_current_special_event())
		{
		case _special_event_christmas:
			christmas_event_map_load();
			break;
		case _special_event_st_paddys:
			paddy_event_map_load();
			break;
		case _special_event_mook_maddness:
			mook_event_map_load();
			break;
		case _special_event_halloween:
			halloween_event_map_load();
			break;
		case _special_event_birthday:
			birthday_event_map_load();
			break;
		default:
			create_new_markers = false;
			break;
		}
		
		if (create_new_markers)
		{
			add_special_event_markers();
		}
	}
	else
	{
#ifndef IMGUI_DISABLE
		if (!NetworkSession::LocalPeerIsSessionHost())
		{
			*Memory::GetAddress<byte*>(0x46DCF1) = 1;
			ImGuiHandler::ImMessageBox::SetMessage(k_cartographer_shared_missing_message);
			ImGuiHandler::ToggleWindow(k_message_box_window_name);
		}
#endif
	}
}

