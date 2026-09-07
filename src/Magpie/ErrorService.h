#pragma once
#include "Event.h"
#include "ScalingOptions.h"

namespace Magpie {

// Reports may originate on a renderer or file-writing thread. Only the UI
// dispatcher touches the stored presentation or invokes Changed.
class ErrorService {
public:
	static ErrorService& Get() noexcept { static ErrorService service; return service; }
	void Report(ScalingError error, std::string context = {}, HWND target = nullptr,
		uint32_t systemError = 0) noexcept;
	bool HasIssue() const noexcept { return !_summary.empty(); }
	bool IsIssueVisible() const noexcept { return _isIssueVisible; }
	void DismissIssue();
	const winrt::hstring& Summary() const noexcept { return _summary; }
	const winrt::hstring& Details() const noexcept { return _details; }
	Event<> Changed;
private:
	bool _isIssueVisible = false;
	winrt::hstring _summary, _details;
	ScalingError _lastError = ScalingError::NoError;
	std::string _lastContext;
	uint32_t _lastSystemError = 0;
	uint32_t _repeatCount = 0;
	std::chrono::steady_clock::time_point _lastToast{};
};

}
