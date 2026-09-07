MagpieFX 基于 DirectX 11 计算着色器

``` hlsl
//!MAGPIE EFFECT
//!VERSION 4
// 使用 USE 指令声明使用的功能，支持以下值的组合：
// MulAdd：使 MulAdd 函数可用
//!USE MulAdd
// 使用 CAPABILITY 指令声明效果所支持的技术，但是否使用这些技术取决于用户配置。支持以下值的组合：
// FP16：声明对 FP16 的支持
//!CAPABILITY FP16
// 使用 SORT_NAME 指定排序时使用的名字，否则按照文件名排序
//!SORT_NAME test1

// 下面一行可以减少 IDE 中的错误。它是可选的
#include "StubDefs.hlsl"

// 参数定义
//!PARAMETER
// LABEL 为用户界面上显示的参数名；字面量 \n 会显示为换行
//!LABEL Sharpness
// GROUP 可选；相同的非空名称会把参数放入同一个界面列。列顺序由分组首次出现顺序决定，
// GROUP 只影响界面布局
//!GROUP Detail
// DEFAULT、MIN、MAX 和 STEP 都必须存在
//!DEFAULT 0.1
//!MIN 0.01
//!MAX 5
//!STEP 0.01
float sharpness;


// 纹理定义
// INPUT、OUTPUT 是特殊关键字
// INPUT 不能作为通道的输出，OUTPUT 不能作为通道的输入
// 只有最后一个通道可以输出到 OUTPUT，最后一个通道也只能输出到 OUTPUT
// 定义 INPUT 和 OUTPUT 是可选的，但为了保持语义的完整性，建议显式定义
// OUTPUT 的尺寸即为此效果的输出尺寸，不指定则表示支持任意尺寸的输出

//!TEXTURE
Texture2D INPUT;

//!TEXTURE
//!WIDTH INPUT_WIDTH * 2
//!HEIGHT INPUT_HEIGHT * 2
Texture2D OUTPUT;

// 计算纹理尺寸时可以使用一些预定义常量
// INPUT_WIDTH
// INPUT_HEIGHT
// OUTPUT_WIDTH
// OUTPUT_HEIGHT

// 支持的纹理格式：
// R32G32B32A32_FLOAT
// R16G16B16A16_FLOAT
// R16G16B16A16_UNORM
// R16G16B16A16_SNORM
// R32G32_FLOAT
// R10G10B10A2_UNORM
// R11G11B10_FLOAT
// R8G8B8A8_UNORM
// R8G8B8A8_SNORM
// R16G16_FLOAT
// R16G16_UNORM
// R16G16_SNORM
// R32_FLOAT
// R8G8_UNORM
// R8G8_SNORM
// R16_FLOAT
// R16_UNORM
// R16_SNORM
// R8_UNORM
// R8_SNORM
// 根据纹理格式的不同，在通道中该纹理的定义也是不同的。如当纹理格式为 R8G8_UNORM，
// 作为通道的输入时定义是 Texture2D<float2>，作为输出时定义是 RWTexture2D<unorm float2>。
// 当使用 FP16 时，符合条件的纹理将被定义为 min16float 类型，例如 R8G8_UNORM 作为输入时定义变为
// Texture2D<min16float2>，作为输出时定义变为 RWTexture2D<unorm min16float2>。

//!TEXTURE
//!WIDTH INPUT_WIDTH + 100
//!HEIGHT INPUT_HEIGHT + 100
//!FORMAT R8G8B8A8_UNORM
Texture2D tex1;

// 采样器定义
// FILTER 必需，ADDRESS 可选
// ADDRESS 默认值为 CLAMP

//!SAMPLER
//!FILTER LINEAR
//!ADDRESS CLAMP
SamplerState sam1;

//!SAMPLER
//!FILTER POINT
//!ADDRESS WRAP
SamplerState sam2;

// 所有通道通用的部分

//!COMMON
#define PI 3.14159265359

// 通道定义

//!PASS 1
// 描述将出现在游戏内覆盖中
//!DESC First Pass
// 可选 PS 风格，依然用 CS 实现
// STYLE 默认为 CS
//!STYLE PS
//!IN INPUT
// 支持多渲染目标，最多 8 个
//!OUT tex1

float func1() {
}

// Pass[n] 为通道入口点
MF4 Pass1(float2 pos) {
    return MF4(1, 1, 1, 1);
}

//!PASS 2
//!IN INPUT, tex1
// 最后一个通道的输出只能是 OUTPUT
//!OUT OUTPUT
// BLOCK_SIZE 指定一次 dispatch 处理多大的区域
// 可以只有一维，即同时指定长和高
//!BLOCK_SIZE 16, 16
// NUM_THREADS 指定一次 dispatch 有多少并行线程
// 可以少于三维，缺少的维数默认为 1
//!NUM_THREADS 64, 1, 1

void Pass2(uint2 blockStart, uint3 threadId) {
    // 写入 OUPUT
    OUTPUT[blockStart] = MF4(1,1,1,1);
}
```

### 参数分组

`GROUP` 必须有非空名称，并且在每个 `PARAMETER` 块中最多出现一次。重复使用同一名称会复用
该列，组内仍保持参数声明顺序。未写 `GROUP` 的参数属于无标题默认组；完全不使用 `GROUP`
的效果维持原有单个 260 像素无标题列。默认组可以与命名组混用，各列按其首个参数出现
的顺序排列。分组元数据不会改变参数名、参数值、常量布局或着色器编译结果。`LABEL`
中的字面量 `\n` 会在界面显示时解码为换行，不会开始一条新的效果指令。

### 预定义函数

**uint2 GetInputSize()**：获取输入纹理尺寸。

**float2 GetInputPt()**：获取输入纹理每个像素的尺寸。

**uint2 GetOutputSize()**：获取输出纹理尺寸。

**float2 GetOutputPt()**：获取输出纹理每个像素的尺寸。

**float2 GetScale()**：获取输出纹理相对于输入纹理的缩放。

**uint2 Rmp8x8(uint id)**：将 0~63 的值以 swizzle 顺序映射到 8x8 的正方形内的坐标，用以提高纹理缓存的命中率。

**MF{n} MulAdd(MF{m} x, MF{m}x{n} y, MF{n} a)**：等效于 `mul(x, y) + a`，但性能更高，在基于机器学习的效果中非常有用。必须声明 "USE MulAdd" 才能使用此函数。原理参见 [#1049](https://github.com/Blinue/Magpie/pull/1049)。


### 预定义宏

**MP_BLOCK_WIDTH、MP_BLOCK_HEIGHT**：当前通道处理的块的大小（由 "BLOCK_SIZE" 指定）

**MP_NUM_THREADS_X、MP_NUM_THREADS_Y、MP_NUM_THREADS_Z**：当前通道每个线程组的线程数（由 "NUM_THREADS" 指定）

**MP_PS_STYLE**：当前通道是否是像素着色器样式（由 "STYLE" 指定）

**MP_INLINE_PARAMS**：当前通道的参数是否为静态常量（由用户指定）

**MP_DEBUG**：当前是否为调试模式（调试模式下编译的着色器不进行优化且含有调试信息）

**MP_FP16**：当前是否使用半精度浮点数

**MF、MF1、MF2、...、MF4x4**：遵守 fp16 参数的浮点数类型。当未指定 fp16，它们为 float... 的别名，否则为 min16float... 的别名


### 多渲染目标（MRT）

PS 风格下 OUT 指令可指定多个输出（由于 DirectX 的限制最多 8 个）：
``` hlsl
//!OUT tex1, tex2
```

这时通道入口有不同的签名：
``` hlsl
void Pass1(float2 pos, out MF4 target1, out MF4 target2);
```

### 从文件加载纹理

``` hlsl
//!TEXTURE
//!SOURCE test.bmp
Texture2D testTex;
```

TEXTURE 指令支持从文件加载纹理，支持的格式有 BMP，PNG，JPG 等常见图像格式以及 DDS 文件。加载 DDS 文件时只支持 2D 纹理，但允许使用 mipmap。可选使用 FORMAT，指定后可以帮助解析器生成正确的定义，不指定始终假设是 float4 类型。

从文件加载的纹理不能作为通道的输出。
