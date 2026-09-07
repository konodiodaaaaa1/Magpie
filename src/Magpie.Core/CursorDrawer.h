#pragma once
#include <parallel_hashmap/phmap.h>

namespace Magpie {

class DeviceResources;

class CursorDrawer {
public:
	CursorDrawer() noexcept = default;
	CursorDrawer(const CursorDrawer&) = delete;
	CursorDrawer(CursorDrawer&&) = delete;

	bool Initialize(DeviceResources& deviceResources) noexcept;

	// sceneTexture is the destination-sized scene underneath an independent UI surface.
	void Draw(ID3D11Texture2D* backBuffer, POINT drawOffset,
		ID3D11Texture2D* sceneTexture = nullptr) noexcept;

	bool IsBackgroundDependent() const noexcept { return _isBackgroundDependent; }

	void IsCursorVisible(bool value) noexcept {
		_isCursorVisible = value;
	}

	bool IsCursorVisible() const noexcept {
		return _isCursorVisible;
	}

	bool NeedRedraw() const noexcept;

private:
	std::pair<HCURSOR, POINT> _GetCursorState(bool& isActive) const noexcept;

	enum class _CursorType {
		// 彩色光标：RGB 已预乘 A，A 为标准覆盖率（0 透明，1 不透明），支持双线性插值。
		// FinalColor = CursorColor + ScreenColor * (1 - CursorColor.a)
		// FinalAlpha = CursorColor.a + ScreenAlpha * (1 - CursorColor.a)
		// 纹理格式: DXGI_FORMAT_R8G8B8A8_UNORM
		Color = 0,
		// 彩色掩码光标，此时 A 通道可能为 0 或 255
		// 为 0 时表示 RGB 通道取代屏幕颜色，为 255 时表示 RGB 通道和屏幕颜色进行异或操作
		// 纹理格式: DXGI_FORMAT_R8G8B8A8_UNORM
		MaskedColor,
		// 单色光标，此时 R 通道为 AND 掩码，G 通道为 XOR 掩码，其他通道不使用
		// RG 通道的值只能是 0 或 255
		// 纹理格式: DXGI_FORMAT_R8G8_UNORM
		Monochrome
	};

	struct _CursorInfo {
		POINT hotSpot{};
		SIZE size{};
		winrt::com_ptr<ID3D11ShaderResourceView> textureSrv = nullptr;
		_CursorType type = _CursorType::Color;
	};

	const _CursorInfo* _ResolveCursor(HCURSOR hCursor) noexcept;

	bool _SetPremultipliedAlphaBlend() noexcept;

	DeviceResources* _deviceResources = nullptr;

	phmap::flat_hash_map<HCURSOR, _CursorInfo> _cursorInfos;

	winrt::com_ptr<ID3D11VertexShader> _simpleVS;
	winrt::com_ptr<ID3D11InputLayout> _simpleIL;
	winrt::com_ptr<ID3D11Buffer> _vtxBuffer;
	winrt::com_ptr<ID3D11PixelShader> _simplePS;
	winrt::com_ptr<ID3D11BlendState> premultipliedAlphaBlendBlendState;
	winrt::com_ptr<ID3D11PixelShader> _maskedCursorPS;
	winrt::com_ptr<ID3D11PixelShader> _monochromeCursorPS;

	// 用于渲染彩色掩码光标和单色光标的临时纹理
	winrt::com_ptr<ID3D11Texture2D> _tempCursorTexture;
	winrt::com_ptr<ID3D11ShaderResourceView> _tempCursorTextureRtv;
	SIZE _tempCursorTextureSize{};
	winrt::com_ptr<ID3D11Texture2D> _tempSceneTexture;
	winrt::com_ptr<ID3D11ShaderResourceView> _tempSceneSrv;
	bool _isBackgroundDependent = false;

	// 这两个成员用于检查自动隐藏光标
	HCURSOR _lastRawCursorHandle = NULL;
	std::chrono::steady_clock::time_point _lastCursorActiveTime;
	// 上次绘制的光标形状和位置
	HCURSOR _lastCursorHandle = NULL;
	POINT _lastCursorPos{ std::numeric_limits<LONG>::max(), std::numeric_limits<LONG>::max() };

	bool _isCursorVisible = true;
};

}
