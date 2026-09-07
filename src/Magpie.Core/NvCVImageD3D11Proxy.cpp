#include "pch.h"
#include "NvCVImageD3D11Proxy.h"

namespace {

HMODULE GetNvCVImageModule() noexcept {
	static HMODULE module = []() noexcept {
		if (HMODULE loaded = GetModuleHandleW(L"NVCVImage.dll")) {
			return loaded;
		}

		wchar_t modulePath[MAX_PATH]{};
		const DWORD length = GetModuleFileNameW(nullptr, modulePath, ARRAYSIZE(modulePath));
		if (length != 0 && length < ARRAYSIZE(modulePath)) {
			std::wstring path(modulePath, length);
			const size_t separator = path.find_last_of(L"\\/");
			if (separator != std::wstring::npos) {
				path.resize(separator);
				path += L"\\NVCVImage.dll";
				if (HMODULE runtime = LoadLibraryExW(
					path.c_str(), nullptr,
					LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_SYSTEM32)) {
					return runtime;
				}
			}
		}
		return LoadLibraryExW(
			L"NVCVImage.dll", nullptr,
			LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_SYSTEM32);
	}();
	return module;
}

template <typename Function>
Function GetNvCVImageFunction(const char* name) noexcept {
	return reinterpret_cast<Function>(GetProcAddress(GetNvCVImageModule(), name));
}

}

NvCV_Status NvCVImage_InitFromD3D11Texture(NvCVImage* image, ID3D11Texture2D* texture) {
	using Function = NvCV_Status(__cdecl*)(NvCVImage*, ID3D11Texture2D*);
	const auto function = GetNvCVImageFunction<Function>("NvCVImage_InitFromD3D11Texture");
	return function ? function(image, texture) : NVCV_ERR_LIBRARY;
}

NvCV_Status NvCVImage_MapResource(NvCVImage* image, struct CUstream_st* stream) {
	using Function = NvCV_Status(__cdecl*)(NvCVImage*, struct CUstream_st*);
	const auto function = GetNvCVImageFunction<Function>("NvCVImage_MapResource");
	return function ? function(image, stream) : NVCV_ERR_LIBRARY;
}

NvCV_Status NvCVImage_UnmapResource(NvCVImage* image, struct CUstream_st* stream) {
	using Function = NvCV_Status(__cdecl*)(NvCVImage*, struct CUstream_st*);
	const auto function = GetNvCVImageFunction<Function>("NvCVImage_UnmapResource");
	return function ? function(image, stream) : NVCV_ERR_LIBRARY;
}
