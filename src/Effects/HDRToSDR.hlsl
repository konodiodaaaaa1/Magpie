//!MAGPIE EFFECT
//!VERSION 4
//!SORT_NAME HDR To SDR (HDR Capture Bridge)

//!PARAMETER
//!LABEL SDR White Point (scRGB)
//!DEFAULT 4.5
//!MIN 1
//!MAX 10
//!STEP 0.1
float sdrWhiteScale;

//!TEXTURE
//!FORMAT R16G16B16A16_FLOAT
Texture2D INPUT;

//!TEXTURE
//!FORMAT R8G8B8A8_UNORM
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
	float4 c = INPUT.SampleLevel(sam, pos, 0) / sdrWhiteScale;
	return float4(Srgb(max(c.r, 0.0)), Srgb(max(c.g, 0.0)), Srgb(max(c.b, 0.0)), 1.0);
}
