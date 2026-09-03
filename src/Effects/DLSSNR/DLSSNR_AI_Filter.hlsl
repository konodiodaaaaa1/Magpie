// Experimental NVIDIA DLSS neural same-resolution filter. The native D3D12
// backend replaces this pass and supplies explicit zero motion/depth guides.

//!MAGPIE EFFECT
//!VERSION 4
//!SORT_NAME DLSSNR AI Filter (Experimental)

//!PARAMETER
//!LABEL Adjust Input Resolution (Reduces DLSSNR Quality)
//!DEFAULT 0
//!MIN 0
//!MAX 1
//!STEP 1
int enableInputResolutionScaling;

//!PARAMETER
//!LABEL Input Resolution (%)
//!DEFAULT 100
//!MIN 25
//!MAX 100
//!STEP 1
int inputResolutionPercent;

//!PARAMETER
//!LABEL Residual Multiplier
//!DEFAULT 1
//!MIN 1
//!MAX 2
//!STEP 0.05
float residualMultiplier;

//!PARAMETER
//!LABEL NR Preset (0 Default, 1 Preset #1, 2 Preset #2, 3 Preset #3)
//!DEFAULT 0
//!MIN 0
//!MAX 3
//!STEP 1
int nrPreset;

//!PARAMETER
//!LABEL NR Style (0 Default, 1 Natural, 2 Cinematic)
//!DEFAULT 0
//!MIN 0
//!MAX 2
//!STEP 1
int style;

//!PARAMETER
//!LABEL NR Intensity
//!DEFAULT 1
//!MIN 0
//!MAX 2
//!STEP 0.05
float intensity;

//!PARAMETER
//!LABEL Local Tone Strength
//!DEFAULT 1
//!MIN 0
//!MAX 2
//!STEP 0.05
float localToneStrength;

//!PARAMETER
//!LABEL Local Structure Strength
//!DEFAULT 1
//!MIN 0
//!MAX 2
//!STEP 0.05
float localStructureStrength;

//!PARAMETER
//!LABEL Skin Structure Strength
//!DEFAULT -1
//!MIN -1
//!MAX 2
//!STEP 0.05
float skinStructureStrength;

//!PARAMETER
//!LABEL Automatic Mask
//!DEFAULT 0
//!MIN 0
//!MAX 1
//!STEP 1
int useAutoMask;

//!PARAMETER
//!LABEL NR UI Correction
//!DEFAULT 0
//!MIN 0
//!MAX 1
//!STEP 1
int uiCorrection;

//!PARAMETER
//!LABEL Frame Guidance (0 Available, 1 Force Zero, 2 Motion Only, 3 Depth Only)
//!DEFAULT 0
//!MIN 0
//!MAX 3
//!STEP 1
int guidanceMode;

//!PARAMETER
//!LABEL Depth Inference Interval
//!DEFAULT 4
//!MIN 1
//!MAX 8
//!STEP 1
int depthInferenceInterval;

//!TEXTURE
Texture2D INPUT;

//!TEXTURE
//!WIDTH INPUT_WIDTH
//!HEIGHT INPUT_HEIGHT
Texture2D OUTPUT;

//!SAMPLER
//!FILTER LINEAR
SamplerState sam;

//!PASS 1
//!STYLE PS
//!IN INPUT
//!OUT OUTPUT

MF4 Pass1(float2 pos) {
	return INPUT.SampleLevel(sam, pos, 0);
}
