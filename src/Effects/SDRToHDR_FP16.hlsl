//!MAGPIE EFFECT
//!VERSION 4
//!SORT_NAME sRGB FP16 To HDR

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

float Linear(float x) {
	return x <= 0.04045 ? x / 12.92 : pow((x + 0.055) / 1.055, 2.4);
}

float4 Pass1(float2 pos) {
	const float sdrWhiteScale = 4.5;
	float4 c = INPUT.SampleLevel(sam, pos, 0);
	return float4(Linear(c.r), Linear(c.g), Linear(c.b), 1.0) * sdrWhiteScale;
}
