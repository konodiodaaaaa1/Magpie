#pragma once

#include "EffectDesc.h"
#include "HdrFrame.h"
#include <string>
#include <vector>

namespace Magpie {

struct HdrSourceTestResult {
	std::string effectId;
	std::string routeId;
	DXGI_FORMAT inputFormat = DXGI_FORMAT_UNKNOWN;
	DXGI_FORMAT outputFormat = DXGI_FORMAT_UNKNOWN;
	uint32_t passCount = 0;
	uint32_t textureCount = 0;
	bool routeValid = false;
	bool canonicalInput = false;
	bool canonicalOutput = false;
};

// Source-level contract probe. It uses the production EffectCompiler and
// route providers; it does not emulate shader math or invoke a substitute SDK.
class HdrSourceTestBridge final {
public:
	static std::vector<HdrSourceTestResult> CompileAndDescribeAll(
		const std::vector<std::string>& effectNames,
		bool noFP16 = false
	) noexcept;
};

}
