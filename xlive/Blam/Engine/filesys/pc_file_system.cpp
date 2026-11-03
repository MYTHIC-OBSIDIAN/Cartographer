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

// LAN edition: player profiles and saved games are kept in a "savegame" folder next to halo2.exe
// instead of %LocalAppData%\Microsoft\Halo 2\Saved Games
static const wchar_t k_saved_games_folder_name[] = L"savegame";

// <folder of halo2.exe>\savegame\<sub_path>, creating the folders on the way like the game does
static bool pc_file_system_saved_games_path(const wchar_t* sub_path, wchar_t* out_path, bool trailing_slash)
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

	if (wcscat_s(path, NUMBEROF(path), k_saved_games_folder_name) != 0)
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

// first run with the savegame folder: copy the profiles and saves from the old folder, which is left as it was
static void pc_file_system_saved_games_copy_from_appdata(void)
{
	wchar_t new_folder[MAX_PATH];
	if (!pc_file_system_saved_games_path(nullptr, new_folder, false))
	{
		return;
	}
	if (GetFileAttributesW(new_folder) != INVALID_FILE_ATTRIBUTES)
	{
		// already exists (pc_file_system_saved_games_path only creates the folders before the last part)
		return;
	}

	wchar_t old_folder[MAX_PATH];
	if (FAILED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, old_folder))
		|| wcscat_s(old_folder, NUMBEROF(old_folder), L"\\Microsoft\\Halo 2\\Saved Games") != 0)
	{
		return;
	}
	const DWORD old_attributes = GetFileAttributesW(old_folder);
	if (old_attributes == INVALID_FILE_ATTRIBUTES || !(old_attributes & FILE_ATTRIBUTE_DIRECTORY))
	{
		return;
	}

	if (!CreateDirectoryW(new_folder, NULL))
	{
		return;
	}

	// SHFileOperation takes double null terminated lists
	wchar_t from[MAX_PATH + 4] = {};
	wchar_t to[MAX_PATH + 2] = {};
	if (wcscpy_s(from, MAX_PATH + 2, old_folder) != 0 || wcscat_s(from, MAX_PATH + 2, L"\\*") != 0 || wcscpy_s(to, MAX_PATH + 1, new_folder) != 0)
	{
		return;
	}

	SHFILEOPSTRUCTW operation = {};
	operation.wFunc = FO_COPY;
	operation.pFrom = from;
	operation.pTo = to;
	operation.fFlags = FOF_NO_UI;
	SHFileOperationW(&operation);
	return;
}

// type, path appended to the type's folder, out path (MAX_PATH), add a trailing backslash
static bool __cdecl pc_file_system_create_directory_hierarchy(e_pc_file_system_type type, wchar_t* sub_path, wchar_t* out_path, bool trailing_slash)
{
	if (type == _pc_file_system_type_saved_games)
	{
		return pc_file_system_saved_games_path(sub_path, out_path, trailing_slash);
	}

	return p_pc_file_system_create_directory_hierarchy(type, sub_path, out_path, trailing_slash);
}

bool __cdecl pc_file_system_get_drive_letter_for_type(e_pc_file_system_type type, wchar_t* buffer)
{
	return INVOKE(0x8F1FE, 0, pc_file_system_get_drive_letter_for_type, type, buffer);
}

void pc_file_system_apply_hooks()
{
	pc_file_system_saved_games_copy_from_appdata();

	DETOUR_ATTACH(p_pc_file_system_create_directory_hierarchy, Memory::GetAddress<t_pc_file_system_create_directory_hierarchy>(0x8EF9E), pc_file_system_create_directory_hierarchy);
}
