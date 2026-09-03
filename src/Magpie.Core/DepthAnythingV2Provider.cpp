#include "pch.h"
#include "DepthAnythingV2Provider.h"
#include "DeviceResources.h"
#include "DirectXHelper.h"
#include "Logger.h"
#include "Win32Helper.h"
#include "FrameGuidancePerformance.h"

#ifdef MP_ENABLE_DEPTH_ANYTHING_V2
#include <bcrypt.h>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "version.lib")

namespace Magpie {

namespace {

constexpr wchar_t MODEL_RELATIVE_PATH[] =
	L"FrameGuidance\\DepthAnythingV2\\model_fp16.onnx";
constexpr char MODEL_SHA256[] =
	"2df6223f206b5164e21f664ace61dabeb9bb6a49b8b5a3e00510b4807d0f5b04";
// DLSSNR consumes depth as low-frequency guidance. 336 reduces the ViT token
// grid while NVOF reprojection preserves temporal continuity between updates.
constexpr uint32_t MODEL_LONG_SIDE = 336;
constexpr double NGX_GPU_SOFT_BUDGET_MS = 10.5;
constexpr double NGX_GPU_HARD_BUDGET_MS = 16.0;
constexpr auto NGX_GPU_SOFT_MAX_DEPTH_AGE = std::chrono::milliseconds(250);
constexpr auto NGX_GPU_HARD_MAX_DEPTH_AGE = std::chrono::milliseconds(500);
constexpr uint32_t CAPTURE_SLOT_COUNT = 3;

constexpr char PREPROCESS_DEPTH_HLSL[] = R"(
Texture2D<float4> Color : register(t0);
SamplerState LinearClamp : register(s0);
RWTexture2DArray<float> ModelInput : register(u0);

cbuffer Params : register(b0) {
    uint2 SourceExtent;
    uint2 InferenceExtent;
    uint4 Padding;
};

[numthreads(8, 8, 1)]
void Preprocess(uint3 tid : SV_DispatchThreadID) {
    if (any(tid.xy >= InferenceExtent)) return;
    float2 uv = (float2(tid.xy) + 0.5) / float2(InferenceExtent);
    // A typed BGRA SRV already returns logical RGBA components.
    float3 rgb = Color.SampleLevel(LinearClamp, uv, 0).rgb;
    static const float3 mean = float3(0.485, 0.456, 0.406);
    static const float3 deviation = float3(0.229, 0.224, 0.225);
    float3 normalized = (rgb - mean) / deviation;
    ModelInput[uint3(tid.xy, 0)] = normalized.r;
    ModelInput[uint3(tid.xy, 1)] = normalized.g;
    ModelInput[uint3(tid.xy, 2)] = normalized.b;
}
)";

constexpr char POSTPROCESS_DEPTH_HLSL[] = R"(
Texture2D<float> ModelDepth : register(t0);
SamplerState LinearClamp : register(s0);
RWTexture2D<float> CurrentDepth : register(u0);
RWTexture2D<float> RawDepth : register(u1);

cbuffer Params : register(b0) {
    uint2 SourceExtent;
    uint2 InferenceExtent;
    float P02;
    float ReciprocalRange;
    uint2 Padding;
};

[numthreads(8, 8, 1)]
void Postprocess(uint3 tid : SV_DispatchThreadID) {
    if (any(tid.xy >= SourceExtent)) return;
    float2 uv = (float2(tid.xy) + 0.5) / float2(SourceExtent);
    float raw = ModelDepth.SampleLevel(LinearClamp, uv, 0);
    RawDepth[tid.xy] = raw;
    CurrentDepth[tid.xy] = saturate((raw - P02) * ReciprocalRange);
}
)";

constexpr char TEMPORAL_DEPTH_HLSL[] = R"(
Texture2D<float> CurrentDepth : register(t0);
Texture2D<float> HistoryDepth : register(t1);
Texture2D<float2> Motion : register(t2);
Texture2D<float> Confidence : register(t3);
SamplerState LinearClamp : register(s0);
RWTexture2D<float> FilteredDepth : register(u0);
RWTexture2D<float> DepthResidual : register(u1);

cbuffer Params : register(b0) {
    uint2 SourceExtent;
    uint ResetHistory;
    uint HasMotion;
    uint HasCurrentInference;
    uint3 Padding;
};

[numthreads(8, 8, 1)]
void Filter(uint3 tid : SV_DispatchThreadID) {
    if (any(tid.xy >= SourceExtent)) return;
    float current = CurrentDepth.Load(int3(tid.xy, 0));
    float2 pixel = float2(tid.xy) + 0.5;
    float2 motion = HasMotion != 0 ? Motion.Load(int3(tid.xy, 0)) : 0.0;
    float2 previousPixel = pixel + motion;
    bool inside = all(previousPixel >= 0.0) &&
        all(previousPixel < float2(SourceExtent));
    float2 previousUv = previousPixel / float2(SourceExtent);
    float previous = HistoryDepth.SampleLevel(LinearClamp, previousUv, 0);
    float motionConfidence = HasMotion != 0 ?
        Confidence.Load(int3(tid.xy, 0)) : 1.0;
    if (HasCurrentInference == 0) {
        float held = HistoryDepth.Load(int3(tid.xy, 0));
        FilteredDepth[tid.xy] = inside && motionConfidence >= 0.05 ?
            previous : held;
        DepthResidual[tid.xy] = 0.0;
        return;
    }
    float residual = abs(current - previous);
    float confidence = HasMotion != 0 ? motionConfidence : 0.0;
    float historyWeight = (ResetHistory == 0 && inside) ?
        0.85 * confidence * saturate(1.0 - residual * 8.0) : 0.0;
    FilteredDepth[tid.xy] = lerp(current, previous, historyWeight);
    DepthResidual[tid.xy] = ResetHistory != 0 ? 0.0 : residual;
}
)";

std::string ComputeSha256(const std::filesystem::path& path) noexcept {
	wil::unique_hfile file(CreateFileW(
		path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL, nullptr));
	if (!file) return {};
	BCRYPT_ALG_HANDLE algorithm = nullptr;
	BCRYPT_HASH_HANDLE hash = nullptr;
	DWORD objectSize = 0;
	DWORD bytes = 0;
	std::vector<uint8_t> object;
	std::array<uint8_t, 32> digest{};
	if (BCryptOpenAlgorithmProvider(
		&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0 ||
		BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
			reinterpret_cast<PUCHAR>(&objectSize), sizeof(objectSize), &bytes, 0) < 0) {
		if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
		return {};
	}
	object.resize(objectSize);
	if (BCryptCreateHash(
		algorithm, &hash, object.data(), objectSize, nullptr, 0, 0) < 0) {
		BCryptCloseAlgorithmProvider(algorithm, 0);
		return {};
	}
	std::array<uint8_t, 1 << 16> buffer;
	bool okay = true;
	while (true) {
		DWORD read = 0;
		if (!ReadFile(file.get(), buffer.data(), static_cast<DWORD>(buffer.size()),
			&read, nullptr)) {
			okay = false;
			break;
		}
		if (!read) break;
		if (BCryptHashData(hash, buffer.data(), read, 0) < 0) {
			okay = false;
			break;
		}
	}
	if (okay && BCryptFinishHash(
		hash, digest.data(), static_cast<ULONG>(digest.size()), 0) < 0) {
		okay = false;
	}
	BCryptDestroyHash(hash);
	BCryptCloseAlgorithmProvider(algorithm, 0);
	if (!okay) return {};
	std::string result;
	result.reserve(64);
	for (uint8_t value : digest) result += fmt::format("{:02x}", value);
	return result;
}

std::filesystem::path LocalAppDataPath() noexcept {
	std::wstring value(32768, L'\0');
	const DWORD length = GetEnvironmentVariableW(
		L"LOCALAPPDATA", value.data(), static_cast<DWORD>(value.size()));
	if (!length || length >= value.size()) {
		return Win32Helper::GetExePath().parent_path();
	}
	value.resize(length);
	return value;
}

uint64_t QueryVideoMemoryUsage(IDXGIAdapter4* adapter) noexcept {
	DXGI_QUERY_VIDEO_MEMORY_INFO info{};
	return adapter && SUCCEEDED(adapter->QueryVideoMemoryInfo(
		0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info)) ? info.CurrentUsage : 0;
}

double VideoMemoryDeltaMiB(uint64_t after, uint64_t before) noexcept {
	const double bytes = after >= before ? double(after - before) :
		-double(before - after);
	return bytes / (1024.0 * 1024.0);
}

std::string FileVersionKey(const std::filesystem::path& path) noexcept {
	DWORD ignored = 0;
	const DWORD size = GetFileVersionInfoSizeW(path.c_str(), &ignored);
	if (!size) return "missing";
	std::vector<uint8_t> data(size);
	if (!GetFileVersionInfoW(path.c_str(), 0, size, data.data())) return "unknown";
	VS_FIXEDFILEINFO* info = nullptr;
	UINT infoSize = 0;
	if (!VerQueryValueW(data.data(), L"\\", reinterpret_cast<void**>(&info),
		&infoSize) || !info || infoSize < sizeof(*info)) return "unknown";
	return fmt::format("{}.{}.{}.{}",
		HIWORD(info->dwFileVersionMS), LOWORD(info->dwFileVersionMS),
		HIWORD(info->dwFileVersionLS), LOWORD(info->dwFileVersionLS));
}

std::filesystem::path FindModulePath(const wchar_t* name) noexcept {
	std::wstring result(32768, L'\0');
	const DWORD length = SearchPathW(
		nullptr, name, nullptr, static_cast<DWORD>(result.size()),
		result.data(), nullptr);
	if (!length || length >= result.size()) return {};
	result.resize(length);
	return result;
}

struct CudaIdentity {
	uint32_t deviceIndex = 0;
	int driverVersion = 0;
	int computeMajor = 0;
	int computeMinor = 0;
	bool valid = false;
};

CudaIdentity FindCudaDevice(LUID adapterLuid) noexcept {
	using CuInitFn = int(__stdcall*)(unsigned int);
	using CuDeviceGetCountFn = int(__stdcall*)(int*);
	using CuDeviceGetFn = int(__stdcall*)(int*, int);
	using CuDeviceGetLuidFn = int(__stdcall*)(char*, unsigned int*, int);
	using CuDeviceGetAttributeFn = int(__stdcall*)(int*, int, int);
	using CuDriverGetVersionFn = int(__stdcall*)(int*);

	CudaIdentity result;
	HMODULE module = LoadLibraryExW(
		L"nvcuda.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
	if (!module) return result;
	auto get = [module](const char* name) {
		return GetProcAddress(module, name);
	};
	auto cuInit = reinterpret_cast<CuInitFn>(get("cuInit"));
	auto cuDeviceGetCount = reinterpret_cast<CuDeviceGetCountFn>(
		get("cuDeviceGetCount"));
	auto cuDeviceGet = reinterpret_cast<CuDeviceGetFn>(get("cuDeviceGet"));
	auto cuDeviceGetLuid = reinterpret_cast<CuDeviceGetLuidFn>(
		get("cuDeviceGetLuid"));
	auto cuDeviceGetAttribute = reinterpret_cast<CuDeviceGetAttributeFn>(
		get("cuDeviceGetAttribute"));
	auto cuDriverGetVersion = reinterpret_cast<CuDriverGetVersionFn>(
		get("cuDriverGetVersion"));
	if (cuInit && cuDeviceGetCount && cuDeviceGet && cuDeviceGetLuid &&
		cuDeviceGetAttribute && cuDriverGetVersion && cuInit(0) == 0) {
		int count = 0;
		cuDriverGetVersion(&result.driverVersion);
		if (cuDeviceGetCount(&count) == 0) {
			for (int ordinal = 0; ordinal < count; ++ordinal) {
				int device = 0;
				char luid[sizeof(LUID)]{};
				unsigned int nodeMask = 0;
				if (cuDeviceGet(&device, ordinal) != 0 ||
					cuDeviceGetLuid(luid, &nodeMask, device) != 0 ||
					std::memcmp(luid, &adapterLuid, sizeof(LUID)) != 0) continue;
				// CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR/MINOR.
				if (cuDeviceGetAttribute(&result.computeMajor, 75, device) == 0 &&
					cuDeviceGetAttribute(&result.computeMinor, 76, device) == 0) {
					result.deviceIndex = static_cast<uint32_t>(ordinal);
					result.valid = true;
				}
				break;
			}
		}
	}
	FreeLibrary(module);
	return result;
}

FrameGuidanceMetadata MakeMetadata(
	const FrameGuidanceFrame& frame,
	FrameGuidanceResetReason resetReason,
	bool isZero = false
) noexcept {
	return {
		.frameId = frame.frameId,
		.sourceExtent = frame.sourceExtent,
		.validRegion = frame.validRegion,
		.resetReason = resetReason,
		.valid = true,
		.isZero = isZero,
		.requiresHistoryReset = resetReason != FrameGuidanceResetReason::None
	};
}

uint32_t AlignPatch(float value) noexcept {
	return std::max(14u, static_cast<uint32_t>(
		std::lround(value / 14.0f)) * 14u);
}

struct Dav2TimingSummary {
	double average = 0.0;
	double p95 = 0.0;
	double p99 = 0.0;
	double maximum = 0.0;
	size_t count = 0;
};

struct Dav2TimingWindow {
	static constexpr size_t CAPACITY = 120;
	std::array<double, CAPACITY> values{};
	size_t count = 0;
	size_t next = 0;

	void Add(double value) noexcept {
		values[next] = value;
		next = (next + 1) % CAPACITY;
		count = std::min(count + 1, CAPACITY);
	}

	Dav2TimingSummary Summarize() const noexcept {
		Dav2TimingSummary result{ .count = count };
		if (!count) return result;
		std::array<double, CAPACITY> sorted{};
		std::copy_n(values.begin(), count, sorted.begin());
		std::sort(sorted.begin(), sorted.begin() + count);
		double total = 0.0;
		for (size_t i = 0; i < count; ++i) total += sorted[i];
		result.average = total / static_cast<double>(count);
		result.p95 = sorted[std::min(count - 1, (count * 95 + 99) / 100 - 1)];
		result.p99 = sorted[std::min(count - 1, (count * 99 + 99) / 100 - 1)];
		result.maximum = sorted[count - 1];
		return result;
	}
};

std::string FormatDav2Timing(
	std::string_view name,
	const Dav2TimingSummary& value
) {
	return fmt::format(
		"{}[n={} avg={:.2f} p95={:.2f} p99={:.2f} max={:.2f}]",
		name, value.count, value.average, value.p95, value.p99, value.maximum);
}

}

struct DepthAnythingV2Provider::Impl {
	struct CaptureSlot {
		winrt::com_ptr<ID3D11Texture2D> gpuInput;
		winrt::com_ptr<ID3D11UnorderedAccessView> gpuInputUav;
		winrt::com_ptr<ID3D11Texture2D> staging;
		std::chrono::steady_clock::time_point submittedAt{};
		uint64_t generation = 0;
		uint64_t frameId = 0;
		bool pending = false;
	};

	struct InferenceJob {
		std::vector<float> values;
		double captureLatencyMs = 0.0;
		double readbackCopyMs = 0.0;
		uint64_t generation = 0;
		uint64_t frameId = 0;
	};

	struct InferenceResult {
		DepthInferenceOutput inference;
		std::string backend;
		double captureLatencyMs = 0.0;
		double readbackCopyMs = 0.0;
		double inferenceMs = 0.0;
		double percentileMs = 0.0;
		double workerTotalMs = 0.0;
		float p02 = 0.0f;
		float p98 = 1.0f;
		uint64_t generation = 0;
		uint64_t frameId = 0;
	};

	enum class BackendInitializationResult {
		Ready,
		Failed,
		Stopped
	};

	bool CreateTextures(FrameGuidanceExtent newExtent) noexcept {
		extent = newExtent;
		if (!device || !context || !extent.IsValid()) return false;
		const float scale = float(MODEL_LONG_SIDE) /
			float(std::max(extent.width, extent.height));
		inferenceWidth = AlignPatch(float(extent.width) * scale);
		inferenceHeight = AlignPatch(float(extent.height) * scale);

		constexpr UINT BIND =
			D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		constexpr UINT SHARED =
			D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE;
		currentDepth = DirectXHelper::CreateTexture2D(
			device, DXGI_FORMAT_R32_FLOAT, extent.width, extent.height, BIND);
		historyDepth = DirectXHelper::CreateTexture2D(
			device, DXGI_FORMAT_R32_FLOAT, extent.width, extent.height, BIND);
		filteredDepth = DirectXHelper::CreateTexture2D(
			device, DXGI_FORMAT_R32_FLOAT, extent.width, extent.height,
			BIND, D3D11_USAGE_DEFAULT, SHARED);
		rawDepth = DirectXHelper::CreateTexture2D(
			device, DXGI_FORMAT_R32_FLOAT, extent.width, extent.height,
			BIND, D3D11_USAGE_DEFAULT, SHARED);
		residual = DirectXHelper::CreateTexture2D(
			device, DXGI_FORMAT_R32_FLOAT, extent.width, extent.height,
			BIND, D3D11_USAGE_DEFAULT, SHARED);
		if (!currentDepth || !historyDepth || !filteredDepth || !rawDepth ||
			!residual ||
			FAILED(device->CreateShaderResourceView(
				currentDepth.get(), nullptr, currentDepthSrv.put())) ||
			FAILED(device->CreateShaderResourceView(
				historyDepth.get(), nullptr, historyDepthSrv.put())) ||
			FAILED(device->CreateUnorderedAccessView(
				currentDepth.get(), nullptr, currentDepthUav.put())) ||
			FAILED(device->CreateUnorderedAccessView(
				rawDepth.get(), nullptr, rawDepthUav.put())) ||
			FAILED(device->CreateUnorderedAccessView(
				filteredDepth.get(), nullptr, filteredDepthUav.put())) ||
			FAILED(device->CreateUnorderedAccessView(
				residual.get(), nullptr, residualUav.put()))) {
			return false;
		}
		modelDepth = DirectXHelper::CreateTexture2D(
			device, DXGI_FORMAT_R32_FLOAT, inferenceWidth, inferenceHeight,
			D3D11_BIND_SHADER_RESOURCE);
		if (!modelDepth || FAILED(device->CreateShaderResourceView(
			modelDepth.get(), nullptr, modelDepthSrv.put()))) return false;

		D3D11_TEXTURE2D_DESC gpuInputDesc{
			.Width = inferenceWidth,
			.Height = inferenceHeight,
			.MipLevels = 1,
			.ArraySize = 3,
			.Format = DXGI_FORMAT_R32_FLOAT,
			.SampleDesc{ .Count = 1 },
			.Usage = D3D11_USAGE_DEFAULT,
			.BindFlags = D3D11_BIND_UNORDERED_ACCESS
		};
		D3D11_TEXTURE2D_DESC stagingDesc = gpuInputDesc;
		stagingDesc.Usage = D3D11_USAGE_STAGING;
		stagingDesc.BindFlags = 0;
		stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		D3D11_UNORDERED_ACCESS_VIEW_DESC inputUavDesc{
			.Format = DXGI_FORMAT_R32_FLOAT,
			.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY,
			.Texture2DArray{
				.MipSlice = 0,
				.FirstArraySlice = 0,
				.ArraySize = 3
			}
		};
		for (CaptureSlot& slot : captureSlots) {
			slot = {};
			if (FAILED(device->CreateTexture2D(
				&gpuInputDesc, nullptr, slot.gpuInput.put())) ||
				FAILED(device->CreateUnorderedAccessView(
					slot.gpuInput.get(), &inputUavDesc, slot.gpuInputUav.put())) ||
				FAILED(device->CreateTexture2D(
					&stagingDesc, nullptr, slot.staging.put()))) {
				return false;
			}
		}

		winrt::com_ptr<ID3DBlob> shaderBlob;
		if (!temporalShader) {
			if (!DirectXHelper::CompileComputeShader(
				TEMPORAL_DEPTH_HLSL, "Filter", shaderBlob.put(),
				"FrameGuidance/DAV2_Temporal.hlsl") ||
				FAILED(device->CreateComputeShader(
					shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(),
					nullptr, temporalShader.put()))) {
				return false;
			}
			const D3D11_BUFFER_DESC desc{
				.ByteWidth = 32,
				.Usage = D3D11_USAGE_DYNAMIC,
				.BindFlags = D3D11_BIND_CONSTANT_BUFFER,
				.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE
			};
			if (FAILED(device->CreateBuffer(
				&desc, nullptr, temporalParams.put()))) return false;
		}
		if (!preprocessShader) {
			if (!DirectXHelper::CompileComputeShader(
				PREPROCESS_DEPTH_HLSL, "Preprocess", shaderBlob.put(),
				"FrameGuidance/DAV2_Preprocess.hlsl") ||
				FAILED(device->CreateComputeShader(
					shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(),
					nullptr, preprocessShader.put()))) return false;
			const D3D11_BUFFER_DESC desc{
				.ByteWidth = 32,
				.Usage = D3D11_USAGE_DYNAMIC,
				.BindFlags = D3D11_BIND_CONSTANT_BUFFER,
				.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE
			};
			if (FAILED(device->CreateBuffer(
				&desc, nullptr, preprocessParams.put()))) return false;
		}
		if (!postprocessShader) {
			if (!DirectXHelper::CompileComputeShader(
				POSTPROCESS_DEPTH_HLSL, "Postprocess", shaderBlob.put(),
				"FrameGuidance/DAV2_Postprocess.hlsl") ||
				FAILED(device->CreateComputeShader(
					shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(),
					nullptr, postprocessShader.put()))) return false;
			const D3D11_BUFFER_DESC desc{
				.ByteWidth = 32,
				.Usage = D3D11_USAGE_DYNAMIC,
				.BindFlags = D3D11_BIND_CONSTANT_BUFFER,
				.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE
			};
			if (FAILED(device->CreateBuffer(
				&desc, nullptr, postprocessParams.put()))) return false;
		}
		preprocessSource = nullptr;
		preprocessSourceSrv = nullptr;
		const float zeros[4]{};
		context->ClearUnorderedAccessViewFloat(currentDepthUav.get(), zeros);
		context->ClearUnorderedAccessViewFloat(rawDepthUav.get(), zeros);
		context->ClearUnorderedAccessViewFloat(filteredDepthUav.get(), zeros);
		context->ClearUnorderedAccessViewFloat(residualUav.get(), zeros);
		context->CopyResource(historyDepth.get(), currentDepth.get());
		historyValid = false;
		hasLearnedDepth = false;
		lastInferenceFrameId = std::numeric_limits<uint64_t>::max();
		lastCaptureFrameId = std::numeric_limits<uint64_t>::max();
		nextCaptureTime = {};
		return true;
	}

	bool FindAdapter(uint32_t& index, bool& isNvidia, LUID& luid) noexcept {
		DXGI_ADAPTER_DESC3 selected{};
		if (!adapter || FAILED(adapter->GetDesc3(&selected))) return false;
		isNvidia = selected.VendorId == 0x10de;
		luid = selected.AdapterLuid;
		winrt::com_ptr<IDXGIFactory1> factory;
		if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(factory.put())))) return false;
		for (uint32_t i = 0;; ++i) {
			winrt::com_ptr<IDXGIAdapter1> candidate;
			if (factory->EnumAdapters1(i, candidate.put()) == DXGI_ERROR_NOT_FOUND) break;
			DXGI_ADAPTER_DESC1 desc{};
			if (SUCCEEDED(candidate->GetDesc1(&desc)) &&
				desc.AdapterLuid.HighPart == selected.AdapterLuid.HighPart &&
				desc.AdapterLuid.LowPart == selected.AdapterLuid.LowPart) {
				index = i;
				return true;
			}
		}
		return false;
	}

	BackendInitializationResult InitializeBackends() noexcept {
		uint32_t adapterIndex = 0;
		bool isNvidia = false;
		LUID adapterLuid{};
		if (!FindAdapter(adapterIndex, isNvidia, adapterLuid)) {
			return BackendInitializationResult::Failed;
		}

		const auto exeDirectory = Win32Helper::GetExePath().parent_path();
		const auto providerDirectory = exeDirectory / L"FrameGuidance";
		const CudaIdentity cuda = isNvidia ? FindCudaDevice(adapterLuid) :
			CudaIdentity{};
		const std::string tensorRTVersion = FileVersionKey(
			providerDirectory / L"TensorRT" / L"nvinfer_10.dll");
		const std::string cudaRuntimeVersion = FileVersionKey(
			FindModulePath(L"cudart64_12.dll"));
		const std::filesystem::path cacheKey = fmt::format(
			"{}_ort1.24.4_trt{}_cudart{}_driver{}_sm{}{}_fp16_int8off_opt3_{}x{}_cuda{}",
			MODEL_SHA256, tensorRTVersion, cudaRuntimeVersion,
			cuda.driverVersion, cuda.computeMajor, cuda.computeMinor,
			inferenceWidth, inferenceHeight, cuda.deviceIndex);
		const std::filesystem::path cacheDirectory =
			LocalAppDataPath() / L"Magpie" / L"FrameGuidance" /
			L"TensorRTCache" / cacheKey;
		const std::filesystem::path modelPath = exeDirectory / MODEL_RELATIVE_PATH;

		backendConfig = {
			.modelPath = modelPath,
			.runtimeDirectory = providerDirectory / L"TensorRT",
			.engineCacheDirectory = cacheDirectory,
			.adapterIndex = cuda.deviceIndex,
			.inputWidth = inferenceWidth,
			.inputHeight = inferenceHeight,
			.isNvidiaAdapter = isNvidia && cuda.valid
		};
		const uint64_t before = QueryVideoMemoryUsage(adapter);
		tensorRT = AcquireSharedTensorRTDepthBackend(backendConfig);
		while (tensorRT &&
			tensorRT->State() == SharedDepthBackendState::Initializing) {
			std::unique_lock lock(workerMutex);
			if (workerCv.wait_for(lock, std::chrono::milliseconds(20), [&] {
				return workerStop;
			})) {
				Logger::Get().Info(
					"Frame Guidance stopped waiting for shared TensorRT initialization");
				return BackendInitializationResult::Stopped;
			}
		}
		{
			std::lock_guard lock(workerMutex);
			if (workerStop) return BackendInitializationResult::Stopped;
		}
		const bool tensorRTReady = tensorRT &&
			tensorRT->State() == SharedDepthBackendState::Ready;
		const uint64_t afterTensorRT = QueryVideoMemoryUsage(adapter);
		bool directMLReady = false;
		if (!tensorRTReady) {
			directML = CreateDirectMLDepthBackend();
			DepthInferenceConfig dmlConfig = backendConfig;
			dmlConfig.runtimeDirectory = providerDirectory / L"DirectML";
			dmlConfig.adapterIndex = adapterIndex;
			dmlConfig.isNvidiaAdapter = isNvidia;
			directMLReady = directML && directML->Initialize(dmlConfig);
			if (directMLReady) backendConfig = std::move(dmlConfig);
		}
		const uint64_t afterDirectML = QueryVideoMemoryUsage(adapter);
		Logger::Get().Info(fmt::format(
			"Frame Guidance DAV2 Small FP16 SHA={} opset=14 input={}x{} interval={}; "
			"TRT={} CUDA={} driver={} SM={}.{} cudaDevice={}; "
			"TensorRT={} VRAMDelta={:.1f} MiB, DirectML={} VRAMDelta={:.1f} MiB",
			MODEL_SHA256, inferenceWidth, inferenceHeight, inferenceInterval,
			tensorRTVersion, cudaRuntimeVersion, cuda.driverVersion,
			cuda.computeMajor, cuda.computeMinor, cuda.deviceIndex, tensorRTReady,
			VideoMemoryDeltaMiB(afterTensorRT, before), directMLReady,
			VideoMemoryDeltaMiB(afterDirectML, afterTensorRT)));
		useTensorRT = tensorRTReady;
		return tensorRTReady || directMLReady ?
			BackendInitializationResult::Ready :
			BackendInitializationResult::Failed;
	}

	bool EnsurePreprocessSource(ID3D11Texture2D* color) noexcept {
		D3D11_TEXTURE2D_DESC desc{};
		color->GetDesc(&desc);
		if (desc.Width != extent.width || desc.Height != extent.height ||
			(desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM &&
				desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM)) return false;
		if (preprocessSource.get() == color && preprocessSourceSrv) return true;
		preprocessSource.copy_from(color);
		preprocessSourceSrv = nullptr;
		return SUCCEEDED(device->CreateShaderResourceView(
			color, nullptr, preprocessSourceSrv.put()));
	}

	bool ScheduleCapture(const FrameGuidanceFrame& frame) noexcept {
		CaptureSlot* slot = nullptr;
		for (CaptureSlot& candidate : captureSlots) {
			if (!candidate.pending) {
				slot = &candidate;
				break;
			}
		}
		if (!slot) return false;
		if (!EnsurePreprocessSource(frame.color)) return false;
		D3D11_MAPPED_SUBRESOURCE mapped{};
		if (FAILED(context->Map(
			preprocessParams.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
			return false;
		}
		struct Params {
			uint32_t sourceWidth, sourceHeight, inferenceWidth, inferenceHeight;
			uint32_t padding0, padding1, padding2, padding3;
		};
		*static_cast<Params*>(mapped.pData) = {
			extent.width, extent.height, inferenceWidth, inferenceHeight,
			0, 0, 0, 0
		};
		context->Unmap(preprocessParams.get(), 0);
		ID3D11ShaderResourceView* srv = preprocessSourceSrv.get();
		ID3D11UnorderedAccessView* uav = slot->gpuInputUav.get();
		ID3D11Buffer* params = preprocessParams.get();
		ID3D11SamplerState* sampler = resources->GetSampler(
			D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP);
		context->CSSetShader(preprocessShader.get(), nullptr, 0);
		context->CSSetShaderResources(0, 1, &srv);
		context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
		context->CSSetConstantBuffers(0, 1, &params);
		context->CSSetSamplers(0, 1, &sampler);
		context->Dispatch(
			(inferenceWidth + 7) / 8, (inferenceHeight + 7) / 8, 1);
		ID3D11ShaderResourceView* nullSrv = nullptr;
		ID3D11UnorderedAccessView* nullUav = nullptr;
		context->CSSetShaderResources(0, 1, &nullSrv);
		context->CSSetUnorderedAccessViews(0, 1, &nullUav, nullptr);
		context->CSSetShader(nullptr, nullptr, 0);
		context->CopyResource(slot->staging.get(), slot->gpuInput.get());
		slot->submittedAt = std::chrono::steady_clock::now();
		lastCaptureTime = slot->submittedAt;
		slot->generation = generation;
		slot->frameId = frame.frameId;
		slot->pending = true;
		lastCaptureFrameId = frame.frameId;
		return true;
	}

	static bool CalculatePercentiles(
		const DepthInferenceOutput& inference,
		float& p02,
		float& p98
	) noexcept {
		if (inference.values.empty()) return false;
		std::vector<float> sorted;
		sorted.reserve(inference.values.size());
		for (float value : inference.values) {
			if (std::isfinite(value)) sorted.push_back(value);
		}
		if (sorted.size() < 32) return false;
		const size_t p02Index = size_t(double(sorted.size() - 1) * 0.02);
		const size_t p98Index = size_t(double(sorted.size() - 1) * 0.98);
		std::nth_element(sorted.begin(), sorted.begin() + p02Index, sorted.end());
		p02 = sorted[p02Index];
		std::nth_element(sorted.begin(), sorted.begin() + p98Index, sorted.end());
		p98 = sorted[p98Index];
		if (!(p98 > p02 + 1e-6f)) return false;
		return true;
	}

	void QueueJob(InferenceJob job) noexcept {
		{
			std::lock_guard lock(workerMutex);
			if (pendingJob) ++droppedJobCount;
			pendingJob = std::move(job);
			workerHasPending.store(true, std::memory_order_release);
		}
		workerCv.notify_one();
	}

	void PollCaptures() noexcept {
		for (CaptureSlot& slot : captureSlots) {
			if (!slot.pending) continue;
			if (slot.generation != generation) {
				slot.pending = false;
				continue;
			}
			std::array<D3D11_MAPPED_SUBRESOURCE, 3> mapped{};
			uint32_t mappedCount = 0;
			bool ready = true;
			for (uint32_t slice = 0; slice < 3; ++slice) {
				const HRESULT hr = context->Map(
					slot.staging.get(), D3D11CalcSubresource(0, slice, 1),
					D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped[slice]);
				if (FAILED(hr)) {
					ready = false;
					break;
				}
				++mappedCount;
			}
			if (!ready) {
				for (uint32_t slice = 0; slice < mappedCount; ++slice) {
					context->Unmap(
						slot.staging.get(), D3D11CalcSubresource(0, slice, 1));
				}
				continue;
			}
			const auto readbackCopyStart = std::chrono::steady_clock::now();
			InferenceJob job;
			const size_t plane = size_t(inferenceWidth) * inferenceHeight;
			job.values.resize(plane * 3);
			for (uint32_t slice = 0; slice < 3; ++slice) {
				float* destination = job.values.data() + plane * slice;
				for (uint32_t y = 0; y < inferenceHeight; ++y) {
					std::memcpy(
						destination + size_t(y) * inferenceWidth,
						static_cast<const uint8_t*>(mapped[slice].pData) +
							size_t(y) * mapped[slice].RowPitch,
						size_t(inferenceWidth) * sizeof(float));
				}
				context->Unmap(
					slot.staging.get(), D3D11CalcSubresource(0, slice, 1));
			}
			job.captureLatencyMs = std::chrono::duration<double, std::milli>(
				std::chrono::steady_clock::now() - slot.submittedAt).count();
			job.readbackCopyMs = std::chrono::duration<double, std::milli>(
				std::chrono::steady_clock::now() - readbackCopyStart).count();
			job.generation = slot.generation;
			job.frameId = slot.frameId;
			slot.pending = false;
			QueueJob(std::move(job));
		}
	}

	bool InitializeDirectMLFallback() noexcept {
		if (directML && directML->IsReady()) {
			useTensorRT = false;
			return true;
		}
		const auto exeDirectory = Win32Helper::GetExePath().parent_path();
		uint32_t adapterIndex = 0;
		bool isNvidia = false;
		LUID ignored{};
		if (!FindAdapter(adapterIndex, isNvidia, ignored)) return false;
		DepthInferenceConfig config = backendConfig;
		config.runtimeDirectory = exeDirectory / L"FrameGuidance" / L"DirectML";
		config.adapterIndex = adapterIndex;
		config.isNvidiaAdapter = isNvidia;
		directML = CreateDirectMLDepthBackend();
		if (!directML || !directML->Initialize(config)) return false;
		backendConfig = std::move(config);
		useTensorRT = false;
		return true;
	}

	bool RunInference(
		const DepthInferenceInput& input,
		DepthInferenceOutput& output
	) noexcept {
		return useTensorRT ?
			(tensorRT && tensorRT->Run(input, output)) :
			(directML && directML->Run(input, output));
	}

	std::string_view ActiveBackendName() const noexcept {
		return useTensorRT ?
			(tensorRT ? tensorRT->Name() : "unavailable") :
			(directML ? directML->Name() : "unavailable");
	}

	void WorkerMain() noexcept {
		const HRESULT apartment = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		workerBusy.store(true, std::memory_order_release);
		const BackendInitializationResult initialization = InitializeBackends();
		if (initialization == BackendInitializationResult::Stopped) {
			workerBusy.store(false, std::memory_order_release);
			if (SUCCEEDED(apartment)) CoUninitialize();
			return;
		}
		bool initialized = initialization == BackendInitializationResult::Ready;
		// The process-wide TensorRT backend is warmed once by its initializer.
		// DirectML remains session-owned and therefore needs a local warmup.
		if (initialized && !useTensorRT) {
			std::vector<float> warmup(
				size_t(inferenceWidth) * inferenceHeight * 3, 0.0f);
			DepthInferenceOutput ignored;
			const auto start = std::chrono::steady_clock::now();
			initialized = RunInference({
				.values = warmup.data(),
				.valueCount = warmup.size(),
				.width = inferenceWidth,
				.height = inferenceHeight
			}, ignored);
			Logger::Get().Info(fmt::format(
				"Frame Guidance DAV2 async warmup backend={} result={} time={:.1f} ms",
				ActiveBackendName(), initialized,
				std::chrono::duration<double, std::milli>(
					std::chrono::steady_clock::now() - start).count()));
		} else if (initialized) {
			Logger::Get().Info(
				"Frame Guidance DAV2 using process-wide prewarmed TensorRT backend");
		}
		workerReady.store(initialized, std::memory_order_release);
		workerBusy.store(false, std::memory_order_release);
		if (!initialized) {
			workerFailed.store(true, std::memory_order_release);
			if (SUCCEEDED(apartment)) CoUninitialize();
			return;
		}

		while (true) {
			InferenceJob job;
			{
				std::unique_lock lock(workerMutex);
				workerCv.wait(lock, [&] { return workerStop || pendingJob.has_value(); });
				if (workerStop) break;
				job = std::move(*pendingJob);
				pendingJob.reset();
				workerHasPending.store(false, std::memory_order_release);
			}
			workerBusy.store(true, std::memory_order_release);
			InferenceResult result;
			result.captureLatencyMs = job.captureLatencyMs;
			result.readbackCopyMs = job.readbackCopyMs;
			result.generation = job.generation;
			result.frameId = job.frameId;
			const auto workerStart = std::chrono::steady_clock::now();
			const auto inferenceStart = workerStart;
			bool succeeded = RunInference({
				.values = job.values.data(),
				.valueCount = job.values.size(),
				.width = inferenceWidth,
				.height = inferenceHeight
			}, result.inference);
			result.inferenceMs = std::chrono::duration<double, std::milli>(
				std::chrono::steady_clock::now() - inferenceStart).count();
			if (!succeeded && useTensorRT &&
				InitializeDirectMLFallback()) {
				result.inference = {};
				const auto fallbackStart = std::chrono::steady_clock::now();
				succeeded = RunInference({
					.values = job.values.data(),
					.valueCount = job.values.size(),
					.width = inferenceWidth,
					.height = inferenceHeight
				}, result.inference);
				result.inferenceMs = std::chrono::duration<double, std::milli>(
					std::chrono::steady_clock::now() - fallbackStart).count();
			}
			const auto percentileStart = std::chrono::steady_clock::now();
			succeeded = succeeded && CalculatePercentiles(
				result.inference, result.p02, result.p98);
			result.percentileMs = std::chrono::duration<double, std::milli>(
				std::chrono::steady_clock::now() - percentileStart).count();
			result.workerTotalMs = std::chrono::duration<double, std::milli>(
				std::chrono::steady_clock::now() - workerStart).count();
			result.backend = std::string(ActiveBackendName());
			if (succeeded) {
				std::lock_guard lock(workerMutex);
				if (completedResult) ++droppedResultCount;
				completedResult = std::move(result);
			} else {
				Logger::Get().Warn(fmt::format(
					"Frame Guidance {} async inference failed at frameId={}",
					ActiveBackendName(), job.frameId));
			}
			workerBusy.store(false, std::memory_order_release);
		}
		if (SUCCEEDED(apartment)) CoUninitialize();
	}

	bool StartWorker() noexcept {
		StopWorker();
		{
			std::lock_guard lock(workerMutex);
			workerStop = false;
			pendingJob.reset();
			completedResult.reset();
		}
		workerReady.store(false, std::memory_order_release);
		workerFailed.store(false, std::memory_order_release);
		workerHasPending.store(false, std::memory_order_release);
		workerBusy.store(true, std::memory_order_release);
		try {
			worker = std::thread([this] { WorkerMain(); });
		} catch (...) {
			workerBusy.store(false, std::memory_order_release);
			workerFailed.store(true, std::memory_order_release);
			Logger::Get().Error("Create Frame Guidance DAV2 worker failed");
			return false;
		}
		return true;
	}

	void StopWorker() noexcept {
		{
			std::lock_guard lock(workerMutex);
			workerStop = true;
			pendingJob.reset();
			workerHasPending.store(false, std::memory_order_release);
		}
		workerCv.notify_all();
		if (worker.joinable()) worker.join();
		workerReady.store(false, std::memory_order_release);
		workerBusy.store(false, std::memory_order_release);
	}

	bool ConsumeResult(InferenceResult& result) noexcept {
		std::lock_guard lock(workerMutex);
		if (!completedResult) return false;
		result = std::move(*completedResult);
		completedResult.reset();
		return true;
	}

	bool ApplyResult(InferenceResult& result) noexcept {
		if (result.generation != generation ||
			result.inference.width != inferenceWidth ||
			result.inference.height != inferenceHeight ||
			result.inference.values.size() !=
				size_t(inferenceWidth) * inferenceHeight) return false;
		const auto applyStart = std::chrono::steady_clock::now();
		if (!percentilesValid) {
			emaP02 = result.p02;
			emaP98 = result.p98;
			percentilesValid = true;
		} else {
			constexpr float ALPHA = 0.05f;
			emaP02 = std::lerp(emaP02, result.p02, ALPHA);
			emaP98 = std::lerp(emaP98, result.p98, ALPHA);
		}
		context->UpdateSubresource(
			modelDepth.get(), 0, nullptr, result.inference.values.data(),
			inferenceWidth * sizeof(float), 0);
		D3D11_MAPPED_SUBRESOURCE mapped{};
		if (FAILED(context->Map(
			postprocessParams.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
			return false;
		}
		struct Params {
			uint32_t sourceWidth, sourceHeight, inferenceWidth, inferenceHeight;
			float p02, reciprocalRange;
			uint32_t padding0, padding1;
		};
		*static_cast<Params*>(mapped.pData) = {
			extent.width, extent.height, inferenceWidth, inferenceHeight,
			emaP02, 1.0f / std::max(emaP98 - emaP02, 1e-6f), 0, 0
		};
		context->Unmap(postprocessParams.get(), 0);
		ID3D11ShaderResourceView* srv = modelDepthSrv.get();
		ID3D11UnorderedAccessView* uavs[]{
			currentDepthUav.get(), rawDepthUav.get()
		};
		ID3D11Buffer* params = postprocessParams.get();
		ID3D11SamplerState* sampler = resources->GetSampler(
			D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP);
		context->CSSetShader(postprocessShader.get(), nullptr, 0);
		context->CSSetShaderResources(0, 1, &srv);
		context->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs, nullptr);
		context->CSSetConstantBuffers(0, 1, &params);
		context->CSSetSamplers(0, 1, &sampler);
		context->Dispatch((extent.width + 7) / 8, (extent.height + 7) / 8, 1);
		ID3D11ShaderResourceView* nullSrv = nullptr;
		ID3D11UnorderedAccessView* nullUavs[ARRAYSIZE(uavs)]{};
		context->CSSetShaderResources(0, 1, &nullSrv);
		context->CSSetUnorderedAccessViews(0, ARRAYSIZE(nullUavs), nullUavs, nullptr);
		context->CSSetShader(nullptr, nullptr, 0);
		const double applyMs = std::chrono::duration<double, std::milli>(
			std::chrono::steady_clock::now() - applyStart).count();
		hasLearnedDepth = true;
		lastInferenceFrameId = result.frameId;
		++inferenceCount;
		captureLatencyTimings.Add(result.captureLatencyMs);
		readbackCopyTimings.Add(result.readbackCopyMs);
		inferenceTimings.Add(result.inferenceMs);
		percentileTimings.Add(result.percentileMs);
		workerTotalTimings.Add(result.workerTotalMs);
		applyTimings.Add(applyMs);
		const double cooldownMs = std::clamp(result.workerTotalMs * 2.0, 33.0, 500.0);
		nextCaptureTime = std::chrono::steady_clock::now() +
			std::chrono::milliseconds(static_cast<int64_t>(std::ceil(cooldownMs)));
		if (inferenceCount <= 4) {
			Logger::Get().Info(fmt::format(
				"Frame Guidance depth async result sourceFrame={} backend={} input={}x{} "
				"captureLatency={:.2f} ms readbackCopy={:.2f} ms ORT={:.2f} ms "
				"percentile={:.2f} ms workerTotal={:.2f} ms apply={:.2f} ms "
				"cooldown={:.0f} ms P02={:.5f} P98={:.5f}",
				result.frameId, result.backend, inferenceWidth, inferenceHeight,
				result.captureLatencyMs, result.readbackCopyMs, result.inferenceMs,
				result.percentileMs, result.workerTotalMs, applyMs, cooldownMs,
				emaP02, emaP98));
		} else if (inferenceCount % 30 == 0) {
			Logger::Get().Info(fmt::format(
				"Frame Guidance depth timing window: resultCount={} backend={} input={}x{} "
				"{} {} {} {} {} {} droppedJobs={} droppedResults={} "
				"gpuBudgetSkips={} lastNgxGpuBudget={:.2f} ms",
				inferenceCount, result.backend, inferenceWidth, inferenceHeight,
				FormatDav2Timing("captureLatency", captureLatencyTimings.Summarize()),
				FormatDav2Timing("readbackCopy", readbackCopyTimings.Summarize()),
				FormatDav2Timing("ORT", inferenceTimings.Summarize()),
				FormatDav2Timing("percentile", percentileTimings.Summarize()),
				FormatDav2Timing("workerTotal", workerTotalTimings.Summarize()),
				FormatDav2Timing("apply", applyTimings.Summarize()),
				droppedJobCount, droppedResultCount, gpuBudgetSkipCount,
				lastNgxGpuBudgetMs));
		}
		return true;
	}

	bool AnyCapturePending() const noexcept {
		return std::ranges::any_of(
			captureSlots, [](const CaptureSlot& slot) { return slot.pending; });
	}

	bool ShouldCapture(
		const FrameGuidanceFrame& frame,
		bool reset
	) noexcept {
		if (!workerReady.load(std::memory_order_acquire) ||
			workerFailed.load(std::memory_order_acquire) ||
			workerBusy.load(std::memory_order_acquire) ||
			workerHasPending.load(std::memory_order_acquire) ||
			AnyCapturePending()) return false;
		const auto now = std::chrono::steady_clock::now();
		bool forceForMaximumAge = false;
		if (!reset && lastCaptureTime != std::chrono::steady_clock::time_point{}) {
			const DlssnrGpuTimingSnapshot timing =
				FrameGuidancePerformance::GetDlssnrGpuTiming();
			if (timing.sampleCount) {
				const double budgetMs = std::max(timing.latestMs, timing.emaMs);
				lastNgxGpuBudgetMs = budgetMs;
				const auto depthAge = now - lastCaptureTime;
				const auto maximumAge = budgetMs >= NGX_GPU_HARD_BUDGET_MS ?
					NGX_GPU_HARD_MAX_DEPTH_AGE : NGX_GPU_SOFT_MAX_DEPTH_AGE;
				if (budgetMs >= NGX_GPU_SOFT_BUDGET_MS) {
					if (depthAge < maximumAge) {
						++gpuBudgetSkipCount;
						return false;
					}
					forceForMaximumAge = true;
				}
			}
		}
		if (!reset && !forceForMaximumAge && now < nextCaptureTime) return false;
		return reset || lastCaptureFrameId == std::numeric_limits<uint64_t>::max() ||
			frame.frameId <= lastCaptureFrameId ||
			frame.frameId - lastCaptureFrameId >= inferenceInterval;
	}

	bool TemporalFilter(
		const MotionVectorProviderOutput* motionGuidance,
		bool reset,
		bool hasCurrentInference
	) noexcept {
		const bool resetHistory = reset || !historyValid;
		if (resetHistory) {
			// Avoid reading undefined history even though the reset shader path gives
			// it zero weight. This also makes the residual exactly zero on reset.
			context->CopyResource(historyDepth.get(), currentDepth.get());
		}
		winrt::com_ptr<ID3D11ShaderResourceView> motionSrv;
		winrt::com_ptr<ID3D11ShaderResourceView> confidenceSrv;
		const bool hasMotion = motionGuidance &&
			!motionGuidance->motion.metadata.isZero &&
			!motionGuidance->confidence.metadata.isZero &&
			motionGuidance->motion.texture && motionGuidance->confidence.texture &&
			SUCCEEDED(device->CreateShaderResourceView(
				motionGuidance->motion.texture, nullptr, motionSrv.put())) &&
			SUCCEEDED(device->CreateShaderResourceView(
				motionGuidance->confidence.texture, nullptr, confidenceSrv.put()));
		struct Params {
			uint32_t width, height, reset, hasMotion;
			uint32_t hasCurrentInference, padding0, padding1, padding2;
		};
		D3D11_MAPPED_SUBRESOURCE mapped{};
		if (FAILED(context->Map(
			temporalParams.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
			return false;
		}
		*static_cast<Params*>(mapped.pData) = {
			extent.width, extent.height,
			resetHistory ? 1u : 0u, hasMotion ? 1u : 0u,
			hasCurrentInference ? 1u : 0u, 0, 0, 0
		};
		context->Unmap(temporalParams.get(), 0);
		ID3D11ShaderResourceView* srvs[]{
			currentDepthSrv.get(), historyDepthSrv.get(), motionSrv.get(),
			confidenceSrv.get()
		};
		ID3D11UnorderedAccessView* uavs[]{
			filteredDepthUav.get(), residualUav.get()
		};
		ID3D11Buffer* buffers[]{ temporalParams.get() };
		ID3D11SamplerState* sampler = resources->GetSampler(
			D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP);
		context->CSSetShader(temporalShader.get(), nullptr, 0);
		context->CSSetShaderResources(0, ARRAYSIZE(srvs), srvs);
		context->CSSetUnorderedAccessViews(0, ARRAYSIZE(uavs), uavs, nullptr);
		context->CSSetConstantBuffers(0, 1, buffers);
		context->CSSetSamplers(0, 1, &sampler);
		context->Dispatch((extent.width + 7) / 8, (extent.height + 7) / 8, 1);
		ID3D11ShaderResourceView* nullSrvs[ARRAYSIZE(srvs)]{};
		ID3D11UnorderedAccessView* nullUavs[ARRAYSIZE(uavs)]{};
		context->CSSetShaderResources(0, ARRAYSIZE(nullSrvs), nullSrvs);
		context->CSSetUnorderedAccessViews(0, ARRAYSIZE(nullUavs), nullUavs, nullptr);
		context->CSSetShader(nullptr, nullptr, 0);
		context->CopyResource(historyDepth.get(), filteredDepth.get());
		historyValid = true;
		return true;
	}

	DeviceResources* resources = nullptr;
	ID3D11Device5* device = nullptr;
	ID3D11DeviceContext4* context = nullptr;
	IDXGIAdapter4* adapter = nullptr;
	FrameGuidanceExtent extent{};
	uint32_t inferenceWidth = 0;
	uint32_t inferenceHeight = 0;
	std::array<CaptureSlot, CAPTURE_SLOT_COUNT> captureSlots;
	winrt::com_ptr<ID3D11Texture2D> preprocessSource;
	winrt::com_ptr<ID3D11ShaderResourceView> preprocessSourceSrv;
	winrt::com_ptr<ID3D11Texture2D> currentDepth;
	winrt::com_ptr<ID3D11Texture2D> historyDepth;
	winrt::com_ptr<ID3D11Texture2D> filteredDepth;
	winrt::com_ptr<ID3D11Texture2D> rawDepth;
	winrt::com_ptr<ID3D11Texture2D> residual;
	winrt::com_ptr<ID3D11Texture2D> modelDepth;
	winrt::com_ptr<ID3D11ShaderResourceView> currentDepthSrv;
	winrt::com_ptr<ID3D11ShaderResourceView> historyDepthSrv;
	winrt::com_ptr<ID3D11ShaderResourceView> modelDepthSrv;
	winrt::com_ptr<ID3D11UnorderedAccessView> currentDepthUav;
	winrt::com_ptr<ID3D11UnorderedAccessView> rawDepthUav;
	winrt::com_ptr<ID3D11UnorderedAccessView> filteredDepthUav;
	winrt::com_ptr<ID3D11UnorderedAccessView> residualUav;
	winrt::com_ptr<ID3D11ComputeShader> preprocessShader;
	winrt::com_ptr<ID3D11ComputeShader> postprocessShader;
	winrt::com_ptr<ID3D11ComputeShader> temporalShader;
	winrt::com_ptr<ID3D11Buffer> preprocessParams;
	winrt::com_ptr<ID3D11Buffer> postprocessParams;
	winrt::com_ptr<ID3D11Buffer> temporalParams;
	std::shared_ptr<SharedTensorRTDepthBackend> tensorRT;
	std::unique_ptr<IDepthInferenceBackend> directML;
	DepthInferenceConfig backendConfig{};
	std::mutex workerMutex;
	std::condition_variable workerCv;
	std::thread worker;
	std::optional<InferenceJob> pendingJob;
	std::optional<InferenceResult> completedResult;
	std::atomic<bool> workerReady = false;
	std::atomic<bool> workerFailed = false;
	std::atomic<bool> workerBusy = false;
	std::atomic<bool> workerHasPending = false;
	bool workerStop = false;
	FrameGuidanceResetReason resetReason = FrameGuidanceResetReason::Initialize;
	std::chrono::steady_clock::time_point nextCaptureTime{};
	std::chrono::steady_clock::time_point lastCaptureTime{};
	double lastNgxGpuBudgetMs = 0.0;
	float emaP02 = 0.0f;
	float emaP98 = 1.0f;
	uint64_t generation = 1;
	uint64_t lastCaptureFrameId = std::numeric_limits<uint64_t>::max();
	uint64_t lastInferenceFrameId = std::numeric_limits<uint64_t>::max();
	uint64_t inferenceCount = 0;
	uint64_t skippedInferenceCount = 0;
	uint64_t droppedJobCount = 0;
	uint64_t droppedResultCount = 0;
	uint64_t gpuBudgetSkipCount = 0;
	Dav2TimingWindow captureLatencyTimings;
	Dav2TimingWindow readbackCopyTimings;
	Dav2TimingWindow inferenceTimings;
	Dav2TimingWindow percentileTimings;
	Dav2TimingWindow workerTotalTimings;
	Dav2TimingWindow applyTimings;
	Dav2TimingWindow beginFrameTimings;
	uint32_t inferenceInterval = 1;
	bool percentilesValid = false;
	bool historyValid = false;
	bool hasLearnedDepth = false;
	bool useTensorRT = false;
};

DepthAnythingV2Provider::DepthAnythingV2Provider(uint32_t inferenceInterval) :
	_impl(std::make_unique<Impl>()) {
	_impl->inferenceInterval = std::clamp(inferenceInterval, 1u, 8u);
}
DepthAnythingV2Provider::~DepthAnythingV2Provider() {
	if (_impl) _impl->StopWorker();
}

bool DepthAnythingV2Provider::Initialize(
	DeviceResources& resources,
	FrameGuidanceExtent sourceExtent
) noexcept {
	Impl& impl = *_impl;
	impl.resources = &resources;
	impl.device = resources.GetD3DDevice();
	impl.context = resources.GetD3DDC();
	impl.adapter = resources.GetGraphicsAdapter();
	const std::filesystem::path modelPath =
		Win32Helper::GetExePath().parent_path() / MODEL_RELATIVE_PATH;
	const std::string sha = ComputeSha256(modelPath);
	if (sha != MODEL_SHA256) {
		Logger::Get().Warn(fmt::format(
			"Frame Guidance DAV2 model hash mismatch: expected {}, got {}",
			MODEL_SHA256, sha.empty() ? "unavailable" : sha));
		return false;
	}
	if (!impl.CreateTextures(sourceExtent)) return false;
	Logger::Get().Info(fmt::format(
		"Frame Guidance DAV2 async pipeline source={}x{} model={}x{} slots={} interval={} "
		"ngxBudget=soft/{:.1f}ms/{}ms hard/{:.1f}ms/{}ms",
		sourceExtent.width, sourceExtent.height,
		impl.inferenceWidth, impl.inferenceHeight,
		CAPTURE_SLOT_COUNT, impl.inferenceInterval,
		NGX_GPU_SOFT_BUDGET_MS, NGX_GPU_SOFT_MAX_DEPTH_AGE.count(),
		NGX_GPU_HARD_BUDGET_MS, NGX_GPU_HARD_MAX_DEPTH_AGE.count()));
	return impl.StartWorker();
}

bool DepthAnythingV2Provider::BeginFrame(
	const FrameGuidanceFrame& frame,
	DepthProviderOutput& output
) noexcept {
	Impl& impl = *_impl;
	if (!frame.color || frame.sourceExtent != impl.extent) {
		return false;
	}

	const auto start = std::chrono::steady_clock::now();
	const bool reset = impl.resetReason != FrameGuidanceResetReason::None;
	impl.PollCaptures();
	Impl::InferenceResult completed;
	const bool hasCompleted = impl.ConsumeResult(completed);
	const bool appliedInference = hasCompleted && impl.ApplyResult(completed);
	if (hasCompleted && !appliedInference &&
		completed.generation == impl.generation) {
		Logger::Get().Warn(fmt::format(
			"Frame Guidance discarded malformed async depth result frameId={}",
			completed.frameId));
	}
	if (impl.ShouldCapture(frame, reset) && !impl.ScheduleCapture(frame)) {
		Logger::Get().Warn(fmt::format(
			"Frame Guidance failed to schedule async depth capture frameId={}",
			frame.frameId));
	}
	if (!impl.TemporalFilter(
		frame.motionGuidance, reset, appliedInference && impl.hasLearnedDepth)) {
		return false;
	}

	const FrameGuidanceMetadata metadata = MakeMetadata(
		frame, impl.resetReason, !impl.hasLearnedDepth);
	output.depth = {
		.texture = impl.filteredDepth.get(),
		.format = DXGI_FORMAT_R32_FLOAT,
		.metadata = metadata
	};
	if (impl.hasLearnedDepth) {
		output.rawDepth = {
			.texture = impl.rawDepth.get(),
			.format = DXGI_FORMAT_R32_FLOAT,
			.metadata = metadata
		};
	}
	output.depthResidual = {
		.texture = impl.residual.get(),
		.format = DXGI_FORMAT_R32_FLOAT,
		.metadata = metadata
	};
	const auto elapsed = std::chrono::duration<double, std::milli>(
		std::chrono::steady_clock::now() - start).count();
	impl.beginFrameTimings.Add(elapsed);
	if (!appliedInference) ++impl.skippedInferenceCount;
	if (!appliedInference && (impl.skippedInferenceCount <= 4 ||
		impl.skippedInferenceCount % 120 == 0)) {
		Logger::Get().Info(fmt::format(
			"Frame Guidance depth async frameId={} interval={} skipped={} "
			"mode={} ready={} busy={} learned={} CPU submit={:.3f} ms",
			frame.frameId, impl.inferenceInterval, impl.skippedInferenceCount,
			frame.motionGuidance && !frame.motionGuidance->motion.metadata.isZero ?
				"reproject" : "hold-last",
			impl.workerReady.load(std::memory_order_acquire),
			impl.workerBusy.load(std::memory_order_acquire),
			impl.hasLearnedDepth,
			elapsed));
	}
	if (frame.frameId && frame.frameId % 120 == 0) {
		const DlssnrGpuTimingSnapshot timing =
			FrameGuidancePerformance::GetDlssnrGpuTiming();
		Logger::Get().Info(fmt::format(
			"Frame Guidance renderer timing window: frameId={} {} "
			"ngxGPU latest={:.2f} ms ema={:.2f} ms samples={} budgetSkips={}",
			frame.frameId,
			FormatDav2Timing("beginFrameCPU", impl.beginFrameTimings.Summarize()),
			timing.latestMs, timing.emaMs, timing.sampleCount,
			impl.gpuBudgetSkipCount));
	}
	impl.resetReason = FrameGuidanceResetReason::None;
	return true;
}

void DepthAnythingV2Provider::Reset(
	FrameGuidanceResetReason reason
) noexcept {
	Impl& impl = *_impl;
	impl.resetReason = reason;
	impl.historyValid = false;
	impl.hasLearnedDepth = false;
	impl.lastCaptureFrameId = std::numeric_limits<uint64_t>::max();
	impl.lastCaptureTime = {};
	impl.lastNgxGpuBudgetMs = 0.0;
	impl.lastInferenceFrameId = std::numeric_limits<uint64_t>::max();
	impl.percentilesValid = false;
	impl.nextCaptureTime = {};
	++impl.generation;
	for (Impl::CaptureSlot& slot : impl.captureSlots) slot.pending = false;
	{
		std::lock_guard lock(impl.workerMutex);
		impl.pendingJob.reset();
		impl.completedResult.reset();
		impl.workerHasPending.store(false, std::memory_order_release);
	}
	if (impl.context && impl.currentDepthUav && impl.rawDepthUav &&
		impl.filteredDepthUav && impl.residualUav) {
		const float zeros[4]{};
		impl.context->ClearUnorderedAccessViewFloat(impl.currentDepthUav.get(), zeros);
		impl.context->ClearUnorderedAccessViewFloat(impl.rawDepthUav.get(), zeros);
		impl.context->ClearUnorderedAccessViewFloat(impl.filteredDepthUav.get(), zeros);
		impl.context->ClearUnorderedAccessViewFloat(impl.residualUav.get(), zeros);
		impl.context->CopyResource(impl.historyDepth.get(), impl.currentDepth.get());
	}
}

bool DepthAnythingV2Provider::Resize(
	FrameGuidanceExtent sourceExtent
) noexcept {
	if (!_impl->resources) return false;
	_impl->StopWorker();
	_impl->tensorRT.reset();
	_impl->directML.reset();
	_impl->useTensorRT = false;
	++_impl->generation;
	_impl->captureLatencyTimings = {};
	_impl->readbackCopyTimings = {};
	_impl->inferenceTimings = {};
	_impl->percentileTimings = {};
	_impl->workerTotalTimings = {};
	_impl->applyTimings = {};
	_impl->beginFrameTimings = {};
	const bool result = _impl->CreateTextures(sourceExtent) &&
		_impl->StartWorker();
	if (result) _impl->resetReason = FrameGuidanceResetReason::Resize;
	return result;
}

}

#else

namespace Magpie {

struct DepthAnythingV2Provider::Impl {};
DepthAnythingV2Provider::DepthAnythingV2Provider(uint32_t) :
	_impl(std::make_unique<Impl>()) {}
DepthAnythingV2Provider::~DepthAnythingV2Provider() = default;
bool DepthAnythingV2Provider::Initialize(
	DeviceResources&, FrameGuidanceExtent) noexcept { return false; }
bool DepthAnythingV2Provider::BeginFrame(
	const FrameGuidanceFrame&, DepthProviderOutput&) noexcept { return false; }
void DepthAnythingV2Provider::Reset(FrameGuidanceResetReason) noexcept {}
bool DepthAnythingV2Provider::Resize(FrameGuidanceExtent) noexcept { return false; }

}

#endif
