#include "stdafx.h"
#include "pc_file_system.h"

/* typedefs */

typedef bool(__cdecl* t_pc_file_system_create_directory_hierarchy)(e_pc_file_system_type type, wchar_t* sub_path, wchar_t* out_path, bool trailing_slash);
t_pc_file_system_create_directory_hierarchy p_pc_file_system_create_directory_hierarchy;

/* public code */

s_pc_file_type_save_path_info* pc_file_system_get_save_path_info(e_pc_file_system_type type)
{
	return &Memory::GetAddress<s_pc_file_type_save_path_info*>(0x413868, 0)[type];
}


void file_seek_and_read(FILE* file_handle, uint32 file_offset, uint32 read_size, uint32 read_count, void* out_buffer)
{
#ifdef ASSERTS_ENABLED
	const bool seek_result = fseek(file_handle, file_offset, SEEK_SET) == 0;
	ASSERT(seek_result);
#else
	fseek(file_handle, file_offset, SEEK_SET);
#endif

#ifdef ASSERTS_ENABLED
	const bool read_result = fread(out_buffer, read_size, read_count, file_handle);
	ASSERT(read_result);
#else
	fread(out_buffer, read_size, read_count, file_handle);
#endif
	return;
}

// LAN edition: folders kept next to halo2.exe instead of the user's AppData / Documents folders
//   player profiles and saved games     savegame                  (was %LocalAppData%\Microsoft\Halo 2\Saved Games)
//   custom multiplayer maps             custom_maps               (was Documents\My Games\Halo 2\Maps)
//   custom map data cache               custom_maps\Data Files    (was Documents\My Games\Halo 2\Maps\Data Files)

// <folder of halo2.exe>\<folder_name>\<sub_path>, creating the folders on the way like the game does
static bool pc_file_system_game_folder_path(const wchar_t* folder_name, const wchar_t* sub_path, wchar_t* out_path, bool trailing_slash)
{
	wchar_t path[MAX_PATH];
	const DWORD module_length = GetModuleFileNameW(NULL, path, NUMBEROF(path));
	if (module_length == 0 || module_length >= NUMBEROF(path))
	{
		return false;
	}

	wchar_t* last_backslash = wcsrchr(path, L'\\');
	if (last_backslash == nullptr)
	{
		return false;
	}
	last_backslash[1] = L'\0';
	const size_t exe_folder_length = wcslen(path);

	if (wcscat_s(path, NUMBEROF(path), folder_name) != 0)
	{
		return false;
	}

	if (sub_path != nullptr && sub_path[0] != L'\0')
	{
		if (sub_path[0] != L'\\' && sub_path[0] != L'/' && wcscat_s(path, NUMBEROF(path), L"\\") != 0)
		{
			return false;
		}
		if (wcscat_s(path, NUMBEROF(path), sub_path) != 0)
		{
			return false;
		}
	}

	for (wchar_t* c = path; *c != L'\0'; c++)
	{
		if (*c == L'/')
		{
			*c = L'\\';
		}
	}

	size_t length = wcslen(path);
	if (trailing_slash && length > 0 && path[length - 1] != L'\\')
	{
		if (length + 1 >= NUMBEROF(path))
		{
			return false;
		}
		path[length++] = L'\\';
		path[length] = L'\0';
	}

	// every folder after the halo2.exe folder (a last part without a trailing slash is a file name)
	for (size_t index = exe_folder_length; index < length; index++)
	{
		if (path[index] == L'\\')
		{
			path[index] = L'\0';
			if (!CreateDirectoryW(path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
			{
				path[index] = L'\\';
				return false;
			}
			path[index] = L'\\';
		}
	}

	if (out_path == nullptr || wcscpy_s(out_path, MAX_PATH, path) != 0)
	{
		return false;
	}
	return true;
}

static bool pc_file_system_path_is_folder(const wchar_t* path)
{
	const DWORD attributes = GetFileAttributesW(path);
	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY);
}

// true when the folder holds at least one saved game / profile folder (S0000000 ...)
static bool pc_file_system_folder_has_saved_games(const wchar_t* folder)
{
	wchar_t search[MAX_PATH];
	if (swprintf(search, NUMBEROF(search), L"%s\\S*", folder) < 0)
	{
		return false;
	}

	WIN32_FIND_DATAW find_data;
	HANDLE find = FindFirstFileW(search, &find_data);
	if (find == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	bool result = false;
	do
	{
		if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			result = true;
			break;
		}
	} while (FindNextFileW(find, &find_data));
	FindClose(find);
	return result;
}

// copies the folder's files and sub folders, existing files are kept
static void pc_file_system_copy_folder(const wchar_t* source, const wchar_t* destination, int32 depth)
{
	if (depth > 8 || (!CreateDirectoryW(destination, NULL) && GetLastError() != ERROR_ALREADY_EXISTS))
	{
		return;
	}

	wchar_t search[MAX_PATH];
	if (swprintf(search, NUMBEROF(search), L"%s\\*", source) < 0)
	{
		return;
	}

	WIN32_FIND_DATAW find_data;
	HANDLE find = FindFirstFileW(search, &find_data);
	if (find == INVALID_HANDLE_VALUE)
	{
		return;
	}

	do
	{
		if (wcscmp(find_data.cFileName, L".") == 0 || wcscmp(find_data.cFileName, L"..") == 0)
		{
			continue;
		}

		wchar_t from[MAX_PATH];
		wchar_t to[MAX_PATH];
		if (swprintf(from, NUMBEROF(from), L"%s\\%s", source, find_data.cFileName) < 0
			|| swprintf(to, NUMBEROF(to), L"%s\\%s", destination, find_data.cFileName) < 0)
		{
			continue;
		}

		if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			pc_file_system_copy_folder(from, to, depth + 1);
		}
		else
		{
			CopyFileW(from, to, TRUE);
		}
	} while (FindNextFileW(find, &find_data));
	FindClose(find);
	return;
}

enum e_game_folder_location
{
	_game_folder_location_unknown,
	_game_folder_location_game_folder,		// next to halo2.exe
	_game_folder_location_original,			// the game's own folder, used when the folder next to halo2.exe can't be written to
};

struct s_game_folder_redirect
{
	e_pc_file_system_type type;
	const wchar_t* folder_name;
	e_game_folder_location location;
};

static s_game_folder_redirect g_game_folder_redirects[] =
{
	{ _pc_file_system_type_saved_games, L"savegame", _game_folder_location_unknown },
	{ _pc_file_system_type_custom_map, L"custom_maps", _game_folder_location_unknown },
	{ _pc_file_system_type_my_games_map_data_files, L"custom_maps\\Data Files", _game_folder_location_unknown },
};

// saved games only: copy the profiles and saves from the old folder if the new one has none yet
// (the old folder is left as it was; custom maps are not copied, they can be large)
static void pc_file_system_saved_games_copy_from_appdata(wchar_t* new_folder)
{
	wchar_t old_folder[MAX_PATH];
	if (FAILED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, old_folder))
		|| wcscat_s(old_folder, NUMBEROF(old_folder), L"\\Microsoft\\Halo 2\\Saved Games") != 0)
	{
		return;
	}

	if (pc_file_system_path_is_folder(old_folder)
		&& !pc_file_system_folder_has_saved_games(new_folder)
		&& pc_file_system_folder_has_saved_games(old_folder))
	{
		pc_file_system_copy_folder(old_folder, new_folder, 0);
	}
	return;
}

// first request for the folder (on the game thread, not at dll load): check the folder next to halo2.exe can be
// written to, otherwise keep the game's own folder for this session
static void pc_file_system_game_folder_initialize(s_game_folder_redirect* redirect)
{
	redirect->location = _game_folder_location_original;

	wchar_t new_folder[MAX_PATH];
	if (!pc_file_system_game_folder_path(redirect->folder_name, nullptr, new_folder, true))
	{
		error(_error_log, "a folder could not be created next to halo2.exe, using the game's own folder");
		return;
	}

	wchar_t test_file[MAX_PATH];
	if (swprintf(test_file, NUMBEROF(test_file), L"%swrite_test.tmp", new_folder) < 0)
	{
		return;
	}
	HANDLE test = CreateFileW(test_file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
	if (test == INVALID_HANDLE_VALUE)
	{
		error(_error_log, "a folder next to halo2.exe can't be written to, using the game's own folder");
		return;
	}
	CloseHandle(test);

	redirect->location = _game_folder_location_game_folder;

	if (redirect->type == _pc_file_system_type_saved_games)
	{
		// without the trailing backslash
		new_folder[wcslen(new_folder) - 1] = L'\0';
		pc_file_system_saved_games_copy_from_appdata(new_folder);
	}
	return;
}

// type, path appended to the type's folder, out path (MAX_PATH), add a trailing backslash
static bool __cdecl pc_file_system_create_directory_hierarchy(e_pc_file_system_type type, wchar_t* sub_path, wchar_t* out_path, bool trailing_slash)
{
	for (uint32 index = 0; index < NUMBEROF(g_game_folder_redirects); index++)
	{
		s_game_folder_redirect* redirect = &g_game_folder_redirects[index];
		if (redirect->type != type)
		{
			continue;
		}

		if (redirect->location == _game_folder_location_unknown)
		{
			pc_file_system_game_folder_initialize(redirect);
		}

		if (redirect->location == _game_folder_location_game_folder
			&& pc_file_system_game_folder_path(redirect->folder_name, sub_path, out_path, trailing_slash))
		{
			return true;
		}
		break;
	}

	return p_pc_file_system_create_directory_hierarchy(type, sub_path, out_path, trailing_slash);
}

// the game creates a new profile / saved game slot and reads it back without checking it was created
// (halo2.exe+0x9C9B3: mov eax, [esp+10h]; mov ecx, [eax+4]): fail the request instead of crashing
static uintptr_t g_saved_game_slot_created_return;
static uintptr_t g_saved_game_slot_failed_return;
static __declspec(naked) void saved_game_slot_null_check(void)
{
	__asm
	{
		mov eax, [esp + 0x10]
		test eax, eax
		jz slot_failed
		mov ecx, [eax + 0x4]
		jmp dword ptr [g_saved_game_slot_created_return]

	slot_failed:
		mov edi, 0x57
		jmp dword ptr [g_saved_game_slot_failed_return]
	}
}

bool __cdecl pc_file_system_get_drive_letter_for_type(e_pc_file_system_type type, wchar_t* buffer)
{
	return INVOKE(0x8F1FE, 0, pc_file_system_get_drive_letter_for_type, type, buffer);
}

void pc_file_system_apply_hooks()
{
	g_saved_game_slot_created_return = Memory::GetAddress(0x9C9BA);
	g_saved_game_slot_failed_return = Memory::GetAddress(0x9C9E9);
	WriteJmpTo(Memory::GetAddress(0x9C9B3), (void*)saved_game_slot_null_check);
	NopFill(Memory::GetAddress(0x9C9B8), 2);

	DETOUR_ATTACH(p_pc_file_system_create_directory_hierarchy, Memory::GetAddress<t_pc_file_system_create_directory_hierarchy>(0x8EF9E), pc_file_system_create_directory_hierarchy);
}
