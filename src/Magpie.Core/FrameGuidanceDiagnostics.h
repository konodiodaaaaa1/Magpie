#pragma once
#include "NativeEffectBackend.h"

namespace Magpie {

enum class FrameGuidanceDiagnosticKind : uint8_t {
	Motion,
	Confidence
};

struct FrameGuidanceDiagnosticSettings {
	FrameGuidanceDiagnosticKind kind = FrameGuidanceDiagnosticKind::Motion;
	float gain = 0.08f;
};

class FrameGuidanceDiagnostics final : public NativeEffectBackend {
public:
	FrameGuidanceRequirements GetFrameGuidanceRequirements() const noexcept override;
	EffectParameterApplyMode GetParameterApplyMode(
		std::string_view parameterName
	) const noexcept override;
	EffectParameterRestartReason GetParameterRestartReason(
		std::string_view parameterName
	) const noexcept override;
	bool ApplyLiveParameters(
		const EffectOption& option,
		std::span<const std::string> parameterNames
	) noexcept override;
	bool Initialize(
		DeviceResources& resources,
		ID3D11Texture2D* input,
		ID3D11Texture2D* output,
		const FrameGuidanceDiagnosticSettings& settings
	) noexcept;
	bool Resize(
		DeviceResources& resources,
		ID3D11Texture2D* input,
		ID3D11Texture2D* output
	) noexcept override;
	bool Draw(const NativeEffectDrawContext& context) noexcept override;

private:
	ID3D11Device5* _device = nullptr;
	ID3D11DeviceContext4* _context = nullptr;
	FrameGuidanceDiagnosticSettings _settings;
	winrt::com_ptr<ID3D11ComputeShader> _shader;
	winrt::com_ptr<ID3D11Buffer> _params;
};

}
