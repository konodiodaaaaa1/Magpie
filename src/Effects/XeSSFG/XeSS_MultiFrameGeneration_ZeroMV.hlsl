// Experimental XeSS Multi-Frame Generation marker effect. MFG requests are
// accepted according to the requested multiplier and the XeSS-FG SDK support.

//!MAGPIE EFFECT
//!VERSION 4
//!SORT_NAME XeSS Multi-Frame Generation x2-x4

//!PARAMETER
//!LABEL Frame Multiplier
//!DEFAULT 3
//!MIN 2
//!MAX 4
//!STEP 1
int multiplier;

//!PARAMETER
//!LABEL Optical Flow Method
//!DEFAULT 0
//!OPTION 0 None
//!OPTION 1 AMDOF
int opticalFlowMethod;

//!PARAMETER
//!LABEL OF Quality
//!DEFAULT 1
//!OPTION 0 Performance
//!OPTION 1 Quality
int amdOpticalFlowMode;

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
