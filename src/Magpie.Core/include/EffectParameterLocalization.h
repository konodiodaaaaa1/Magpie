#pragma once
#include "EffectDesc.h"
#include "CommonSharedConstants.h"
#include <winrt/Windows.ApplicationModel.Resources.h>
#include <winrt/Windows.ApplicationModel.Resources.Core.h>
#include <winrt/Windows.Foundation.Collections.h>

namespace Magpie {

struct EffectParameterLocalization {
	static std::wstring KeyPart(std::string_view value) {
		std::wstring result;
		for (char c : value) {
			result.push_back((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
				(c >= '0' && c <= '9') ? wchar_t(c) : L'_');
		}
		return result;
	}

	// Translate a display-only copy. Effect IDs, shader metadata, compilation
	// caches, parameter names and saved numeric values remain language-neutral.
	static void Localize(std::string_view effect, std::vector<EffectParameterDesc>& parameters) noexcept {
		try {
			using namespace winrt::Windows::ApplicationModel::Resources;
			const auto language = Core::ResourceContext::GetForViewIndependentUse()
				.QualifierValues().Lookup(L"Language");
			const std::wstring_view tag(language);
			if (!tag.starts_with(L"zh") && !tag.starts_with(L"ZH")) return;
			const auto loader = ResourceLoader::GetForViewIndependentUse(
				CommonSharedConstants::APP_RESOURCE_MAP_ID);
			const std::wstring prefix = L"EffectParam_" + KeyPart(effect) + L"_";
			auto translate = [&](const std::wstring& key, std::string& text) {
				try {
					const auto value = loader.GetString(key);
					if (!value.empty()) text = winrt::to_string(value);
				} catch (...) { /* Missing third-party translations keep their source text. */ }
			};
			for (auto& parameter : parameters) {
				const std::wstring key = prefix + KeyPart(parameter.name);
				translate(key + L"_Label", parameter.label);
				if (!parameter.group.empty()) {
					translate(prefix + L"Group_" + KeyPart(parameter.group), parameter.group);
				}
				for (auto& choice : parameter.choices) {
					translate(key + L"_Option_" + std::to_wstring(choice.value), choice.label);
				}
			}
		} catch (...) { /* Resource failures must not prevent editing parameters. */ }
	}
};

}
