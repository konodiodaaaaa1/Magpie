#pragma once
#include <windows.h>
#include <rapidjson/document.h>
#include <atomic>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace Magpie {

// Shared by pending saves so a background operation never accesses AppSettings
// after destruction. Revisions are assigned when taking the UI-thread snapshot.
struct ConfigSaveState {
	std::mutex mutex;
	std::atomic<uint64_t> nextRevision{ 0 };
	uint64_t savedRevision = 0;
};

namespace ConfigPersistence {

inline bool IsValid(std::string_view json) {
	if (json.empty()) return false;
	rapidjson::Document doc;
	doc.Parse<rapidjson::kParseValidateEncodingFlag>(json.data(), json.size());
	return !doc.HasParseError() && doc.IsObject();
}

inline std::string Read(const std::filesystem::path& path) {
	std::ifstream stream(path, std::ios::binary);
	return { std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
}

// Keep complete top-level members, and complete objects in a top-level array
// (scalingModes/profiles). Never manufacture a partially written effect/profile.
inline std::string RecoverPrefix(std::string_view damaged) {
	std::vector<char> stack;
	bool inString = false, escaped = false;
	size_t boundary = 0;
	std::string closing;
	for (size_t i = 0; i < damaged.size(); ++i) {
		const char c = damaged[i];
		if (inString) {
			if (escaped) escaped = false;
			else if (c == '\\') escaped = true;
			else if (c == '"') inString = false;
			continue;
		}
		if (c == '"') { inString = true; continue; }
		if (c == '{' || c == '[') {
			stack.push_back(c);
		} else if (c == '}' || c == ']') {
			if (stack.empty() || stack.back() != (c == '}' ? '{' : '[')) break;
			stack.pop_back();
			if (stack.size() == 1 ||
				(stack.size() == 2 && stack[0] == '{' && stack[1] == '[')) {
				boundary = i + 1;
				closing = stack.size() == 1 ? "}" : "]}";
			}
		} else if (c == ',' && stack.size() == 1) {
			boundary = i;
			closing = "}";
		}
	}
	if (!boundary) return {};
	std::string result(damaged.substr(0, boundary));
	result += closing;
	return IsValid(result) ? result : std::string{};
}

inline bool WriteAtomic(const std::filesystem::path& path, std::string_view json,
	uint64_t revision, ConfigSaveState& state) {
	if (!IsValid(json)) { SetLastError(ERROR_INVALID_DATA); return false; }
	std::lock_guard lock(state.mutex);
	if (revision < state.savedRevision) return true;
	std::error_code ec;
	std::filesystem::create_directories(path.parent_path(), ec);
	if (ec) { SetLastError(static_cast<DWORD>(ec.value())); return false; }
	const auto temp = std::filesystem::path(path.native() + L".tmp");
	const auto backup = std::filesystem::path(path.native() + L".bak");
	HANDLE file = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE) return false;
	bool ok = true;
	size_t offset = 0;
	while (offset < json.size()) {
		DWORD written = 0;
		const DWORD length = static_cast<DWORD>((std::min)(json.size() - offset, size_t(MAXDWORD)));
		if (!WriteFile(file, json.data() + offset, length, &written, nullptr) || !written) {
			ok = false;
			break;
		}
		offset += written;
	}
	if (ok) ok = FlushFileBuffers(file) != FALSE;
	DWORD error = GetLastError();
	if (!CloseHandle(file) && ok) { ok = false; error = GetLastError(); }
	if (ok) {
		// Do not replace a usable backup with the damaged input being recovered.
		const std::string previous = Read(path);
		if (IsValid(previous) && !CopyFileW(path.c_str(), backup.c_str(), FALSE)) {
			ok = false;
			error = GetLastError();
		}
	}
	if (ok) {
		ok = MoveFileExW(temp.c_str(), path.c_str(),
			MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
		error = GetLastError();
	}
	if (!ok) {
		DeleteFileW(temp.c_str());
		SetLastError(error);
		return false;
	}
	state.savedRevision = revision;
	return true;
}

} // namespace ConfigPersistence
} // namespace Magpie
