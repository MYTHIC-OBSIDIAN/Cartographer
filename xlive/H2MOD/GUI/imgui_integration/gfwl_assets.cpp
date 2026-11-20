#include "stdafx.h"
#include "gfwl_assets.h"

#include "rasterizer/dx9/rasterizer_dx9_main.h"
#include "H2MOD/Modules/Shell/Startup/Startup.h"

#include <map>
#include <string>
#include <vector>

/* constants */

static const uint32 k_gfwl_offset_invalid = 0xFFFFFFFF;

/* structures */

struct s_gfwl_file
{
	const uint8* data;
	uint32 size;
};

/* globals */

static std::vector<uint8> g_gfwl_dll;
static std::map<std::string, s_gfwl_file> g_gfwl_files;			// "skin/toast_bg.png"
static std::map<std::string, s_gfwl_texture> g_gfwl_textures;
static bool g_gfwl_load_attempted = false;
static bool g_gfwl_loaded = false;

/* private code */

static uint16 gfwl_read_uint16_be(const uint8* data)
{
	return (uint16)((data[0] << 8) | data[1]);
}

static uint32 gfwl_read_uint32_be(const uint8* data)
{
	return ((uint32)data[0] << 24) | ((uint32)data[1] << 16) | ((uint32)data[2] << 8) | (uint32)data[3];
}

static std::string gfwl_file_key(const char* package, const wchar_t* name)
{
	std::string key;
	for (const char* c = package; *c; c++)
	{
		key += (char)tolower((uint8)*c);
	}
	key += '/';
	for (const wchar_t* c = name; *c; c++)
	{
		const wchar_t character = *c == L'\\' ? L'/' : *c;
		key += (char)towlower(character);
	}
	return key;
}

static bool gfwl_file_read(const wchar_t* path, std::vector<uint8>* out_data)
{
	HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	bool result = false;
	LARGE_INTEGER size;
	if (GetFileSizeEx(file, &size) && size.QuadPart > 0 && size.QuadPart < 64 * 1024 * 1024)
	{
		out_data->resize((size_t)size.QuadPart);
		DWORD read = 0;
		result = ReadFile(file, out_data->data(), (DWORD)size.QuadPart, &read, NULL) && read == (DWORD)size.QuadPart;
	}
	CloseHandle(file);
	return result;
}

// RVA to file offset through the section table; k_gfwl_offset_invalid when outside every section
static uint32 gfwl_pe_rva_to_offset(const IMAGE_SECTION_HEADER* sections, uint32 section_count, uint32 rva)
{
	for (uint32 index = 0; index < section_count; index++)
	{
		const IMAGE_SECTION_HEADER* section = &sections[index];
		const uint32 section_size = section->Misc.VirtualSize > section->SizeOfRawData ? section->Misc.VirtualSize : section->SizeOfRawData;
		if (rva >= section->VirtualAddress && rva < section->VirtualAddress + section_size)
		{
			return section->PointerToRawData + (rva - section->VirtualAddress);
		}
	}
	return k_gfwl_offset_invalid;
}

// indexes one XUIZ package: big-endian header "XUIZ", directory size at 0x10, file count at 0x14, then per file
// size, offset (from the end of the directory), name length and UTF-16 name
static void gfwl_package_index(const std::string& package, const uint8* data, uint32 size)
{
	if (size < 0x16 || csmemcmp(data, "XUIZ", 4) != 0)
	{
		return;
	}

	const uint64 base = 0x16 + (uint64)gfwl_read_uint32_be(data + 0x10);
	const uint16 count = gfwl_read_uint16_be(data + 0x14);
	if (base > size)
	{
		return;
	}

	uint32 position = 0x16;
	for (uint16 index = 0; index < count; index++)
	{
		if (position + 9 > base)
		{
			break;
		}
		const uint32 file_size = gfwl_read_uint32_be(data + position);
		const uint32 file_offset = gfwl_read_uint32_be(data + position + 4);
		const uint32 name_length = data[position + 8];
		position += 9;
		if (position + 2 * name_length > base)
		{
			break;
		}

		std::string key = package + "/";
		for (uint32 character = 0; character < name_length; character++)
		{
			const uint16 value = gfwl_read_uint16_be(data + position + 2 * character);
			key += value == '\\' ? '/' : (char)tolower(value < 0x80 ? (int)value : '_');
		}
		position += 2 * name_length;

		if (base + file_offset + file_size <= size)
		{
			g_gfwl_files[key] = { data + base + file_offset, file_size };
		}
	}
	return;
}

// walks the PE resources for type "RT_RCDATA" (a named type in xlive.dll) and indexes every package in it
static bool gfwl_dll_index(const std::vector<uint8>& dll)
{
	const uint8* data = dll.data();
	const uint32 size = (uint32)dll.size();
	if (size < sizeof(IMAGE_DOS_HEADER))
	{
		return false;
	}
	const IMAGE_DOS_HEADER* dos = (const IMAGE_DOS_HEADER*)data;
	if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0 || (uint32)dos->e_lfanew + sizeof(IMAGE_NT_HEADERS32) > size)
	{
		return false;
	}
	const IMAGE_NT_HEADERS32* nt = (const IMAGE_NT_HEADERS32*)(data + dos->e_lfanew);
	if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC)
	{
		return false;
	}

	const IMAGE_SECTION_HEADER* sections = IMAGE_FIRST_SECTION(nt);
	const uint32 section_count = nt->FileHeader.NumberOfSections;
	if ((const uint8*)(sections + section_count) > data + size)
	{
		return false;
	}

	const IMAGE_DATA_DIRECTORY* resource_directory = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE];
	const uint32 resource_offset = gfwl_pe_rva_to_offset(sections, section_count, resource_directory->VirtualAddress);
	if (resource_offset == k_gfwl_offset_invalid || resource_offset + sizeof(IMAGE_RESOURCE_DIRECTORY) > size)
	{
		return false;
	}
	const uint8* resources = data + resource_offset;
	const uint32 resources_size = size - resource_offset;

	auto directory_get = [&](uint32 offset, const IMAGE_RESOURCE_DIRECTORY_ENTRY** out_entries, uint32* out_count) -> bool
	{
		if (offset + sizeof(IMAGE_RESOURCE_DIRECTORY) > resources_size)
		{
			return false;
		}
		const IMAGE_RESOURCE_DIRECTORY* directory = (const IMAGE_RESOURCE_DIRECTORY*)(resources + offset);
		const uint32 count = directory->NumberOfNamedEntries + directory->NumberOfIdEntries;
		if (offset + sizeof(IMAGE_RESOURCE_DIRECTORY) + count * sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY) > resources_size)
		{
			return false;
		}
		*out_entries = (const IMAGE_RESOURCE_DIRECTORY_ENTRY*)(directory + 1);
		*out_count = count;
		return true;
	};
	auto entry_name = [&](const IMAGE_RESOURCE_DIRECTORY_ENTRY* entry) -> std::string
	{
		std::string name;
		if (entry->NameIsString)
		{
			const uint32 offset = entry->NameOffset;
			if (offset + 2 <= resources_size)
			{
				const IMAGE_RESOURCE_DIR_STRING_U* string = (const IMAGE_RESOURCE_DIR_STRING_U*)(resources + offset);
				if (offset + 2 + string->Length * 2 <= resources_size)
				{
					for (uint32 index = 0; index < string->Length; index++)
					{
						name += (char)toupper(string->NameString[index] < 0x80 ? (int)string->NameString[index] : '_');
					}
				}
			}
		}
		return name;
	};

	const IMAGE_RESOURCE_DIRECTORY_ENTRY* types;
	uint32 type_count;
	if (!directory_get(0, &types, &type_count))
	{
		return false;
	}

	for (uint32 type_index = 0; type_index < type_count; type_index++)
	{
		if (!types[type_index].DataIsDirectory || entry_name(&types[type_index]) != "RT_RCDATA")
		{
			continue;
		}

		const IMAGE_RESOURCE_DIRECTORY_ENTRY* names;
		uint32 name_count;
		if (!directory_get(types[type_index].OffsetToDirectory, &names, &name_count))
		{
			continue;
		}
		for (uint32 name_index = 0; name_index < name_count; name_index++)
		{
			const std::string package = entry_name(&names[name_index]);
			const IMAGE_RESOURCE_DIRECTORY_ENTRY* languages;
			uint32 language_count;
			if (package.empty() || !names[name_index].DataIsDirectory
				|| !directory_get(names[name_index].OffsetToDirectory, &languages, &language_count) || language_count == 0
				|| languages[0].DataIsDirectory)
			{
				continue;
			}

			const uint32 data_entry_offset = languages[0].OffsetToData;
			if (data_entry_offset + sizeof(IMAGE_RESOURCE_DATA_ENTRY) > resources_size)
			{
				continue;
			}
			const IMAGE_RESOURCE_DATA_ENTRY* data_entry = (const IMAGE_RESOURCE_DATA_ENTRY*)(resources + data_entry_offset);
			const uint32 package_offset = gfwl_pe_rva_to_offset(sections, section_count, data_entry->OffsetToData);
			if (package_offset == k_gfwl_offset_invalid || (uint64)package_offset + data_entry->Size > size)
			{
				continue;
			}

			std::string package_key;
			for (char c : package)
			{
				package_key += (char)tolower((uint8)c);
			}
			gfwl_package_index(package_key, data + package_offset, data_entry->Size);
		}
	}
	return !g_gfwl_files.empty();
}

static void gfwl_assets_load(void)
{
	if (g_gfwl_load_attempted)
	{
		return;
	}
	g_gfwl_load_attempted = true;

	// a copy in the game folder first (the game folder's own xlive.dll is Cartographer's), then the system one
	wchar_t paths[2][MAX_PATH] = {};
	swprintf_s(paths[0], L"%sxlive_gfwl.dll", g_h2_process_file_path);	// (the game folder path ends with a backslash)
	wchar_t system_directory[MAX_PATH] = {};
	if (GetSystemDirectoryW(system_directory, MAX_PATH) != 0)
	{
		swprintf_s(paths[1], L"%s\\xlive.dll", system_directory);
	}

	for (uint32 index = 0; index < NUMBEROF(paths) && !g_gfwl_loaded; index++)
	{
		g_gfwl_files.clear();
		if (paths[index][0] != L'\0' && gfwl_file_read(paths[index], &g_gfwl_dll) && gfwl_dll_index(g_gfwl_dll))
		{
			g_gfwl_loaded = true;
		}
	}
	if (!g_gfwl_loaded)
	{
		g_gfwl_files.clear();
		g_gfwl_dll.clear();
	}
	return;
}

/* public code */

bool gfwl_assets_available(void)
{
	gfwl_assets_load();
	return g_gfwl_loaded;
}

bool gfwl_assets_get_file(const char* package, const wchar_t* name, const uint8** out_data, uint32* out_size)
{
	if (!gfwl_assets_available())
	{
		return false;
	}

	const auto file = g_gfwl_files.find(gfwl_file_key(package, name));
	if (file == g_gfwl_files.end())
	{
		return false;
	}
	*out_data = file->second.data;
	*out_size = file->second.size;
	return true;
}

const s_gfwl_texture* gfwl_assets_get_texture(const char* package, const wchar_t* name)
{
	const std::string key = gfwl_file_key(package, name);
	const auto existing = g_gfwl_textures.find(key);
	if (existing != g_gfwl_textures.end())
	{
		return existing->second.texture ? &existing->second : NULL;
	}

	// remember failures too, so a missing file isn't looked up every frame
	s_gfwl_texture texture = {};
	const uint8* data;
	uint32 size;
	if (gfwl_assets_get_file(package, name, &data, &size))
	{
		texture.texture = imgui_texture_create_from_png(data, size, &texture.width, &texture.height);
	}
	const s_gfwl_texture* result = &(g_gfwl_textures[key] = texture);
	return result->texture ? result : NULL;
}

void gfwl_assets_play_sound(const char* package, const wchar_t* name)
{
	const uint8* data;
	uint32 size;
	if (gfwl_assets_get_file(package, name, &data, &size))
	{
		// the data stays loaded for the game's lifetime, as SND_MEMORY with SND_ASYNC needs
		PlaySoundW((LPCWSTR)data, NULL, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
	}
	return;
}

void gfwl_assets_release_textures(void)
{
	for (auto& texture : g_gfwl_textures)
	{
		if (texture.second.texture)
		{
			texture.second.texture->Release();
		}
	}
	g_gfwl_textures.clear();
	return;
}

PDIRECT3DTEXTURE9 imgui_texture_create_from_png(const uint8* png, uint32 png_size, uint32* out_width, uint32* out_height)
{
	// decode into system memory, then copy into a dynamic default pool texture, like ImGui's font
	LPDIRECT3DDEVICE9 device = rasterizer_dx9_device_get_interface();
	PDIRECT3DTEXTURE9 decoded = NULL;
	if (device == NULL || FAILED(D3DXCreateTextureFromFileInMemoryEx(device, png, png_size,
		D3DX_DEFAULT_NONPOW2, D3DX_DEFAULT_NONPOW2, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM,
		D3DX_FILTER_NONE, D3DX_FILTER_NONE, 0, NULL, NULL, &decoded)))
	{
		return NULL;
	}

	D3DSURFACE_DESC description;
	PDIRECT3DTEXTURE9 texture = NULL;
	if (SUCCEEDED(decoded->GetLevelDesc(0, &description))
		&& SUCCEEDED(device->CreateTexture(description.Width, description.Height, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &texture, NULL)))
	{
		D3DLOCKED_RECT source;
		D3DLOCKED_RECT destination;
		bool copied = false;
		if (SUCCEEDED(decoded->LockRect(0, &source, NULL, D3DLOCK_READONLY)))
		{
			if (SUCCEEDED(texture->LockRect(0, &destination, NULL, D3DLOCK_DISCARD)))
			{
				for (uint32 row = 0; row < description.Height; row++)
				{
					csmemcpy((uint8*)destination.pBits + row * destination.Pitch, (const uint8*)source.pBits + row * source.Pitch, description.Width * 4);
				}
				texture->UnlockRect(0);
				copied = true;
			}
			decoded->UnlockRect(0);
		}
		if (!copied)
		{
			texture->Release();
			texture = NULL;
		}
		else
		{
			if (out_width)
			{
				*out_width = description.Width;
			}
			if (out_height)
			{
				*out_height = description.Height;
			}
		}
	}
	decoded->Release();
	return texture;
}
