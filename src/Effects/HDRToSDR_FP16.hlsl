//!MAGPIE EFFECT
//!VERSION 4
//!SORT_NAME HDR To sRGB FP16

//!TEXTURE
//!FORMAT R16G16B16A16_FLOAT
Texture2D INPUT;

//!TEXTURE
//!FORMAT R16G16B16A16_FLOAT
Texture2D OUTPUT;

//!SAMPLER
//!FILTER POINT
SamplerState sam;

//!PASS 1
//!STYLE PS
//!IN INPUT
//!OUT OUTPUT

float Srgb(float x) {
	return x <= 0.0031308 ? x * 12.92 : 1.055 * pow(x, 1.0 / 2.4) - 0.055;
}

float4 Pass1(float2 pos) {
	const float sdrWhiteScale = 4.5;
	float4 c = INPUT.SampleLevel(sam, pos, 0) / sdrWhiteScale;
	return float4(Srgb(max(c.r, 0.0)), Srgb(max(c.g, 0.0)), Srgb(max(c.b, 0.0)), 1.0);
}
