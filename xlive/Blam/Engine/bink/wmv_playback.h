#pragma once

/* prototypes */

void __cdecl wmv_update(void);

bool __cdecl wmv_playback_in_progress(void);

void __cdecl wmv_render(void);

// LAN edition: plays movies\afk.wmv after 3 minutes idle on the main menu, call every main loop update
void wmv_afk_movie_update(void);
