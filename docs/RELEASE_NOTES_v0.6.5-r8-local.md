# Magpie 0.6.5 r8 local

在 r7 fix 基础上，修复光流降级和实时参数状态，合并 SR 效果器入口。

r8 后续补充：NVOF 新增最高质量（极高开销），对应 2×2 Slow（2S）；均衡、质量标注“推荐”，高质量标注“高开销”。八个 NVOF 消费者的下拉菜单同步更新，旧档位和默认值不变。见 [2S 实施记录](experimental/todos/20260905-v0.6.5-r8-nvof-2s.md)。

- 光流运行失败后，仅在进入降级或恢复估算时重置历史；连续失败三次后继续使用零向量，不再每帧 reset。尺寸适配保留提供者的 reset 信息。
- 自动重建效果组使用当前已生效参数，保留尚未生效的目标值。参数面板显示尚未生效项的当前值，可重新启用效果组应用。主窗口与运行参数面板同步修改；保存仍自动进行。
- AMD OF 根据生成脚本、SDK shader/include、DXC 及运行库指纹增量生成。相同字节码只生成一份，SDK 所需的其他 permutation 名称使用别名。输出内容未变时不重写文件。
- 参数显隐使用限定效果器 ID 的共同规则，不再把 XeSS 的规则套到同名第三方参数上。
- DLSS SR、FSR 2、FSR 3、FSR 4、XeSS SR 各保留一个效果器入口，均可选择不使用光流、AMD OF 或 NVOF。AMD OF 提供性能／质量，NVOF 提供性能／均衡／质量／高质量／最高质量。来源和档位调整需要重新启用效果组。
- 移除以上 SR 的独立 jitter／Zero MV／Optical Flow 实验入口和 metadata-only jitter 实现，并移除内置半分辨率光流算法。旧 Zero MV／jitter 配置迁移为不使用光流；旧 FSR／XeSS Optical Flow 配置按用户确认迁移到 AMD OF 质量档；旧 DLSS Optical Flow 保持 NVOF 均衡档。
- 多消费者继续共享一次估算：NVOF 优先，其次 AMD OF；只取选定来源实际申请中的较高档位。混用时以非报错提示列出效果器和实际采用的方法。

新建 DLSS SR 默认 NVOF 均衡；新建 FSR 2/3/4、XeSS SR 默认不使用光流。旧 DLSS SR 的关闭状态与 NVOF 档位均保留。已有效果组的名称、效果器顺序与缩放比例不变。

本轮不执行 GUI/GPU 测试，由用户验收。HDR 保留到后续版本。具体构建和交付状态见 [r8 实施记录](experimental/todos/20260905-v0.6.5-r8-TODO.md)。

## English

NVOF adds Highest Quality (Very High Cost), using a 2×2 output grid with the Slow preset (2S). Balanced and Quality are marked Recommended, and High Quality is marked High Cost. Existing saved choices and defaults remain unchanged.

SR effects are consolidated into DLSS SR, FSR 2/3/4 and XeSS SR, with None / AMD OF / NVOF selection and provider quality controls. Metadata-only jitter and built-in half-resolution flow are removed. Legacy FSR/XeSS optical-flow effects migrate to AMD OF Quality; legacy zero-motion variants remain disabled. DLSS SR preserves existing NVOF quality and disabled settings.

Stable provider fallback no longer resets temporal history every frame. Automatic rebuilds retain applied parameter values separately from pending targets, with synchronized parameter editors and visible unapplied values. AMD OF shader generation is incremental and shares identical permutation data. Conditional parameter visibility is scoped to its owning effect.

The shared provider policy remains NVOF first, then AMD OF, using the highest actually requested quality for the selected source. GPU and UI validation is left to the user.
