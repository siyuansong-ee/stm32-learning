# 04 · TIM PWM 输出

> 你的代码：`6-3PWM驱动LED呼吸灯\hardware\PWM.c`、`6-4PWM驱动舵机\hardware\PWM.c`+`Servo.c`、
> `6-5PWM驱动直流电机\hardware\PWM.c`+`motor.c`（motor.c 有个真 bug，见 4.7）。
> 手册：RM0008 §14.4（PWM mode）、§14.4.7~14.4.9（CCMR/CCER/CCR 寄存器）。

## 4.1 你的代码长这样

```c
// 6-3 呼吸灯：1kHz PWM，CH1 = PA0，占空比 0~100 可调
GPIO PA0 = GPIO_Mode_AF_PP;                       // 复用推挽：引脚交给 TIM2_CH1
TIM_TimeBaseInit: PSC=720-1, ARR=100-1;           // 72M/720/100 = 1kHz
TIM_OCInitStructure.TIM_OCMode     = TIM_OCMode_PWM1;
TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
TIM_OCInitStructure.TIM_Pulse      = 90;           // CCR1 = 90
TIM_OC1Init(TIM2, &TIM_OCInitStructure);
```

## 4.2 输出比较通道的硬件电路（RM0008 §14.4.6 框图）

PWM 不是"软件翻转引脚"，是**一路纯硬件比较器**挂在计数链上：

```
CK_CNT ──► CNT(0..100) ──► 数字比较器：CNT < CCR ?   ──► OC1REF(参考波形)
                                   (PWM1: 小于=有效)        │
                                CCR1=90(影子+预装载)        ▼
                                                      [极性 CC1P] ──► [CC1E 使能] ──► 引脚
```

- **比较器每拍都在工作**：CNT 从 0 数到 99，CNT<CCR 时输出有效电平，CNT≥CCR 输出无效电平——**波形由硬件产生，不占任何 CPU 时间**，这才是 PWM 的价值（软件翻转 GPIO 叫 bit-bang，两个世界）；
- **PWM1 vs PWM2**：PWM1 = CNT<CCR 时有效（上图）；PWM2 完全相反。向上计数时 PWM1 的波形：CCR=30、ARR=99 → 前 30 拍高、后 70 拍低，**占空比 = CCR/(ARR+1)**；
- **边界情况**：CCR=0 → 恒无效电平；CCR>ARR → 恒有效电平（两个都用得到：CCR=0 让电机瞬间停，CCR=ARR+1 全速）；
- **CC1P 极性**：比较结果再过一个"非门"才到引脚。`TIM_OCPolarity_High`=不反相。舵机/电机驱动电路若是低有效（光耦反相），改这里而不改代码逻辑。

## 4.3 TIM_OC1Init 改了哪些寄存器（跟着库走一遍）

| 库动作 | 寄存器位 |
|---|---|
| TIM_OCMode=PWM1 | CCMR1.OC1M[2:0]=110 |
| TIM_OCPolarity=High | CCER.CC1P=0 |
| TIM_OutputState=Enable | CCER.CC1E=1 |
| TIM_Pulse=90 | CCR1=90，且 **CCMR1.OC1PE=1**（CCR 预装载，库默认打开） |
| TIM_OCStructInit | 先填默认值（这就是为什么写不写全字段都不出错） |

注意 **CCR 也有影子寄存器**：运行时 `TIM_SetCompare1` 改的是待装载值，**下一次更新事件才生效**——呼吸灯 main 里每 10ms 改一次 CCR，波形切换发生在周期边界，**不会产生比一个时钟周期更窄的毛刺脉冲**。这就是预装载机制存在的全部意义（面试重点）。

## 4.4 呼吸灯：为什么是 1kHz、100 级？

6-3 的参数链：72MHz → ÷720 = 100kHz → ÷100 = **1kHz PWM**，占空比分辨率 = 1/100 = 1%。

- **1kHz** 远高于人眼闪烁融合频率（~100Hz 以上看不出闪），LED 亮度表现为"平均电流"——PWM 调光本质是**调平均功率**；
- **占空比 100 级**：呼吸灯 0→100→0 只有 100 级亮度，渐变略生硬。要更细腻：ARR 提到 1000（分辨率 0.1%），频率降到 100Hz 仍在融合频率之上——**频率与分辨率的乘积 = 计数频率**，这是 PWM 设计的核心权衡公式：`f_PWM × 分辨率 = CK_CNT`。

## 4.5 舵机：20ms 周期的由来（协议+参数推导）

6-4：PSC=72-1 → CK_CNT=1MHz（**1 个计数 = 1µs**，直接用人话单位）；ARR=20000-1 → 周期 20ms。Servo.c L10：

```c
PWM_SetCompare2(Angle / 180 * 2000 + 500);   // 0° → 500µs，180° → 2500µs
```

- 舵机协议（SG90 等模拟舵机）：周期固定 20ms，高电平 **0.5ms–2.5ms 线性对应 0°–180°**。CCR 值 500~2500 就是 µs 数——选 1MHz 计数频率就是为了"计数值=微秒"，**参数自解释**；
- 面试题"为什么周期必须 20ms"：模拟舵机内部用电位器闭环，控制脉冲太密（周期短）内部电路来不及比较，太疏（>20ms）舵机掉力抖动。所以**换舵机要先看它的协议**（也有 333Hz 的数码舵机）；
- 工程点：CCR 超界（Angle<0 或 >180）会输出 0.5ms 以下/2.5ms 以上脉冲，舵机可能堵转烧机——你的代码没做限幅，产品级要 clamp。

## 4.6 直流电机：H 桥 + 两路信号

6-5：TIM2 CH3=PA2 输出 PWM 调速，PA4/PA5 两个 GPIO 控制方向。你的板子电机驱动是 H 桥（或 L298N 类），拓扑：

```
PA4=1, PA5=0 → 电流 A→B 正转；PA4=0, PA5=1 → 反转
PA2 PWM → 速度（占空比）
```

**为什么调速用 PWM 而方向用 GPIO？** 方向信号是慢速逻辑电平，GPIO 足够；速度需要"平均电压"调制，正好是 PWM 的活。把三个信号都接 TIM 高级定时器的互补通道（TIM1 的 CH+CHN+死区）是正式产品的做法——F1 的高级定时器自带**死区发生器**（BDTR/MOE），防止 H 桥上下管同时导通烧桥。你用的是 TIM2（通用定时器，无死区），所以方向切换靠软件保证"先关后开"，切换瞬间如果两个 GPIO 同时翻转可能短暂直通——低速玩具级没事，**面试时讲出"高级定时器+死区"就是加分项**。

另外注意：**用 TIM1/TIM8 做 PWM 必须 `TIM_CtrlPWMOutputs(TIM1, ENABLE)` 开 MOE 主输出**——江协课程用 TIM2 避开了这个坑，但这是"为什么高级定时器没波形"的标准答案。

## 4.7 工程体检：motor.c 的反转分支是失效代码

```c
// motor.c L21-L32
if (Speed >= 0) { GPIO_SetBits(PA4);    GPIO_ResetBits(PA5);   PWM_SetCompare3(Speed); }
else            { GPIO_ResetBits(PA5);  GPIO_SetBits(PA4);      PWM_SetCompare3(-Speed); }
//                 ↑ 两行顺序换了，但结果完全一样：PA4=1、PA5=0
```

else 分支和 if 分支的**效果完全相同**（都是 PA4=1、PA5=0）——正确应为 `GPIO_ResetBits(PA4); GPIO_SetBits(PA5);`。**症状：电机只能正转，Speed<0 时不反转**。这是典型的复制粘贴 bug：换了语句顺序以为换了方向。修复后还应注意：正反转切换瞬间最好先 `PWM_SetCompare3(0)` 停能再换方向，避免 H 桥瞬间直通。

## 4.8 面试官视角

- "PWM 是软件翻转引脚吗？"→ 不是，硬件比较器：CNT 与 CCR 比较 → OCxREF → 极性 → 使能 → 引脚。
- "PWM1 和 PWM2 的区别？占空比公式？"→ CNT<CCR 有效 vs 反之；CCR/(ARR+1)。
- "运行中改 CCR 会不会出毛刺？"→ 不会，CCR 预装载，更新事件时换班。
- "PWM 频率和分辨率什么关系？"→ f×2^N = CK_CNT，三者只能取其二。
- "高级定时器 TIM1 和通用 TIM2 做 PWM 差什么？"→ 互补输出、死区、刹车（BDTR）、RCR、MOE 使能。
- "为什么舵机用 1MHz 计数频率？"→ 计数值=µs，CCR 直接当脉宽写。

## 4.9 本篇自测题

1. 画出 OC 通道信号链：CNT/CCR 比较器 → OCxREF → CCxP → CCxE → 引脚。
2. 72MHz 想得到 2kHz、0.1% 分辨率的 PWM，PSC 和 ARR 怎么算？
3. 舵机 20ms 周期、0.5-2.5ms 脉宽，你的公式怎么映射 0-180°？超界会怎样？
4. motor.c 的 else 分支错在哪？为什么还能编译通过？
5. TIM1 做 PWM 没波形，第一件事查什么？
