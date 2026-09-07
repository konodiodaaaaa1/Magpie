#include "pch.h"
#include "DLSSFrameGenerator.h"
#include "DeviceResources.h"
#include "FrameGuidanceD3D12Interop.h"
#include "Logger.h"
#include "NgxD3D12Core.h"

#ifdef MP_ENABLE_DLSS_FRAME_GENERATION
#include <d3d12.h>
#include <nvsdk_ngx.h>
#include <nvsdk_ngx_helpers_dlssg.h>

namespace Magpie {

struct DLSSFrameGenerator::Impl {
	~Impl();

	ID3D11Device5* device11 = nullptr;
	ID3D11DeviceContext4* context11 = nullptr;
	NgxD3D12Core* coreOwner = nullptr;
	winrt::com_ptr<ID3D12Device> device12;
	winrt::com_ptr<ID3D12CommandQueue> queue12;
	winrt::com_ptr<ID3D12CommandAllocator> allocator12;
	winrt::com_ptr<ID3D12GraphicsCommandList> commandList12;
	winrt::com_ptr<ID3D11Texture2D> sharedInput11;
	winrt::com_ptr<ID3D11Texture2D> sharedGenerated11;
	winrt::com_ptr<ID3D12Resource> sharedInput12;
	winrt::com_ptr<ID3D12Resource> sharedGenerated12;
	winrt::com_ptr<ID3D12Resource> zeroMotion12;
	winrt::com_ptr<ID3D12Resource> zeroDepth12;
	std::array<winrt::com_ptr<ID3D12Resource>, 4> interpolationDisable12;
	std::array<winrt::com_ptr<ID3D12Resource>, 4> interpolationDisableReadback12;
	std::unique_ptr<FrameGuidanceD3D12Interop> guidanceInterop;
	winrt::com_ptr<ID3D12DescriptorHeap> descriptorHeap12;
	winrt::com_ptr<ID3D11Fence> fence11;
	winrt::com_ptr<ID3D12Fence> fence12;
	NVSDK_NGX_Handle* feature = nullptr;
	NVSDK_NGX_Parameter* parameters = nullptr;
	uint64_t fenceValue = 0;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t renderWidth = 0;
	uint32_t renderHeight = 0;
	uint32_t multiplier = 2;
	uint32_t maxSupportedMultiplier = 2;
	uint32_t diagnosticRealFrames = 0;
	std::array<uint32_t, 4> diagnosticEvaluateSuccess{};
	std::array<uint32_t, 4> diagnosticEvaluateFailure{};
	std::array<uint32_t, 4> diagnosticInterpolationEnabled{};
	std::array<uint32_t, 4> diagnosticInterpolationDisabled{};
	std::array<uint32_t, 4> diagnosticInterpolationReadbackFailure{};
	uint32_t diagnosticGeneratedPublishSuccess = 0;
	uint32_t diagnosticGeneratedPublishFailure = 0;
	DLSSFrameGenerationSettings settings{};
	FrameGuidanceFrameId lastGuidanceResetFrameId =
		std::numeric_limits<FrameGuidanceFrameId>::max();
	uint8_t lastGuidanceBinding = UINT8_MAX;
	bool coreRegistered = false;
	bool resetHistory = true;
};

static bool NGXSucceeded(NVSDK_NGX_Result result) noexcept {
	return NVSDK_NGX_SUCCEED(result);
}

static LONG CaptureNgxException(DWORD code, DWORD* sehCode) noexcept {
	*sehCode = code;
	return EXCEPTION_EXECUTE_HANDLER;
}

static NVSDK_NGX_Result ReleaseFeatureSafely(
	NVSDK_NGX_Handle* feature,
	DWORD* sehCode
) noexcept {
	*sehCode = 0;
	__try {
		return NVSDK_NGX_D3D12_ReleaseFeature(feature);
	} __except (CaptureNgxException(GetExceptionCode(), sehCode)) {
		return NVSDK_NGX_Result_FAIL_PlatformError;
	}
}

static NVSDK_NGX_Result GetParameterISafely(
	NVSDK_NGX_Parameter* parameters,
	const char* name,
	int* value,
	DWORD* sehCode
) noexcept {
	*sehCode = 0;
	__try {
		return NVSDK_NGX_Parameter_GetI(parameters, name, value);
	} __except (CaptureNgxException(GetExceptionCode(), sehCode)) {
		return NVSDK_NGX_Result_FAIL_PlatformError;
	}
}

static NVSDK_NGX_Result GetParameterUISafely(
	NVSDK_NGX_Parameter* parameters,
	const char* name,
	uint32_t* value,
	DWORD* sehCode
) noexcept {
	*sehCode = 0;
	__try {
		return NVSDK_NGX_Parameter_GetUI(parameters, name, value);
	} __except (CaptureNgxException(GetExceptionCode(), sehCode)) {
		return NVSDK_NGX_Result_FAIL_PlatformError;
	}
}

static bool SetParameterUISafely(
	NVSDK_NGX_Parameter* parameters,
	const char* name,
	uint32_t value,
	DWORD* sehCode
) noexcept {
	*sehCode = 0;
	__try {
		NVSDK_NGX_Parameter_SetUI(parameters, name, value);
		return true;
	} __except (CaptureNgxException(GetExceptionCode(), sehCode)) {
		return false;
	}
}

static bool SetParameterULLSafely(
	NVSDK_NGX_Parameter* parameters,
	const char* name,
	uint64_t value,
	DWORD* sehCode
) noexcept {
	*sehCode = 0;
	__try {
		NVSDK_NGX_Parameter_SetULL(parameters, name, value);
		return true;
	} __except (CaptureNgxException(GetExceptionCode(), sehCode)) {
		return false;
	}
}

static NVSDK_NGX_Result CreateDlssgSafely(
	ID3D12GraphicsCommandList* commandList,
	NVSDK_NGX_Handle** feature,
	NVSDK_NGX_Parameter* parameters,
	NVSDK_NGX_DLSSG_Create_Params* createParams,
	DWORD* sehCode
) noexcept {
	*sehCode = 0;
	__try {
		return NGX_D3D12_CREATE_DLSSG(
			commandList, 1, 1, feature, parameters, createParams);
	} __except (CaptureNgxException(GetExceptionCode(), sehCode)) {
		return NVSDK_NGX_Result_FAIL_PlatformError;
	}
}

static NVSDK_NGX_Result EvaluateDlssgSafely(
	ID3D12GraphicsCommandList* commandList,
	NVSDK_NGX_Handle* feature,
	NVSDK_NGX_Parameter* parameters,
	NVSDK_NGX_D3D12_DLSSG_Eval_Params* evalParams,
	NVSDK_NGX_DLSSG_Opt_Eval_Params* optionalParams,
	DWORD* sehCode
) noexcept {
	*sehCode = 0;
	__try {
		return NGX_D3D12_EVALUATE_DLSSG(
			commandList, feature, parameters, evalParams, optionalParams);
	} __except (CaptureNgxException(GetExceptionCode(), sehCode)) {
		return NVSDK_NGX_Result_FAIL_PlatformError;
	}
}

static bool WaitForFence(DLSSFrameGenerator::Impl& impl, uint64_t value) noexcept {
	if (!value || impl.fence12->GetCompletedValue() >= value) {
		return true;
	}

	wil::unique_event_nothrow event;
	if (FAILED(event.create())) {
		return false;
	}
	if (FAILED(impl.fence12->SetEventOnCompletion(value, event.get()))) {
		return false;
	}
	return WaitForSingleObject(event.get(), 3000) == WAIT_OBJECT_0;
}

static bool WaitForQueue(DLSSFrameGenerator::Impl& impl) noexcept {
	const uint64_t value = ++impl.fenceValue;
	if (FAILED(impl.queue12->Signal(impl.fence12.get(), value))) {
		return false;
	}
	return WaitForFence(impl, value);
}

DLSSFrameGenerator::Impl::~Impl() {
	if (queue12 && fence12) {
		WaitForQueue(*this);
	}
	if (feature) {
		DWORD sehCode = 0;
		const NVSDK_NGX_Result result = ReleaseFeatureSafely(feature, &sehCode);
		if (sehCode) {
			Logger::Get().Warn(fmt::format(
				"DLSSFG ReleaseFeature raised SEH {:#x}", sehCode));
		} else if (!NGXSucceeded(result)) {
			Logger::Get().Warn(fmt::format(
				"DLSSFG ReleaseFeature failed ({:#x})",
				static_cast<uint32_t>(result)));
		}
		feature = nullptr;
	}
	if (parameters) {
		if (!coreOwner || !coreOwner->DestroyParameters(parameters, "DLSSFG")) {
			Logger::Get().Warn("DLSSFG shared Core parameter destruction failed");
		}
	}
	if (coreRegistered && coreOwner) {
		coreOwner->Release("DLSSFG");
	}
}

static bool CreateSharedTexture(
	DLSSFrameGenerator::Impl& impl,
	const D3D11_TEXTURE2D_DESC& sourceDesc,
	bool allowUav,
	winrt::com_ptr<ID3D11Texture2D>& texture11,
	winrt::com_ptr<ID3D12Resource>& texture12
) noexcept {
	D3D11_TEXTURE2D_DESC desc = sourceDesc;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.CPUAccessFlags = 0;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE |
		(allowUav ? D3D11_BIND_UNORDERED_ACCESS : 0);
	desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE;

	HRESULT hr = impl.device11->CreateTexture2D(&desc, nullptr, texture11.put());
	if (FAILED(hr)) {
		Logger::Get().ComError("Create DLSSFG shared D3D11 texture failed", hr);
		return false;
	}

	winrt::com_ptr<IDXGIResource1> dxgiResource;
	hr = texture11->QueryInterface(IID_PPV_ARGS(dxgiResource.put()));
	if (FAILED(hr)) {
		return false;
	}

	HANDLE rawHandle = nullptr;
	hr = dxgiResource->CreateSharedHandle(nullptr, GENERIC_ALL, nullptr, &rawHandle);
	if (FAILED(hr)) {
		return false;
	}
	wil::unique_handle handle(rawHandle);
	hr = impl.device12->OpenSharedHandle(handle.get(), IID_PPV_ARGS(texture12.put()));
	if (FAILED(hr)) {
		Logger::Get().ComError("Open DLSSFG shared texture in D3D12 failed", hr);
		return false;
	}
	return true;
}

static bool CreateZeroTexture(
	DLSSFrameGenerator::Impl& impl,
	DXGI_FORMAT format,
	winrt::com_ptr<ID3D12Resource>& resource,
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle,
	D3D12_RESOURCE_BARRIER& barrier
) noexcept {
	D3D12_RESOURCE_DESC desc{};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	desc.Width = impl.renderWidth;
	desc.Height = impl.renderHeight;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = format;
	desc.SampleDesc.Count = 1;
	desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	D3D12_HEAP_PROPERTIES heap{};
	heap.Type = D3D12_HEAP_TYPE_DEFAULT;
	HRESULT hr = impl.device12->CreateCommittedResource(
		&heap,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		nullptr,
		IID_PPV_ARGS(resource.put())
	);
	if (FAILED(hr)) {
		Logger::Get().ComError("Create DLSSFG virtual input texture failed", hr);
		return false;
	}

	D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};
	uav.Format = format;
	uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	impl.device12->CreateUnorderedAccessView(resource.get(), nullptr, &uav, cpuHandle);
	static constexpr float ZERO[4]{};
	impl.commandList12->ClearUnorderedAccessViewFloat(
		gpuHandle, cpuHandle, resource.get(), ZERO, 0, nullptr);

	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition = {
		resource.get(),
		D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
	};
	return true;
}

static bool CreateInterpolationDisableResources(
	DLSSFrameGenerator::Impl& impl,
	uint32_t frameIndex
) noexcept {
	D3D12_RESOURCE_DESC desc{};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Width = 4;
	desc.Height = 1;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.SampleDesc.Count = 1;
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	D3D12_HEAP_PROPERTIES heap{};
	heap.Type = D3D12_HEAP_TYPE_DEFAULT;
	HRESULT hr = impl.device12->CreateCommittedResource(
		&heap, D3D12_HEAP_FLAG_NONE, &desc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr,
		IID_PPV_ARGS(impl.interpolationDisable12[frameIndex].put()));
	if (FAILED(hr)) {
		Logger::Get().ComError(
			"Create DLSSFG interpolation-disable output failed", hr);
		return false;
	}

	heap.Type = D3D12_HEAP_TYPE_READBACK;
	desc.Flags = D3D12_RESOURCE_FLAG_NONE;
	hr = impl.device12->CreateCommittedResource(
		&heap, D3D12_HEAP_FLAG_NONE, &desc,
		D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
		IID_PPV_ARGS(impl.interpolationDisableReadback12[frameIndex].put()));
	if (FAILED(hr)) {
		Logger::Get().ComError(
			"Create DLSSFG interpolation-disable readback failed", hr);
		return false;
	}
	return true;
}

static std::optional<bool> ReadInterpolationDisabled(
	DLSSFrameGenerator::Impl& impl,
	uint32_t frameIndex
) noexcept {
	D3D12_RANGE readRange{ 0, 1 };
	void* mapped = nullptr;
	const HRESULT hr = impl.interpolationDisableReadback12[frameIndex]->Map(
		0, &readRange, &mapped);
	if (FAILED(hr) || !mapped) {
		++impl.diagnosticInterpolationReadbackFailure[frameIndex];
		return std::nullopt;
	}
	const bool disabled = *static_cast<const uint8_t*>(mapped) != 0;
	D3D12_RANGE writtenRange{};
	impl.interpolationDisableReadback12[frameIndex]->Unmap(0, &writtenRange);
	if (disabled) {
		++impl.diagnosticInterpolationDisabled[frameIndex];
	} else {
		++impl.diagnosticInterpolationEnabled[frameIndex];
	}
	return disabled;
}

static void SetIdentity(float matrix[4][4]) noexcept {
	for (uint32_t row = 0; row < 4; ++row) {
		for (uint32_t column = 0; column < 4; ++column) {
			matrix[row][column] = row == column ? 1.0f : 0.0f;
		}
	}
}

DLSSFrameGenerator::DLSSFrameGenerator() = default;
DLSSFrameGenerator::~DLSSFrameGenerator() = default;

bool DLSSFrameGenerator::Initialize(
	DeviceResources& resources,
	NgxD3D12Core& ngxCore,
	ID3D11Texture2D* input,
	FrameGuidanceExtent guidanceExtent,
	const DLSSFrameGenerationSettings& settings
) noexcept {
	_requestedSettings = settings;
	_requestedSettings.multiplier = std::clamp(settings.multiplier, 2u, 4u);
	_impl.reset();
	auto impl = std::make_unique<Impl>();
	impl->device11 = resources.GetD3DDevice();
	impl->context11 = resources.GetD3DDC();
	impl->coreOwner = &ngxCore;
	impl->settings = _requestedSettings;

	D3D11_TEXTURE2D_DESC inputDesc{};
	input->GetDesc(&inputDesc);
	impl->width = inputDesc.Width;
	impl->height = inputDesc.Height;
	const bool guidanceRequested = impl->settings.motionVectorQuality !=
		NvidiaOpticalFlowQuality::None;
	const bool compatibleGuidanceExtent = guidanceRequested &&
		guidanceExtent.IsValid() &&
		guidanceExtent.width <= impl->width &&
		guidanceExtent.height <= impl->height;
	impl->renderWidth = compatibleGuidanceExtent ?
		guidanceExtent.width : impl->width;
	impl->renderHeight = compatibleGuidanceExtent ?
		guidanceExtent.height : impl->height;
	if (guidanceRequested && !compatibleGuidanceExtent) {
		Logger::Get().Warn(fmt::format(
			"DLSS FG guidance extent {}x{} is incompatible with backbuffer {}x{}; "
			"using Zero guidance at backbuffer size",
			guidanceExtent.width, guidanceExtent.height,
			impl->width, impl->height));
	}

	if (!ngxCore.Acquire(resources, "DLSSFG")) {
		return false;
	}
	impl->coreRegistered = true;
	impl->device12.copy_from(ngxCore.Device());
	HRESULT hr = S_OK;

	D3D12_COMMAND_QUEUE_DESC queueDesc{};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	hr = impl->device12->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(impl->queue12.put()));
	if (SUCCEEDED(hr)) {
		hr = impl->device12->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(impl->allocator12.put()));
	}
	if (SUCCEEDED(hr)) {
		hr = impl->device12->CreateCommandList(
			0, D3D12_COMMAND_LIST_TYPE_DIRECT, impl->allocator12.get(), nullptr,
			IID_PPV_ARGS(impl->commandList12.put()));
	}
	if (FAILED(hr)) {
		Logger::Get().ComError("Create DLSSFG D3D12 command objects failed", hr);
		return false;
	}

	if (!CreateSharedTexture(*impl, inputDesc, false,
		impl->sharedInput11, impl->sharedInput12) ||
		!CreateSharedTexture(*impl, inputDesc, true,
			impl->sharedGenerated11, impl->sharedGenerated12)) {
		return false;
	}

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.NumDescriptors = 2;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	hr = impl->device12->CreateDescriptorHeap(
		&heapDesc, IID_PPV_ARGS(impl->descriptorHeap12.put()));
	if (FAILED(hr)) {
		return false;
	}
	ID3D12DescriptorHeap* heaps[]{ impl->descriptorHeap12.get() };
	impl->commandList12->SetDescriptorHeaps(1, heaps);
	const UINT stride = impl->device12->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	D3D12_CPU_DESCRIPTOR_HANDLE cpu =
		impl->descriptorHeap12->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE gpu =
		impl->descriptorHeap12->GetGPUDescriptorHandleForHeapStart();
	D3D12_RESOURCE_BARRIER auxBarriers[2]{};
	if (!CreateZeroTexture(*impl, DXGI_FORMAT_R16G16_FLOAT,
		impl->zeroMotion12, cpu, gpu, auxBarriers[0])) {
		return false;
	}
	cpu.ptr += stride;
	gpu.ptr += stride;
	if (!CreateZeroTexture(*impl, DXGI_FORMAT_R32_FLOAT,
		impl->zeroDepth12, cpu, gpu, auxBarriers[1])) {
		return false;
	}
	impl->commandList12->ResourceBarrier(2, auxBarriers);

	NVSDK_NGX_Result result = NVSDK_NGX_Result_Success;
	if (!ngxCore.GetCapabilityParameters(&impl->parameters, "DLSSFG")) {
		return false;
	}

	int available = 0;
	DWORD sehCode = 0;
	result = GetParameterISafely(
		impl->parameters, NVSDK_NGX_Parameter_FrameGeneration_Available,
		&available, &sehCode);
	if (!NGXSucceeded(result) || !available) {
		int initResult = 0;
		DWORD initSehCode = 0;
		GetParameterISafely(
			impl->parameters,
			NVSDK_NGX_Parameter_FrameGeneration_FeatureInitResult,
			&initResult, &initSehCode);
		Logger::Get().Error(fmt::format(
			"DLSS Frame Generation is unavailable (result={:#x}, seh={:#x})",
			(uint32_t)initResult, sehCode ? sehCode : initSehCode));
		return false;
	}

	uint32_t maxGeneratedFrames = 1;
	sehCode = 0;
	if (!NGXSucceeded(GetParameterUISafely(
		impl->parameters,
		NVSDK_NGX_DLSSG_Parameter_MultiFrameCountMax,
		&maxGeneratedFrames,
		&sehCode))) {
		maxGeneratedFrames = 1;
	}
	maxGeneratedFrames = std::clamp(maxGeneratedFrames, 1u, 3u);
	impl->maxSupportedMultiplier = maxGeneratedFrames + 1;
	impl->multiplier = std::min(
		_requestedSettings.multiplier, impl->maxSupportedMultiplier);
	if (impl->multiplier != _requestedSettings.multiplier) {
		Logger::Get().Warn(fmt::format(
			"DLSSFG {}x requested, hardware supports up to {}x; using {}x",
			_requestedSettings.multiplier,
			maxGeneratedFrames + 1, impl->multiplier));
	}
	for (uint32_t frameIndex = 1;
		frameIndex < impl->multiplier; ++frameIndex) {
		if (!CreateInterpolationDisableResources(*impl, frameIndex)) {
			return false;
		}
	}

	const uint32_t neverProvidedFlags =
		NVSDK_NGX_DLSSG_ResourceFlags_HUDLess |
		NVSDK_NGX_DLSSG_ResourceFlags_UI |
		NVSDK_NGX_DLSSG_ResourceFlags_UIAlpha |
		NVSDK_NGX_DLSSG_ResourceFlags_BidirectionalDistortionField |
		NVSDK_NGX_DLSSG_ResourceFlags_OutputReal;
	sehCode = 0;
	const bool resourceFlagsSet = SetParameterUISafely(impl->parameters,
		NVSDK_NGX_DLSSG_Parameter_ResourceNeverProvided_Flags,
		neverProvidedFlags, &sehCode);
	if (!resourceFlagsSet) {
		Logger::Get().Error(fmt::format(
			"Set DLSSFG resource flags raised SEH {:#x}", sehCode));
		return false;
	}

	NVSDK_NGX_DLSSG_Create_Params createParams{};
	createParams.Width = impl->width;
	createParams.Height = impl->height;
	createParams.NativeBackbufferFormat = inputDesc.Format;
	createParams.RenderWidth = impl->renderWidth;
	createParams.RenderHeight = impl->renderHeight;
	createParams.DynamicResolutionScaling = false;
	sehCode = 0;
	result = CreateDlssgSafely(
		impl->commandList12.get(), &impl->feature,
		impl->parameters, &createParams, &sehCode);
	if (!NGXSucceeded(result) || !impl->feature) {
		Logger::Get().Error(fmt::format(
			"NGX_D3D12_CREATE_DLSSG failed ({:#x}, seh={:#x})",
			(uint32_t)result, sehCode));
		return false;
	}

	hr = impl->commandList12->Close();
	if (FAILED(hr)) {
		return false;
	}
	ID3D12CommandList* lists[]{ impl->commandList12.get() };
	impl->queue12->ExecuteCommandLists(1, lists);

	hr = impl->device11->CreateFence(
		0, D3D11_FENCE_FLAG_SHARED, IID_PPV_ARGS(impl->fence11.put()));
	if (FAILED(hr)) {
		return false;
	}
	HANDLE rawFence = nullptr;
	hr = impl->fence11->CreateSharedHandle(nullptr, GENERIC_ALL, nullptr, &rawFence);
	if (FAILED(hr)) {
		return false;
	}
	wil::unique_handle fenceHandle(rawFence);
	hr = impl->device12->OpenSharedHandle(
		fenceHandle.get(), IID_PPV_ARGS(impl->fence12.put()));
	if (FAILED(hr) || !WaitForQueue(*impl)) {
		return false;
	}
	impl->guidanceInterop = std::make_unique<FrameGuidanceD3D12Interop>();
	if (!impl->guidanceInterop->Initialize(
		impl->device12.get(), impl->fence12.get())) {
		return false;
	}

	Logger::Get().Info(fmt::format(
		"DLSS FG_Experimental initialized: backbuffer={}x{}, render={}x{}, "
		"multiplier={}x, requestedMotion={}, depth=zero-contract, "
		"motionContract=current-to-previous/source-pixels scale=1,1",
		impl->width, impl->height, impl->renderWidth, impl->renderHeight,
		impl->multiplier,
		static_cast<uint32_t>(impl->settings.motionVectorQuality)));
	_impl = std::move(impl);
	return true;
}

bool DLSSFrameGenerator::Resize(
	DeviceResources& resources,
	NgxD3D12Core& ngxCore,
	ID3D11Texture2D* input,
	FrameGuidanceExtent guidanceExtent
) noexcept {
	return Initialize(resources, ngxCore, input, guidanceExtent, _requestedSettings);
}

FrameGuidanceRequirements
DLSSFrameGenerator::GetFrameGuidanceRequirements() const noexcept {
	FrameGuidanceRequirements result{ .zero = true };
	result.Add(MotionVectorRequest::Nvidia(
		_requestedSettings.motionVectorQuality));
	return result;
}

uint32_t DLSSFrameGenerator::Multiplier() const noexcept {
	return _impl ? _impl->multiplier : _requestedSettings.multiplier;
}

uint32_t DLSSFrameGenerator::MaxSupportedMultiplier() const noexcept {
	return _impl ? _impl->maxSupportedMultiplier : 2;
}

bool DLSSFrameGenerator::Draw(
	ID3D11Texture2D* input,
	FrameGuidanceFrameId frameId,
	const FrameGuidanceView& guidance,
	const FrameGuidanceView& zeroGuidance,
	const PublishCallback& publishGeneratedFrame
) noexcept {
	if (!_impl || !_impl->feature || !_impl->parameters) {
		return false;
	}
	Impl& impl = *_impl;
	const FrameGuidanceExtent renderExtent{
		impl.renderWidth, impl.renderHeight
	};
	const FrameGuidanceView selected = SelectFrameGuidanceChannels(
		guidance, zeroGuidance, frameId, renderExtent,
		impl.settings.motionVectorQuality != NvidiaOpticalFlowQuality::None);
	bool sharedGuidanceBound = false;
	bool realMotion = false;
	if (impl.settings.motionVectorQuality != NvidiaOpticalFlowQuality::None &&
		selected.IsValidFor(frameId, renderExtent) &&
		impl.guidanceInterop->Update(selected, frameId, renderExtent) &&
		impl.guidanceInterop->WaitForProducer(impl.context11, selected)) {
		sharedGuidanceBound = true;
		realMotion = impl.settings.motionVectorQuality !=
			NvidiaOpticalFlowQuality::None &&
			!selected.motion.metadata.isZero;
	}

	const uint8_t guidanceBinding = uint8_t(realMotion) |
		(uint8_t(impl.settings.motionVectorQuality !=
			NvidiaOpticalFlowQuality::None) << 1) |
		(uint8_t(sharedGuidanceBound) << 2);
	const bool bindingChanged = impl.lastGuidanceBinding != UINT8_MAX &&
		impl.lastGuidanceBinding != guidanceBinding;
	if (impl.lastGuidanceBinding != guidanceBinding) {
		Logger::Get().Info(fmt::format(
			"DLSS FG guidance frameId={}: requested motion={}, "
			"produced motion={}, bound motion={} depth=zero, fallback={}",
			frameId, impl.settings.motionVectorQuality !=
				NvidiaOpticalFlowQuality::None,
			guidance.motion.metadata.valid && !guidance.motion.metadata.isZero,
			realMotion ? "real" : "zero",
			!sharedGuidanceBound ? "interop-or-extent-zero" :
			(impl.settings.motionVectorQuality != NvidiaOpticalFlowQuality::None && !realMotion ?
				"provider-zero" : "none")));
	}
	const bool guidanceReset = bindingChanged ||
		(sharedGuidanceBound && selected.requiresHistoryReset &&
			impl.lastGuidanceResetFrameId != frameId);

	impl.context11->CopyResource(impl.sharedInput11.get(), input);
	const uint64_t inputReady = ++impl.fenceValue;
	HRESULT hr = impl.context11->Signal(impl.fence11.get(), inputReady);
	if (FAILED(hr)) {
		return false;
	}
	impl.context11->Flush();
	hr = impl.queue12->Wait(impl.fence12.get(), inputReady);
	if (FAILED(hr)) {
		return false;
	}

	const bool resetThisFrame = impl.resetHistory || guidanceReset;
	const uint32_t generatedFrameCount = resetThisFrame ? 1 : impl.multiplier - 1;
	DWORD frameIdSehCode = 0;
	if (!SetParameterULLSafely(
		impl.parameters, NVSDK_NGX_DLSSG_Parameter_BackbufferFrameID,
		frameId, &frameIdSehCode)) {
		Logger::Get().Error(fmt::format(
			"Set DLSSFG BackbufferFrameID raised SEH {:#x}", frameIdSehCode));
		return false;
	}
	for (uint32_t frameIndex = 1; frameIndex <= generatedFrameCount; ++frameIndex) {
		hr = impl.allocator12->Reset();
		if (SUCCEEDED(hr)) {
			hr = impl.commandList12->Reset(impl.allocator12.get(), nullptr);
		}
		if (FAILED(hr)) {
			return false;
		}

		D3D12_RESOURCE_BARRIER barriers[2]{};
		barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barriers[0].Transition = {
			impl.sharedInput12.get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
			D3D12_RESOURCE_STATE_COMMON,
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
		};
		barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barriers[1].Transition = {
			impl.sharedGenerated12.get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
			D3D12_RESOURCE_STATE_COMMON,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS
		};
		impl.commandList12->ResourceBarrier(2, barriers);
		if (sharedGuidanceBound) {
			impl.guidanceInterop->Transition(
				impl.commandList12.get(), D3D12_RESOURCE_STATE_COMMON,
				D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		}

		NVSDK_NGX_D3D12_DLSSG_Eval_Params evalParams{};
		evalParams.pBackbuffer = impl.sharedInput12.get();
		evalParams.pDepth = sharedGuidanceBound ?
			impl.guidanceInterop->Depth() : impl.zeroDepth12.get();
		evalParams.pMVecs = sharedGuidanceBound ?
			impl.guidanceInterop->Motion() : impl.zeroMotion12.get();
		evalParams.pOutputInterpFrame = impl.sharedGenerated12.get();
		evalParams.pOutputDisableInterpolation =
			impl.interpolationDisable12[frameIndex].get();

		NVSDK_NGX_DLSSG_Opt_Eval_Params optionalParams{};
		optionalParams.multiFrameCount = generatedFrameCount;
		optionalParams.multiFrameIndex = frameIndex;
		SetIdentity(optionalParams.cameraViewToClip);
		SetIdentity(optionalParams.clipToCameraView);
		SetIdentity(optionalParams.clipToLensClip);
		SetIdentity(optionalParams.clipToPrevClip);
		SetIdentity(optionalParams.prevClipToClip);
		// Frame Guidance motion is already current-to-previous in pixels at the
		// motion-vector resource resolution. DLSSG therefore requires unit scale.
		optionalParams.mvecScale[0] = 1.0f;
		optionalParams.mvecScale[1] = 1.0f;
		optionalParams.cameraUp[1] = 1.0f;
		optionalParams.cameraRight[0] = 1.0f;
		optionalParams.cameraFwd[2] = 1.0f;
		optionalParams.cameraNear = 0.1f;
		optionalParams.cameraFar = 1000.0f;
		optionalParams.cameraFOV = 1.04719755f;
		optionalParams.cameraAspectRatio =
			float(impl.renderWidth) / float(impl.renderHeight);
		optionalParams.depthInverted = false;
		optionalParams.cameraMotionIncluded = realMotion;
		optionalParams.reset = resetThisFrame;
		optionalParams.motionVectorsInvalidValue = 0.0f;
		optionalParams.motionVectorsDilated = realMotion;
		optionalParams.menuDetectionEnabled = false;
		const FrameGuidanceRegion guidanceRegion = sharedGuidanceBound ?
			selected.motion.metadata.validRegion :
			FrameGuidanceRegion::Full(renderExtent);
		optionalParams.mvecsSubrectBase = {
			guidanceRegion.x, guidanceRegion.y
		};
		optionalParams.mvecsSubrectSize = {
			guidanceRegion.width, guidanceRegion.height
		};
		const FrameGuidanceRegion depthRegion = sharedGuidanceBound ?
			selected.depth.metadata.validRegion :
			FrameGuidanceRegion::Full(renderExtent);
		optionalParams.depthSubrectBase = {
			depthRegion.x, depthRegion.y
		};
		optionalParams.depthSubrectSize = {
			depthRegion.width, depthRegion.height
		};
		optionalParams.backbufferSubrectSize = {
			impl.width, impl.height
		};
		optionalParams.outputInterpSubrectSize = {
			impl.width, impl.height
		};

		DWORD sehCode = 0;
		const NVSDK_NGX_Result result = EvaluateDlssgSafely(
			impl.commandList12.get(), impl.feature, impl.parameters,
			&evalParams, &optionalParams, &sehCode);
		if (!NGXSucceeded(result)) {
			++impl.diagnosticEvaluateFailure[frameIndex];
			impl.commandList12->Close();
			Logger::Get().Error(fmt::format(
				"NGX_D3D12_EVALUATE_DLSSG failed ({:#x}, seh={:#x}, index={}/{})",
				(uint32_t)result, sehCode, frameIndex, generatedFrameCount));
			return false;
		}
		++impl.diagnosticEvaluateSuccess[frameIndex];
		{
			D3D12_RESOURCE_BARRIER diagnosticBarrier{};
			diagnosticBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			diagnosticBarrier.Transition = {
				impl.interpolationDisable12[frameIndex].get(),
				D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				D3D12_RESOURCE_STATE_COPY_SOURCE
			};
			impl.commandList12->ResourceBarrier(1, &diagnosticBarrier);
			impl.commandList12->CopyBufferRegion(
				impl.interpolationDisableReadback12[frameIndex].get(), 0,
				impl.interpolationDisable12[frameIndex].get(), 0, 4);
			std::swap(
				diagnosticBarrier.Transition.StateBefore,
				diagnosticBarrier.Transition.StateAfter);
			impl.commandList12->ResourceBarrier(1, &diagnosticBarrier);
		}

		for (D3D12_RESOURCE_BARRIER& barrier : barriers) {
			std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
		}
		if (sharedGuidanceBound) {
			impl.guidanceInterop->Transition(
				impl.commandList12.get(),
				D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_COMMON);
		}
		impl.commandList12->ResourceBarrier(2, barriers);
		hr = impl.commandList12->Close();
		if (FAILED(hr)) {
			return false;
		}
		ID3D12CommandList* lists[]{ impl.commandList12.get() };
		impl.queue12->ExecuteCommandLists(1, lists);
		const uint64_t outputReady = ++impl.fenceValue;
		hr = impl.queue12->Signal(impl.fence12.get(), outputReady);
		if (SUCCEEDED(hr) && sharedGuidanceBound) {
			impl.guidanceInterop->MarkSubmitted(outputReady);
		}
		if (SUCCEEDED(hr)) {
			hr = impl.context11->Wait(impl.fence11.get(), outputReady);
		}
		if (FAILED(hr)) {
			return false;
		}
		// The flag is part of the SDK output, not optional telemetry. Also wait
		// on reset/disabled frames before reusing the allocator and output buffer.
		if (!WaitForFence(impl, outputReady)) return false;
		const auto disabled = ReadInterpolationDisabled(impl, frameIndex);
		if (!disabled) return false;
		if (!resetThisFrame && !*disabled) {
			if (!publishGeneratedFrame(impl.sharedGenerated11.get())) {
				++impl.diagnosticGeneratedPublishFailure;
				return false;
			}
			++impl.diagnosticGeneratedPublishSuccess;
		}
	}

	impl.resetHistory = false;
	impl.lastGuidanceBinding = guidanceBinding;
	if (guidanceReset) impl.lastGuidanceResetFrameId = frameId;
	if (++impl.diagnosticRealFrames >= 120) {
		Logger::Get().Info(fmt::format(
			"DLSSFG 120-real-frame diagnostics: multiplier={}x "
			"evaluate[index1={}/{} index2={}/{} index3={}/{}] "
			"generatedPublish={}/{} "
			"interpolation[index1={}/{}/{} index2={}/{}/{} index3={}/{}/{}]",
			impl.multiplier,
			impl.diagnosticEvaluateSuccess[1],
			impl.diagnosticEvaluateFailure[1],
			impl.diagnosticEvaluateSuccess[2],
			impl.diagnosticEvaluateFailure[2],
			impl.diagnosticEvaluateSuccess[3],
			impl.diagnosticEvaluateFailure[3],
			impl.diagnosticGeneratedPublishSuccess,
			impl.diagnosticGeneratedPublishFailure,
			impl.diagnosticInterpolationEnabled[1],
			impl.diagnosticInterpolationDisabled[1],
			impl.diagnosticInterpolationReadbackFailure[1],
			impl.diagnosticInterpolationEnabled[2],
			impl.diagnosticInterpolationDisabled[2],
			impl.diagnosticInterpolationReadbackFailure[2],
			impl.diagnosticInterpolationEnabled[3],
			impl.diagnosticInterpolationDisabled[3],
			impl.diagnosticInterpolationReadbackFailure[3]));
		impl.diagnosticRealFrames = 0;
		impl.diagnosticEvaluateSuccess.fill(0);
		impl.diagnosticEvaluateFailure.fill(0);
		impl.diagnosticInterpolationEnabled.fill(0);
		impl.diagnosticInterpolationDisabled.fill(0);
		impl.diagnosticInterpolationReadbackFailure.fill(0);
		impl.diagnosticGeneratedPublishSuccess = 0;
		impl.diagnosticGeneratedPublishFailure = 0;
	}
	return true;
}

void DLSSFrameGenerator::RequestHistoryReset() noexcept {
	if (_impl) {
		_impl->resetHistory = true;
	}
}

bool DLSSFrameGenerator::Drain() noexcept {
	return !_impl || !_impl->queue12 || !_impl->fence12 || WaitForQueue(*_impl);
}

}

#else

namespace Magpie {

struct DLSSFrameGenerator::Impl {};
DLSSFrameGenerator::DLSSFrameGenerator() = default;
DLSSFrameGenerator::~DLSSFrameGenerator() = default;
bool DLSSFrameGenerator::Initialize(
	DeviceResources&, NgxD3D12Core&, ID3D11Texture2D*, FrameGuidanceExtent,
	const DLSSFrameGenerationSettings&) noexcept {
	Logger::Get().Error("DLSS Frame Generation is disabled at build time");
	return false;
}
bool DLSSFrameGenerator::Resize(
	DeviceResources&, NgxD3D12Core&, ID3D11Texture2D*,
	FrameGuidanceExtent) noexcept {
	return false;
}
bool DLSSFrameGenerator::Draw(
	ID3D11Texture2D*, FrameGuidanceFrameId,
	const FrameGuidanceView&, const FrameGuidanceView&,
	const PublishCallback&) noexcept {
	return false;
}
void DLSSFrameGenerator::RequestHistoryReset() noexcept {}
bool DLSSFrameGenerator::Drain() noexcept { return true; }
FrameGuidanceRequirements
DLSSFrameGenerator::GetFrameGuidanceRequirements() const noexcept { return {}; }
uint32_t DLSSFrameGenerator::Multiplier() const noexcept {
	return _requestedSettings.multiplier;
}
uint32_t DLSSFrameGenerator::MaxSupportedMultiplier() const noexcept { return 2; }

}

#endif
