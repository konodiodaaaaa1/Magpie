#include "pch.h"
#include "OverlayDrawer.h"
#include "CursorManager.h"
#include "DeviceResources.h"
#include "EffectDesc.h"
#include "EffectParameterValue.h"
#include "EffectParameterRules.h"
#include "EffectParameterLocalization.h"
#include "FrameSourceBase.h"
#include "ImGuiFontsCacheManager.h"
#include "Logger.h"
#include "OverlayHelper.h"
#include "Renderer.h"
#include "ScalingOptions.h"
#include "ScalingWindow.h"
#include "StrHelper.h"
#include "Win32Helper.h"
#include <ShlObj.h>
#include <imgui_internal.h>

using namespace std::chrono;

namespace Magpie {

static const char* COLOR_INDICATOR = "■";
static const wchar_t COLOR_INDICATOR_W = L'■';

static const float CORNER_ROUNDING = 6;

static const char* TOOLBAR_WINDOW_ID = "toolbar";
static const char* PROFILER_WINDOW_ID = "profiler";
static const char* EFFECT_PARAMETERS_WINDOW_ID = "effectParameters";

static constexpr float EFFECT_PARAMETERS_DEFAULT_WIDTH = 420.0f;
static constexpr float EFFECT_PARAMETERS_DEFAULT_HEIGHT = 600.0f;
static constexpr float EFFECT_PARAMETERS_MIN_WIDTH = 360.0f;
static constexpr float EFFECT_PARAMETERS_MIN_HEIGHT = 400.0f;

static void SetDefaultWindowOptions(
	phmap::flat_hash_map<std::string, OverlayWindowOption>& windowOptions
) noexcept {
	if (!windowOptions.contains(PROFILER_WINDOW_ID)) {
		// 右侧竖直居中
		windowOptions.emplace(PROFILER_WINDOW_ID, OverlayWindowOption{
			.hArea = 2,
			.vArea = 1,
			.hPos = 60.0f,
			.vPos = 0.5f
		});
	}
	if (!windowOptions.contains(EFFECT_PARAMETERS_WINDOW_ID)) {
		windowOptions.emplace(EFFECT_PARAMETERS_WINDOW_ID, OverlayWindowOption{
			.hArea = 0,
			.vArea = 1,
			.hPos = 60.0f,
			.vPos = 0.5f
		});
	}
}

bool OverlayDrawer::Initialize(DeviceResources& deviceResources, OverlayOptions& overlayOptions) noexcept {
	_overlayOptions = &overlayOptions;
	SetDefaultWindowOptions(overlayOptions.windows);

	if (!_imguiImpl.Initialize(deviceResources)) {
		Logger::Get().Error("初始化 ImGuiImpl 失败");
		return false;
	}

	_dpiScale = GetDpiForWindow(ScalingWindow::Get().Handle()) / float(USER_DEFAULT_SCREEN_DPI);

	ImGui::StyleColorsDark();
	ImGuiStyle& style = ImGui::GetStyle();
	style.PopupRounding = style.WindowRounding = CORNER_ROUNDING;
	// 由于我们是按需渲染，显示 tooltip 时不要有延迟
	style.HoverFlagsForTooltipMouse = ImGuiHoveredFlags_DelayNone;
	style.FrameBorderSize = 1;
	style.FrameRounding = 2;
	style.WindowMinSize = ImVec2(10, 10);
	style.ScaleAllSizes(_dpiScale);

	if (!_BuildFonts()) {
		Logger::Get().Error("_BuildFonts 失败");
		return false;
	}

	// 将 _fontUI 设为默认字体
	ImGui::GetIO().FontDefault = _fontUI;

	// 获取硬件信息
	DXGI_ADAPTER_DESC desc{};
	HRESULT hr = deviceResources.GetGraphicsAdapter()->GetDesc(&desc);
	_hardwareInfo.gpuName = SUCCEEDED(hr) ? StrHelper::UTF16ToUTF8(desc.Description) : "UNAVAILABLE";

	UpdateAfterActiveEffectsChanged();
	return true;
}

void OverlayDrawer::Draw(
	uint32_t fps,
	const SmallVector<float>& effectTimings,
	POINT drawOffset
) noexcept {
	// This method is called only after Presenter::BeginFrame has provided a valid
	// back buffer. Build and draw one complete ImGui frame in the same frontend
	// render that will present it.
	_overlayDirty = false;
	if (!AnyVisibleWindow()) {
		_lastComparisonStatusAlpha = 0.0f;
		return;
	}

	_lastFPS = fps;
	_lastFrameRateText = _FormatFrameRate(fps);
	const bool needsLayoutFollowUp = std::exchange(_isFirstFrame, false);

	const bool oldProfilerVisible = _isProfilerVisible;

	// 为了符合 Fitts 法则，鼠标在工具栏上时稍微下移逻辑位置使得在上边缘可以选中工具栏按钮
	float fittsLawAdjustment = 0;
	const char* hoveredWindowId = _imguiImpl.GetHoveredWindowId();
	if (hoveredWindowId && hoveredWindowId == std::string_view(TOOLBAR_WINDOW_ID)) {
		fittsLawAdjustment = 4 * _dpiScale;
	}

	_imguiImpl.NewFrame(_overlayOptions->windows, fittsLawAdjustment, _dpiScale);
	if (!_isEffectParametersVisible || _imguiImpl.FrameInputCanceled()) {
		_parameterResetGesture.Clear();
	}

	bool needRedraw = false;
	// 防止 ID 冲突
	int itemId = 0;

	if (_isToolbarVisible && _DrawToolbar(fps, itemId)) {
		needRedraw = true;
	}

	if (_isProfilerVisible && _DrawProfiler(effectTimings, fps, itemId)) {
		needRedraw = true;
	}

	if (_isEffectParametersVisible && _DrawEffectParameters(itemId)) {
		needRedraw = true;
	}
	_isEffectParameterInputActive = _isEffectParametersVisible && ImGui::IsAnyItemActive();
	const float comparisonAlpha = _CalcComparisonStatusAlpha();
	_lastComparisonStatusAlpha = comparisonAlpha;
	if (comparisonAlpha > 0.0f) {
		const std::string& statusText = _GetResourceString(_comparisonStatusOriginal ?
			L"Overlay_Comparison_Original" : L"Overlay_Comparison_Processed");
		const ImVec2 textSize = ImGui::CalcTextSize(statusText.c_str());
		const ImVec2 padding = ImGui::GetStyle().WindowPadding;
		const float margin = 12.0f * _dpiScale;
		// Size from this label now, avoiding an invisible auto-fit frame or
		// clipping when switching between labels on a static source.
		ImGui::SetNextWindowSize({ textSize.x + padding.x * 2, textSize.y + padding.y * 2 });
		ImGui::SetNextWindowPos({ ImGui::GetIO().DisplaySize.x - margin,
			ImGui::GetIO().DisplaySize.y - margin }, ImGuiCond_Always, { 1.0f, 1.0f });
		// The explicit background alpha overrides ImGui's style alpha.
		ImGui::SetNextWindowBgAlpha(0.72f * comparisonAlpha);
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, comparisonAlpha);
		if (ImGui::Begin("##passThroughStatus", nullptr,
			ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove)) {
			ImGui::TextUnformatted(statusText.c_str());
		}
		ImGui::End();
		ImGui::PopStyleVar();
	}

#ifdef _DEBUG
	if (_isDemoWindowVisible) {
		ImGui::ShowDemoWindow(&_isDemoWindowVisible);
	}
#endif

	_imguiImpl.Draw(drawOffset);
	_overlayDirty = needRedraw || needsLayoutFollowUp ||
		ImGui::IsAnyMouseDown() || ImGui::IsAnyItemActive();

	if (_isProfilerVisible != oldProfilerVisible) {
		Renderer& renderer = ScalingWindow::Get().Renderer();
		if (_isProfilerVisible) {
			renderer.StartProfile();
		} else {
			renderer.StopProfile();
		}
	}

	_ClearStatesIfNoVisibleWindow();
}

void OverlayDrawer::ClearStates() noexcept {
	_isEffectParameterInputActive = false;
	_parameterResetGesture.Clear();
	_imguiImpl.ClearStates();
	_isCursorOnCaptionArea = false;
	_isToolbarItemActive = false;
	_overlayDirty = true;
}

void OverlayDrawer::OnPresentSucceeded() noexcept {
	_imguiImpl.OnPresentSucceeded();
}

void OverlayDrawer::OnPresentFailed() noexcept {
	_overlayDirty = true;
}

ToolbarState OverlayDrawer::ToolbarState() const noexcept {
	if (!_isToolbarVisible) {
		return ToolbarState::Off;
	} else {
		return _isToolbarPinned ? ToolbarState::AlwaysShow : ToolbarState::AutoHide;
	}
}

void OverlayDrawer::ToolbarState(Magpie::ToolbarState value) noexcept {
	if (ToolbarState() == value) {
		return;
	}

	if (value == ToolbarState::Off) {
		_isToolbarVisible = false;
		_ClearStatesIfNoVisibleWindow();
	} else if (value == ToolbarState::AlwaysShow) {
		_isToolbarVisible = true;
		_isToolbarPinned = true;
	} else {
		_isToolbarVisible = true;
		_isToolbarPinned = false;
	}

	_overlayDirty = true;
}

void OverlayDrawer::InvokeAction(OverlayAction action) noexcept {
    switch (action) {
    case OverlayAction::Profiler:
        _isProfilerVisible = !_isProfilerVisible;
        if (_isProfilerVisible) ScalingWindow::Get().Renderer().StartProfile();
        else ScalingWindow::Get().Renderer().StopProfile();
        break;
    case OverlayAction::EffectParameters:
        _isEffectParametersVisible = !_isEffectParametersVisible;
        break;
    case OverlayAction::ToolbarPin:
        _isToolbarPinned = !_isToolbarPinned;
        _isToolbarVisible = true;
        break;
    case OverlayAction::Comparison:
        {
            auto& renderer = ScalingWindow::Get().Renderer();
            const bool original = !renderer.IsPassThroughActive();
            if (renderer.SetPassThroughActive(original)) _ShowComparisonStatus(original);
        }
        break;
    default:
        return;
    }
    _overlayDirty = true;
    _ClearStatesIfNoVisibleWindow();
}

bool OverlayDrawer::AnyVisibleWindow() const noexcept {
	bool result = _isToolbarVisible || _isProfilerVisible ||
		_isEffectParametersVisible || _CalcComparisonStatusAlpha() > 0.0f;
#ifdef _DEBUG
	result = result || _isDemoWindowVisible;
#endif
	return result;
}

bool OverlayDrawer::MessageHandler(UINT msg, WPARAM wParam, LPARAM lParam) noexcept {
	if (!AnyVisibleWindow()) {
		return false;
	}

	const ImGuiInputResult result = _imguiImpl.MessageHandler(msg, wParam, lParam);
	if (result != ImGuiInputResult::None) {
		_overlayDirty = true;
	}
	_ClearStatesIfNoVisibleWindow();
	return result == ImGuiInputResult::Urgent;
}

bool OverlayDrawer::HasPendingInput() const noexcept {
	return _imguiImpl.HasPendingInput();
}

bool OverlayDrawer::HasUrgentInput() const noexcept {
	return _imguiImpl.HasUrgentInput();
}

bool OverlayDrawer::NeedRedraw(uint32_t fps) const noexcept {
	if (_overlayDirty || _imguiImpl.HasPendingInput() ||
		_CalcComparisonStatusAlpha() != _lastComparisonStatusAlpha) {
		return true;
	}
	if (!AnyVisibleWindow()) {
		return false;
	}
	// Independent overlays also need timing refreshes when the FPS text is
	// unchanged. Bound them to 2 Hz, including when no new sample is available.
	if (_isProfilerVisible && steady_clock::now() - _lastProfilerDrawTime >= 500ms) return true;
	if (_isEffectParametersVisible && (!_effectParametersInitialized ||
		_lastEffectParametersSaveResult != _effectParametersSaveState->result.load(std::memory_order_acquire) ||
		ScalingWindow::Get().Options().parameterSession->HasChanges(_parameterSessionSnapshot.revision))) return true;
	if (_CalcToolbarAlpha() != _lastToolbarAlpha) {
		return true;
	}
	return _lastFrameRateText != _FormatFrameRate(fps) &&
		(_lastToolbarAlpha > FLOAT_EPSILON<float> ||
			_isProfilerVisible || _isEffectParametersVisible);
}

std::string OverlayDrawer::_FormatFrameRate(uint32_t fps) const noexcept {
	const auto& renderer = ScalingWindow::Get().Renderer();
	if (!renderer.HasFrameGeneration()) return fmt::format("{} FPS", fps);
	const auto rate = renderer.PresentationRate();
	return rate.totalKnown ? fmt::format("{}/{} FPS", rate.total, rate.real) :
		fmt::format("—/{} FPS", rate.real);
}

void OverlayDrawer::_ShowComparisonStatus(bool original) noexcept {
	_comparisonStatusOriginal = original;
	_comparisonStatusStarted = steady_clock::now();
	_overlayDirty = true;
}

float OverlayDrawer::_CalcComparisonStatusAlpha() const noexcept {
	if (_comparisonStatusStarted == steady_clock::time_point{}) return 0.0f;
	const auto elapsed = steady_clock::now() - _comparisonStatusStarted;
	if (elapsed <= 2s) return 1.0f;
	if (elapsed >= 2500ms) return 0.0f;
	return 1.0f - duration<float>(elapsed - 2s).count() / 0.5f;
}

void OverlayDrawer::UpdateAfterActiveEffectsChanged() noexcept {
	_parameterResetGesture.Clear();
	const std::vector<const EffectDesc*>& effectDescs =
		ScalingWindow::Get().Renderer().ActiveEffectDescs();
	_timelineColors = OverlayHelper::GenerateTimelineColors(effectDescs);

	uint32_t passCount = 0;
	for (const EffectDesc* info : effectDescs) {
		passCount += (uint32_t)info->passes.size();
	}

	// 清空旧时间数据
	_effectTimingsStatistics.clear();
	_effectTimingsStatistics.resize(passCount);
	_lastestAvgEffectTimings.clear();
	_lastestAvgEffectTimings.resize(passCount);
	_lastUpdateTime = {};
	_overlayDirty = true;
}

static const std::wstring& GetAppLanguage() noexcept {
	static std::wstring language;
	if (language.empty()) {
		winrt::ResourceContext resourceContext = winrt::ResourceContext::GetForViewIndependentUse();
		language = resourceContext.QualifierValues().Lookup(L"Language");
		StrHelper::ToLowerCase(language);
	}
	return language;
}

static const std::wstring& GetSystemFontsFolder() noexcept {
	static std::wstring result;

	if (result.empty()) {
		wil::unique_cotaskmem_string fontsFolder;
		HRESULT hr = SHGetKnownFolderPath(FOLDERID_Fonts, 0, NULL, fontsFolder.put());
		if (FAILED(hr)) {
			Logger::Get().ComError("SHGetKnownFolderPath 失败", hr);
			return result;
		}

		result = fontsFolder.get();
	}

	return result;
}

bool OverlayDrawer::_BuildFonts() noexcept {
	const std::wstring& language = GetAppLanguage();
	ImFontAtlas& fontAtlas = *ImGui::GetIO().Fonts;

	auto buildFontAtlas = [&]() {
		fontAtlas.Flags |= ImFontAtlasFlags_NoPowerOfTwoHeight | ImFontAtlasFlags_NoMouseCursors;

		std::wstring uiFontPath = GetSystemFontsFolder();
		std::string iconFontPath = StrHelper::UTF16ToUTF8(uiFontPath);
		if (Win32Helper::GetOSVersion().IsWin11()) {
			uiFontPath += L"\\SegUIVar.ttf";
			iconFontPath += "\\SegoeIcons.ttf";
		} else {
			uiFontPath += L"\\segoeui.ttf";
			iconFontPath += "\\segmdl2.ttf";
		}

		std::vector<uint8_t> uiFontData;
		if (!Win32Helper::ReadFile(uiFontPath.c_str(), uiFontData)) {
			Logger::Get().Error("读取字体文件失败");
			return false;
		}

		// 构建 ImFontAtlas 前 ranges 不能析构，因为 ImGui 只保存了指针
		SmallVector<ImWchar> uiRanges = _BuildFontUI(language, uiFontData);
		_BuildFontIcons(iconFontPath.c_str());

		if (!fontAtlas.Build()) {
			Logger::Get().Error("构建 ImFontAtlas 失败");
			return false;
		}

		return true;
	};

	if (ScalingWindow::Get().Options().IsFontCacheDisabled()) {
		if (!buildFontAtlas()) {
			return false;
		}
	} else {
		const uint32_t dpi = (uint32_t)std::lroundf(_dpiScale * USER_DEFAULT_SCREEN_DPI);
		if (ImGuiFontsCacheManager::Get().Load(language, dpi, fontAtlas)) {
			_fontUI = fontAtlas.Fonts[0];
			_fontMonoNumbers = fontAtlas.Fonts[1];
			_fontIcons = fontAtlas.Fonts[2];
		} else {
			if (!buildFontAtlas()) {
				return false;
			}

			ImGuiFontsCacheManager::Get().Save(language, dpi, fontAtlas);
		}
	}

	if (!_imguiImpl.BuildFonts()) {
		Logger::Get().Error("构建字体失败");
		return false;
	}

	return true;
}

template <size_t SIZE>
static void SetGlyphRanges(SmallVector<ImWchar>& uiRanges, const ImWchar (&ranges)[SIZE]) noexcept {
	uiRanges.assign(std::begin(ranges), std::end(ranges));
}

// 指针重载，但不能直接使用指针
template <typename T, typename = std::enable_if_t<std::is_same_v<T, const ImWchar*>>>
static void SetGlyphRanges(SmallVector<ImWchar>& uiRanges, T ranges) noexcept {
	// 删除末尾的 0
	for (const ImWchar* range = ranges; *range; ++range) {
		uiRanges.push_back(*range);
	}
}

SmallVector<ImWchar> OverlayDrawer::_BuildFontUI(
	std::wstring_view language,
	const std::vector<uint8_t>& fontData
) noexcept {
	ImFontAtlas& fontAtlas = *ImGui::GetIO().Fonts;

	std::string extraFontPath;
	const ImWchar* extraRanges = nullptr;
	int extraFontNo = 0;

	SmallVector<ImWchar> ranges;
	if (language == L"en-us") {
		SetGlyphRanges(ranges, OverlayHelper::BASIC_LATIN_RANGES);
	} else if (language == L"ru" || language == L"uk") {
		SetGlyphRanges(ranges, fontAtlas.GetGlyphRangesCyrillic());
	} else if (language == L"tr" || language == L"pl" || language == L"fi") {
		SetGlyphRanges(ranges, OverlayHelper::EXTENDED_LATIN_RANGES);
	} else if (language == L"vi") {
		SetGlyphRanges(ranges, fontAtlas.GetGlyphRangesVietnamese());
	} else if (language == L"fr") {
		SetGlyphRanges(ranges, OverlayHelper::FRENCH_RANGES);
	} else {
		// Basic Latin 使用默认字体
		SetGlyphRanges(ranges, OverlayHelper::BASIC_LATIN_RANGES);

		// 一些语言需要加载额外的字体:
		// 简体中文 -> Microsoft YaHei UI
		// 繁体中文 -> Microsoft JhengHei UI
		// 日语 -> Yu Gothic UI
		// 韩语/朝鲜语 -> Malgun Gothic
		// 泰米尔语 -> Nirmala UI
		// 参见 https://learn.microsoft.com/en-us/windows/apps/design/style/typography#fonts-for-non-latin-languages
		if (language == L"zh-hans") {
			// msyh.ttc: 0 是微软雅黑，1 是 Microsoft YaHei UI
			extraFontPath = StrHelper::Concat(StrHelper::UTF16ToUTF8(GetSystemFontsFolder()), "\\msyh.ttc");
			extraFontNo = 1;
			extraRanges = OverlayHelper::GetGlyphRangesChineseSimplifiedOfficial();
		} else if (language == L"zh-hant") {
			// msjh.ttc: 0 是 Microsoft JhengHei，1 是 Microsoft JhengHei UI
			extraFontPath = StrHelper::Concat(StrHelper::UTF16ToUTF8(GetSystemFontsFolder()), "\\msjh.ttc");
			extraFontNo = 1;
			extraRanges = OverlayHelper::GetGlyphRangesChineseTraditionalOfficial();
		} else if (language == L"ja") {
			// YuGothM.ttc: 0 是 Yu Gothic Medium，1 是 Yu Gothic UI
			extraFontPath = StrHelper::Concat(StrHelper::UTF16ToUTF8(GetSystemFontsFolder()), "\\YuGothM.ttc");
			extraFontNo = 1;
			extraRanges = fontAtlas.GetGlyphRangesJapanese();
		} else if (language == L"ko") {
			extraFontPath = StrHelper::Concat(StrHelper::UTF16ToUTF8(GetSystemFontsFolder()), "\\malgun.ttf");
			extraRanges = fontAtlas.GetGlyphRangesKorean();
		} else if (language == L"ta") {
			extraFontPath = StrHelper::Concat(StrHelper::UTF16ToUTF8(GetSystemFontsFolder()), "\\Nirmala.ttf");
			extraRanges = OverlayHelper::EXTRA_TAMIL_RANGES;
		}
	}

	ranges.push_back(COLOR_INDICATOR_W);
	ranges.push_back(COLOR_INDICATOR_W);
	ranges.push_back(0);

	ImFontConfig config;
	// fontData 需要多次使用，我们自己读取并管理生命周期
	config.FontDataOwnedByAtlas = false;

	const float fontSize = 18 * _dpiScale;

	//////////////////////////////////////////////////////////
	//
	// ranges (+ extraRanges) -> _fontUI
	//
	//////////////////////////////////////////////////////////


#ifdef _DEBUG
	std::char_traits<char>::copy(config.Name, "_fontUI", std::size(config.Name));
#endif

	_fontUI = fontAtlas.AddFontFromMemoryTTF(
		(void*)fontData.data(), (int)fontData.size(), fontSize, &config, ranges.data());

	if (extraRanges) {
		assert(Win32Helper::FileExists(StrHelper::UTF8ToUTF16(extraFontPath).c_str()));

		// 在 MergeMode 下已有字符会跳过而不是覆盖
		config.MergeMode = true;
		config.FontNo = extraFontNo;
		// 额外字体数据由 ImGui 管理，初始化完成后释放
		config.FontDataOwnedByAtlas = true;
		fontAtlas.AddFontFromFileTTF(extraFontPath.c_str(), fontSize, &config, extraRanges);
		config.FontDataOwnedByAtlas = false;
		config.FontNo = 0;
		config.MergeMode = false;
	}

	//////////////////////////////////////////////////////////
	//
	// NUMBER_RANGES + NOT_NUMBER_RANGES -> _fontMonoNumbers
	//
	//////////////////////////////////////////////////////////

#ifdef _DEBUG
	std::char_traits<char>::copy(config.Name, "_fontMonoNumbers", std::size(config.Name));
#endif

	// 等宽的数字字符
	config.GlyphMinAdvanceX = config.GlyphMaxAdvanceX = fontSize * 0.42f;
	_fontMonoNumbers = fontAtlas.AddFontFromMemoryTTF(
		(void*)fontData.data(), (int)fontData.size(), fontSize, &config, OverlayHelper::NUMBER_RANGES);

	// 其他不等宽的字符
	config.MergeMode = true;
	config.GlyphMinAdvanceX = 0;
	config.GlyphMaxAdvanceX = std::numeric_limits<float>::max();
	fontAtlas.AddFontFromMemoryTTF(
		(void*)fontData.data(), (int)fontData.size(), fontSize, &config, OverlayHelper::NOT_NUMBER_RANGES);

	return ranges;
}

void OverlayDrawer::_BuildFontIcons(const char* fontPath) noexcept {
	ImFontConfig config;
#ifdef _DEBUG
	std::char_traits<char>::copy(config.Name, "_fontIcons", std::size(config.Name));
#endif

	const float fontSize = 16 * _dpiScale;
	_fontIcons = ImGui::GetIO().Fonts->AddFontFromFileTTF(
		fontPath, fontSize, &config, OverlayHelper::ICON_RANGES);
}

static std::string_view GetEffectDisplayName(const EffectDesc& effectDesc) noexcept {
	if (!effectDesc.sortName.empty()) {
		return effectDesc.sortName;
	}
	auto delimPos = effectDesc.name.find_last_of('\\');
	if (delimPos == std::string::npos) {
		return effectDesc.name;
	} else {
		return std::string_view(effectDesc.name.begin() + delimPos + 1, effectDesc.name.end());
	}
}

bool OverlayDrawer::_DrawTimingItem(
	int& itemId,
	const char* text,
	const ImColor* color,
	float time,
	bool isExpanded
) const noexcept {
	ImGui::TableNextRow();
	ImGui::TableNextColumn();

	const std::string timeStr = fmt::format("{:.3f} ms", time);
	const float timeWidth = _fontMonoNumbers->CalcTextSizeA(
		ImGui::GetFontSize(), FLT_MAX, 0.0f, timeStr.c_str()).x;

	// 计算布局
	static constexpr float spacingBeforeText = 3;
	static constexpr float spacingAfterText = 8;
	const float descWrapPos = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - timeWidth - spacingAfterText;
	const float descHeight = ImGui::CalcTextSize(
		text, nullptr, false, descWrapPos - ImGui::GetCursorPosX() - (color ? ImGui::CalcTextSize(COLOR_INDICATOR).x + spacingBeforeText : 0)).y;

	const float fontHeight = ImGui::GetFont()->FontSize;

	ImGui::PushID(itemId++);

	bool isHovered = false;
	if (color) {
		ImGui::Selectable("", false, 0, ImVec2(0, descHeight));
		if (ImGui::IsItemHovered()) {
			isHovered = true;
		}
		ImGui::SameLine(0, 0);

		ImGui::PushStyleColor(ImGuiCol_Text, (ImU32)*color);

		if (descHeight >= fontHeight * 2) {
			// 不知为何 SetCursorPos 不起作用
			// 所以这里使用占位竖直居中颜色框
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2());
			ImGui::BeginGroup();
			ImGui::Dummy(ImVec2(0, (descHeight - fontHeight) / 2));
			ImGui::TextUnformatted(COLOR_INDICATOR);
			ImGui::EndGroup();
			ImGui::PopStyleVar();
		} else {
			ImGui::TextUnformatted(COLOR_INDICATOR);
		}
		ImGui::PopStyleColor();

		ImGui::SameLine(0, spacingBeforeText);
	}

	ImGui::PushTextWrapPos(descWrapPos);
	ImGui::TextUnformatted(text);
	ImGui::PopTextWrapPos();
	ImGui::SameLine(0, 0);

	// 描述过长导致换行时竖直居中时间
	if (color && descHeight >= fontHeight * 2) {
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (descHeight - fontHeight) / 2);
	}

	if (isExpanded) {
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 0.5f));
	}

	ImGui::PushFont(_fontMonoNumbers);
	ImGui::SetCursorPosX(descWrapPos + spacingAfterText);
	ImGui::TextUnformatted(timeStr.c_str());
	ImGui::PopFont();

	if (isExpanded) {
		ImGui::PopStyleColor();
	}

	ImGui::PopID();

	return isHovered;
}

// 返回鼠标悬停的项的序号，未悬停于任何项返回 -1
int OverlayDrawer::_DrawEffectTimings(
	int& itemId,
	const _EffectDrawInfo& drawInfo,
	bool showPasses,
	std::span<const ImColor> colors,
	bool singleEffect
) const noexcept {
	int result = -1;

	showPasses &= drawInfo.passTimings.size() > 1;

	std::string effectName;
	if (ScalingWindow::Get().Options().IsDeveloperMode() && drawInfo.desc->flags & EffectFlags::FP16) {
		// 开发者选项开启时显示效果是否使用 FP16
		effectName = StrHelper::Concat(GetEffectDisplayName(*drawInfo.desc), " (FP16)");
	} else {
		effectName = std::string(GetEffectDisplayName(*drawInfo.desc));
	}

	if (_DrawTimingItem(
		itemId,
		effectName.c_str(),
		(!singleEffect && !showPasses) ? &colors[0] : nullptr,
		drawInfo.totalTime,
		showPasses
	)) {
		result = 0;
	}

	if (showPasses) {
		for (size_t j = 0; j < drawInfo.passTimings.size(); ++j) {
			ImGui::Indent(16);

			if (_DrawTimingItem(
				itemId,
				drawInfo.desc->passes[j].desc.c_str(),
				&colors[j],
				drawInfo.passTimings[j]
			)) {
				result = (int)j;
			}

			ImGui::Unindent(16);
		}
	}

	return result;
}

void OverlayDrawer::_DrawTimelineItem(
	int& itemId,
	ImU32 color,
	float dpiScale,
	std::string_view name,
	float time,
	float effectsTotalTime,
	bool selected
) {
	ImGui::PushID(itemId++);

	ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, color);
	ImGui::PushStyleColor(ImGuiCol_HeaderActive, color);
	ImGui::PushStyleColor(ImGuiCol_HeaderHovered, color);
	ImGui::PushStyleColor(ImGuiCol_Header, color);
	ImGui::Selectable("", selected);
	ImGui::PopStyleColor(3);

	if (ImGui::IsItemHovered() || ImGui::IsItemClicked()) {
		std::string content = fmt::format("{}\n{:.3f} ms\n{}%", name, time, std::lroundf(time / effectsTotalTime * 100));
		ImGui::PushFont(_fontMonoNumbers);
		_imguiImpl.Tooltip(content.c_str(), _dpiScale, nullptr, 500 * dpiScale);
		ImGui::PopFont();
	}

	// 空间足够时显示文字
	std::string text;
	if (selected) {
		text = fmt::format("{}%", std::lroundf(time / effectsTotalTime * 100));
	} else {
		text.assign(name);
	}

	float textWidth = ImGui::CalcTextSize(text.c_str()).x;
	float itemWidth = ImGui::GetItemRectSize().x;
	float itemSpacing = ImGui::GetStyle().ItemSpacing.x;
	if (itemWidth - (selected ? 0 : itemSpacing) > textWidth + 4 * _dpiScale) {
		ImGui::SameLine(0, 0);
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (itemWidth - textWidth - itemSpacing) / 2);
		// 竖直方向居中
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 0.5f * _dpiScale);
		ImGui::TextUnformatted(text.c_str());
	}

	ImGui::PopID();
}

static std::string IconLabel(ImWchar iconChar) noexcept {
	const wchar_t text[] = { iconChar, L'\0' };
	return StrHelper::UTF16ToUTF8(text);
}

bool OverlayDrawer::_DrawToolbar(uint32_t fps, int& itemId) noexcept {
	bool needRedraw = false;

	const float windowWidth = 392 * _dpiScale;
	ImGui::SetNextWindowSize({ windowWidth, (CORNER_ROUNDING + 31) * _dpiScale });
	ImGui::SetNextWindowPos(
		ImVec2((ImGui::GetIO().DisplaySize.x - windowWidth) / 2, -CORNER_ROUNDING * _dpiScale));

	_lastToolbarAlpha = _CalcToolbarAlpha();
	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, _lastToolbarAlpha);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, (ImU32)ImColor(15, 15, 15, 180));
	const ImVec2 originalWindowPadding = ImGui::GetStyle().WindowPadding;
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 6 * _dpiScale,0.0f });

	_isToolbarItemActive = false;

	if (ImGui::Begin(StrHelper::Concat("##", TOOLBAR_WINDOW_ID).c_str(), nullptr,
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse))
	{
		// 通过工具栏拖拽缩放窗口时不要更新 _isCursorOnCaptionArea
		if (!ScalingWindow::Get().IsResizingOrMoving()) {
			// 鼠标被 ImGui 捕获时禁止拖拽缩放窗口
			_isCursorOnCaptionArea = !ImGui::IsAnyMouseDown();
			if (_isCursorOnCaptionArea) {
				// 检查鼠标是否被其他窗口遮挡
				const char* hoveredWindowId = _imguiImpl.GetHoveredWindowId();
				_isCursorOnCaptionArea = hoveredWindowId &&
					hoveredWindowId == std::string_view(TOOLBAR_WINDOW_ID);
			}
		}

		ImGui::SetCursorPosY((CORNER_ROUNDING + 3) * _dpiScale);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, originalWindowPadding);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 4 * _dpiScale,4 * _dpiScale });
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4 * _dpiScale);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 4 * _dpiScale, 0.0f });
		// 禁用仅为阻止交互，不应有视觉改变
		ImGui::PushStyleVar(ImGuiStyleVar_DisabledAlpha, 1.0f);
		ImGui::PushStyleColor(ImGuiCol_Button, { 0,0,0,0 });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.118f, 0.533f, 0.894f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 0.118f, 0.533f, 0.894f, 0.8f });

		auto drawToggleButton = [&](bool& value, ImWchar icon, const char* tooltip) {
			bool stylePushed = value;
			if (stylePushed) {
				ImGui::PushStyleColor(ImGuiCol_Button, { 0.118f, 0.533f, 0.894f, 0.8f });
			}

			ImGui::PushFont(_fontIcons);
			if (ImGui::Button(IconLabel(icon).c_str())) {
				value = !value;
				needRedraw = true;
			}
			if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
				_isCursorOnCaptionArea = false;
				_isToolbarItemActive = true;
			}
			ImGui::PopFont();
			if (ImGui::IsItemHovered() || ImGui::IsItemClicked()) {
				_imguiImpl.Tooltip(tooltip, _dpiScale);
			}

			if (stylePushed) {
				ImGui::PopStyleColor();
			}
		};

		auto drawButton = [&](ImWchar icon, const char* tooltip, const char* description = nullptr) {
			ImGui::PushFont(_fontIcons);
			const bool clicked = ImGui::Button(IconLabel(icon).c_str());
			if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
				_isCursorOnCaptionArea = false;
				_isToolbarItemActive = true;
			}
			ImGui::PopFont();
			if (ImGui::IsItemHovered()) {
				_imguiImpl.Tooltip(tooltip, _dpiScale, description);
			}
			return clicked;
		};

		// 光标不在缩放窗口上时阻止交互
		ImGui::BeginDisabled(!ScalingWindow::Get().CursorManager().CursorHandle());

		const std::string& pinStr = _GetResourceString(L"Overlay_Toolbar_Pin");
		drawToggleButton(_isToolbarPinned, OverlayHelper::SegoeIcons::Pinned, pinStr.c_str());
		ImGui::SameLine();
		const std::string& profilerStr = _GetResourceString(L"Overlay_Toolbar_Profiler");
		drawToggleButton(_isProfilerVisible, OverlayHelper::SegoeIcons::Diagnostic, profilerStr.c_str());
		ImGui::SameLine();
		const std::string& parametersStr =
			_GetResourceString(L"Overlay_Toolbar_EffectParameters");
		drawToggleButton(_isEffectParametersVisible,
			OverlayHelper::SegoeIcons::Parameters, parametersStr.c_str());
		ImGui::SameLine();
		Renderer& renderer = ScalingWindow::Get().Renderer();
		bool passThrough = renderer.IsPassThroughActive();
		const std::string& comparisonTip = _GetResourceString(L"Overlay_Toolbar_PassThrough");
		drawToggleButton(passThrough, OverlayHelper::SegoeIcons::View, comparisonTip.c_str());
		if (passThrough != renderer.IsPassThroughActive() && renderer.SetPassThroughActive(passThrough)) {
			_ShowComparisonStatus(passThrough);
		}
#ifdef _DEBUG
		ImGui::SameLine();
		const std::string& demoStr = _GetResourceString(L"Overlay_Toolbar_Demo");
		drawToggleButton(_isDemoWindowVisible, OverlayHelper::SegoeIcons::Design, demoStr.c_str());
#endif
		ImGui::SameLine();
		const std::string& screenshotStr = _GetResourceString(L"Overlay_Toolbar_TakeScreenshot");
		const std::string& screenshotDescStr = _GetResourceString(L"Overlay_Toolbar_TakeScreenshot_Description");
		if (drawButton(OverlayHelper::SegoeIcons::Camera, screenshotStr.c_str(), screenshotDescStr.c_str())) {
			ScalingWindow::Get().Renderer().TakeDisplayedScreenshot();
		}
		// 截图按钮右键菜单
		if (ImGui::BeginPopupContextItem()) {
			_isCursorOnCaptionArea = false;
			_isToolbarItemActive = true;

			ImGui::SeparatorText(_GetResourceString(L"Overlay_Toolbar_TakeScreenshot_PopupTitle").c_str());
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f * _dpiScale);
			ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 6.0f * _dpiScale);

			const std::vector<const EffectDesc*>& effectDescs =
				ScalingWindow::Get().Renderer().ActiveEffectDescs();
			const uint32_t effectCount = (uint32_t)effectDescs.size();

			const bool isDeveloperMode = ScalingWindow::Get().Options().IsDeveloperMode();
			for (uint32_t i = 0; i < effectCount; ++i) {
				const EffectDesc& effectDesc = *effectDescs[i];
				std::string_view effectName = GetEffectDisplayName(effectDesc);

				if (isDeveloperMode && effectDesc.passes.size() > 1) {
					// 开发者模式允许保存任意通道的输出
					ImGui::PushID(itemId++);
					if (ImGui::BeginMenu(effectName.data())) {
						const uint32_t passCount = (uint32_t)effectDesc.passes.size();
						for (uint32_t j = 0; j < passCount; ++j) {
							const EffectPassDesc& passDesc = effectDesc.passes[j];
							const uint32_t outputCount = (uint32_t)passDesc.outputs.size();

							if (outputCount == 1) {
								ImGui::PushID(itemId++);
								if (ImGui::MenuItem(passDesc.desc.c_str())) {
									ScalingWindow::Get().Renderer().TakeScreenshot(i, j);
								}
								ImGui::PopID();
							} else {
								ImGui::PushID(itemId++);
								if (ImGui::BeginMenu(passDesc.desc.c_str())) {
									for (uint32_t k = 0; k < outputCount; ++k) {
										ImGui::PushID(itemId++);
										if (ImGui::MenuItem(effectDesc.textures[passDesc.outputs[k]].name.c_str())) {
											ScalingWindow::Get().Renderer().TakeScreenshot(i, j, k);
										}
										ImGui::PopID();
									}

									ImGui::EndMenu();
								}
								ImGui::PopID();
							}
						}

						ImGui::EndMenu();
					}
					ImGui::PopID();
				} else {
					ImGui::PushID(itemId++);
					if (ImGui::MenuItem(effectName.data())) {
						ScalingWindow::Get().Renderer().TakeScreenshot(i);
					}
					ImGui::PopID();
				}
			}

			ImGui::PopStyleVar();
			ImGui::EndPopup();
		}

		// 居中绘制 FPS
		ImGui::SameLine();
		const std::string fpsText = _FormatFrameRate(fps);
		ImGui::SetCursorPosX((ImGui::GetContentRegionMax().x - ImGui::CalcTextSize(fpsText.c_str()).x) / 2);
		ImGui::SetCursorPosY((CORNER_ROUNDING + 1) * _dpiScale);
		ImGui::PushFont(_fontMonoNumbers);
		ImGui::TextUnformatted(fpsText.c_str());
		ImGui::PopFont();

		ImGui::SameLine();
		ImGui::SetCursorPosY((CORNER_ROUNDING + 3) * _dpiScale);

		// 源窗口支持最小化时才显示最小化按钮
		const HWND hwndSrc = ScalingWindow::Get().SrcTracker().Handle();
		const bool canSrcMinimized = GetWindowStyle(hwndSrc) & WS_MINIMIZEBOX;
		ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x -
			((canSrcMinimized ? 3 : 2) * 28 - 4) * _dpiScale);

		if (canSrcMinimized) {
			const std::string& minimizeStr = _GetResourceString(L"Overlay_Toolbar_Minimize");
			const ImWchar icon = Win32Helper::GetOSVersion().IsWin11() ?
				OverlayHelper::SegoeIcons::CheckboxIndeterminate : OverlayHelper::SegoeIcons::Remove;
			if (drawButton(icon, minimizeStr.c_str())) {
				// 模拟通过标题栏最小化，失败则回落到 ShowWindow
				if (!PostMessage(hwndSrc, WM_SYSCOMMAND, SC_MINIMIZE, 0)) {
					ShowWindowAsync(hwndSrc, SW_SHOWMINIMIZED);
				}
			}
			ImGui::SameLine();
		}

		{
			const bool isWindowedMode = ScalingWindow::Get().Options().IsWindowedMode();
			const ImWchar icon = isWindowedMode ?
				OverlayHelper::SegoeIcons::FullScreen : OverlayHelper::SegoeIcons::Favicon;
			const std::string& switchScalingStr = _GetResourceString(
				isWindowedMode ? L"Overlay_Toolbar_SwitchToFullscreen" : L"Overlay_Toolbar_SwitchToWindowed");
			if (drawButton(icon, switchScalingStr.c_str())) {
				ScalingWindow::Dispatcher().TryEnqueue([]() {
					ScalingWindow::Get().ToggleScaling(!ScalingWindow::Get().Options().IsWindowedMode());
				});
			}
		}
		ImGui::SameLine();

		// 和主窗口保持一致 (#C42B1C)
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.769f, 0.169f, 0.11f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 0.769f, 0.169f, 0.11f, 0.8f });

		const std::string& closeStr = _GetResourceString(L"Overlay_Toolbar_Close");
		const std::string& closeDescStr = _GetResourceString(L"Overlay_Toolbar_Close_Description");
		if (drawButton(OverlayHelper::SegoeIcons::Cancel, closeStr.c_str(), closeDescStr.c_str())) {
			ScalingWindow::Dispatcher().TryEnqueue([]() {
				ScalingWindow::Get().Stop();
			});
		}
		if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
			ScalingWindow::Dispatcher().TryEnqueue([this, runId(ScalingWindow::RunId())]() {
				if (runId == ScalingWindow::RunId()) {
					ToolbarState(ToolbarState::Off);
					ScalingWindow::Get().Renderer().Render(true);
				}
			});
		}

		ImGui::EndDisabled();

		ImGui::PopStyleColor(5);
		ImGui::PopStyleVar(6);
	} else {
		_isCursorOnCaptionArea = false;
	}
	ImGui::End();

	ImGui::PopStyleColor();
	ImGui::PopStyleVar(2);

	return needRedraw;
}

#ifdef MP_DEBUG_INFO_ON_OVERLAY
static std::string RectToStr(const RECT& rect) noexcept {
	return fmt::format("{},{},{},{} ({}x{})",
		rect.left, rect.top, rect.right, rect.bottom,
		rect.right - rect.left, rect.bottom - rect.top);
}
#endif

static bool ParameterValuesEqual(float left, float right) noexcept {
	return std::abs(left - right) <= 1e-5f;
}

static bool IsEffectParameterDescriptorValid(
	const EffectParameterDesc& parameter
) noexcept {
	if (parameter.name.empty()) {
		return false;
	}

	if (IsChoiceEffectParameter(parameter)) {
		const int defaultValue = std::get<1>(parameter.constant).defaultValue;
		bool hasDefault = false;
		for (size_t i = 0; i < parameter.choices.size(); ++i) {
			const EffectParameterChoice& choice = parameter.choices[i];
			if (choice.label.empty()) return false;
			hasDefault = hasDefault || choice.value == defaultValue;
			for (size_t j = 0; j < i; ++j) {
				if (parameter.choices[j].value == choice.value) return false;
			}
		}
		return hasDefault;
	}

	if (parameter.constant.index() == 0) {
		const EffectConstant<float>& value = std::get<0>(parameter.constant);
		if (!std::isfinite(value.defaultValue) || !std::isfinite(value.minValue) ||
			!std::isfinite(value.maxValue) || !std::isfinite(value.step) ||
			value.minValue > value.maxValue || value.step <= 0.0f ||
			value.defaultValue < value.minValue || value.defaultValue > value.maxValue) {
			return false;
		}
	} else {
		const EffectConstant<int>& value = std::get<1>(parameter.constant);
		if (value.minValue > value.maxValue || value.step <= 0 ||
			value.defaultValue < value.minValue || value.defaultValue > value.maxValue) {
			return false;
		}
	}

	int currentTick = 0;
	int maximumTick = 0;
	return GetEffectParameterTicks(
		parameter,
		parameter.constant.index() == 0
			? std::get<0>(parameter.constant).defaultValue
			: static_cast<float>(std::get<1>(parameter.constant).defaultValue),
		currentTick,
		maximumTick);
}

void OverlayDrawer::_InitEffectParameterValues() noexcept {
	const ScalingOptions& options = ScalingWindow::Get().Options();
	_effectParametersSaveState = options.parameterSession->saveState;
	_effectParametersRevision = options.parameterSession->saveRevision;
	const std::vector<const EffectDesc*>& descriptions =
		ScalingWindow::Get().Renderer().ActiveEffectDescs();
	const size_t effectCount = std::min(options.effects.size(), descriptions.size());
	if (descriptions.size() < options.effects.size()) {
		Logger::Get().Error(fmt::format(
			"Overlay parameter initialization is missing {} active effect description(s)",
			options.effects.size() - descriptions.size()));
	} else if (descriptions.size() > options.effects.size() + 1) {
		Logger::Get().Warn(fmt::format(
			"Overlay parameter initialization found {} unexpected appended effect(s)",
			descriptions.size() - options.effects.size()));
	}

	_localizedEffectParameters.clear();
	_localizedEffectParameters.resize(effectCount);
	_startupEffectParameterValues.clear();
	_startupEffectParameterValues.resize(effectCount);
	for (size_t effectIdx = 0; effectIdx < effectCount; ++effectIdx) {
		const EffectDesc& description = *descriptions[effectIdx];
		_localizedEffectParameters[effectIdx] = description.params;
		EffectParameterLocalization::Localize(options.effects[effectIdx].name,
			_localizedEffectParameters[effectIdx]);
		std::vector<float>& values = _startupEffectParameterValues[effectIdx];
		values.reserve(description.params.size());
		for (const EffectParameterDesc& parameter : description.params) {
			float value = parameter.constant.index() == 0
				? std::get<0>(parameter.constant).defaultValue
				: static_cast<float>(std::get<1>(parameter.constant).defaultValue);
			if (auto it = options.effects[effectIdx].parameters.find(parameter.name);
				it != options.effects[effectIdx].parameters.end()) {
				value = it->second;
			}
			values.push_back(NormalizeEffectParameterValue(parameter, value));
		}
	}
	_appliedEffectParameterValues = _startupEffectParameterValues;
	_draftEffectParameterValues = _startupEffectParameterValues;
	_submittedEffectParameterValues = _startupEffectParameterValues;
	_submittedEffectOptions = options.effects;
	_startupFrameSync = { options.isFrontEdgeSyncEnabled, options.frontEdgeSyncFrameRate };
	_draftFrameSync = _submittedFrameSync = _startupFrameSync;
	_effectParametersInitialized = true;
}


void OverlayDrawer::_SyncEffectParameterValues() noexcept {
	const auto& session = ScalingWindow::Get().Options().parameterSession;
	if (!session || !session->ReadIfChanged(_parameterSessionSnapshot)) return;
	const auto frameSync = _parameterSessionSnapshot.frameSync;
	if (frameSync.enabled != _submittedFrameSync.enabled) {
		_draftFrameSync.enabled = _submittedFrameSync.enabled = frameSync.enabled;
	}
	if (frameSync.frameRate != _submittedFrameSync.frameRate) {
		_draftFrameSync.frameRate = _submittedFrameSync.frameRate = frameSync.frameRate;
	}
	const auto& descriptions = ScalingWindow::Get().Renderer().ActiveEffectDescs();
	for (size_t i = 0; i < _draftEffectParameterValues.size(); ++i) {
		if (i >= _parameterSessionSnapshot.applied.size() ||
			i >= _parameterSessionSnapshot.desired.size()) break;
		const auto& desired = _parameterSessionSnapshot.desired[i];
		const auto& applied = _parameterSessionSnapshot.applied[i];
		for (size_t j = 0; j < descriptions[i]->params.size(); ++j) {
			const auto& parameter = descriptions[i]->params[j];
			const float fallback = parameter.constant.index() == 0 ?
				std::get<0>(parameter.constant).defaultValue : float(std::get<1>(parameter.constant).defaultValue);
			auto value = [&](const EffectOption& option) {
				const auto it = option.parameters.find(parameter.name);
				return NormalizeEffectParameterValue(parameter, it == option.parameters.end() ? fallback : it->second);
			};
			_appliedEffectParameterValues[i][j] = value(applied);
			const float target = value(desired);
			if (!ParameterValuesEqual(target, _submittedEffectParameterValues[i][j])) {
				_draftEffectParameterValues[i][j] = target;
				_submittedEffectParameterValues[i][j] = target;
			}
		}
	}
	_submittedEffectOptions = _parameterSessionSnapshot.desired;
}

std::vector<EffectOption> OverlayDrawer::_BuildDraftEffectOptions() const noexcept {
	std::vector<EffectOption> effects = _submittedEffectOptions;
	const std::vector<const EffectDesc*>& descriptions =
		ScalingWindow::Get().Renderer().ActiveEffectDescs();
	for (size_t effectIdx = 0;
		effectIdx < _draftEffectParameterValues.size() &&
			effectIdx < effects.size() && effectIdx < descriptions.size();
		++effectIdx) {
		const EffectDesc& description = *descriptions[effectIdx];
		for (size_t parameterIdx = 0;
			parameterIdx < description.params.size() &&
				parameterIdx < _draftEffectParameterValues[effectIdx].size();
			++parameterIdx) {
			if (ParameterValuesEqual(_draftEffectParameterValues[effectIdx][parameterIdx],
				_submittedEffectParameterValues[effectIdx][parameterIdx])) continue;
			effects[effectIdx].parameters[description.params[parameterIdx].name] =
				_draftEffectParameterValues[effectIdx][parameterIdx];
		}
	}
	return effects;
}

bool OverlayDrawer::_RequestEffectParameters(EffectParametersRequestKind kind) noexcept {
	const ScalingOptions& options = ScalingWindow::Get().Options();
	const uint64_t revision = ++options.parameterSession->saveRevision;
	_effectParametersRevision = revision;
	try {
		auto effects = _BuildDraftEffectOptions();
		EffectParametersRequest request{
			.kind = kind,
			.effects = effects,
			.previousEffects = _submittedEffectOptions,
			.frameSync = _draftFrameSync,
			.previousFrameSync = _submittedFrameSync,
			.saveState = _effectParametersSaveState,
			.revision = revision,
			.hwndSource = ScalingWindow::Get().SrcTracker().Handle(),
			.hwndScaling = ScalingWindow::Get().Handle(),
			.scalingRunId = ScalingWindow::RunId()
		};
		if (options.requestEffectParameters &&
			options.requestEffectParameters(options, std::move(request))) {
			options.parameterSession->Desired(effects);
			options.parameterSession->DesiredFrameSync(_draftFrameSync);
			_submittedFrameSync = _draftFrameSync;
			_submittedEffectOptions = std::move(effects);
			_submittedEffectParameterValues = _draftEffectParameterValues;
			return true;
		}
	} catch (...) {
		Logger::Get().Error("Unable to submit effect parameter snapshot");
	}
	_effectParametersSaveState->Complete(revision, EffectParametersSaveError::WriteFailed);
	return false;
}

bool OverlayDrawer::_DrawEffectParameters(int& itemId) noexcept {
	const ImGuiIO& resetInput = ImGui::GetIO();
	_parameterResetGesture.Move(resetInput.MousePos.x, resetInput.MousePos.y, 4.0f * _dpiScale);
	if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
		_parameterResetGesture.Release(_imguiImpl.LeftReleaseWasDrag());
	}
	bool resetPressClaimed = false;
	Renderer& renderer = ScalingWindow::Get().Renderer();
	const std::vector<const EffectDesc*>& descriptions = renderer.ActiveEffectDescs();
	const auto& runtimeInfos = renderer.EffectParameterRuntimeInfos();
	if (!_effectParametersInitialized) {
		_InitEffectParameterValues();
	}
	_SyncEffectParameterValues();

	const size_t configuredEffectCount = std::min(
		ScalingWindow::Get().Options().effects.size(), descriptions.size());
	auto frameSyncChangeCount = [&]() noexcept -> uint32_t {
		const auto& options = ScalingWindow::Get().Options();
		return uint32_t(_draftFrameSync.enabled != options.isFrontEdgeSyncEnabled) +
			uint32_t(_draftFrameSync.frameRate != options.frontEdgeSyncFrameRate);
	};
	uint32_t restartChangeCount = frameSyncChangeCount();
	for (size_t effectIdx = 0; effectIdx < configuredEffectCount; ++effectIdx) {
		for (size_t parameterIdx = 0;
			parameterIdx < _draftEffectParameterValues[effectIdx].size();
			++parameterIdx) {
			const bool changed = !ParameterValuesEqual(
				_draftEffectParameterValues[effectIdx][parameterIdx],
				_appliedEffectParameterValues[effectIdx][parameterIdx]);
			if (changed && effectIdx < runtimeInfos.size() &&
				parameterIdx < runtimeInfos[effectIdx].size() &&
				runtimeInfos[effectIdx][parameterIdx].applyMode !=
					EffectParameterApplyMode::Unavailable) {
				++restartChangeCount;
			}
		}
	}

	const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
	const float viewportMargin = 16.0f * _dpiScale;
	const ImVec2 maxWindowSize{
		std::max(160.0f * _dpiScale, displaySize.x - viewportMargin),
		std::max(120.0f * _dpiScale, displaySize.y - viewportMargin)
	};
	const ImVec2 minWindowSize{
		std::min(EFFECT_PARAMETERS_MIN_WIDTH * _dpiScale, maxWindowSize.x),
		std::min(EFFECT_PARAMETERS_MIN_HEIGHT * _dpiScale, maxWindowSize.y)
	};
	ImGui::SetNextWindowSizeConstraints(minWindowSize, maxWindowSize);
	if (!_effectParametersWindowSizeInitialized) {
		ImGui::SetNextWindowSize({
			std::clamp(EFFECT_PARAMETERS_DEFAULT_WIDTH * _dpiScale,
				minWindowSize.x, maxWindowSize.x),
			std::clamp(EFFECT_PARAMETERS_DEFAULT_HEIGHT * _dpiScale,
				minWindowSize.y, maxWindowSize.y)
		});
		_effectParametersWindowSizeInitialized = true;
	}

	const std::string title = StrHelper::Concat(
		_GetResourceString(L"Overlay_EffectParameters"),
		"##", EFFECT_PARAMETERS_WINDOW_ID);
	ImGui::PushStyleColor(
		ImGuiCol_ResizeGrip, ImVec4(0.55f, 0.55f, 0.55f, 0.28f));
	ImGui::PushStyleColor(
		ImGuiCol_ResizeGripHovered, ImVec4(0.35f, 0.67f, 0.95f, 0.72f));
	ImGui::PushStyleColor(
		ImGuiCol_ResizeGripActive, ImVec4(0.35f, 0.67f, 0.95f, 1.0f));
	if (!ImGui::Begin(title.c_str(), &_isEffectParametersVisible)) {
		ImGui::End();
		ImGui::PopStyleColor(3);
		_parameterResetGesture.Clear();
		return false;
	}

	if (!renderer.MotionConfigurationNotice().empty()) {
		ImGui::TextWrapped("%s", StrHelper::UTF16ToUTF8(renderer.MotionConfigurationNotice()).c_str());
		ImGui::Separator();
	}

	const ImGuiStyle& style = ImGui::GetStyle();
	const float actionHeight = ImGui::GetFrameHeight() + style.ItemSpacing.y * 2.0f;
	const float statusHeight = ImGui::GetTextLineHeight() + style.ItemSpacing.y;
	const float footerHeight =
		actionHeight + statusHeight + style.ItemSpacing.y * 3.0f;
	const float contentHeight = std::max(
		100.0f * _dpiScale, ImGui::GetContentRegionAvail().y - footerHeight);
	ImGui::BeginChild(
		"##effectParametersContent", ImVec2(0.0f, contentHeight),
		ImGuiChildFlags_None, ImGuiWindowFlags_AlwaysVerticalScrollbar);

	bool needRedraw = false;
	bool queueFailure = false;
	bool parameterEdited = false;
	bool requestRestart = false;
	ImGui::PushID("frameSync");
	ImGui::SeparatorText(_GetResourceString(L"Overlay_FrameSync_Title").c_str());
	if (ImGui::Checkbox("Front Edge Sync", &_draftFrameSync.enabled)) {
		parameterEdited = needRedraw = true;
	}
	ImGui::SameLine();
	ImGui::TextDisabled("%s", _GetResourceString(L"Overlay_EffectParameters_RestartRequired").c_str());
	ImGui::TextUnformatted(_GetResourceString(L"Overlay_FrameSync_Target").c_str());
	ImGui::SameLine();
	ImGui::TextDisabled("%s", _GetResourceString(L"Overlay_EffectParameters_RestartRequired").c_str());
	ImGui::SetNextItemWidth(-1.0f);
	if (ImGui::InputFloat("##targetFps", &_draftFrameSync.frameRate, 1.0f, 10.0f, "%.3f")) {
		_draftFrameSync.frameRate = SanitizePresentationFrameRate(_draftFrameSync.frameRate);
		parameterEdited = needRedraw = true;
	}
	ImGui::TextWrapped("%s", _GetResourceString(L"Overlay_FrameSync_Help").c_str());
	ImGui::PopID();
	auto getDraftValue = [&](size_t effectIdx, const EffectDesc& description,
		std::string_view name, float fallback) noexcept {
		for (size_t i = 0; i < description.params.size(); ++i) {
			if (description.params[i].name == name &&
				effectIdx < _draftEffectParameterValues.size() &&
				i < _draftEffectParameterValues[effectIdx].size()) {
				return _draftEffectParameterValues[effectIdx][i];
			}
		}
		return fallback;
	};
	auto isSessionLive = [&](size_t effectIdx, size_t parameterIdx) noexcept {
		if (effectIdx >= runtimeInfos.size() ||
			parameterIdx >= runtimeInfos[effectIdx].size() ||
			runtimeInfos[effectIdx][parameterIdx].applyMode !=
				EffectParameterApplyMode::Live) {
			return false;
		}
		return true;
	};

	for (size_t effectIdx = 0; effectIdx < configuredEffectCount; ++effectIdx) {
		const EffectDesc& description = *descriptions[effectIdx];
		if (description.params.empty()) {
			continue;
		}
		ImGui::PushID(itemId++);
		ImGui::SeparatorText(
			std::string(GetEffectDisplayName(description)).c_str());

		std::string_view currentGroup;
		uint32_t validCount = 0;
		uint32_t invalidCount = 0;
		for (size_t parameterIdx = 0;
			parameterIdx < description.params.size(); ++parameterIdx) {
			const EffectParameterDesc& parameter =
				_localizedEffectParameters[effectIdx][parameterIdx];
			const std::string& effectName =
				ScalingWindow::Get().Options().effects[effectIdx].name;
			if (!IsEffectParameterVisible(effectName, parameter.name,
				[&](std::string_view name, float fallback) { return getDraftValue(effectIdx, description, name, fallback); })) continue;
			if (parameter.group != currentGroup) {
				currentGroup = parameter.group;
				if (!currentGroup.empty()) {
					ImGui::Spacing();
					ImGui::SeparatorText(std::string(currentGroup).c_str());
				}
			}

			const EffectParameterRuntimeInfo* info =
				effectIdx < runtimeInfos.size() &&
				parameterIdx < runtimeInfos[effectIdx].size()
					? &runtimeInfos[effectIdx][parameterIdx] : nullptr;
			const bool hasValueStorage =
				effectIdx < _draftEffectParameterValues.size() &&
				parameterIdx < _draftEffectParameterValues[effectIdx].size();
			const bool backendUnavailable = info &&
				info->applyMode == EffectParameterApplyMode::Unavailable;
			bool parameterValid = !backendUnavailable && hasValueStorage && info &&
				info->name == parameter.name &&
				IsEffectParameterDescriptorValid(parameter);
			float placeholderValue = parameter.constant.index() == 0
				? std::get<0>(parameter.constant).defaultValue
				: static_cast<float>(std::get<1>(parameter.constant).defaultValue);
			if (!std::isfinite(placeholderValue)) {
				placeholderValue = 0.0f;
			}
			const bool frontEdgeSyncEnabled = ScalingWindow::Get().Options().isFrontEdgeSyncEnabled;
			const bool parameterEnabled = IsEffectParameterEnabled(effectName, parameter.name,
				frontEdgeSyncEnabled, [&](std::string_view name, float fallback) {
					return getDraftValue(effectIdx, description, name, fallback);
				});
			// Show the effective mode without overwriting the saved custom preference.
			const bool forcedFollowMode = IsFrameRateFilterEffect(effectName) &&
				frontEdgeSyncEnabled && parameter.name == "frameRateMode";
			float forcedValue = 0.0f;
			float& value = forcedFollowMode ? forcedValue : hasValueStorage
				? _draftEffectParameterValues[effectIdx][parameterIdx]
				: placeholderValue;
			const bool isBoolean = IsBooleanEffectParameter(parameter);
			const bool isChoice = IsChoiceEffectParameter(parameter);
			int currentTick = 0;
			int maximumTick = 0;
			if (!isBoolean && !isChoice && parameterValid && !GetEffectParameterTicks(
				parameter, value, currentTick, maximumTick)) {
				parameterValid = false;
			}
			const bool isLive = parameterValid &&
				isSessionLive(effectIdx, parameterIdx);
			std::string badge = backendUnavailable ?
				_GetResourceString(L"Overlay_EffectParameters_Unavailable") : parameterValid
				? _GetResourceString(info->automaticRestart ? L"Overlay_EffectParameters_AutoRestart" : isLive
					? L"Overlay_EffectParameters_Live"
					: L"Overlay_EffectParameters_RestartRequired")
				: "[!]";
			if (parameterValid && !forcedFollowMode && !ParameterValuesEqual(value, _appliedEffectParameterValues[effectIdx][parameterIdx])) {
				const float applied = _appliedEffectParameterValues[effectIdx][parameterIdx];
				std::string actual = fmt::format("{:.7g}", applied);
				if (isChoice) {
					const auto choice = std::ranges::find(parameter.choices, int(std::lround(applied)), &EffectParameterChoice::value);
					if (choice != parameter.choices.end()) actual = choice->label;
				}
				badge += " · " + fmt::format(fmt::runtime(_GetResourceString(
					L"Overlay_EffectParameters_NotApplied")), actual);
			}

			const std::string& label =
				parameter.label.empty() ? parameter.name : parameter.label;
			if (parameterValid) ++validCount;
			else if (!backendUnavailable) ++invalidCount;

			ImGui::PushID(itemId++);
			ImGui::BeginDisabled(!parameterValid || !parameterEnabled);
			bool changed = false;
			bool parameterHovered = false;
			if (isBoolean) {
				bool boolValue = std::lround(value) != 0;
				changed = ImGui::Checkbox("##value", &boolValue);
				parameterHovered |= ImGui::IsItemHovered(
					ImGuiHoveredFlags_AllowWhenDisabled);
				ImGui::SameLine();
				ImGui::TextWrapped("%s  %s", label.c_str(), badge.c_str());
				parameterHovered |= ImGui::IsItemHovered(
					ImGuiHoveredFlags_AllowWhenDisabled);
				if (changed) {
					value = boolValue ? 1.0f : 0.0f;
				}
			} else if (isChoice) {
				ImGui::TextWrapped("%s  %s", label.c_str(), badge.c_str());
				parameterHovered |= ImGui::IsItemHovered(
					ImGuiHoveredFlags_AllowWhenDisabled);
				ImGui::SetNextItemWidth(-FLT_MIN);
				const int selectedValue = static_cast<int>(std::lround(
					NormalizeEffectParameterValue(parameter, value)));
				const auto selected = std::ranges::find(
					parameter.choices, selectedValue,
					&EffectParameterChoice::value);
				const char* preview = selected == parameter.choices.end()
					? "--" : selected->label.c_str();
				if (ImGui::BeginCombo("##value", preview)) {
					for (const EffectParameterChoice& choice : parameter.choices) {
						const bool isSelected = choice.value == selectedValue;
						if (ImGui::Selectable(choice.label.c_str(), isSelected)) {
							value = static_cast<float>(choice.value);
							changed = true;
						}
						if (isSelected) ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}
				parameterHovered |= ImGui::IsItemHovered(
					ImGuiHoveredFlags_AllowWhenDisabled);
			} else {
				ImGui::TextWrapped("%s  %s", label.c_str(), badge.c_str());
				parameterHovered |= ImGui::IsItemHovered(
					ImGuiHoveredFlags_AllowWhenDisabled);
				ImGui::SetNextItemWidth(-FLT_MIN);
				if (parameterValid) {
					const std::string displayValue =
						parameter.constant.index() == 0
							? fmt::format("{:.{}f}", value,
								GetEffectParameterDisplayPrecision(parameter))
							: fmt::format("{}",
								static_cast<int>(std::lround(value)));
					changed = ImGui::SliderInt(
						"##value", &currentTick, 0, maximumTick,
						displayValue.c_str(), ImGuiSliderFlags_AlwaysClamp);
					const float previousValue = value;
					if (changed) {
						value = GetEffectParameterValueFromTick(
							parameter, currentTick, maximumTick);
					}
					if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
						!_imguiImpl.LeftPressHasControl() && !resetInput.KeyCtrl &&
						!ImGui::TempInputIsActive(ImGui::GetItemID())) {
						resetPressClaimed = true;
						const uint64_t control = (uint64_t(effectIdx) << 32) | (parameterIdx + 1);
						if (_parameterResetGesture.Press(control, _imguiImpl.LeftPressTimeUs(),
							resetInput.MousePos.x, resetInput.MousePos.y, 4.0f * _dpiScale)) {
							value = NormalizeEffectParameterValue(parameter, placeholderValue);
							changed = !ParameterValuesEqual(previousValue, value);
							ImGui::ClearActiveID();
						}
					}
				} else {
					float unavailableValue = 0.0f;
					ImGui::SliderFloat(
						"##value", &unavailableValue, 0.0f, 1.0f, "--");
				}
				parameterHovered |= ImGui::IsItemHovered(
					ImGuiHoveredFlags_AllowWhenDisabled);
			}
			ImGui::EndDisabled();

			if (changed && parameterValid && parameterEnabled) {
				parameterEdited = true;
				value = NormalizeEffectParameterValue(parameter, value);
				if (isLive && !renderer.QueueEffectParameterUpdate(
					static_cast<uint32_t>(effectIdx),
					static_cast<uint32_t>(parameterIdx), value)) {
					queueFailure = true;
				}
				needRedraw = true;
			}

			if (parameterHovered) {
				std::string resetHint;
				if (parameterValid && parameterEnabled && !isBoolean && !isChoice) {
					resetHint = fmt::format(fmt::runtime(_GetResourceString(
						L"Overlay_EffectParameters_ResetDefault")),
						fmt::format("{:.7g}", placeholderValue));
				}
				if (!parameterValid) {
					_imguiImpl.Tooltip(_GetResourceString(
						backendUnavailable ? L"Overlay_EffectParameters_BackendUnavailable" :
						L"Overlay_EffectParameters_Invalid").c_str(), _dpiScale);
				} else if (info->automaticRestart) {
					_imguiImpl.Tooltip(_GetResourceString(L"Overlay_EffectParameters_Reason_AutoRestart").c_str(),
						_dpiScale, resetHint.empty() ? nullptr : resetHint.c_str());
				} else if (!isLive) {
					std::wstring_view reasonKey =
						L"Overlay_EffectParameters_Reason_Native";
					if (info->applyMode == EffectParameterApplyMode::Live) {
						reasonKey =
							L"Overlay_EffectParameters_Reason_Resources";
					} else {
						switch (info->restartReason) {
						case EffectParameterRestartReason::InlineParameters:
							reasonKey =
								L"Overlay_EffectParameters_Reason_Inline";
							break;
						case EffectParameterRestartReason::ResourceRecreation:
							reasonKey =
								L"Overlay_EffectParameters_Reason_Resources";
							break;
						case EffectParameterRestartReason::FrameGuidance:
							reasonKey =
								L"Overlay_EffectParameters_Reason_Guidance";
							break;
						case EffectParameterRestartReason::FrameGeneration:
							reasonKey =
								L"Overlay_EffectParameters_Reason_FrameGeneration";
							break;
						default:
							break;
						}
					}
					_imguiImpl.Tooltip(
						_GetResourceString(reasonKey).c_str(), _dpiScale,
						resetHint.empty() ? nullptr : resetHint.c_str());
				} else if (!resetHint.empty()) {
					_imguiImpl.Tooltip(resetHint.c_str(), _dpiScale);
				}
			}
			ImGui::PopID();
		}

#ifdef _DEBUG
		if (invalidCount > 0) {
			Logger::Get().Warn(fmt::format(
				"Overlay parameter metadata mismatch: effect#{} ({}) "
				"declared={} valid={} invalid={}",
				effectIdx, description.name, description.params.size(),
				validCount, invalidCount));
		}
#endif
		ImGui::PopID();
	}
	ImGui::EndChild();
	if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !resetPressClaimed) {
		_parameterResetGesture.Clear();
	}

	{
		ImGui::Separator();
		if (ImGui::Button(
			_GetResourceString(L"Overlay_EffectParameters_Revert").c_str())) {
			parameterEdited = true;
			_draftEffectParameterValues = _startupEffectParameterValues;
			_draftFrameSync = _startupFrameSync;
			for (size_t effectIdx = 0;
				effectIdx < configuredEffectCount; ++effectIdx) {
				for (size_t parameterIdx = 0;
					parameterIdx <
						_draftEffectParameterValues[effectIdx].size();
					++parameterIdx) {
					if (isSessionLive(effectIdx, parameterIdx) &&
						!renderer.QueueEffectParameterUpdate(
							static_cast<uint32_t>(effectIdx),
							static_cast<uint32_t>(parameterIdx),
							_draftEffectParameterValues[effectIdx][parameterIdx])) {
						queueFailure = true;
					}
				}
			}
			needRedraw = true;
		}
		ImGui::SameLine();
		const bool saveFailed = (_effectParametersSaveState->result.load(std::memory_order_acquire) & 7) != 0;
		ImGui::BeginDisabled((restartChangeCount == 0 && !saveFailed) ||
			!ScalingWindow::Get().Options().requestEffectParameters);
		if (ImGui::Button(_GetResourceString(
			L"Overlay_EffectParameters_ApplyAndRestart").c_str())) {
			requestRestart = true;
			needRedraw = true;
		}
		ImGui::EndDisabled();
	}
	if (queueFailure) {
		ScalingWindow::Get().ShowToast(ScalingWindow::Get().GetLocalizedString(
			L"Overlay_EffectParameters_LiveApplyFailed"));
	}

	if (parameterEdited || requestRestart) {
		_RequestEffectParameters(requestRestart ? EffectParametersRequestKind::SaveAndRestart
			: EffectParametersRequestKind::AutoSave);
	}
	// Recount after this frame's edits (including Revert), keeping persistence
	// separate from the values that have actually reached the backend.
	restartChangeCount = frameSyncChangeCount();
	for (size_t i = 0; i < _draftEffectParameterValues.size(); ++i) {
		for (size_t j = 0; j < _draftEffectParameterValues[i].size(); ++j) {
			if (i < runtimeInfos.size() && j < runtimeInfos[i].size() &&
				runtimeInfos[i][j].applyMode != EffectParameterApplyMode::Unavailable && !ParameterValuesEqual(
				_draftEffectParameterValues[i][j], _appliedEffectParameterValues[i][j])) {
				++restartChangeCount;
			}
		}
	}
	_lastEffectParametersSaveResult =
		_effectParametersSaveState->result.load(std::memory_order_acquire);
	const uint64_t completedRevision = _lastEffectParametersSaveResult >> 3;
	const auto error = static_cast<EffectParametersSaveError>(_lastEffectParametersSaveResult & 7);
	std::string status;
	if (completedRevision < _effectParametersRevision) {
		status = _GetResourceString(L"Overlay_EffectParameters_RequestPending");
	} else if (error != EffectParametersSaveError::None) {
		std::wstring_view key = L"Overlay_EffectParameters_SaveFailed";
		if (error == EffectParametersSaveError::Conflict) key = L"Overlay_EffectParameters_ScalingModeConflict";
		else if (error == EffectParametersSaveError::SessionExpired) key = L"Overlay_EffectParameters_SessionExpired";
		else if (error == EffectParametersSaveError::SourceUnavailable) key = L"Overlay_EffectParameters_SourceUnavailable";
		status = _GetResourceString(key);
	} else if (_effectParametersRevision) {
		status = _GetResourceString(L"Overlay_EffectParameters_AutoSaved");
	}
	if (restartChangeCount > 0) {
		if (!status.empty()) status += "  ";
		status += fmt::format(fmt::runtime(_GetResourceString(
			L"Overlay_EffectParameters_ApplyPending")), restartChangeCount);
	}

	ImGui::Separator();
	ImGui::BeginChild(
		"##effectParametersStatus",
		ImVec2(-ImGui::GetFrameHeight(), statusHeight),
		ImGuiChildFlags_None,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	ImGui::TextDisabled("%s", status.c_str());
	if (ImGui::IsItemHovered() &&
		ImGui::CalcTextSize(status.c_str()).x >
		ImGui::GetContentRegionAvail().x) {
		_imguiImpl.Tooltip(
			status.c_str(), _dpiScale, nullptr, 480.0f * _dpiScale);
	}
	ImGui::EndChild();

	ImGui::End();
	ImGui::PopStyleColor(3);
	return needRedraw;
}

bool OverlayDrawer::_DrawProfiler(const SmallVector<float>& effectTimings, uint32_t fps, int& itemId) noexcept {
	_lastProfilerDrawTime = steady_clock::now();
	const ScalingOptions& options = ScalingWindow::Get().Options();
	const Renderer& renderer = ScalingWindow::Get().Renderer();

	const uint32_t passCount = (uint32_t)_effectTimingsStatistics.size();

	bool needRedraw = false;

	// Samples arrive asynchronously and are consumed once by the visible UI.
	// Ignore a stale chain's sample while effect descriptors are being updated.
	if (!effectTimings.empty() && effectTimings.size() == passCount) {
		steady_clock::time_point now = steady_clock::now();
		if (_lastUpdateTime == steady_clock::time_point{}) {
			// 后端渲染的第一帧
			_lastUpdateTime = now;

			for (uint32_t i = 0; i < passCount; ++i) {
				_lastestAvgEffectTimings[i] = effectTimings[i];
			}
		} else {
			if (now - _lastUpdateTime > 500ms) {
				// 更新间隔不少于 500ms，而不是 500ms 更新一次
				_lastUpdateTime = now;

				for (uint32_t i = 0; i < passCount; ++i) {
					auto& [total, count] = _effectTimingsStatistics[i];
					if (count > 0) {
						_lastestAvgEffectTimings[i] = total / count;
					}

					count = 0;
					total = 0;
				}
			}

			for (uint32_t i = 0; i < passCount; ++i) {
				auto& [total, count] = _effectTimingsStatistics[i];
				// 有时会跳过某些效果的渲染，即渲染时间为 0，这时不应计入
				if (effectTimings[i] > 1e-3) {
					++count;
					total += effectTimings[i];
				}
			}
		}
	}

	{
		const float windowWidth = 310 * _dpiScale;
		ImGui::SetNextWindowSizeConstraints(ImVec2(windowWidth, 0.0f), ImVec2(windowWidth, 500 * _dpiScale));
	}

	std::string profilerStr =
		StrHelper::Concat(_GetResourceString(L"Overlay_Profiler"), "##", PROFILER_WINDOW_ID);
	if (!ImGui::Begin(profilerStr.c_str(), &_isProfilerVisible, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::End();
		return needRedraw;
	}

	ImGui::PushTextWrapPos();
	ImGui::TextUnformatted(StrHelper::Concat("GPU: ", _hardwareInfo.gpuName).c_str());
	const std::string& captureMethodStr = _GetResourceString(L"Overlay_Profiler_CaptureMethod");
	ImGui::TextUnformatted(StrHelper::Concat(captureMethodStr.c_str(), ": ", renderer.FrameSource().Name()).c_str());
	if (options.IsStatisticsForDynamicDetectionEnabled() &&
		options.duplicateFrameDetectionMode == DuplicateFrameDetectionMode::Dynamic) {
		const std::pair<uint32_t, uint32_t> statistics =
			renderer.FrameSource().GetStatisticsForDynamicDetection();
		ImGui::TextUnformatted(StrHelper::Concat(_GetResourceString(L"Overlay_Profiler_DynamicDetection"), ": ").c_str());
		ImGui::SameLine(0, 0);
		ImGui::PushFont(_fontMonoNumbers);
		ImGui::TextUnformatted(fmt::format("{}/{} ({:.1f}%)", statistics.first, statistics.second,
			statistics.second == 0 ? 0.0f : statistics.first * 100.0f / statistics.second).c_str());
		ImGui::PopFont();
	}
	const std::string& frameRateStr = _GetResourceString(ScalingWindow::Get().Renderer().HasFrameGeneration() ?
		L"Overlay_Profiler_FrameRateWithFG" : L"Overlay_Profiler_FrameRate");
	ImGui::TextUnformatted(fmt::format("{}: {}", frameRateStr, _FormatFrameRate(fps)).c_str());
	ImGui::PopTextWrapPos();

	const std::vector<const EffectDesc*>& effectDescs = renderer.ActiveEffectDescs();
	const uint32_t nEffect = (uint32_t)effectDescs.size();

	SmallVector<_EffectDrawInfo, 4> effectDrawInfos(effectDescs.size());

	{
		uint32_t idx = 0;
		for (uint32_t i = 0; i < nEffect; ++i) {
			_EffectDrawInfo& drawInfo = effectDrawInfos[i];
			drawInfo.desc = effectDescs[i];

			uint32_t nPass = (uint32_t)drawInfo.desc->passes.size();
			drawInfo.passTimings = { _lastestAvgEffectTimings.begin() + idx, nPass };
			idx += nPass;

			for (float t : drawInfo.passTimings) {
				drawInfo.totalTime += t;
			}
		}
	}

	static bool showPasses = false;

	// 开发者选项
	if (options.IsDeveloperMode()) {
		ImGui::Spacing();
		const std::string& developerOptionsStr = _GetResourceString(L"Home_Advanced_DeveloperOptions/Header");
		if (ImGui::CollapsingHeader(developerOptionsStr.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
			bool showSwitchButton = false;
			for (const _EffectDrawInfo& drawInfo : effectDrawInfos) {
				// 某个效果有多个通道，显示切换按钮
				if (drawInfo.passTimings.size() > 1) {
					showSwitchButton = true;
					break;
				}
			}

			if (showSwitchButton) {
				const std::string& buttonStr = _GetResourceString(showPasses
					? L"Overlay_Profiler_Timings_SwitchToEffects"
					: L"Overlay_Profiler_Timings_SwitchToPasses");
				if (ImGui::Button(buttonStr.c_str())) {
					showPasses = !showPasses;
					// 需要再次渲染以处理滚动条导致的布局变化
					needRedraw = true;
				}
			} else {
				showPasses = false;
			}
		}
	} else {
		showPasses = false;
	}

#ifdef MP_DEBUG_INFO_ON_OVERLAY
	ImGui::Spacing();
	if (ImGui::CollapsingHeader("调试信息", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::TextUnformatted(StrHelper::Concat("源矩形: ",
			RectToStr(renderer.SrcRect())).c_str());
		ImGui::TextUnformatted(StrHelper::Concat("目标矩形: ",
			RectToStr(renderer.DestRect())).c_str());
		ImGui::TextUnformatted(StrHelper::Concat("渲染矩形: ",
			RectToStr(ScalingWindow::Get().RendererRect())).c_str());
		RECT scalingWndRect;
		GetWindowRect(ScalingWindow::Get().Handle(), &scalingWndRect);
		ImGui::TextUnformatted(StrHelper::Concat("缩放窗口矩形: ",
			RectToStr(scalingWndRect)).c_str());

		bool isTopMost = GetWindowExStyle(ScalingWindow::Get().Handle()) & WS_EX_TOPMOST;
		ImGui::TextUnformatted(
			StrHelper::Concat("缩放窗口置顶: ", isTopMost ? "是" : "否").c_str());

		ImGui::TextUnformatted(StrHelper::Concat("已捕获光标: ",
			ScalingWindow::Get().CursorManager().IsCursorCaptured() ? "是" : "否").c_str());

		RECT cursorClip;
		GetClipCursor(&cursorClip);
		ImGui::TextUnformatted(StrHelper::Concat("光标限制区域: ",
			RectToStr(cursorClip)).c_str());
	}
#endif

	ImGui::Spacing();
	// 效果渲染用时
	const std::string& timingsStr = _GetResourceString(L"Overlay_Profiler_Timings");
	if (ImGui::CollapsingHeader(timingsStr.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
		float effectsTotalTime = 0.0f;
		for (const _EffectDrawInfo& drawInfo : effectDrawInfos) {
			effectsTotalTime += drawInfo.totalTime;
		}

		SmallVector<ImColor, 4> colors;
		colors.reserve(_timelineColors.size());
		if (nEffect == 1) {
			colors.resize(_timelineColors.size());
			for (size_t i = 0; i < _timelineColors.size(); ++i) {
				colors[i] = OverlayHelper::TIMELINE_COLORS[_timelineColors[i]];
			}
		} else if (showPasses) {
			uint32_t i = 0;
			for (const _EffectDrawInfo& drawInfo : effectDrawInfos) {
				if (drawInfo.passTimings.size() == 1) {
					colors.push_back(OverlayHelper::TIMELINE_COLORS[_timelineColors[i]]);
					++i;
					continue;
				}

				++i;
				for (uint32_t j = 0; j < drawInfo.passTimings.size(); ++j) {
					colors.push_back(OverlayHelper::TIMELINE_COLORS[_timelineColors[i]]);
					++i;
				}
			}
		} else {
			size_t i = 0;
			for (const _EffectDrawInfo& drawInfo : effectDrawInfos) {
				colors.push_back(OverlayHelper::TIMELINE_COLORS[_timelineColors[i]]);

				++i;
				if (drawInfo.passTimings.size() > 1) {
					i += drawInfo.passTimings.size();
				}
			}
		}

		static int selectedIdx = -1;

		if (nEffect > 1 || showPasses) {
			ImGui::Spacing();
			ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0, 0));
			ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.5f));
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(5, 5));
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

			if (effectsTotalTime > 0) {
				if (showPasses) {
					if (ImGui::BeginTable("timeline", (int)passCount)) {
						for (uint32_t i = 0; i < passCount; ++i) {
							if (_lastestAvgEffectTimings[i] < 1e-3f) {
								continue;
							}

							ImGui::TableSetupColumn(
								std::to_string(i).c_str(),
								ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_NoReorder,
								_lastestAvgEffectTimings[i] / effectsTotalTime
							);
						}

						ImGui::TableNextRow();

						uint32_t i = 0;
						for (const _EffectDrawInfo& drawInfo : effectDrawInfos) {
							for (uint32_t j = 0, end = (uint32_t)drawInfo.passTimings.size(); j < end; ++j) {
								if (drawInfo.passTimings[j] < FLOAT_EPSILON<float>) {
									continue;
								}

								ImGui::TableNextColumn();

								std::string name;
								if (drawInfo.passTimings.size() == 1) {
									name = std::string(GetEffectDisplayName(*drawInfo.desc));
								} else if (nEffect == 1) {
									name = drawInfo.desc->passes[j].desc;
								} else {
									name = StrHelper::Concat(
										GetEffectDisplayName(*drawInfo.desc), "/",
										drawInfo.desc->passes[j].desc
									);
								}

								_DrawTimelineItem(itemId, colors[i], _dpiScale, name, drawInfo.passTimings[j],
									effectsTotalTime, selectedIdx == (int)i);

								++i;
							}
						}

						ImGui::EndTable();
					}
				} else {
					if (ImGui::BeginTable("timeline", nEffect)) {
						for (uint32_t i = 0; i < nEffect; ++i) {
							if (effectDrawInfos[i].totalTime < FLOAT_EPSILON<float>) {
								continue;
							}

							ImGui::TableSetupColumn(
								std::to_string(i).c_str(),
								ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_NoReorder,
								effectDrawInfos[i].totalTime / effectsTotalTime
							);
						}

						ImGui::TableNextRow();

						for (uint32_t i = 0; i < nEffect; ++i) {
							auto& drawInfo = effectDrawInfos[i];
							if (drawInfo.totalTime < FLOAT_EPSILON<float>) {
								continue;
							}

							ImGui::TableNextColumn();
							_DrawTimelineItem(
								itemId,
								colors[i],
								_dpiScale,
								GetEffectDisplayName(*drawInfo.desc),
								drawInfo.totalTime,
								effectsTotalTime,
								selectedIdx == (int)i
							);
						}

						ImGui::EndTable();
					}
				}
			} else {
				// 还未统计出时间时渲染占位
				if (ImGui::BeginTable("timeline", 1)) {
					ImGui::TableSetupColumn("0", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_NoReorder);
					ImGui::TableNextRow();
					ImGui::TableNextColumn();

					ImU32 color = ImColor();
					ImGui::PushStyleColor(ImGuiCol_HeaderActive, color);
					ImGui::PushStyleColor(ImGuiCol_HeaderHovered, color);
					ImGui::Selectable("");
					ImGui::PopStyleColor(2);

					ImGui::EndTable();
				}
			}

			ImGui::PopStyleVar(4);

			ImGui::Spacing();
		}

		selectedIdx = -1;

		if (ImGui::BeginTable("timings", 1, ImGuiTableFlags_PadOuterX)) {
			ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_NoReorder);

			if (nEffect == 1) {
				int hovered = _DrawEffectTimings(itemId, effectDrawInfos[0], showPasses, colors, true);
				if (hovered >= 0) {
					selectedIdx = hovered;
				}
			} else {
				int idx = 0;
				for (const _EffectDrawInfo& effectDesc : effectDrawInfos) {
					int idxBegin = idx;

					std::span<const ImColor> colorSpan;
					if (!showPasses || effectDesc.passTimings.size() == 1) {
						colorSpan = std::span(colors.begin() + idx, colors.begin() + idx + 1);
						++idx;
					} else {
						colorSpan = std::span(colors.begin() + idx, colors.begin() + idx + effectDesc.passTimings.size());
						idx += (int)effectDesc.passTimings.size();
					}

					int hovered = _DrawEffectTimings(itemId, effectDesc, showPasses, colorSpan, false);
					if (hovered >= 0) {
						selectedIdx = idxBegin + hovered;
					}
				}
			}

			ImGui::EndTable();
		}

		if (nEffect > 1) {
			ImGui::Separator();

			if (ImGui::BeginTable("total", 1, ImGuiTableFlags_PadOuterX)) {
				ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_NoReorder);

				_DrawTimingItem(itemId, _GetResourceString(L"Overlay_Profiler_Timings_Total").c_str(), nullptr, effectsTotalTime);

				ImGui::EndTable();
			}
		}
	}

	ImGui::End();
	return needRedraw;
}

const std::string& OverlayDrawer::_GetResourceString(const std::wstring_view& key) noexcept {
	static phmap::flat_hash_map<std::wstring_view, std::string> cache;

	if (auto it = cache.find(key); it != cache.end()) {
		return it->second;
	}

	return cache[key] = StrHelper::UTF16ToUTF8(ScalingWindow::Get().GetLocalizedString(key));
}

float OverlayDrawer::_CalcToolbarAlpha() const noexcept {
	if (ScalingWindow::Get().IsResizingOrMoving()) {
		// 调整缩放窗口大小时不能按需渲染，所以不要改变工具栏透明度
		return _lastToolbarAlpha;
	}

	// 鼠标被工具栏中的按钮捕获时不要隐藏工具栏
	if (_isToolbarPinned || _isToolbarItemActive) {
		return 1.0f;
	}

	std::optional<ImVec4> windowRect = _imguiImpl.GetWindowRect(TOOLBAR_WINDOW_ID);
	if (!windowRect) {
		return 0.0f;
	}

	// 为了裁掉圆角，顶部有一部分在屏幕外
	windowRect->y = 0.0f;

	// ImGui::GetIO().MousePos 在调整缩放窗口大小或鼠标被前台窗口捕获时不是真实位置，这里应重新计算
	const POINT cursorPos = ScalingWindow::Get().CursorManager().CursorPos();
	const RECT& destRect = ScalingWindow::Get().Renderer().DestRect();
	const float cursorX = float(cursorPos.x - destRect.left);
	const float cursorY = float(cursorPos.y - destRect.top);

	// 计算离边或角最短的距离
	float dist = 0;
	if (cursorX < windowRect->x) {
		if (cursorY < windowRect->y) {
			dist = std::hypot(windowRect->x - cursorX, windowRect->y - cursorY);
		} else if (cursorY > windowRect->w) {
			dist = std::hypot(windowRect->x - cursorX, cursorY - windowRect->w);
		} else {
			dist = windowRect->x - cursorX;
		}
	} else if (cursorX > windowRect->z) {
		if (cursorY < windowRect->y) {
			dist = std::hypot(cursorX - windowRect->z, windowRect->y - cursorY);
		} else if (cursorY > windowRect->w) {
			dist = std::hypot(cursorX - windowRect->z, cursorY - windowRect->w);
		} else {
			dist = cursorX - windowRect->z;
		}
	} else {
		if (cursorY < windowRect->y) {
			dist = windowRect->y - cursorY;
		} else if (cursorY > windowRect->w) {
			dist = cursorY - windowRect->w;
		} else {
			dist = 0;
		}
	}
	dist /= _dpiScale;

	return (40.0f - std::clamp(dist - 10.0f, 0.0f, 40.0f)) / 40.0f;
}

void OverlayDrawer::_ClearStatesIfNoVisibleWindow() noexcept {
	if (AnyVisibleWindow()) {
		return;
	}

	ClearStates();
}

}
