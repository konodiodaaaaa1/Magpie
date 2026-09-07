#pragma once

namespace Magpie {

struct EffectHelper {
	static std::wstring_view GetDisplayName(std::wstring_view fullName) noexcept {
		// Display aliases only: saved effect IDs and all other effect names stay intact.
		if (fullName == L"XeSSFG\\XeSS_FrameGeneration_x2_ZeroMV") {
			return L"XeSS_FrameGeneration_x2";
		}
		if (fullName == L"XeSSFG\\XeSS_MultiFrameGeneration_ZeroMV") {
			return L"XeSS_MultiFrameGeneration";
		}
		size_t delimPos = fullName.find_last_of(L'\\');
		return delimPos != std::wstring::npos ? fullName.substr(delimPos + 1) : fullName;
	}
};

}
