//!MAGPIE EFFECT
//!VERSION 4
//!SORT_NAME SDR To HDR10 (XeSS-FG HDR Bridge)

//!PARAMETER
//!LABEL SDR White Point (scRGB)
//!DEFAULT 4.5
//!MIN 1
//!MAX 10
//!STEP 0.1
float sdrWhiteScale;

//!TEXTURE
//!FORMAT R8G8B8A8_UNORM
Texture2D INPUT;

//!TEXTURE
//!FORMAT R10G10B10A2_UNORM
Texture2D OUTPUT;

//!SAMPLER
//!FILTER POINT
SamplerState sam;

float Linear(float x) {
    return x <= 0.04045 ? x / 12.92 : pow((x + 0.055) / 1.055, 2.4);
}

float PQ(float nits) {
    const float m1 = 2610.0 / 16384.0;
    const float m2 = 2523.0 / 32.0;
    const float c1 = 3424.0 / 4096.0;
    const float c2 = 2413.0 / 128.0;
    const float c3 = 2392.0 / 128.0;
    float v = pow(max(nits, 0.0) / 10000.0, m1);
    return pow((c1 + c2 * v) / (1.0 + c3 * v), m2);
}

float3 SrgbToBt2020(float3 c) {
    return float3(
        dot(c, float3(0.6274040, 0.3292820, 0.0433136)),
        dot(c, float3(0.0690970, 0.9195400, 0.0113612)),
        dot(c, float3(0.0163916, 0.0880132, 0.8955950)));
}

//!PASS 1
//!STYLE PS
//!IN INPUT
//!OUT OUTPUT

float4 Pass1(float2 pos) {
    float3 srgb = INPUT.SampleLevel(sam, pos, 0).rgb;
    float3 linearSrgb = float3(Linear(srgb.r), Linear(srgb.g), Linear(srgb.b));
    float3 bt2020 = max(SrgbToBt2020(linearSrgb), 0.0);
    float3 nits = bt2020 * (sdrWhiteScale * 80.0);
    return float4(PQ(nits.r), PQ(nits.g), PQ(nits.b), 1.0);
}
