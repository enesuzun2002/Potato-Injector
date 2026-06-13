#pragma once
#include <string>
#include "syscalls/strcrypt.hpp"

namespace vars
{
	constexpr auto str_game_process_name_enc = crypt::encrypted("cs2.exe");
	constexpr auto str_dll_name_enc = crypt::encrypted("cheat.dll");
	constexpr auto str_game_mod_name_enc = crypt::encrypted("client.dll");
	constexpr auto str_dll_dir_path_enc = crypt::encrypted("./dlls");

	inline std::wstring get_game_process_name() { return crypt::decrypt_w(str_game_process_name_enc); }
	inline std::wstring get_dll_name() { return crypt::decrypt_w(str_dll_name_enc); }
	inline std::wstring get_game_mod_name() { return crypt::decrypt_w(str_game_mod_name_enc); }
	inline std::wstring get_dll_dir_path() { return crypt::decrypt_w(str_dll_dir_path_enc); }
}