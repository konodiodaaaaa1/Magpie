#pragma once

#include "HdrColorTransform.h"

#include <string>
#include <string_view>

namespace Magpie {

class BackendDescriptorStore;
class DeviceResources;

class HdrCaptureProcessor {
public:
	~HdrCaptureProcessor() noexcept;

	bool Initialize(DeviceResources& deviceResources, BackendDescriptorStore& descriptorStore) noexcept;
	bool Process(ID3D11Texture2D* source, const HdrFrameMetadata& metadata) noexcept;
	// Interface shape prepared for Renderer/FrameSourceBase integration. The
	// source color description may be invalid; the processor then substitutes a
	// neutral default and records the assumption in LastAssumption().
	bool Process(
		ID3D11Texture2D* sourceTexture,
		DXGI_FORMAT sourceFormat,
		const ColorDescription& sourceColorDescription
	) noexcept;
	bool Prepare(
		ID3D11Texture2D* sourceTexture,
		DXGI_FORMAT sourceFormat,
		const ColorDescription& sourceColorDescription
	) noexcept;
	void ResetForResize() noexcept;

	ID3D11Texture2D* GetCanonicalTexture() const noexcept { return _canonical.get(); }
	const HdrFrameMetadata& GetFrameMetadata() const noexcept { return _metadata; }
	std::string_view LastAssumption() const noexcept { return _lastAssumption; }

private:
	bool _EnsureResources(const D3D11_TEXTURE2D_DESC& sourceDesc) noexcept;

	DeviceResources* _deviceResources = nullptr;
	BackendDescriptorStore* _descriptorStore = nullptr;
	winrt::com_ptr<ID3D11Texture2D> _canonical;
	winrt::com_ptr<ID3D11ComputeShader> _shader;
	winrt::com_ptr<ID3D11Buffer> _constants;
	ID3D11ShaderResourceView* _sourceSrv = nullptr;
	ID3D11UnorderedAccessView* _canonicalUav = nullptr;
	HdrFrameMetadata _metadata{};
	std::string _lastAssumption;
};

}
