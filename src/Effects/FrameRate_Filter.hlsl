// Limits Magpie's capture-processing rate through the native frame limiter.
// The shader itself is a same-resolution pass-through.

//!MAGPIE EFFECT
//!VERSION 4
//!SORT_NAME Frame Rate Filter

//!PARAMETER
//!LABEL Frame Rate Mode
//!DEFAULT 0
//!OPTION 0 Based on Front Edge Sync
//!OPTION 1 Custom
int frameRateMode;

//!PARAMETER
//!LABEL Custom Frame Rate
//!DEFAULT 60
//!MIN 1
//!MAX 240
//!STEP 1
float targetFrameRate;

//!TEXTURE
Texture2D INPUT;

//!TEXTURE
//!WIDTH INPUT_WIDTH
//!HEIGHT INPUT_HEIGHT
Texture2D OUTPUT;

//!SAMPLER
//!FILTER POINT
SamplerState sam;

//!PASS 1
//!STYLE PS
//!IN INPUT
//!OUT OUTPUT

MF4 Pass1(float2 pos) {
	return INPUT.SampleLevel(sam, pos, 0);
}
