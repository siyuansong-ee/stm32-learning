# 03 · TIM 时基与更新中断

> 你的代码：`6-1定时器中断\system\Timer.c`（1Hz 更新中断）、`6-2定时器外部时钟\system\Timer.c`（ETR 外部时钟）。
> 手册：RM0008 §14（TIM2~TIM5）、§14.2（时基单元）、§14.3.3（外部时钟模式）。

## 3.1 你的代码长这样

```c
// 6-1 Timer.c L10-L11
TIM_TimeBaseInitStructure.TIM_Period    = 10000 - 1;   // ARR
TIM_TimeBaseInitStructure.TIM_Prescaler = 7200 - 1;    // PSC
// 72MHz / 7200 / 10000 = 1Hz → 每秒进一次 TIM2_IRQHandler，Num++
```

## 3.2 定时器的核心电路：一条计数流水线（RM0008 §14.2 框图）

TIM 的本质是一条**同步计数链路**：

```
CK_PSC(72MHz) ──► [预分频器 PSC] ──► CK_CNT ──► [计数器 CNT] ──┬──► CNT == ARR → 溢出
                     ÷(PSC+1)                      0..ARR      │        │
                                                               │        ▼
                                                               └──► 更新事件 UEV ──► SR.UIF=1
                                                                        │
                                                                        ├──► 中断请求（DIER.UIE 开门）
                                                                        ├──► DMA 请求（DIER.UDE）
                                                                        ├──► 预装载寄存器换班（PSC/ARR/CCR 影子生效）
                                                                        └──► TRGO 输出（同步其他定时器）
```

三个必须理解透彻的概念：

1. **PSC 是"预分频计数器"**：每个 CK_PSC 时钟 PSC 加 1，加到 (PSC+1) 时输出一个 CK_CNT 脉冲并归零。所以写 `PSC=7199` 得到 ÷7200——**"存 PSC+1" 是硬件约定，全部定时器寄存器的 -1 魔咒都源于此**（ARR、CCR 同理）。
2. **CNT 计数到 ARR 后归零再往上数**（向上计数模式），从 ARR→0 的那个瞬间叫**更新事件**。更新事件不是中断，它是一堆动作的集合（上图右侧）；只有 DIER.UIE=1 时才把"更新中断请求"发给 NVIC。
3. **影子寄存器（预装载）**：PSC/ARR 有"实际生效值"和"待装载值"两份。`TIM_TimeBaseInit` 默认打开 ARPE（ARR 预装载），于是你运行中改 ARR **不会立刻生效**，要等下一次更新事件才换班——好处是"改频率"变成原子动作，不会出现在周期中间产生半个异常周期。想立即生效就 `TIM_GenerateEvent(TIM2, TIM_EventSource_Update)` 手动制造一次更新，或关 ARPE。

## 3.3 CR1 里那些"配角"位（TIM_TimeBaseInit 也写了它们）

| 位 | 名字 | 作用 | 你的配置 |
|---|---|---|---|
| CEN | Counter enable | 总开关（TIM_Cmd 写它） | 最后才开 |
| UDIS | Update disable | =1 时禁止更新事件 | 0（允许） |
| URS | Update request source | =1 时只有溢出/下溢产生更新，UG 不算 | 0 |
| ARPE | Auto-reload preload | ARR 预装载使能 | 1（库默认开） |
| CKD[1:0] | Clock division | **不是时钟分频！** 是输入滤波的采样时钟 tDTS = CK_INT/1/2/4 | DIV1 |
| OPM | One pulse | 单脉冲模式 | 0 |

`TIM_ClockDivision = TIM_CKD_DIV1`（你每处都写的这行）最容易被误解成"时钟不分频"——它和 PSC 无关，**只管输入滤波器和死区时间用哪个采样时钟**。面试官问"CKD 和 PSC 有什么区别"就是考这个。

## 3.4 你的 ISR 与三个标志位陷阱

```c
TIM_ClearFlag(TIM2, TIM_FLAG_Update);   // Init 末尾：清上电残留 UIF（好习惯）
…
if (TIM_GetITStatus(TIM2, TIM_IT_Update) == SET)   // 读 SR.UIF & DIER.UIE
{ Num++; TIM_ClearITPendingBit(TIM2, TIM_IT_Update); }  // 写 SR.UIF=0
```

- **SR 寄存器"写 0 清 1"**：清 UIF 是**写 0**（`TIM_ClearITPendingBit` 内部做 `&= ~UIF`），而 EXTI 的 PR 是"写 1 清零"——**每个外设清标志的方向不一样**，芯片手册的乐趣（和坑）就在这。NVIC 侧挂起位则是响应时硬件自动清。
- **UIF 不清的后果**：ISR 返回后更新中断请求还在 → 立即重进 → 卡死在中断里（主循环"死机"但 HardFault 不进）。这是 TIM 中断最经典的事故。
- **为什么要先清一次再使能**：SR 的 UIF 上电可能是 1（计数器一跑就溢出过），不清的话 `TIM_ITConfig` 一开门，还没启动定时器就先进一次假中断。你 L14 的 `TIM_ClearFlag` 就是防这个——面试能主动讲出来，说明你被坑过。

## 3.5 时钟从哪来：TIM 的四种时钟源（RM0008 §14.3）

| 时钟源 | 配置 | 你的工程 |
|---|---|---|
| 内部时钟 CK_INT | `TIM_InternalClockConfig` | 6-1/6-3/6-4/6-6/6-7 都是 |
| **外部引脚 ETR** | `TIM_ETRClockMode2Config` | **6-2** |
| 内部触发 ITRx（另一个定时器的 TRGO） | `TIM_SelectInputTrigger` | 未用（级联定时器的钥匙） |
| 外部引脚 TIx（CH1/CH2 输入） | 从模式 + TI1FP1 | 6-6/6-7 的"从模式复位"用了一部分 |

6-2 你的配置：

```c
GPIO PA0 = GPIO_Mode_IPU;                                  // PA0 = TIM2_ETR 引脚
TIM_ETRClockMode2Config(TIM2, TIM_ExtTRGPSC_OFF,           // 不分频
                              TIM_ExtTRGPolarity_NonInverted, // 上升沿
                              0x00);                       // ETF=0：不滤波
TIM_TimeBaseInit: PSC=0(÷1), ARR=9(10-1);
```

寄存器落地：`TIM_ETRClockMode2Config` 写 **SMCR**（ECE=1、ETPS、ETP、ETF）+ 时基。**外部时钟模式 2**：CNT 直接由 ETR 边沿驱动，CK_PSC 完全不参与——所以此时 PSC/ARR 是"对 ETR 边沿计数"：每 10 个上升沿溢出一次，TIM2 中断 10 次 = 100 个脉冲。**ETF=0 意味着不滤波**，对射传感器接 PA0 若有毛刺会多数——对比 05 篇 IC 的滤波参数。

## 3.6 从模式与主模式：定时器之间的"神经网"

每个 TIM 有 4 路内部连接：TIM1→TIM2→TIM3→TIM4 链（ITR0/1/2/3）。TRGO（主模式输出）可以输出：更新事件、使能信号、比较脉冲、OCx 参考信号。用这套机制可以做：

- **定时器级联**：TIM2 的 TRGO 当 TIM3 的时钟，扩宽计时范围；
- **同步启动**：主定时器 TRGO=ENABLE，从定时器收到后同一拍启动；
- **门控/复位**：6-6/6-7 用的"从模式复位"就是 TRGI 触发 CNT 清零（05 篇展开）。

面试时能画出"PSC→CNT→ARR→更新事件→(UIE→NVIC / TRGO→其他TIM)"这张信号流图，TIM 的所有模式（PWM/捕获/编码器）都只是在这条链上**接插件**。

## 3.7 面试官视角

- "定时器溢出后硬件做了什么？"→ 更新事件：UIF 置 1 + 影子寄存器换班 +（若使能）中断/DMA/TRGO。事件≠中断。
- "PSC 为什么写 7200-1？运行时改 ARR 为什么不立即生效？"→ 存 PSC+1；ARPE 预装载，更新事件时才换班。
- "TIM_ClearFlag 和 TIM_ClearITPendingBit 区别？"→ 前者只清 SR 标志位，后者清标志（标准库实现里两者几乎同源，但语义上后者配合中断使用；真正的区别在同名 NVIC 函数——别混）。
- "UIF 忘了清会怎样？"→ 中断风暴卡死主循环。
- "CKD 是时钟分频吗？"→ 不是，是滤波器/死区采样时钟 tDTS。
- "RCR 重复计数器是干嘛的？"→ **只在高级定时器 TIM1/TIM8 存在**（F1），更新事件每 RCR+1 次溢出才产生一次——PWM 周期数控制（电机 FOC 里每 N 个 PWM 周期进一次电流环中断）。你代码里写 `TIM_RepetitionCounter=0` 在 TIM2 上其实是无效字段。

## 3.8 本篇自测题

1. 画出 TIM 时基信号流图，标注 PSC/CNT/ARR/SR/DIER/NVIC 的位置。
2. 72MHz、PSC=7200-1、ARR=10000-1 的中断频率是多少？推导过程写出来。
3. ARPE=1 时运行中改 ARR，什么时候生效？怎么让它立刻生效？
4. 6-2 的 ETR 模式下，PSC 和 ARR 计数的对象是什么？ETF=0 的风险？
5. 更新事件和更新中断是不是一回事？更新事件还触发哪些事？
