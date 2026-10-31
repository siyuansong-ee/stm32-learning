# 05 · TIM 输入捕获 / PWMI / 编码器接口

> 你的代码：`6-6输入捕获模式测评\…\hardware\IC.c`、`6-7PWMI模式测频率占空比\hardware\IC.c`、
> `6-8编码器测速\hardware\Encode.c` + `main.c`（6-6/6-7 有频率 10 倍 bug，6-8 有链接级 bug，见 5.6）。
> 手册：RM0008 §14.3（输入捕获/从模式）、§14.3.12（编码器接口模式）、§14.4（寄存器位）。

## 5.1 你的代码长这样

```c
// 6-6 IC.c：TIM3 CH1(PA6) 捕获上升沿，从模式复位测频率
GPIO PA6 = GPIO_Mode_IPU;
TIM_TimeBaseInit: PSC=72-1, ARR=65536-1;              // 1MHz 自由计数
TIM_ICInit: CH1, ICSelection=DirectTI, ICPolarity=Rising, ICFilter=0xF, ICPSC=DIV1;
TIM_SelectInputTrigger(TIM3, TIM_TS_TI1FP1);
TIM_SelectSlaveMode(TIM3, TIM_SlaveMode_Reset);       // 关键：捕获瞬间 CNT 清零
uint32_t IC_GetFreq(void) { return 100000/(TIM_GetCapture1(TIM3)+1); }  // ⚠ 见 5.6
```

## 5.2 输入通路的信号链：TI1 → TI1FP1 要过几关（RM0008 §14.3.5 框图）

```
引脚 PA6 ──► [滤波 IC1F] ──► [边沿/分频 IC1PSC] ──► TI1FP1
                 │                  │
            CCMR1.IC1F[3:0]    CCMR1.IC1PSC + CCER.CC1P
```

- **IC1F 数字滤波**（你写的 0xF）：输入每个 tDTS 采样一次，**连续 8 次电平一致才认为有效**（0xF = fDTS/32 采样 + N=8 确认）。CKD_DIV1 下 tDTS=1/72MHz，等效毛刺滤除窗口约 3.5µs。**这是 EXTI 没有的功能**（02 篇对比过）——测频率怕毛刺就上 TIM 捕获；
- **IC1PSC 分频**（你写 DIV1）：每 1 个有效边沿捕获一次。DIV4 = 每 4 个边沿才捕获，测高频时用（计数器没那么快溢出）；
- **极性**：CC1P=0 → 上升沿捕获；CC1P=1 → 下降沿。PWMI 里 CH2 借 TI1 但极性取反（5.4）。

## 5.3 从模式"复位"：为什么能一个寄存器就测出频率

`TIM_SelectSlaveMode(TIM3, TIM_SlaveMode_Reset)` 写 **SMCR.SMS=100**：TI1FP1 的每个有效边沿**先**把 CNT 值拷进 CCR1，**再**把 CNT 清零。于是：

```
         ↑捕获(CCR1=上一个周期长度)      ↑捕获      ↑捕获
信号 ────┴────────────┴────────────┴────
CNT      0 → 计到 T → 清零 → 计到 T → 清零 …
```

**CCR1 里躺着的永远是"上一个完整周期的计数"**。你的时基是 1MHz（PSC=72-1），所以 CCR1+1 = 周期 µs 数，频率 = 1MHz/(CCR1+1)。硬件一劳永逸，main 里只管读 CCR1——这就是"测频率不用进中断"的真相。

细节：**ARR=65536-1 是"备用"而非必需**——从模式复位会在 ARR 溢出前就清 CNT，ARR 只是防止"信号突然消失后 CNT 自由溢出"的兜底。信号没了 → CCR1 停在最后一拍 → 显示频率定格（这是特性，产品里要加"信号丢失检测"：读 CNT 看它是否还在涨）。

## 5.4 PWMI：一条腿同时测频率和占空比

6-7 比 6-6 多一行 `TIM_PWMIConfig(TIM3, &TIM_ICInitStructure)`，它做的事（对着 CCMR1/CCMR2 讲）：

| 通道 | 配置 | 捕获什么 |
|---|---|---|
| CH1 (CC1S=01) | TI1 直连，上升沿 | 周期（上升沿→上升沿） |
| CH2 (CC2S=10) | **也是 TI1**，但极性被库自动取反 | 高电平时间（上升沿→下降沿） |

`TIM_PWMIConfig` 会自动把 CH2 的 ICSelection 配成 IndirectTI、极性取反。于是：

```c
uint32_t IC_GetFreq(void) { return 100000/(TIM_GetCapture1(TIM3)+1); }          // ⚠ 10 倍 bug
uint32_t IC_GetDuty(void) { return (TIM_GetCapture2(TIM3)+1)*100/(TIM_GetCapture1(TIM3)+1); }
//                               CCR2=高电平时间，CCR1=周期 → 占空比 = CCR2/CCR1 ✓
```

Duty 公式正确；Freq 公式**少了一个 0**（见 5.6）。另外注意 Duty 公式里的 `*100` 是整数运算，中间结果溢出风险：CCR2 最大 65536×100 超过 uint32？不会，65536×100=6.5M < 4G，安全。

## 5.5 编码器接口模式：把方向判断交给硬件（RM0008 §14.3.12）

6-8 Encode.c：TIM3 CH1=PA6、CH2=PA7 接编码器 A/B 两相，`TIM_EncoderInterfaceConfig(TIM3, TIM_EncoderMode_TI12, Rising, Rising)` 写 **SMCR.SMS=011**。

编码器接口模式 = **从模式的第三种**：CNT 的计数时钟不再是 CK_CNT，而是 **TI1FP1 与 TI2FP2 两个信号的边沿**：

```
A 相 ──► TI1FP1 ─┐
                 ├──► 编码器接口逻辑：每来一个边沿，CNT±1
B 相 ──► TI2FP2 ─┘    （A 领先 B 就加，B 领先 A 就减，硬件判断）
```

- **TI12 模式数四个沿**：A 上升、A 下降、B 上升、B 下降都计数 → **分辨率 ×4**（光电编码器 1000 线 → 每圈 4000 个计数），这是硬件白送的精度；
- **方向由硬件判断**：CNT 自动增减，不需要你像 5-2 那样读另一相电平——**再也不丢步**（02 篇 2.6 的对比结论落地）；
- CNT 读出来是**位置**（或增量），速度 = 单位时间内增量。你 main.c 里每秒 `Encoder_Get()` 读一次并清零 → 得到"每秒脉冲数"= 速度。

## 5.6 工程体检：三处真实问题

**① 6-6/6-7 频率显示差 10 倍**：`return 100000/(CCR1+1)` 应为 `1000000/(CCR1+1)`（时基 1MHz）。1kHz 信号 → CCR1+1=1000 → 你的代码算出 100，屏幕显示 "100Hz"，实际 1000Hz。**症状隐蔽**：数字"看起来合理"，不对比标准信号源发现不了——这正是"测频仪要校准"的意义。

**② 6-8 main.c L23 的 `TIm_GetITStatus`**：`TIm` 不是任何已定义符号，**若这个文件参与编译将直接报错**（`identifier "TIm_GetITStatus" is undefined`）。这行正确写法 `TIM_GetITStatus`。

**③ 6-8 的 TIM2_IRQHandler 被定义了两次**：`system\Timer.c` L28 和 `main.c` L21 各有一个 `TIM2_IRQHandler` → **重复符号链接错误**（Keil L6200E: Symbol TIM2_IRQHandler multiply defined）。一个工程里每个中断函数**只能有一份强定义**（内核系列 01 篇的 WEAK 规则的反面）。你的 6-8 能"跑起来"，大概率是当时只编译了其中一个文件，或提交的是未收敛的中间版本——git 里这类状态要警惕。

**④ IC_Init 的 ARR 与 PSC 组合**：1MHz 时基下测 100kHz 信号，CCR1+1=10，分辨率粗糙（±10% 相对误差）；测 10Hz 信号 CCR1+1=100000 没问题。**测频精度 = 计数频率/信号频率**，工程上测宽范围频率要按量程切 PSC。

## 5.7 面试官视角

- "输入捕获测频率，为什么 ARR 用不着？"→ 从模式复位在溢出前就清 CNT，ARR 是兜底。
- "IC1F=0xF 是什么意思？"→ 采样率 fDTS/32 + 连续 8 次一致，毛刺滤除窗口的计算。
- "PWMI 的 CH2 为什么能测高电平时间？"→ CC2S=10 选 TI1 输入，极性自动取反。
- "编码器接口模式为什么是 ×4？"→ TI12 对两相四沿计数；方向硬件判断。
- "信号突然断了，你的测频代码显示什么？为什么？"→ 定格最后一拍；复位模式不再发生，CCR1 不更新。产品级要加丢失检测。

## 5.8 本篇自测题

1. TI1 到 TI1FP1 经过哪三级处理？各自寄存器位？
2. 从模式复位下，捕获和清零的先后顺序是什么？为什么顺序重要？
3. 你的 IC_GetFreq 为什么是 10 倍偏差？写出正确的公式。
4. 1000 线编码器用 TI12 模式，每圈计数多少？
5. 同一个工程里两个 .c 都定义 TIM2_IRQHandler 会怎样？链接器报什么？
