#include "stdafx.h"
#include "map_repository.h"

#include "bitmaps/bitmap_group.h"
#include "cache/cache_files.h"
#include "interface/signal_slot.h"
#include "interface/user_interface_memory.h"
#include "memory/data.h"
#include "multithreading/synchronization.h"
#include "scenario/scenario_definitions.h"
#include "shell/shell.h"
#include "tag_files/files.h"
#include "text/unicode.h"
#include "main/levels.h"
#include "main/level_definitions.h"

#include <algorithm>

/* prototypes */

static int32 get_directory_path_from_id(e_directory_id id, LPCWSTR pMore, LPWSTR pszPath, bool is_folder);

static bool open_cache_header(const wchar_t* file_path, void* cache_header_ptr, HANDLE* map_handle);

static void __cdecl close_cache_header(HANDLE* map_handle);

static bool __cdecl validate_and_read_custom_map_data(s_custom_map_entry* custom_map_entry);

static void digsite_custom_maps_synchronize(c_map_manager* manager);


void map_repository_apply_sapien_patches(void)
{
	NopFill(Memory::GetAddress(0x31FB3E), 52);	// Prevent "m_map_count < k_maximum_number_of_multiplayer_maps" assert from triggering
	return;
}

#pragma region 50 map limit removal

// TODO: consider replacing this implementation with a linked list

const wchar_t* custom_map_cache_filename_client = L".h2mdat";

static s_custom_map_entry* get_custom_map_entry_list_from_header(s_custom_map_file_cache_header* header)
{
	return (s_custom_map_entry*)((uint8*)header + sizeof(s_custom_map_file_cache_header));
}

c_map_manager* map_manager_get()
{
	c_map_manager* custom_map_manager = Memory::GetAddress<c_map_manager*>(0x482D70, 0x4A70D8);
	return custom_map_manager;
}

const wchar_t* get_custom_map_folder_path()
{
	return map_manager_get()->m_custom_maps_folder_path;
}

void c_map_manager::load_data(void)
{
	map_repository_load();
	return;
}

void c_map_manager::map_synchronize_process(bool async)
{
	INVOKE_TYPE(0x4D31F, 0x0, void(__thiscall*)(c_map_manager*, bool), this, async);
	return;
}

void c_map_manager::create_custom_map_data_directory()
{
	auto p__create_custom_map_data_directory = Memory::GetAddressRelative<void(__cdecl*)()>(0x4C1FD4, 0x48F715);
	p__create_custom_map_data_directory();
}


CLASS_HOOK_DECLARE_LABEL(c_custom_map_manager__mark_all_cached_maps_for_deletion, c_map_manager::mark_all_cached_maps_for_deletion);
void __thiscall c_map_manager::mark_all_cached_maps_for_deletion()
{
	c_critical_section_scope lock(m_lock);

	if (m_new_custom_map_entry_list_buffer == nullptr)
	{
		return;
	}

	for (uint16 i = 0; i < m_map_count; i++)
		m_new_custom_map_entry_list_buffer[i].entry_marked_for_deletion = true;

}

CLASS_HOOK_DECLARE_LABEL(c_custom_map_manager__remove_marked_for_deletion, c_map_manager::remove_marked_for_deletion);
bool __thiscall c_map_manager::remove_marked_for_deletion()
{
	c_critical_section_scope lock(m_lock);

	// runs at the end of the custom maps folder scan: DIGSITE maps come from the game's maps folder instead
	digsite_custom_maps_synchronize(this);

	bool maps_removed = false;
	uint16 map_count_before_remove = m_map_count;

	for (uint16 i = 0; i < m_map_count; )
	{
		if (m_new_custom_map_entry_list_buffer[i].entry_marked_for_deletion)
		{
			remove_entry_by_index(i);
		}
		else
		{
			i++;
		}
	}

	maps_removed = map_count_before_remove > m_map_count;
	return maps_removed;
}

bool c_map_manager::read_custom_map_data_cache_from_file(const utf8* path, 
	s_custom_map_file_cache_header* custom_map_data_header, 
	uint32 custom_map_data_cache_buffer_size
)
{
	s_file_reference cache_file;

	bool success = false;
	bool invalid_file = false;
	e_file_open_error open_file_error_code;

	file_reference_create_from_path(&cache_file, path, false);
	e_file_open_flags flags = (e_file_open_flags)(_file_open_sequential_scan_bit | _permission_read_bit);

	// ### TODO FIXME clean this up
	uint32 entry_count = 0;
	s_custom_map_entry* entry_list = get_custom_map_entry_list_from_header(custom_map_data_header);

	do 
	{
		if (!file_open(&cache_file, flags, &open_file_error_code))
		{
			LOG_TRACE_GAME("{} - failed to open custom map data cache file!",
				__FUNCTION__);

			break;
		}

		// first we read the header
		if (!file_read(&cache_file, sizeof(s_custom_map_file_cache_header), true, custom_map_data_header))
		{
			invalid_file = true;
			LOG_TRACE_GAME("{} - failed to read custom map data cache file header!",
				__FUNCTION__);

			break;
		}

		if (custom_map_data_header->signature != custom_map_cache_signature)
		{
			LOG_TRACE_GAME("{} - invalid custom map data cache file signature! deleting file if possible, name is reserved!",
				__FUNCTION__);

			invalid_file = true;
			break;
		}

		if (custom_map_data_header->version != k_custom_map_data_cache_header_version)
		{
			LOG_TRACE_GAME("{} - custom map data cache file header version mismatch! deleting file if possible",
				__FUNCTION__);

			invalid_file = true;
			break;
		}

		entry_count = custom_map_data_header->entry_count;
		if (entry_count > k_maximum_number_of_custom_multiplayer_maps_new)
		{
			LOG_TRACE_GAME("{} - custom map data cache file exceeds new map limit size, {} > max: {}",
				__FUNCTION__,
				entry_count,
				k_maximum_number_of_custom_multiplayer_maps_new);

			break;
		}

		// if signature matches, read the cache contents
		uint32 file_custom_map_entries_size = entry_count * sizeof(s_custom_map_entry);

		if (!file_read(&cache_file, file_custom_map_entries_size, true, entry_list))
		{
			LOG_TRACE_GAME("{} - failed reading custom map data cache file!",
				__FUNCTION__);

			break;
		}

		// clear unused custom map entries
		size_t remaining_size = custom_map_data_cache_buffer_size - sizeof(s_custom_map_file_cache_header) - file_custom_map_entries_size;
		csmemset((uint8*)entry_list + file_custom_map_entries_size, 0, remaining_size);
		success = true;

	} while (0);

	if (open_file_error_code == _file_open_error_success)
	{
		file_close(&cache_file);
		if (invalid_file)
			file_delete(&cache_file);
	}

	if (success)
	{
		LOG_TRACE_GAME("loaded custom map paths: ");
		for (uint32 i = 0; i < entry_count; i++)
		{
			LOG_TRACE_GAME(L"	custom map path: {}", entry_list[i].file_path);
		}
	}

	return success;
}

bool c_map_manager::write_custom_map_data_cache_to_file(const utf8* path, s_custom_map_file_cache_header* custom_map_data_cache)
{
	s_file_reference cache_file;

	bool success = false;
	e_file_open_error open_file_error_code = _file_open_error_unknown;

	file_reference_create_from_path(&cache_file, path, false);
	bool create_file_success = file_create(&cache_file);

	do 
	{
		if (!file_open(&cache_file, _permission_write_bit, &open_file_error_code))
		{
			LOG_ERROR_GAME("{} - failed to open custom map cache file, error code: {}", __FUNCTION__, (uint32)open_file_error_code);

			break;
		}

		file_set_hidden(&cache_file, true);
		
		uint32 size_to_write = sizeof(s_custom_map_file_cache_header) + sizeof(s_custom_map_entry) * custom_map_data_cache->entry_count;

		if (!file_write(&cache_file, size_to_write, custom_map_data_cache))
		{
			LOG_ERROR_GAME("{} - failed to write custom map cache to file!",
				__FUNCTION__);

			break;
		}

		LOG_TRACE_GAME("{} - saved custom map data cache, map count: {}, write size: {}",
			__FUNCTION__, custom_map_data_cache->entry_count, size_to_write);

		if (!file_set_eof(&cache_file, cache_file.api_result))
		{
			LOG_ERROR_GAME("{} - failed to set custom map cache file size!",
				__FUNCTION__);

			break;
		}

		success = true;

	} while (0);


	if (open_file_error_code == _file_open_error_success)
	{
		file_close(&cache_file);
	}
	else if (!create_file_success && open_file_error_code == _file_open_error_not_found)
	{
		LOG_ERROR_GAME("{} - failed to create custom map data cache while file not present!", __FUNCTION__);
	}

	return success;
}

CLASS_HOOK_DECLARE_LABEL(c_custom_map_manager__save_custom_map_data, c_map_manager::save_custom_map_data);
void __thiscall c_map_manager::save_custom_map_data()
{
	wchar_t path_wide[MAX_PATH];
	char path_multibyte[MAX_PATH];
	bool custom_map_data_path_available = false;

	c_critical_section_scope lock(m_lock);

	s_custom_map_file_cache_header* custom_map_data_to_save = m_custom_map_file_data_cache;

	custom_map_data_to_save->signature = custom_map_cache_signature;
	custom_map_data_to_save->entry_count = m_map_count;
	custom_map_data_to_save->index_of_last_previewed_bitmap = m_index_of_last_previewed_bitmap;

	if (get_directory_path_from_id(e_directory_id::_custom_map_data_dir, L"\0", path_wide, true))
	{
		custom_map_data_path_available = true;
		ustrncat(path_wide, custom_map_cache_filename_client, NUMBEROF(path_wide));
	}

	if (!custom_map_data_path_available
		|| !WideCharToMultiByte(CP_UTF8, 0, path_wide, -1, path_multibyte, MAX_PATH, 0, 0)
		|| !write_custom_map_data_cache_to_file(path_multibyte, custom_map_data_to_save))
	{
		LOG_ERROR_GAME("{} - failed to save custom map data cache!", __FUNCTION__);
	}
}

void c_map_manager::load_map_data_cache_from_file_cache(s_custom_map_file_cache_header* custom_map_file_cache)
{

}

CLASS_HOOK_DECLARE_LABEL(c_custom_map_manager__load_custom_map_data_cache, c_map_manager::map_repository_load);
void __thiscall c_map_manager::map_repository_load()
{
	//typedef void(__thiscall* cache_custom_map_file_image_preview)(c_map_manager* thisx);
	//auto p_cache_custom_map_file_image_previews = Memory::GetAddress<cache_custom_map_file_image_preview>(0x4CC30, 0x41501);
	//return p_cache_custom_map_file_image_previews(this);

	c_critical_section_scope lock(m_lock);

	wchar_t path_wide[MAX_PATH];
	char path_multibyte[MAX_PATH];
	bool custom_map_data_path_available = false;

	uint32 map_file_cache_buffer_size = sizeof(s_custom_map_file_cache_header) + sizeof(s_custom_map_entry) * k_maximum_number_of_custom_multiplayer_maps_new;
	s_custom_map_file_cache_header* custom_map_cache_file_header = (s_custom_map_file_cache_header*)new char[map_file_cache_buffer_size];

	// first create the custom map data directory (if not created already)
	create_custom_map_data_directory();

	if (get_directory_path_from_id(e_directory_id::_custom_map_data_dir, L"\0", path_wide, true))
	{
		custom_map_data_path_available = true;
		ustrncat(path_wide, custom_map_cache_filename_client, NUMBEROF(path_wide));
	}

	if (!custom_map_data_path_available
		|| !WideCharToMultiByte(CP_UTF8, 0, path_wide, -1, path_multibyte, MAX_PATH, 0, 0)
		|| !read_custom_map_data_cache_from_file(path_multibyte, custom_map_cache_file_header, map_file_cache_buffer_size))
	{
		// if we fail clear the buffer
		csmemset(custom_map_cache_file_header, 0, map_file_cache_buffer_size);
		custom_map_cache_file_header->signature = custom_map_cache_signature;
		custom_map_cache_file_header->version = k_custom_map_data_cache_header_version;

		LOG_ERROR_GAME("{} - failed to read custom map data cache!", 
			__FUNCTION__);
	}

	//load_map_data_cache_from_file_cache(custom_map_cache_file_header);

	if (m_custom_map_file_data_cache != nullptr)
		delete[] m_custom_map_file_data_cache;

	this->m_custom_map_file_data_cache = custom_map_cache_file_header;
	this->m_map_count = (uint16)custom_map_cache_file_header->entry_count;
	this->m_new_custom_map_entry_list_buffer	
		= get_custom_map_entry_list_from_header(custom_map_cache_file_header);

	this->m_index_of_last_previewed_bitmap = custom_map_cache_file_header->index_of_last_previewed_bitmap;
}

CLASS_HOOK_DECLARE_LABEL(c_custom_map_manager__start_custom_map_sync, c_map_manager::start_map_synchronize);
void __thiscall c_map_manager::start_map_synchronize(void)
{
	INVOKE_TYPE(0x4D021, 0x419B5, void(__thiscall*)(c_map_manager*), this);
	return;
}

uint32 __thiscall c_map_manager::get_custom_map_list_ids(s_secure_map_id* out_ids, uint32 out_ids_count)
{
	c_critical_section_scope lock(m_lock);
	for (uint16 i = 0; i < m_map_count; i++)
	{
		m_new_custom_map_entry_list_buffer[i].entry_marked_for_deletion = false;
		if (out_ids != nullptr
			&& out_ids_count != 0)
		{
			if (i < out_ids_count)
			{
				csmemcpy(out_ids[i].hash, m_new_custom_map_entry_list_buffer[i].hash, k_sha256_hash_size_bytes);
				ustrncpy(out_ids[i].map_name, m_new_custom_map_entry_list_buffer[i].map_name, k_custom_map_name_length);
			}
		}
	}

	uint32 map_count = this->m_map_count;
	return map_count;
}

CLASS_HOOK_DECLARE_LABEL(c_custom_map_manager__get_custom_map_list_ids_by_map_name, c_map_manager::get_custom_map_list_ids_by_map_name);
uint32 __thiscall c_map_manager::get_custom_map_list_ids_by_map_name(const wchar_t* map_name, s_secure_map_id* out_ids, uint32 out_ids_count)
{
	c_critical_section_scope lock(m_lock);

	uint32 matching_count_found = 0;

	// used by dedicated server
	for (uint16 i = 0; i < m_map_count; i++)
	{
		if (!_wcsnicmp(m_new_custom_map_entry_list_buffer[i].map_name, map_name, k_custom_map_name_length))
		{
			m_new_custom_map_entry_list_buffer[i].entry_marked_for_deletion = false;
			if (out_ids != nullptr
				&& out_ids_count != 0)
			{
				if (matching_count_found < out_ids_count)
				{
					csmemcpy(out_ids[matching_count_found].hash, m_new_custom_map_entry_list_buffer[i].hash, k_sha256_hash_size_bytes);
					ustrncpy(out_ids[matching_count_found].map_name, m_new_custom_map_entry_list_buffer[i].map_name, k_custom_map_name_length);
				}
			}

			matching_count_found++;
		}
	}

	return matching_count_found;
}

// TODO de-duplicate custom map entry matching code
// because just for 1 line of code we re-write same code over and over

CLASS_HOOK_DECLARE_LABEL(c_custom_map_manager__find_matching_entries_by_file_path, c_map_manager::find_matching_entries_by_file_path);
uint32 __thiscall c_map_manager::find_matching_entries_by_file_path(const wchar_t* file_path, s_custom_map_entry** out_custom_map_entries, uint32 out_custom_map_entries_count)
{
	c_critical_section_scope lock(m_lock);

	if (m_new_custom_map_entry_list_buffer == nullptr
		|| file_path == nullptr)
	{
		return 0;
	}

	uint32 matching_count_found = 0;

	for (uint16 i = 0; i < m_map_count; i++)
	{
		if (!_wcsnicmp(m_new_custom_map_entry_list_buffer[i].file_path, file_path, k_map_file_path_size))
		{
			m_new_custom_map_entry_list_buffer[i].entry_marked_for_deletion = false;
			if (out_custom_map_entries)
			{
				if (matching_count_found < out_custom_map_entries_count)
					out_custom_map_entries[matching_count_found] = &m_new_custom_map_entry_list_buffer[i];
			}

			matching_count_found++;
		}
	}
	return matching_count_found;
}

uint32 __thiscall c_map_manager::find_matching_entries_by_sha256_hash(const BYTE* hash, s_custom_map_entry** out_custom_map_entries, uint32 out_custom_map_entries_count)
{
	c_critical_section_scope lock(m_lock);

	if (m_new_custom_map_entry_list_buffer == nullptr
		|| hash == nullptr)
	{
		return 0;
	}

	uint32 matching_count_found = 0;

	for (uint16 i = 0; i < m_map_count; i++)
	{
		if (!memcmp(m_new_custom_map_entry_list_buffer[i].hash, hash, k_sha256_hash_size_bytes))
		{
			m_new_custom_map_entry_list_buffer[i].entry_marked_for_deletion = false;
			if (out_custom_map_entries)
			{
				if (matching_count_found < out_custom_map_entries_count)
					out_custom_map_entries[matching_count_found] = &m_new_custom_map_entry_list_buffer[i];
			}

			matching_count_found++;
		}
	}
	return matching_count_found;
}

CLASS_HOOK_DECLARE_LABEL(c_custom_map_manager__find_matching_entries_by_map_name_and_hash, c_map_manager::find_matching_entries_by_map_name_and_hash);
uint32 __thiscall c_map_manager::find_matching_entries_by_map_name_and_hash(const wchar_t* map_name, const BYTE* sha256_hash, s_custom_map_entry** out_custom_map_entries, uint32 out_custom_map_entries_count)
{
	c_critical_section_scope lock(m_lock);
	if (m_new_custom_map_entry_list_buffer == nullptr
		|| map_name == nullptr)
	{
		return 0;
	}

	uint32 matching_count_found = 0;

	for (uint16 i = 0; i < m_map_count; i++)
	{
		if (!memcmp(m_new_custom_map_entry_list_buffer[i].hash, sha256_hash, k_sha256_hash_size_bytes) && !_wcsnicmp(m_new_custom_map_entry_list_buffer[i].map_name, map_name, k_custom_map_name_length))
		{
			m_new_custom_map_entry_list_buffer[i].entry_marked_for_deletion = false;
			if (out_custom_map_entries)
			{
				if (matching_count_found < out_custom_map_entries_count)
					out_custom_map_entries[matching_count_found] = &m_new_custom_map_entry_list_buffer[i];
			}

			matching_count_found++;
		}
	}
	return matching_count_found;
}

uint32 __thiscall c_map_manager::find_matching_entries_by_map_name(const wchar_t* map_name, s_custom_map_entry** out_custom_map_entries, uint32 out_custom_map_entries_count)
{
	c_critical_section_scope lock(m_lock);

	if (m_new_custom_map_entry_list_buffer == nullptr
		|| map_name == nullptr)
	{
		return 0;
	}

	uint32 matching_count_found = 0;

	for (uint16 i = 0; i < m_map_count; i++)
	{
		if (!_wcsnicmp(m_new_custom_map_entry_list_buffer[i].map_name, map_name, k_custom_map_name_length))
		{
			m_new_custom_map_entry_list_buffer[i].entry_marked_for_deletion = false;
			if (out_custom_map_entries)
			{
				if (matching_count_found < out_custom_map_entries_count)
					out_custom_map_entries[matching_count_found] = &m_new_custom_map_entry_list_buffer[i];
			}

			matching_count_found++;
		}
	}

	return matching_count_found;
}

CLASS_HOOK_DECLARE_LABEL(c_custom_map_manager__get_entry_by_id, c_map_manager::get_entry_by_id);
bool __thiscall c_map_manager::get_entry_by_id(const s_secure_map_id* custom_map_id, s_custom_map_entry** out_entry)
{
	// custom_scenario_test_map_name_instead_of_hash
	if (shell_command_line_flag_is_set(_shell_command_line_flag_custom_map_entry_test_map_name_instead_of_hash))
		return find_matching_entries_by_map_name(custom_map_id->map_name, out_entry, 1) != 0;
	else
		return find_matching_entries_by_sha256_hash(custom_map_id->hash, out_entry, 1) != 0;
}

bool __thiscall c_map_manager::entry_is_duplicate(const s_custom_map_entry* entry)
{
	c_critical_section_scope lock(m_lock);

	bool duplicate = false;

	if (find_matching_entries_by_file_path(entry->file_path, 0, 0) > 0u
		|| find_matching_entries_by_map_name_and_hash(entry->map_name, entry->hash, 0, 0) > 0u)
	{
		duplicate = true;
	}

	return duplicate;
}

bool __thiscall c_map_manager::validate_entry_data(const s_custom_map_entry* entries, uint32 count)
{
	typedef bool(__thiscall* validate_entry_data_t)(c_map_manager*, const s_custom_map_entry* entry, uint32 count);
	auto p_validate_entry_data = Memory::GetAddressRelative<validate_entry_data_t>(0x4C1E01, 0x48F542);
	return p_validate_entry_data(this, entries, count * sizeof(s_custom_map_entry));
}

void __thiscall c_map_manager::remove_entry_by_index(uint16 idx)
{
	// check if the element to be deleted is the last one
	// if so just memset to 0
	if (idx == m_map_count - 1)
	{
		csmemset(&m_new_custom_map_entry_list_buffer[idx], 0, sizeof(s_custom_map_entry));
	}
	else
	{
		// otherwise just move the memory from the next element to last in the now empty entry
		// TODO: consider replacing this implementation with a linked list
		// this might get very expensive
		// but it shouldn't execute too many times
		csmemmove(&m_new_custom_map_entry_list_buffer[idx], &m_new_custom_map_entry_list_buffer[idx + 1], (m_map_count - 1 - idx) * sizeof(s_custom_map_entry));
	}

	m_map_count--;
}

bool __thiscall c_map_manager::remove_entries_matching_file_path(const s_custom_map_entry* entry)
{
	c_critical_section_scope lock(m_lock);

	bool removed = false;
	for (uint16 i = 0; i < m_map_count; )
	{
		if (!_wcsnicmp(m_new_custom_map_entry_list_buffer[i].file_path, entry->file_path, k_map_file_path_size))
		{
			removed = true;
			remove_entry_by_index(i);
		}
		else
		{
			i++;
		}
	}

	return removed;
}

bool __thiscall c_map_manager::remove_duplicates_by_map_name_and_hash(const s_custom_map_entry* entry)
{
	c_critical_section_scope lock(m_lock);

	bool removed = false;
	for (uint16 i = 0; i < m_map_count; i++)
	{
		if (!memcmp(m_new_custom_map_entry_list_buffer[i].hash, entry->hash, k_sha256_hash_size_bytes) 
			&& !_wcsnicmp(m_new_custom_map_entry_list_buffer[i].map_name, entry->map_name, k_custom_map_name_length))
		{
			//LOG_TRACE_GAME(L"{} - removed: {} from cache matching cached entry: map name: {} map path: {}",
				//__FUNCTIONW__, entry->file_path, new_custom_map_entries_buffer[i].map_name, new_custom_map_entries_buffer[i].file_path);
			remove_entry_by_index(i);
			removed = true;
		}
	}

	return removed;
}

CLASS_HOOK_DECLARE_LABEL(c_custom_map_manager__delete_single_entry, c_map_manager::delete_single_entry);
bool __thiscall c_map_manager::delete_single_entry(const s_custom_map_entry* entry)
{
	c_critical_section_scope lock(m_lock);

	bool removed = false;

	for (uint16 i = 0; i < m_map_count; i++)
	{
		if (!memcmp(&m_new_custom_map_entry_list_buffer[i], entry, sizeof(s_custom_map_entry)))
		{
			remove_entry_by_index(i);
			removed = true;
			break;
		}
	}

	return removed;
}

CLASS_HOOK_DECLARE_LABEL(c_custom_map_manager__add_entry, c_map_manager::add_entry);
bool __thiscall c_map_manager::add_entry(const s_custom_map_entry* entry)
{
	c_critical_section_scope lock(m_lock);

	bool success = true;

	if (m_map_count < k_maximum_number_of_custom_multiplayer_maps_new && validate_entry_data(entry, 1) && !entry_is_duplicate(entry))
	{
		csmemcpy(&m_new_custom_map_entry_list_buffer[m_map_count++], entry, sizeof(s_custom_map_entry));
	}
	else
	{
		success = false;
	}

	return success;
}

CLASS_HOOK_DECLARE_LABEL(c_custom_map_manager__remove_duplicates_and_add_entry, c_map_manager::remove_duplicates_and_add_entry);
bool __thiscall c_map_manager::remove_duplicates_and_add_entry(const s_custom_map_entry* entry)
{
	remove_entries_matching_file_path(entry);
	remove_duplicates_by_map_name_and_hash(entry);
	return add_entry(entry);
}

// compared to the other function
// this reads the file and populates the map entry datas
CLASS_HOOK_DECLARE_LABEL(c_custom_map_manager__remove_duplicates_write_entry_data_and_add, c_map_manager::remove_duplicates_write_entry_data_and_add);
bool __thiscall c_map_manager::remove_duplicates_write_entry_data_and_add(s_custom_map_entry* entry)
{
	remove_entries_matching_file_path(entry);
	remove_duplicates_by_map_name_and_hash(entry);

	typedef bool(__thiscall* read_map_data_and_add_entry_t)(c_map_manager*, s_custom_map_entry* entry);
	auto p_read_map_data_and_add_entry = Memory::GetAddressRelative<read_map_data_and_add_entry_t>(0x44CC35);

	return p_read_map_data_and_add_entry(this, entry);
}

bool __thiscall c_map_manager::add_custom_map_entry_by_map_file_path(const std::wstring& file_path)
{
	return add_custom_map_entry_by_map_file_path(file_path.c_str());
}

bool __thiscall c_map_manager::add_custom_map_entry_by_map_file_path(const wchar_t* file_path)
{
	s_custom_map_entry custom_map_new_entry;
	ZeroMemory(&custom_map_new_entry, sizeof(s_custom_map_entry));

	wcscpy_s(custom_map_new_entry.file_path, k_map_file_path_size, file_path);

	bool map_loaded = false;
	if (validate_and_read_custom_map_data(&custom_map_new_entry)) {

		if (remove_duplicates_write_entry_data_and_add(&custom_map_new_entry)) {
			map_loaded = true;
		}
	}

	return map_loaded;
}

CLASS_HOOK_DECLARE_LABEL(c_custom_map_manager__initialize, c_map_manager::initialize);
void __thiscall c_map_manager::initialize()
{
	m_index_of_last_previewed_bitmap = 0;
	m_map_count = 0;
	m_lock = new CRITICAL_SECTION;
	InitializeCriticalSection(m_lock);
}

// atexit
CLASS_HOOK_DECLARE_LABEL(c_custom_map_manager__cleanup, c_map_manager::cleanup);
void __thiscall c_map_manager::cleanup()
{
	m_new_custom_map_entry_list_buffer = nullptr;
	if (m_custom_map_file_data_cache)
		delete[] m_custom_map_file_data_cache;

	m_custom_map_file_data_cache = nullptr;

	DeleteCriticalSection(m_lock);
	delete m_lock;
}

static __declspec(naked) void jmp_get_entry_by_id() 
{ 
	CLASS_HOOK_JMP(c_custom_map_manager__get_entry_by_id, c_map_manager::get_entry_by_id);
}
static __declspec(naked) void jmp_load_custom_map_data_cache()
{
	CLASS_HOOK_JMP(c_custom_map_manager__load_custom_map_data_cache, c_map_manager::map_repository_load);
}
static __declspec(naked) void jmp_start_custom_map_sync()
{
	CLASS_HOOK_JMP(c_custom_map_manager__start_custom_map_sync, c_map_manager::start_map_synchronize);
}
static __declspec(naked) void jmp_mark_all_cached_maps_for_deletion()
{
	CLASS_HOOK_JMP(c_custom_map_manager__mark_all_cached_maps_for_deletion, c_map_manager::mark_all_cached_maps_for_deletion);
}
static __declspec(naked) void jmp_find_matching_entries_by_file_path()
{
	CLASS_HOOK_JMP(c_custom_map_manager__find_matching_entries_by_file_path, c_map_manager::find_matching_entries_by_file_path);
}
static __declspec(naked) void jmp_find_matching_entries_by_map_name_and_hash()
{
	CLASS_HOOK_JMP(c_custom_map_manager__find_matching_entries_by_map_name_and_hash, c_map_manager::find_matching_entries_by_map_name_and_hash);
}
static __declspec(naked) void jmp_delete_single_entry()
{
	CLASS_HOOK_JMP(c_custom_map_manager__delete_single_entry, c_map_manager::delete_single_entry);
}
static __declspec(naked) void jmp_remove_duplicates_and_add_entry()
{
	CLASS_HOOK_JMP(c_custom_map_manager__remove_duplicates_and_add_entry, c_map_manager::remove_duplicates_and_add_entry);
}
static __declspec(naked) void jmp_save_custom_map_data()
{
	CLASS_HOOK_JMP(c_custom_map_manager__save_custom_map_data, c_map_manager::save_custom_map_data);
}
static __declspec(naked) void jmp_remove_marked_for_deletion()
{
	CLASS_HOOK_JMP(c_custom_map_manager__remove_marked_for_deletion, c_map_manager::remove_marked_for_deletion);
}
static __declspec(naked) void jmp_add_entry()
{
	CLASS_HOOK_JMP(c_custom_map_manager__add_entry, c_map_manager::add_entry);
}
static __declspec(naked) void jmp_remove_duplicates_write_entry_data_and_add()
{
	CLASS_HOOK_JMP(c_custom_map_manager__remove_duplicates_write_entry_data_and_add, c_map_manager::remove_duplicates_write_entry_data_and_add);
}
static __declspec(naked) void jmp_cleanup()
{
	CLASS_HOOK_JMP(c_custom_map_manager__cleanup, c_map_manager::cleanup);
}
static __declspec(naked) void jmp_initialize()
{
	CLASS_HOOK_JMP(c_custom_map_manager__initialize, c_map_manager::initialize);
}
static __declspec(naked) void jmp_get_custom_map_list_ids_by_map_name()
{
	CLASS_HOOK_JMP(c_custom_map_manager__get_custom_map_list_ids_by_map_name, c_map_manager::get_custom_map_list_ids_by_map_name);
}

/* DIGSITE map selection tab (LAN edition) */

// The custom game map select screen has a DEFAULT tab (built-in levels from mainmenu.map) and a CUSTOM tab (map files
// from the custom maps folder). A third tab, DIGSITE, is added after CUSTOM:
//  - built-in levels sorted after Tombstone move from DEFAULT to DIGSITE
//  - the custom map files listed below move from CUSTOM to DIGSITE
// DIGSITE reuses the CUSTOM list widget. Built-in levels in it are stored as an s_secure_map_id with a marker in place
// of the SHA-256 hash, and are selected the same way the DEFAULT tab selects them.

// custom map files (name without .map) listed under DIGSITE instead of CUSTOM
static const wchar_t* const k_digsite_custom_map_files[] =
{
	L"03_ascension",
	L"03_burial_mounds",
	L"03_colossus",
	L"03_headlong",
	L"03_lockout",
	L"03_midship",
	L"03_waterworks",
	L"artery",
	L"cloud_city",
	L"earth_ark",
	L"evergreen",
	L"fourth_station",
	L"gauntlet",
	L"grotto",
	L"missile_beach",
	L"overboard",
	L"shaft",
	L"soccer",
	L"solitude",
	L"sunken_chamber",
	L"arcology",
	L"bloodgulch_test",
	L"m1_assault",
	L"m1_defense",
	L"m1_exploration",
};

// preview pictures of the DIGSITE map files, stored in preview bitmap tags that the LAN edition mainmenu.map doesn't otherwise use
struct s_digsite_custom_map_preview
{
	const wchar_t* map_file_name;
	const char* bitmap_tag_name;
	int16 bitmap_index;
};

static const s_digsite_custom_map_preview k_digsite_custom_map_previews[] =
{
	{ L"03_ascension",     "ui\\code_global_bitmaps\\multiplayer35", 0 },
	{ L"03_burial_mounds", "ui\\code_global_bitmaps\\multiplayer36", 0 },
	{ L"03_colossus",      "ui\\code_global_bitmaps\\multiplayer37", 0 },
	{ L"03_headlong",      "ui\\code_global_bitmaps\\multiplayer38", 0 },
	{ L"03_lockout",       "ui\\code_global_bitmaps\\multiplayer39", 0 },
	{ L"03_midship",       "ui\\code_global_bitmaps\\multiplayer40", 0 },
	{ L"03_waterworks",    "ui\\code_global_bitmaps\\multiplayer41", 0 },
	{ L"artery",           "ui\\code_global_bitmaps\\multiplayer42", 0 },
	{ L"cloud_city",       "ui\\code_global_bitmaps\\multiplayer43", 0 },
	{ L"earth_ark",        "ui\\code_global_bitmaps\\campaign19", 0 },
	{ L"evergreen",        "ui\\code_global_bitmaps\\multiplayer44", 0 },
	{ L"fourth_station",   "ui\\code_global_bitmaps\\multiplayer45", 0 },
	{ L"gauntlet",         "ui\\code_global_bitmaps\\multiplayer46", 0 },
	{ L"grotto",           "ui\\code_global_bitmaps\\multiplayer47", 0 },
	{ L"missile_beach",    "ui\\code_global_bitmaps\\multiplayer48", 0 },
	{ L"overboard",        "ui\\code_global_bitmaps\\multiplayer32", 0 },
	{ L"shaft",            "ui\\code_global_bitmaps\\campaign15", 0 },
	{ L"soccer",           "ui\\code_global_bitmaps\\campaign16", 0 },
	{ L"solitude",         "ui\\code_global_bitmaps\\campaign17", 0 },
	{ L"sunken_chamber",   "ui\\code_global_bitmaps\\campaign18", 0 },
	// these five share one tag that holds five bitmaps
	{ L"arcology",         "ui\\code_global_bitmaps\\multiplayer49", 0 },
	{ L"bloodgulch_test",  "ui\\code_global_bitmaps\\multiplayer49", 1 },
	{ L"m1_assault",       "ui\\code_global_bitmaps\\multiplayer49", 2 },
	{ L"m1_defense",       "ui\\code_global_bitmaps\\multiplayer49", 3 },
	{ L"m1_exploration",   "ui\\code_global_bitmaps\\multiplayer49", 4 },
};

// the DEFAULT list ends at this level (Tombstone), everything sorted after it is listed under DIGSITE
#define k_digsite_last_default_level_path "\\highplains"
#define k_digsite_last_default_level_sort_order_fallback 1500

// first bytes of the hash of a DIGSITE list entry that refers to a built-in level, followed by the level's map id
static const uint8 k_digsite_builtin_level_marker[8] = { 'D', 'I', 'G', 'S', 'I', 'T', 'E', 0xB1 };

// most built-in levels the DIGSITE tab lists (same as the game's multiplayer level limit)
#define k_maximum_number_of_digsite_builtin_levels 50u
// extra room in the CUSTOM/DIGSITE list for custom maps found after the screen opened
#define k_digsite_custom_map_list_spare_slots 32u

#define k_map_select_tab_index_offset 0x1A74
#define k_map_select_tab_changed_offset 0x1A78
#define k_map_select_custom_list_offset 0xA60
#define k_map_select_header_text_widget_index 3
#define k_map_list_data_offset 0x70
#define k_map_list_ignore_selection_offset 0x7E8
#define k_map_list_save_to_profile_offset 0x804
#define k_custom_map_ui_level_definition_size 0xC58

static bool g_map_select_digsite_tab_active = false;

static data_array* __cdecl levels_get_multiplayer_level_data(void)
{
	return INVOKE(0x5945E, 0x0, levels_get_multiplayer_level_data);
}

static const wchar_t* __cdecl multiplayer_ui_level_get_display_name(const s_multiplayer_ui_level_definition* level)
{
	return INVOKE(0x381CA5, 0x0, multiplayer_ui_level_get_display_name, level);
}

static void* list_data_entry_get(data_array* data, datum index)
{
	return (uint8*)data->data + data->size * DATUM_INDEX_TO_ABSOLUTE_INDEX(index);
}

static int32 digsite_get_last_default_level_sort_order(void)
{
	data_array* levels = levels_get_multiplayer_level_data();
	if (levels != nullptr)
	{
		for (datum level_index = data_next_index(levels, NONE); level_index != NONE; level_index = data_next_index(levels, level_index))
		{
			const int32 map_id = *(int32*)((uint8*)list_data_entry_get(levels, level_index) + 4);
			const s_multiplayer_ui_level_definition* level = levels_get_multiplayer_ui_level(map_id);
			if (level != nullptr && strstr(level->path, k_digsite_last_default_level_path) != nullptr)
			{
				return level->sort_order;
			}
		}
	}
	return k_digsite_last_default_level_sort_order_fallback;
}

static bool digsite_level_is_builtin_digsite(const s_multiplayer_ui_level_definition* level, int32 last_default_sort_order)
{
	return level != nullptr && level->sort_order > last_default_sort_order;
}

// file name of a map file without its folder and .map extension
static void custom_map_file_get_name(const wchar_t* file_path, wchar_t* out_name, size_t out_name_count)
{
	const wchar_t* file_name = file_path;
	for (const wchar_t* c = file_path; *c != L'\0'; c++)
	{
		if (*c == L'\\' || *c == L'/')
		{
			file_name = c + 1;
		}
	}

	wcsncpy_s(out_name, out_name_count, file_name, _TRUNCATE);
	wchar_t* extension = wcsrchr(out_name, L'.');
	if (extension != nullptr && _wcsicmp(extension, L".map") == 0)
	{
		*extension = L'\0';
	}
}

// compares map file names ignoring case, spaces, underscores and hyphens ("Earth Ark" matches "earth_ark")
static bool digsite_map_file_names_match(const wchar_t* a, const wchar_t* b)
{
	auto is_separator = [](wchar_t c) { return c == L' ' || c == L'_' || c == L'-'; };
	for (;;)
	{
		while (is_separator(*a)) a++;
		while (is_separator(*b)) b++;
		if (*a == L'\0' || *b == L'\0')
		{
			return *a == *b;
		}
		if (towlower(*a) != towlower(*b))
		{
			return false;
		}
		a++;
		b++;
	}
}

static bool custom_map_file_is_digsite(const wchar_t* file_path)
{
	wchar_t name[MAX_PATH];
	custom_map_file_get_name(file_path, name, NUMBEROF(name));

	for (const wchar_t* digsite_name : k_digsite_custom_map_files)
	{
		if (digsite_map_file_names_match(name, digsite_name))
		{
			return true;
		}
	}
	return false;
}

// <game install folder>\maps\ (DIGSITE map files are read from here, not from the custom maps folder)
static const wchar_t* digsite_get_game_maps_folder(void)
{
	static wchar_t s_game_maps_folder[MAX_PATH] = L"";
	if (s_game_maps_folder[0] == L'\0')
	{
		wchar_t exe_path[MAX_PATH];
		const DWORD length = GetModuleFileNameW(NULL, exe_path, NUMBEROF(exe_path));
		if (length > 0 && length < NUMBEROF(exe_path))
		{
			wchar_t* last_separator = wcsrchr(exe_path, L'\\');
			if (last_separator != nullptr)
			{
				last_separator[1] = L'\0';
				swprintf_s(s_game_maps_folder, L"%smaps\\", exe_path);
			}
		}
	}
	return s_game_maps_folder;
}

static bool custom_map_file_is_in_game_maps_folder(const wchar_t* file_path)
{
	const wchar_t* game_maps_folder = digsite_get_game_maps_folder();
	const size_t folder_length = wcslen(game_maps_folder);
	if (folder_length == 0 || _wcsnicmp(file_path, game_maps_folder, folder_length) != 0)
	{
		return false;
	}
	// directly in the folder, not in a sub-folder
	return wcschr(file_path + folder_length, L'\\') == nullptr && wcschr(file_path + folder_length, L'/') == nullptr;
}

static bool custom_map_entry_is_game_maps_folder_digsite(const s_custom_map_entry* entry)
{
	return custom_map_file_is_digsite(entry->file_path) && custom_map_file_is_in_game_maps_folder(entry->file_path);
}

// preview bitmap tag of a DIGSITE map file, NONE if it has none
static datum digsite_custom_map_preview_bitmap_get(const wchar_t* file_path, int16* out_bitmap_index)
{
	wchar_t name[MAX_PATH];
	custom_map_file_get_name(file_path, name, NUMBEROF(name));

	for (const s_digsite_custom_map_preview& preview : k_digsite_custom_map_previews)
	{
		if (digsite_map_file_names_match(name, preview.map_file_name))
		{
			if (out_bitmap_index != nullptr)
			{
				*out_bitmap_index = preview.bitmap_index;
			}
			return tag_loaded('bitm', preview.bitmap_tag_name);
		}
	}
	return NONE;
}

static void ui_bitmap_precache(datum bitmap_tag_index)
{
	INVOKE_TYPE(0x21D228, 0x0, void(__cdecl*)(datum), bitmap_tag_index);
}

// called with the map manager locked, at the end of the custom maps folder scan, before entries of missing files are removed
static void digsite_custom_maps_synchronize(c_map_manager* manager)
{
	if (manager->m_new_custom_map_entry_list_buffer == nullptr)
	{
		return;
	}

	// DIGSITE map files in the custom maps folder are ignored
	for (uint16 i = 0; i < manager->m_map_count; i++)
	{
		s_custom_map_entry* entry = &manager->m_new_custom_map_entry_list_buffer[i];
		if (custom_map_file_is_digsite(entry->file_path) && !custom_map_file_is_in_game_maps_folder(entry->file_path))
		{
			entry->entry_marked_for_deletion = true;
		}
	}

	s_custom_map_entry_linked_list** link = &manager->m_list_of_maps_to_insert_to_cache;
	while (*link != nullptr)
	{
		s_custom_map_entry_linked_list* node = *link;
		if (custom_map_file_is_digsite(node->custom_map_entry.file_path) && !custom_map_file_is_in_game_maps_folder(node->custom_map_entry.file_path))
		{
			*link = node->next;
			INVOKE_TYPE(0x287C23, 0x0, void(__cdecl*)(void*), node);
		}
		else
		{
			link = &node->next;
		}
	}

	// DIGSITE map files in the game's maps folder: keep the ones already loaded, queue new or changed ones
	const wchar_t* game_maps_folder = digsite_get_game_maps_folder();
	if (game_maps_folder[0] == L'\0')
	{
		return;
	}

	wchar_t search_path[k_map_file_path_size];
	if (swprintf_s(search_path, L"%s*.map", game_maps_folder) < 0)
	{
		return;
	}

	WIN32_FIND_DATAW find_data;
	HANDLE find_handle = FindFirstFileW(search_path, &find_data);
	if (find_handle == INVALID_HANDLE_VALUE)
	{
		return;
	}

	do
	{
		if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0
			|| !custom_map_file_is_digsite(find_data.cFileName))
		{
			continue;
		}

		wchar_t file_path[k_map_file_path_size];
		if (swprintf_s(file_path, L"%s%s", game_maps_folder, find_data.cFileName) < 0)
		{
			continue;
		}


		// also clears the entry's deletion mark
		s_custom_map_entry* existing_entry = nullptr;
		if (manager->find_matching_entries_by_file_path(file_path, &existing_entry, 1) > 0
			&& existing_entry != nullptr
			&& CompareFileTime(&existing_entry->file_time, &find_data.ftLastWriteTime) == 0)
		{
			continue;
		}

		bool already_queued = false;
		for (s_custom_map_entry_linked_list* node = manager->m_list_of_maps_to_insert_to_cache; node != nullptr; node = node->next)
		{
			if (_wcsicmp(node->custom_map_entry.file_path, file_path) == 0)
			{
				already_queued = true;
				break;
			}
		}
		if (already_queued)
		{
			continue;
		}

		// same as the game's folder scan: the node is read, validated and added to the cache by map_synchronize_process
		s_custom_map_entry_linked_list* node = (s_custom_map_entry_linked_list*)INVOKE_TYPE(0x287C86, 0x0, void*(__cdecl*)(uint32), sizeof(s_custom_map_entry_linked_list));
		if (node == nullptr)
		{
			break;
		}
		csmemset(node, 0, sizeof(s_custom_map_entry_linked_list));
		custom_map_file_get_name(find_data.cFileName, node->custom_map_entry.map_name, NUMBEROF(node->custom_map_entry.map_name));
		wcsncpy_s(node->custom_map_entry.file_path, file_path, _TRUNCATE);
		node->custom_map_entry.file_time = find_data.ftLastWriteTime;
		node->task_in_progress = false;
		node->async_task_id = NONE;
		node->map_valid = false;
		node->task_finished = false;
		node->next = manager->m_list_of_maps_to_insert_to_cache;
		manager->m_list_of_maps_to_insert_to_cache = node;
	} while (FindNextFileW(find_handle, &find_data));

	FindClose(find_handle);
}

// same as c_map_manager::get_custom_map_list_ids, but only the maps that belong to the CUSTOM tab (digsite == false) or the DIGSITE tab
static uint32 custom_map_list_get_ids_for_tab(s_secure_map_id* out_ids, uint32 out_ids_count, bool digsite)
{
	c_map_manager* manager = map_manager_get();
	c_critical_section_scope lock(manager->m_lock);

	uint32 count = 0;
	for (uint16 i = 0; i < manager->m_map_count; i++)
	{
		const s_custom_map_entry* entry = &manager->m_new_custom_map_entry_list_buffer[i];
		// CUSTOM: everything except DIGSITE map files, DIGSITE: DIGSITE map files from the game's maps folder only
		const bool include = (digsite
			? custom_map_entry_is_game_maps_folder_digsite(entry)
			: !custom_map_file_is_digsite(entry->file_path));
		if (!include)
		{
			continue;
		}

		if (out_ids != nullptr && count < out_ids_count)
		{
			csmemcpy(out_ids[count].hash, entry->hash, k_sha256_hash_size_bytes);
			ustrncpy(out_ids[count].map_name, entry->map_name, k_custom_map_name_length);

			if (digsite)
			{
				const datum preview_bitmap = digsite_custom_map_preview_bitmap_get(entry->file_path, nullptr);
				if (preview_bitmap != NONE)
				{
					ui_bitmap_precache(preview_bitmap);
				}
			}
		}
		count++;
	}
	return count;
}

static void digsite_builtin_level_make_id(s_secure_map_id* id, int32 map_id, const wchar_t* name)
{
	csmemset(id, 0, sizeof(*id));
	if (name != nullptr)
	{
		wcsncpy_s(id->map_name, name, _TRUNCATE);
	}
	csmemcpy(id->hash, k_digsite_builtin_level_marker, sizeof(k_digsite_builtin_level_marker));
	csmemcpy(&id->hash[sizeof(k_digsite_builtin_level_marker)], &map_id, sizeof(map_id));
}

static bool digsite_builtin_level_get_map_id(const s_secure_map_id* id, int32* out_map_id)
{
	if (memcmp(id->hash, k_digsite_builtin_level_marker, sizeof(k_digsite_builtin_level_marker)) != 0)
	{
		return false;
	}
	csmemcpy(out_map_id, &id->hash[sizeof(k_digsite_builtin_level_marker)], sizeof(*out_map_id));
	return true;
}

// fills the CUSTOM list widget's data with the entries of the CUSTOM tab or the DIGSITE tab
static void custom_game_custom_map_list_populate(data_array* data, bool digsite)
{
	if (data == nullptr)
	{
		return;
	}

	std::vector<s_secure_map_id> ids;

	if (digsite)
	{
		// built-in levels after Tombstone, in the same order as the DEFAULT list
		const int32 last_default_sort_order = digsite_get_last_default_level_sort_order();
		data_array* levels = levels_get_multiplayer_level_data();
		if (levels != nullptr)
		{
			for (datum level_index = data_next_index(levels, NONE); level_index != NONE; level_index = data_next_index(levels, level_index))
			{
				const int32 map_id = *(int32*)((uint8*)list_data_entry_get(levels, level_index) + 4);
				const s_multiplayer_ui_level_definition* level = levels_get_multiplayer_ui_level(map_id);
				if (digsite_level_is_builtin_digsite(level, last_default_sort_order)
					&& ids.size() < k_maximum_number_of_digsite_builtin_levels)
				{
					s_secure_map_id id;
					digsite_builtin_level_make_id(&id, map_id, multiplayer_ui_level_get_display_name(level));
					ids.push_back(id);
				}
			}
		}
	}

	const size_t first_custom_map = ids.size();
	const uint32 custom_map_count = custom_map_list_get_ids_for_tab(nullptr, 0, digsite);
	if (custom_map_count > 0)
	{
		ids.resize(first_custom_map + custom_map_count);
		const uint32 filled = custom_map_list_get_ids_for_tab(&ids[first_custom_map], custom_map_count, digsite);
		ids.resize(first_custom_map + (filled < custom_map_count ? filled : custom_map_count));

		if (digsite)
		{
			// DIGSITE custom maps in alphabetical order
			std::sort(ids.begin() + first_custom_map, ids.end(),
				[](const s_secure_map_id& a, const s_secure_map_id& b) { return _wcsicmp(a.map_name, b.map_name) < 0; });
		}
		else
		{
			// CUSTOM keeps the order Cartographer used (last map first)
			std::reverse(ids.begin() + first_custom_map, ids.end());
		}
	}

	data_delete_all(data);
	for (const s_secure_map_id& id : ids)
	{
		if (data->actual_count >= data->maximum_count)
		{
			break;
		}

		const datum index = datum_new(data);
		if (index == NONE)
		{
			break;
		}
		csmemcpy(list_data_entry_get(data, index), &id, sizeof(id));
	}
}

// c_custom_game_map_list (DEFAULT tab): drop the levels that are listed under DIGSITE
static void* __fastcall c_custom_game_map_list__constructor_hook(void* list, void* edx, int32 a2)
{
	void* result = INVOKE_TYPE(0x25A95E, 0x0, void*(__thiscall*)(void*, int32), list, a2);

	data_array* data = *(data_array**)((uint8*)list + k_map_list_data_offset);
	if (data != nullptr && data->size == 8)
	{
		const int32 last_default_sort_order = digsite_get_last_default_level_sort_order();

		std::vector<uint64> kept_entries;
		bool removed = false;
		for (datum index = data_next_index(data, NONE); index != NONE; index = data_next_index(data, index))
		{
			uint64 entry;
			csmemcpy(&entry, list_data_entry_get(data, index), sizeof(entry));
			const int32 map_id = (int32)(entry >> 32);
			if (digsite_level_is_builtin_digsite(levels_get_multiplayer_ui_level(map_id), last_default_sort_order))
			{
				removed = true;
			}
			else
			{
				kept_entries.push_back(entry);
			}
		}

		if (removed)
		{
			data_delete_all(data);
			for (uint64 entry : kept_entries)
			{
				const datum index = datum_new(data);
				if (index == NONE)
				{
					break;
				}
				// keep the new datum header (first 2 bytes), restore the rest of the entry
				csmemcpy((uint8*)list_data_entry_get(data, index) + 2, (uint8*)&entry + 2, sizeof(entry) - 2);
			}
		}
	}

	return result;
}

// c_screen_custom_game_map_select::handle_event: tabs go DEFAULT -> CUSTOM -> DIGSITE and wrap around both ways
// the game's own tab index stays 0 (DEFAULT list) or 1 (CUSTOM list widget, also used for DIGSITE)
static bool __fastcall c_screen_custom_game_map_select__handle_event(void* screen, void* edx, int32* event_record)
{
	const int32 event_type = *event_record;
	if (event_type == 2 || event_type == 4)
	{
		uint8* screen_data = (uint8*)screen;
		int32* game_tab = (int32*)(screen_data + k_map_select_tab_index_offset);

		int32 tab = (*game_tab == 0 ? 0 : (g_map_select_digsite_tab_active ? 2 : 1));
		tab = (event_type == 2 ? (tab + 1) % 3 : (tab + 2) % 3);

		const bool digsite = (tab == 2);
		if (digsite != g_map_select_digsite_tab_active)
		{
			g_map_select_digsite_tab_active = digsite;
			data_array* custom_list_data = *(data_array**)(screen_data + k_map_select_custom_list_offset + k_map_list_data_offset);
			custom_game_custom_map_list_populate(custom_list_data, digsite);
		}

		*game_tab = (tab == 0 ? 0 : 1);
		*(bool*)(screen_data + k_map_select_tab_changed_offset) = true;
		return true;
	}

	return INVOKE_TYPE(0x25A4C0, 0x0, bool(__thiscall*)(void*, int32*), screen, event_record);
}

// c_screen_custom_game_map_select::update: header reads DIGSITE on the third tab
static void __fastcall c_screen_custom_game_map_select__update(void* screen, void* edx)
{
	INVOKE_TYPE(0x25A65D, 0x0, void(__thiscall*)(void*), screen);

	if (g_map_select_digsite_tab_active
		&& *(int32*)((uint8*)screen + k_map_select_tab_index_offset) == 1)
	{
		void* header_text = INVOKE_TYPE(0x211973, 0x0, void*(__thiscall*)(void*, int32), screen, k_map_select_header_text_widget_index);
		if (header_text != nullptr)
		{
			INVOKE_TYPE(0x21BAA9, 0x0, void(__thiscall*)(void*, const wchar_t*), header_text, L"DIGSITE");
		}
	}
}

// map preview and description of the selected CUSTOM/DIGSITE entry
static bool __cdecl map_select_get_custom_map_ui_level_definition(const s_secure_map_id* id, uint8* out_level_definition)
{
	int32 map_id;
	if (id != nullptr && digsite_builtin_level_get_map_id(id, &map_id))
	{
		csmemset(out_level_definition, 0, k_custom_map_ui_level_definition_size);

		const s_multiplayer_ui_level_definition* level = levels_get_multiplayer_ui_level(map_id);
		if (level == nullptr)
		{
			return false;
		}

		if (level->bitmap.index != NONE)
		{
			*(void**)out_level_definition = INVOKE_TYPE(0x37D482, 0x0, void*(__cdecl*)(datum, int32), level->bitmap.index, 0);
		}
		const wchar_t* name = multiplayer_ui_level_get_display_name(level);
		if (name != nullptr)
		{
			wcsncpy_s((wchar_t*)(out_level_definition + 4), k_custom_map_name_length, name, _TRUNCATE);
		}
		csmemcpy(out_level_definition + 0x244, level->level_descriptions.description, sizeof(level->level_descriptions.description));
		return true;
	}

	const bool result = INVOKE_TYPE(0x59369, 0x0, bool(__cdecl*)(const s_secure_map_id*, uint8*), id, out_level_definition);

	// custom maps have no preview picture, except the DIGSITE ones
	s_custom_map_entry* entry = nullptr;
	if (result
		&& id != nullptr
		&& map_manager_get()->get_entry_by_id(id, &entry)
		&& entry != nullptr
		&& custom_map_entry_is_game_maps_folder_digsite(entry))
	{
		int16 bitmap_index = 0;
		const datum preview_bitmap = digsite_custom_map_preview_bitmap_get(entry->file_path, &bitmap_index);
		if (preview_bitmap != NONE
			&& bitmap_index < bitmap_group_get(preview_bitmap)->bitmaps.count)
		{
			ui_bitmap_precache(preview_bitmap);
			*(void**)out_level_definition = bitmap_group_get_bitmap(preview_bitmap, bitmap_index);
		}
	}
	return result;
}

struct bitmap_data* custom_map_get_preview_bitmap(const s_secure_map_id* custom_map_id)
{
	if (custom_map_id == nullptr)
	{
		return nullptr;
	}

	uint8* level_definition = new uint8[k_custom_map_ui_level_definition_size];
	csmemset(level_definition, 0, k_custom_map_ui_level_definition_size);
	bitmap_data* preview = nullptr;
	if (map_select_get_custom_map_ui_level_definition(custom_map_id, level_definition))
	{
		preview = *(bitmap_data**)level_definition;
	}
	delete[] level_definition;
	return preview;
}

// c_custom_game_custom_map_list item pressed: built-in DIGSITE levels are selected like on the DEFAULT tab
static void __fastcall c_custom_game_custom_map_list__handle_item_pressed(void* list, void* edx, void* item, datum* item_index)
{
	uint8* list_data = (uint8*)list;
	data_array* data = *(data_array**)(list_data + k_map_list_data_offset);

	int32 map_id;
	if (item_index != nullptr
		&& *item_index != NONE
		&& data != nullptr
		&& *(bool*)(list_data + k_map_list_ignore_selection_offset) == false
		&& digsite_builtin_level_get_map_id((const s_secure_map_id*)list_data_entry_get(data, *item_index), &map_id))
	{
		if (*(bool*)(list_data + k_map_list_save_to_profile_offset))
		{
			uint8* profile = (uint8*)INVOKE_TYPE(0x209B9D, 0x0, void*(__cdecl*)(void));
			*(uint8*)(profile + 0xEE) = 3;
			*(int32*)(profile + 0xF4) = map_id;
			csmemset(profile + 0xF8, 0, sizeof(s_secure_map_id));
			const s_multiplayer_ui_level_definition* level = levels_get_multiplayer_ui_level(map_id);
			const wchar_t* name = (level != nullptr ? multiplayer_ui_level_get_display_name(level) : nullptr);
			wcsncpy_s((wchar_t*)(profile + 0x258), 32, (name != nullptr ? name : L""), _TRUNCATE);
			INVOKE_TYPE(0x209C3E, 0x0, void(__cdecl*)(void));
		}
		else
		{
			INVOKE_TYPE(0x21557F, 0x0, void(__cdecl*)(int32, int32), NONE, map_id);
		}

		void* screen = INVOKE_TYPE(0x211BA1, 0x0, void*(__thiscall*)(void*), list);
		void** screen_vtable = *(void***)screen;
		const int32 channel = ((int32(__thiscall*)(void*))screen_vtable[0x80 / 4])(screen);
		const int32 window = ((int32(__thiscall*)(void*))screen_vtable[0x84 / 4])(screen);
		INVOKE_TYPE(0x2096DA, 0x0, void(__cdecl*)(int32, int32), channel, window);
		return;
	}

	INVOKE_TYPE(0x25ACD8, 0x0, void(__thiscall*)(void*, void*, datum*), list, item, item_index);
}

static void map_select_digsite_tab_apply_patches(void)
{
	// DEFAULT list without the DIGSITE levels
	PatchCall(Memory::GetAddressRelative(0x64F715), c_custom_game_map_list__constructor_hook);

	// c_screen_custom_game_map_select vtable: handle_event and update
	WritePointer(Memory::GetAddressRelative(0x7D9194) + 12 * sizeof(void*), (void*)c_screen_custom_game_map_select__handle_event);
	WritePointer(Memory::GetAddressRelative(0x7D9194) + 3 * sizeof(void*), (void*)c_screen_custom_game_map_select__update);

	// c_custom_game_custom_map_list vtable: item pressed
	WritePointer(Memory::GetAddressRelative(0x7DB06C) + 26 * sizeof(void*), (void*)c_custom_game_custom_map_list__handle_item_pressed);

	// preview and description of built-in DIGSITE levels on the map select screen, and the preview of DIGSITE map files
	// everywhere the game shows a custom map's details (map select screen, pregame lobby, game setup screens)
	const uintptr_t custom_map_ui_level_definition_calls[] =
	{
		0x5B0955, 0x61668A, 0x6196D5, 0x643208, 0x643BA7, 0x647674, 0x64A9BF, 0x64F214, 0x65A84D
	};
	for (uintptr_t call : custom_map_ui_level_definition_calls)
	{
		PatchCall(Memory::GetAddressRelative(call), map_select_get_custom_map_ui_level_definition);
	}
}

// custom map selection list code
class c_custom_game_custom_map_list // : public c_list_widget
{
public:
	//c_list_item_widget item_widget[14];

	c_custom_game_custom_map_list* constructor_hook(int a2)
	{
		typedef c_custom_game_custom_map_list*(__thiscall* original_constructor_t)(c_custom_game_custom_map_list*, int);
		auto p_original_constructor = Memory::GetAddress<original_constructor_t>(0x25AE3B);

		// execute first part of the function
		p_original_constructor(this, a2);
		// then load the map list

		// here we replace the custom map list allocator
		DWORD thisptr = (DWORD)this;

		data_array** custom_map_menu_list = (data_array**)(thisptr + 112);

		// the list is shared by the CUSTOM and DIGSITE tabs, so leave room for the built-in DIGSITE levels as well
		uint32 custom_map_available_count = map_manager_get()->get_custom_map_list_ids(nullptr, 0);
		*custom_map_menu_list = ui_list_data_new("custom game custom maps",
			custom_map_available_count + k_maximum_number_of_digsite_builtin_levels + k_digsite_custom_map_list_spare_slots,
			sizeof(s_secure_map_id));
		data_make_valid(*custom_map_menu_list);

		g_map_select_digsite_tab_active = false;
		custom_game_custom_map_list_populate(*custom_map_menu_list, false);

		_slot_linker* linker = (_slot_linker*)(thisptr + 172);
		_slot* slot = (_slot*)(thisptr + 2032);
		linker->link(slot);
		return this;
	}
};
//ASSERT_STRUCT_SIZE(c_custom_game_custom_map_list, 3292);

CLASS_HOOK_DECLARE_LABEL(c_custom_game_custom_map_list__constructor_hook, c_custom_game_custom_map_list::constructor_hook);
static __declspec(naked) void jmp_c_custom_game_custom_map_list_constructor_hook()
{
	CLASS_HOOK_JMP(c_custom_game_custom_map_list__constructor_hook, c_custom_game_custom_map_list::constructor_hook);
}

void c_map_manager::ApplyCustomMapExtensionLimitPatches()
{
	WriteJmpTo(Memory::GetAddressRelative(0x44CF41, 0x441819), jmp_get_entry_by_id);
	WriteJmpTo(Memory::GetAddressRelative(0x44CC30, 0x490356), jmp_load_custom_map_data_cache);
	WriteJmpTo(Memory::GetAddressRelative(0x4C19B4, 0x48F0F5), jmp_mark_all_cached_maps_for_deletion);
	WriteJmpTo(Memory::GetAddressRelative(0x4C1D65, 0x48F4A6), jmp_find_matching_entries_by_file_path);
	WriteJmpTo(Memory::GetAddressRelative(0x4C1C1F, 0x48F360), jmp_find_matching_entries_by_map_name_and_hash);
	WriteJmpTo(Memory::GetAddressRelative(0x4C2669, 0x48FDAA), jmp_delete_single_entry);
	WriteJmpTo(Memory::GetAddressRelative(0x44CDA6, 0x441677), jmp_remove_duplicates_and_add_entry);
	WriteJmpTo(Memory::GetAddressRelative(0x4C2D4D, 0x49048E), jmp_save_custom_map_data);
	WriteJmpTo(Memory::GetAddressRelative(0x4C27F3, 0x48FF34), jmp_remove_marked_for_deletion);
	WriteJmpTo(Memory::GetAddressRelative(0x4C259B, 0x48FCDC), jmp_add_entry);
	WriteJmpTo(Memory::GetAddressRelative(0x44CD1E, 0x4415EF), jmp_remove_duplicates_write_entry_data_and_add);

	PatchCall(Memory::GetAddressRelative(0x439E56, 0x40BAB8), jmp_start_custom_map_sync);

	// custom map data menu list hook/patches
	if (!shell_is_dedicated_server())
	{
		PatchCall(Memory::GetAddressRelative(0x64F708), jmp_c_custom_game_custom_map_list_constructor_hook);

		// DIGSITE tab on the custom game map select screen
		map_select_digsite_tab_apply_patches();

		// jump to function end just before updating custom map list
		// jmp near 0xB7
		BYTE jmp_to_epilog_sub_65AE3B[] = { 0xE9, 0xB7, 0x00, 0x00, 0x00 };
		WriteBytes(Memory::GetAddressRelative(0x65AEFD), jmp_to_epilog_sub_65AE3B, sizeof(jmp_to_epilog_sub_65AE3B));
	}
	else
	{
		WriteJmpTo(Memory::GetAddressRelative(0x0, 0x4418F9), jmp_get_custom_map_list_ids_by_map_name);
	}

	WriteJmpTo(Memory::GetAddressRelative(0x4C18BD, 0x48EFFE), jmp_initialize);

	// replace game's atexit cleanup impl
	WriteJmpTo(Memory::GetAddressRelative(0x79A0DC, 0x73E9E5), jmp_cleanup);

	PatchCall(Memory::GetAddress(0x1E49A2, 0x1EDF0), validate_and_read_custom_map_data);
	PatchCall(Memory::GetAddress(0x4D3BA, 0x417FE), validate_and_read_custom_map_data);
	PatchCall(Memory::GetAddress(0x4CF26, 0x41D4E), validate_and_read_custom_map_data);
	PatchCall(Memory::GetAddress(0x8928, 0x1B6482), validate_and_read_custom_map_data);
}

/* private code */

static int32 get_directory_path_from_id(e_directory_id id, LPCWSTR pMore, LPWSTR pszPath, bool is_folder)
{
	typedef int(__cdecl* t_get_directory_path_from_id)(e_directory_id id, LPCWSTR pMore, LPWSTR pszPath, bool is_folder);
	auto p_get_directory_path_by_id = Memory::GetAddressRelative<t_get_directory_path_from_id>(0x48EF9E, 0x474A15);
	return p_get_directory_path_by_id(id, pMore, pszPath, is_folder);
}

static bool open_cache_header(const wchar_t* file_path, void* cache_header_ptr, HANDLE* map_handle)
{
	typedef char(__cdecl open_cache_header_t)(const wchar_t* file_path, void* lpBuffer, HANDLE* map_handle, DWORD NumberOfBytesRead);
	auto p_open_cache_header = Memory::GetAddress<open_cache_header_t*>(0x642D0, 0x4C327);
	return p_open_cache_header(file_path, cache_header_ptr, map_handle, 0);
}

static void __cdecl close_cache_header(HANDLE* map_handle)
{
	INVOKE(0x64C03, 0x4CC5A, close_cache_header, map_handle);
	return;
}

static bool __cdecl validate_and_read_custom_map_data(s_custom_map_entry* custom_map_entry)
{
	cache_file_header header;
	HANDLE map_cache_handle;
	wchar_t* file_name = custom_map_entry->file_path;
	if (!open_cache_header(file_name, &header, &map_cache_handle))
		return false;
	if (header.header_signature != 'head' || header.footer_signature != 'foot' || header.file_size <= 0 || header.version != 8)
	{
		LOG_TRACE_FUNCW(L"\"{}\" has invalid header", file_name);
		return false;
	}
	if (header.type > 5 || header.type < 0)
	{
		LOG_TRACE_FUNCW(L"\"{}\" has bad scenario type", file_name);
		return false;
	}
	if (strnlen_s(header.name, k_custom_map_name_length) >= 32 || strnlen_s(header.version_string, 32) >= 32)
	{
		LOG_TRACE_FUNCW(L"\"{}\" has invalid version or name string", file_name);
		return false;
	}
	if (header.type != _scenario_type_multiplayer && header.type != _scenario_type_solo)
	{
		LOG_TRACE_FUNCW(L"\"{}\" is not playable", file_name);
		return false;
	}

	close_cache_header(&map_cache_handle);
	// needed because the game loads the human readable map name and description from scenario after checks
	// without this the map is just called by it's file name

	// todo move the code for loading the descriptions to our code and get rid of this
	typedef int(__cdecl* validate_and_add_custom_map_internal_t)(s_custom_map_entry*);
	auto validate_and_add_custom_map_internal = Memory::GetAddress<validate_and_add_custom_map_internal_t>(0x4F690, 0x56890);
	if (!validate_and_add_custom_map_internal(custom_map_entry))
	{
		LOG_TRACE_FUNCW(L"warning \"{}\" has bad checksums or is blacklisted, map may not work correctly", file_name);
		std::wstring fallback_name;
		if (strnlen_s(header.name, sizeof(header.name)) > 0) {
			wchar_t fallback_name_c[32];
			utf8_string_to_wchar_string(header.name, fallback_name_c, NUMBEROF(fallback_name_c));
			fallback_name.append(fallback_name_c);
		}
		else {
			std::wstring full_file_name = file_name;
			auto start = full_file_name.find_last_of('\\');
			fallback_name = full_file_name.substr(start != std::wstring::npos ? start : 0, full_file_name.find_last_not_of('.'));
		}
		wcsncpy_s(custom_map_entry->map_name, fallback_name.c_str(), fallback_name.length());
	}
	// load the map even if some of the checks failed, will still mostly work
	return true;
}

// '50 map limit removal' region end
#pragma endregion