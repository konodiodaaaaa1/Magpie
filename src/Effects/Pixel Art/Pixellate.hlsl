// 移植自 https://github.com/libretro/common-shaders/blob/master/interpolation/shaders/pixellate.cg

//!MAGPIE EFFECT
//!VERSION 4


//!TEXTURE
Texture2D INPUT;

//!TEXTURE
Texture2D OUTPUT;

//!SAMPLER
//!FILTER POINT
SamplerState sam;


//!PASS 1
//!STYLE PS
//!IN INPUT
//!OUT OUTPUT

float4 Pass1(float2 pos) {
	float2 texelSize = GetInputPt();

	float2 range = GetOutputPt() / 2.0f * 0.999f;

	float left = pos.x - range.x;
	float top = pos.y + range.y;
	float right = pos.x + range.x;
	float bottom = pos.y - range.y;

	#ifdef MP_HDR_COMPATIBILITY
	float4 topLeft = INPUT.SampleLevel(sam, (floor(float2(left, top) / texelSize) + 0.5) * texelSize, 0);
	float4 bottomRight = INPUT.SampleLevel(sam, (floor(float2(right, bottom) / texelSize) + 0.5) * texelSize, 0);
	float4 bottomLeft = INPUT.SampleLevel(sam, (floor(float2(left, bottom) / texelSize) + 0.5) * texelSize, 0);
	float4 topRight = INPUT.SampleLevel(sam, (floor(float2(right, top) / texelSize) + 0.5) * texelSize, 0);
	#else
	float3 topLeftColor = INPUT.SampleLevel(sam, (floor(float2(left, top) / texelSize) + 0.5) * texelSize, 0).rgb;
	float3 bottomRightColor = INPUT.SampleLevel(sam, (floor(float2(right, bottom) / texelSize) + 0.5) * texelSize, 0).rgb;
	float3 bottomLeftColor = INPUT.SampleLevel(sam, (floor(float2(left, bottom) / texelSize) + 0.5) * texelSize, 0).rgb;
	float3 topRightColor = INPUT.SampleLevel(sam, (floor(float2(right, top) / texelSize) + 0.5) * texelSize, 0).rgb;
	#endif

	float2 border = clamp(round(pos / texelSize) * texelSize, float2(left, bottom), float2(right, top));

	float totalArea = 4.0 * range.x * range.y;

	float3 averageColor;
	#ifdef MP_HDR_COMPATIBILITY
	float averageAlpha;
	const float topLeftWeight = (border.x - left) * (top - border.y) / totalArea;
	const float bottomRightWeight = (right - border.x) * (border.y - bottom) / totalArea;
	const float bottomLeftWeight = (border.x - left) * (border.y - bottom) / totalArea;
	const float topRightWeight = (right - border.x) * (top - border.y) / totalArea;
	averageColor = topLeftWeight * topLeft.rgb;
	averageColor += bottomRightWeight * bottomRight.rgb;
	averageColor += bottomLeftWeight * bottomLeft.rgb;
	averageColor += topRightWeight * topRight.rgb;
	averageAlpha = topLeftWeight * topLeft.a;
	averageAlpha += bottomRightWeight * bottomRight.a;
	averageAlpha += bottomLeftWeight * bottomLeft.a;
	averageAlpha += topRightWeight * topRight.a;
	return float4(averageColor, saturate(averageAlpha));
	#else
	averageColor = ((border.x - left) * (top - border.y) / totalArea) * topLeftColor;
	averageColor += ((right - border.x) * (border.y - bottom) / totalArea) * bottomRightColor;
	averageColor += ((border.x - left) * (border.y - bottom) / totalArea) * bottomLeftColor;
	averageColor += ((right - border.x) * (top - border.y) / totalArea) * topRightColor;
	return float4(averageColor, 1.0);
	#endif
}
