#pragma once

#include <nvCVImage.h>

struct ID3D11Texture2D;

NvCV_Status NvCVImage_InitFromD3D11Texture(NvCVImage* image, ID3D11Texture2D* texture);
NvCV_Status NvCVImage_MapResource(NvCVImage* image, struct CUstream_st* stream);
NvCV_Status NvCVImage_UnmapResource(NvCVImage* image, struct CUstream_st* stream);
