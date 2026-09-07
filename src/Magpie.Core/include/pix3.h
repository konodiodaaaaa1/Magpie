#pragma once

// FidelityFX's D3D12 backend only needs PIX_COLOR at compile time. The PIX
// entry points themselves are loaded dynamically, so the WinPixEventRuntime
// development package is not a runtime dependency of Magpie.
#ifndef PIX_COLOR
#define PIX_COLOR(red, green, blue) \
	((UINT64)(0xff000000ull | ((UINT64)(red) << 16) | ((UINT64)(green) << 8) | (UINT64)(blue)))
#endif
