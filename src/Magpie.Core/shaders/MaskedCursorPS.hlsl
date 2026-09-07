Texture2D originTex : register(t0);
Texture2D cursorTex : register(t1);
Texture2D sceneTex : register(t2);

SamplerState pointSampler : register(s0);

float4 main(noperspective float2 coord : TEXCOORD) : SV_TARGET {
	float4 mask = cursorTex.Sample(pointSampler, coord);
	
	if (mask.a < 0.5f) {
		return float4(mask.rgb, 1);
	} else {
		// XOR with zero is transparent, not an opaque copy of the UI surface.
		if (all(mask.rgb < (0.5f / 255.0f))) {
			discard;
		}
		float4 overlay = originTex.Sample(pointSampler, coord);
		float3 origin = overlay.rgb + sceneTex.Sample(pointSampler, coord).rgb * (1 - overlay.a);
		// 255.001953 的由来见 https://stackoverflow.com/questions/52103720/why-does-d3dcolortoubyte4-multiplies-components-by-255-001953f
		return float4((uint3(origin * 255.001953f) ^ uint3(mask.rgb * 255.001953f)) / 255.0f, 1);
	}
}
