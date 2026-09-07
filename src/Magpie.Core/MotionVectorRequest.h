#pragma once
#include <cstdint>

namespace Magpie {

enum class OpticalFlowMethod : uint8_t {
	None = 0,
	Amd = 1,
	Nvidia = 2
};

enum class NvidiaOpticalFlowQuality : uint8_t {
	None = 0,
	Performance = 1,
	Balanced = 2,
	Quality = 3,
	HighQuality = 4,
	HighestQuality = 5
};

inline constexpr uint8_t NVIDIA_OPTICAL_FLOW_MAX_QUALITY =
	static_cast<uint8_t>(NvidiaOpticalFlowQuality::HighestQuality);

enum class AmdOpticalFlowMode : uint8_t {
	Performance = 0,
	Quality = 1
};

enum class OpticalFlowInitializationError : uint8_t {
	None,
	ProviderUnavailable,
	QualityUnsupported,
	CapabilityUnsupported,
	InteropFailed
};

struct MotionVectorRequest {
	OpticalFlowMethod method = OpticalFlowMethod::None;
	uint8_t quality = 0;

	static MotionVectorRequest Nvidia(
		NvidiaOpticalFlowQuality quality
	) noexcept {
		return quality == NvidiaOpticalFlowQuality::None ? MotionVectorRequest{} :
			MotionVectorRequest{ OpticalFlowMethod::Nvidia,
				static_cast<uint8_t>(quality) };
	}

	static MotionVectorRequest Amd(AmdOpticalFlowMode mode) noexcept {
		return { OpticalFlowMethod::Amd, static_cast<uint8_t>(mode) };
	}

	bool operator==(const MotionVectorRequest&) const noexcept = default;
};

// Retain requested configurations for diagnostics, then select one shared
// provider: NVOF first, AMD second; highest REQUESTED quality.
struct FrameGuidanceRequirements {
 bool zero = false;
 uint8_t nvidiaMask = 0;
 uint8_t amdMask = 0;

 bool HasMotion() const noexcept { return nvidiaMask || amdMask; }
 bool Any() const noexcept { return zero || HasMotion(); }
 void Add(MotionVectorRequest request) noexcept {
  if (request.method == OpticalFlowMethod::Nvidia && request.quality >= 1 && request.quality <= NVIDIA_OPTICAL_FLOW_MAX_QUALITY)
   nvidiaMask |= uint8_t(1u << request.quality);
  else if (request.method == OpticalFlowMethod::Amd && request.quality <= 1)
   amdMask |= uint8_t(1u << request.quality);
 }
 void Merge(const FrameGuidanceRequirements& other) noexcept {
  zero |= other.zero;
  nvidiaMask |= other.nvidiaMask;
  amdMask |= other.amdMask;
 }
 bool Contains(MotionVectorRequest request) const noexcept {
  FrameGuidanceRequirements one;
  one.Add(request);
  return one.HasMotion() && (nvidiaMask & one.nvidiaMask) == one.nvidiaMask &&
   (amdMask & one.amdMask) == one.amdMask;
 }
 template<class F> void ForEachMotion(F&& visit) const {
  for (uint8_t q = 1; q <= NVIDIA_OPTICAL_FLOW_MAX_QUALITY; ++q)
   if (nvidiaMask & (1u << q)) visit(MotionVectorRequest::Nvidia(static_cast<NvidiaOpticalFlowQuality>(q)));
  for (uint8_t q = 0; q <= 1; ++q)
   if (amdMask & (1u << q)) visit(MotionVectorRequest::Amd(static_cast<AmdOpticalFlowMode>(q)));
 }
 MotionVectorRequest FirstMotion() const noexcept {
  MotionVectorRequest result;
  ForEachMotion([&](MotionVectorRequest request) {
   if (result.method == OpticalFlowMethod::None) result = request;
  });
  return result;
 }
 MotionVectorRequest PreferredMotion() const noexcept {
  for (int q = NVIDIA_OPTICAL_FLOW_MAX_QUALITY; q >= 1; --q)
   if (nvidiaMask & (1u << q)) return { OpticalFlowMethod::Nvidia, static_cast<uint8_t>(q) };
  for (int q = 1; q >= 0; --q)
   if (amdMask & (1u << q)) return { OpticalFlowMethod::Amd, static_cast<uint8_t>(q) };
  return {};
 }
 FrameGuidanceRequirements Resolved() const noexcept {
  FrameGuidanceRequirements result{ .zero = zero };
  result.Add(PreferredMotion());
  return result;
 }
 static MotionVectorRequest ResolveConsumer(MotionVectorRequest requested,
  MotionVectorRequest selected) noexcept {
  return requested.method == OpticalFlowMethod::None ? MotionVectorRequest{} : selected;
 }
 bool operator==(const FrameGuidanceRequirements&) const noexcept = default;
};

}
