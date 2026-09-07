Texture2D originTex : register(t0);
Texture2D<float2> cursorTex : register(t1);
// Optional scene below the independent UI layer; unbound on opaque targets.
Texture2D sceneTex : register(t2);

SamplerState pointSampler : register(s0);

float4 main(noperspective float2 coord : TEXCOORD) : SV_TARGET {
	float2 mask = cursorTex.Sample(pointSampler, coord);
	
	if (mask.x > 0.5f) {
		if (mask.y > 0.5f) {
			float4 overlay = originTex.Sample(pointSampler, coord);
			float3 origin = overlay.rgb + sceneTex.Sample(pointSampler, coord).rgb * (1 - overlay.a);
			return float4(1 - origin, 1);
		} else {
			// AND=1, XOR=0 means leave the destination untouched, including alpha.
			discard;
			return 0;
		}
	} else {
		if (mask.y > 0.5f) {
			return float4(1, 1, 1, 1);
		} else {
			return float4(0, 0, 0, 1);
		}
	}
}
