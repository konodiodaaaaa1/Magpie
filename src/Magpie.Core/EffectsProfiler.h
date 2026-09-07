#pragma once
#include "SmallVector.h"
#include <array>
#include <chrono>

namespace Magpie {

class DeviceResources;

class EffectsProfiler {
public:
	EffectsProfiler() = default;

	EffectsProfiler(const EffectsProfiler&) = delete;
	EffectsProfiler(EffectsProfiler&&) = delete;

	void Start(ID3D11Device* d3dDevice, uint32_t passCount) noexcept;

	void Stop() noexcept;

	bool IsProfiling() const noexcept;

	void SetPassCount(ID3D11Device* d3dDevice, uint32_t passCount) noexcept;

	void OnBeginEffects(ID3D11DeviceContext* d3dDC) noexcept;

	void OnEndPass(ID3D11DeviceContext* d3dDC) noexcept;

	void OnEndEffects(ID3D11DeviceContext* d3dDC) noexcept;

	void QueryTimings(ID3D11DeviceContext* d3dDC) noexcept;

	// 从前端线程调用
	SmallVector<float> GetTimings() noexcept;

private:
	struct QuerySlot {
		winrt::com_ptr<ID3D11Query> disjoint;
		winrt::com_ptr<ID3D11Query> start;
		std::vector<winrt::com_ptr<ID3D11Query>> passes;
		std::chrono::steady_clock::time_point submitted{};
	};
	void _Disable(const char* operation, HRESULT hr) noexcept;
	bool _ReadQuery(ID3D11DeviceContext* d3dDC, ID3D11Query* query,
		void* data, UINT size, std::chrono::steady_clock::time_point submitted) noexcept;

	SmallVector<float> _timings;
	wil::srwlock _timingsLock;

	// Backend-owned FIFO. Busy slots skip samples instead of blocking rendering.
	std::array<QuerySlot, 3> _slots;
	uint32_t _passCount = 0;
	uint32_t _readSlot = 0;
	uint32_t _writeSlot = 0;
	uint32_t _pendingCount = 0;
	uint64_t _skippedSamples = 0;
	bool _recording = false;
	uint32_t _curPass = 0;
};

}
