// AMD-style Morphological Anti-Aliasing for Magpie.
// Adapted from https://github.com/GPUOpen-LibrariesAndSDKs/MLAA11.
// This port derives luma from RGB rather than expecting luma in alpha.
//
// Copyright (c) 2016 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

//!MAGPIE EFFECT
//!VERSION 4
//!SORT_NAME MLAA

//!PARAMETER
//!LABEL Edge threshold
//!DEFAULT 0.08
//!MIN 0.02
//!MAX 0.30
//!STEP 0.01
float threshold;

//!PARAMETER
//!LABEL Strength
//!DEFAULT 1.0
//!MIN 0.0
//!MAX 1.0
//!STEP 0.05
float strength;

//!TEXTURE
Texture2D INPUT;

//!TEXTURE
//!WIDTH INPUT_WIDTH
//!HEIGHT INPUT_HEIGHT
Texture2D OUTPUT;

//!TEXTURE
//!WIDTH INPUT_WIDTH
//!HEIGHT INPUT_HEIGHT
//!FORMAT R8G8_UNORM
Texture2D edgeMask;

//!TEXTURE
//!WIDTH INPUT_WIDTH
//!HEIGHT INPUT_HEIGHT
//!FORMAT R8G8B8A8_UNORM
Texture2D edgeCounts;

//!COMMON

static const uint MAX_EDGE_LENGTH = 7;

float Luma(float3 color) {
	return dot(color, float3(0.2126, 0.7152, 0.0722));
}

int2 ClampPos(int2 p) {
	return clamp(p, int2(0, 0), int2(GetInputSize()) - 1);
}

float4 LoadColor(int2 p) {
	return INPUT.Load(int3(ClampPos(p), 0));
}

float LoadLuma(int2 p) {
	return Luma(LoadColor(p).rgb);
}

int2 PixelPos(float2 pos) {
	return ClampPos(int2(pos * GetInputSize()));
}

//!PASS 1
//!DESC Edge detection
//!STYLE PS
//!IN INPUT
//!OUT edgeMask

float2 Pass1(float2 pos) {
	const int2 p = PixelPos(pos);
	const float center = LoadLuma(p);
	const float upper = LoadLuma(p + int2(0, -1));
	const float right = LoadLuma(p + int2(1, 0));

	// R: edge above this pixel. G: edge to the right of this pixel.
	return float2(abs(center - upper), abs(center - right)) > threshold;
}

//!PASS 2
//!DESC Morphological line search
//!STYLE PS
//!IN INPUT, edgeMask
//!OUT edgeCounts

float LoadEdge(int2 p, uint channel) {
	return edgeMask.Load(int3(ClampPos(p), 0))[channel];
}

// Low three bits contain the length; bit 3 records a found endpoint.
float EncodeCount(uint count, bool endpointFound) {
	return float(min(count, MAX_EDGE_LENGTH) + (endpointFound ? 8 : 0)) / 15.0;
}

float2 SearchEdge(int2 p, int2 negativeDir, int2 positiveDir, uint channel) {
	uint negativeCount = 0;
	uint positiveCount = 0;
	bool negativeActive = true;
	bool positiveActive = true;
	bool negativeEnd = false;
	bool positiveEnd = false;
	[unroll]
	for (uint i = 1; i <= MAX_EDGE_LENGTH; ++i) {
		if (negativeActive) {
			if (LoadEdge(p + negativeDir * int(i), channel) > 0.5) ++negativeCount;
			else { negativeActive = false; negativeEnd = true; }
		}
		if (positiveActive) {
			if (LoadEdge(p + positiveDir * int(i), channel) > 0.5) ++positiveCount;
			else { positiveActive = false; positiveEnd = true; }
		}
	}
	return float2(EncodeCount(negativeCount, negativeEnd),
		EncodeCount(positiveCount, positiveEnd));
}

float4 Pass2(float2 pos) {
	const int2 p = PixelPos(pos);
	const float2 edges = edgeMask.Load(int3(p, 0)).rg;
	float4 result = 0;

	if (edges.r > 0.5) {
		result.xy = SearchEdge(p, int2(-1, 0), int2(1, 0), 0);
	}
	if (edges.g > 0.5) {
		result.zw = SearchEdge(p, int2(0, 1), int2(0, -1), 1);
	}

	return result;
}

uint DecodeRaw(float value) {
	return (uint)round(saturate(value) * 15.0);
}

bool ColorsDiffer(int2 a, int2 b) {
	return abs(LoadLuma(a) - LoadLuma(b)) > threshold;
}

void BlendEdge(
	float2 encodedCounts,
	int2 edgePos,
	int2 acrossEdge,
	int2 alongEdge,
	bool inverse,
	inout float4 color) {

	const uint rawNegative = DecodeRaw(encodedCounts.x);
	const uint rawPositive = DecodeRaw(encodedCounts.y);
	const bool negativeEnd = rawNegative >= 8;
	const bool positiveEnd = rawPositive >= 8;
	if (!negativeEnd && !positiveEnd) {
		return;
	}

	uint negativeCount = rawNegative & 7;
	uint positiveCount = rawPositive & 7;
	const float4 adjacent = LoadColor(edgePos + acrossEdge);

	if (negativeCount + positiveCount == 0) {
		const float weight = 0.125 * strength;
		color.rgb = sqrt(lerp(color.rgb * color.rgb, adjacent.rgb * adjacent.rgb, weight));
		return;
	}

	if (!positiveEnd) positiveCount = MAX_EDGE_LENGTH + 1;
	if (!negativeEnd) negativeCount = MAX_EDGE_LENGTH + 1;

	const float length = negativeCount + positiveCount + 1.0;
	const float midpoint = length * 0.5;
	const float distance = negativeCount;
	uint shape = 0;

	if (ColorsDiffer(
		edgePos - alongEdge * int(negativeCount),
		edgePos - alongEdge * int(negativeCount + 1))) {
		shape |= 1;
	}
	if (ColorsDiffer(
		edgePos + alongEdge * int(positiveCount),
		edgePos + alongEdge * int(positiveCount + 1))) {
		shape |= 2;
	}

	const bool shouldBlend =
		(inverse && ((shape == 2 && float(negativeCount) <= midpoint) ||
			(shape == 1 && float(negativeCount) >= midpoint) || shape == 0)) ||
		(!inverse && ((shape == 2 && float(negativeCount) >= midpoint) ||
			(shape == 1 && float(negativeCount) <= midpoint) || shape == 3));

	if (shouldBlend) {
		const float h0 = abs((length - distance) / length - 0.5);
		const float h1 = abs((length - distance - 1.0) / length - 0.5);
		const float area = 0.5 * (h0 + h1) * strength;
		color.rgb = sqrt(lerp(color.rgb * color.rgb, adjacent.rgb * adjacent.rgb, area));
	}
}

//!PASS 3
//!DESC Morphological blending
//!STYLE PS
//!IN INPUT, edgeCounts
//!OUT OUTPUT

// Pass 3 is compiled as an independent shader. Keep its helpers in this
// pass section so the effect compiler includes them in the generated source.
uint DecodeRawPass3(float value) {
	return (uint)round(saturate(value) * 15.0);
}

bool ColorsDifferPass3(int2 a, int2 b) {
	return abs(LoadLuma(a) - LoadLuma(b)) > threshold;
}

void BlendEdgePass3(float2 encodedCounts, int2 edgePos, int2 acrossEdge,
	int2 alongEdge, bool inverse, inout float4 color) {
	const uint rawNegative = DecodeRawPass3(encodedCounts.x);
	const uint rawPositive = DecodeRawPass3(encodedCounts.y);
	const bool negativeEnd = rawNegative >= 8;
	const bool positiveEnd = rawPositive >= 8;
	if (!negativeEnd && !positiveEnd) return;
	uint negativeCount = rawNegative & 7;
	uint positiveCount = rawPositive & 7;
	const float4 adjacent = LoadColor(edgePos + acrossEdge);
	if (negativeCount + positiveCount == 0) {
		const float weight = 0.125 * strength;
		color.rgb = sqrt(lerp(color.rgb * color.rgb, adjacent.rgb * adjacent.rgb, weight));
		return;
	}
	if (!positiveEnd) positiveCount = MAX_EDGE_LENGTH + 1;
	if (!negativeEnd) negativeCount = MAX_EDGE_LENGTH + 1;
	const float length = negativeCount + positiveCount + 1.0;
	const float midpoint = length * 0.5;
	const float distance = negativeCount;
	uint shape = 0;
	if (ColorsDifferPass3(edgePos - alongEdge * int(negativeCount),
		edgePos - alongEdge * int(negativeCount + 1))) shape |= 1;
	if (ColorsDifferPass3(edgePos + alongEdge * int(positiveCount),
		edgePos + alongEdge * int(positiveCount + 1))) shape |= 2;
	const bool shouldBlend =
		(inverse && ((shape == 2 && float(negativeCount) <= midpoint) ||
			(shape == 1 && float(negativeCount) >= midpoint) || shape == 0)) ||
		(!inverse && ((shape == 2 && float(negativeCount) >= midpoint) ||
			(shape == 1 && float(negativeCount) <= midpoint) || shape == 3));
	if (shouldBlend) {
		const float h0 = abs((length - distance) / length - 0.5);
		const float h1 = abs((length - distance - 1.0) / length - 0.5);
		const float area = 0.5 * (h0 + h1) * strength;
		color.rgb = sqrt(lerp(color.rgb * color.rgb, adjacent.rgb * adjacent.rgb, area));
	}
}

float4 Pass3(float2 pos) {
	const int2 p = PixelPos(pos);
	float4 color = LoadColor(p);

	const float4 current = edgeCounts.Load(int3(p, 0));
	const int2 below = ClampPos(p + int2(0, 1));
	const int2 left = ClampPos(p + int2(-1, 0));
	const float2 belowHorizontal = edgeCounts.Load(int3(below, 0)).xy;
	const float2 leftVertical = edgeCounts.Load(int3(left, 0)).zw;

	if (any(current.xy > 0)) {
			BlendEdgePass3(current.xy, p, int2(0, -1), int2(1, 0), false, color);
	}
	if (any(belowHorizontal > 0)) {
			BlendEdgePass3(belowHorizontal, below, int2(0, 1), int2(1, 0), true, color);
	}
	if (any(current.zw > 0)) {
			BlendEdgePass3(current.zw, p, int2(1, 0), int2(0, -1), false, color);
	}
	if (any(leftVertical > 0)) {
			BlendEdgePass3(leftVertical, left, int2(-1, 0), int2(0, -1), true, color);
	}

	return color;
}
