# 02 · EXTI 外部中断电路

> 你的代码：`5-1对射式红外传感器计次\hardware\CountSensor.c`（干净版）、
> `5-2旋转编码器计次\hardware\Encoder.c`（**有两个真 bug，见 2.6**）。
> 手册：RM0008 §9.2（External interrupt/event controller）、§8.4.3（EXTICR）。

## 2.1 你的代码长这样

```c
// CountSensor.c：PB14 下降沿触发 EXTI14，进 EXTI15_10_IRQHandler 计数
RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);          // L7：EXTI 选引脚必须开 AFIO
GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource14);  // L15：PB14 → EXTI14
EXTI_InitStructure.EXTI_Line    = EXTI_Line14;
EXTI_InitStructure.EXTI_Mode    = EXTI_Mode_Interrupt;        // 中断模式（另一个是事件模式）
EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;       // 下降沿
EXTI_Init(&EXTI_InitStructure);
NVIC_Init(…EXTI15_10_IRQn, pre=1, sub=1…);                    // L24-L31：内核侧开门
```

## 2.2 EXTI 的寄存器电路（RM0008 §9.2 框图，面试白板题）

```
引脚电平 ──► [边沿检测电路] ──┬──► RTSR/FTSR 选上升/下降沿
                              │        │
                              │        ▼
                              │   或门 ──► PR 挂起寄存器（bit 置 1，写 1 清零）
                              │        │
                              └──► 直接电平 ──► EMR 事件模式（不经过边沿检测）
```

寄存器清单（每条 EXTI 线一位）：

| 寄存器 | 作用 |
|---|---|
| IMR | 中断屏蔽：置 1 才把该线的请求发给 NVIC |
| EMR | 事件屏蔽：置 1 才产生"事件"（事件≠中断：事件可唤醒睡眠/触发其他外设，不打断 CPU） |
| RTSR / FTSR | 上升沿 / 下降沿触发选择（**不配置 = 不触发**） |
| PR | 挂起标志：检测到边沿硬件置 1，**软件写 1 清零**（你的 `EXTI_ClearITPendingBit`） |
| SWIER | 软件触发：写 1 人为制造一次中断（调试用） |

`EXTI_Init` 内部依次写：IMR/EMR（按 Mode）→ RTSR/FTSR（按 Trigger）→ PR 清零（防上电残留误触发）。你可以反汇编或跟进去验证，面试时能说出"EXTI_Init 动了哪四个寄存器"直接加分。

## 2.3 边沿检测的关键细节：**脉冲检测器**

下降沿是怎么"锁存"的？EXTI 内部是**边沿检测电路**：输入信号每个 APB2 时钟周期被采样，与上一拍比较，出现 1→0 跳变就输出一个**单周期脉冲**去置位 PR。含义：

1. **边沿会被捕获并保持**：即使边沿持续不到一个时钟周期（毛刺），只要采样到变化就置 PR；CPU 晚点进 ISR 也没关系，PR 一直挂着等清除；
2. **毛刺一样触发**：EXTI **没有硬件滤波**（对比 TIM 输入捕获有滤波+分频，05 篇）。你的对射式红外计次若线缆有干扰，会出现一次过门计多个数——工程上要靠屏蔽/软件去抖/换 TIM 捕获滤波；
3. 上升沿+下降沿都配（`EXTI_Trigger_Rising_Falling`）= 电平翻转计数器，5-2 编码器其实可以这么玩，但你的实现走的是"两路 EXTI"方案（见 2.6）。

## 2.4 引脚 → EXTI 线：AFIO_EXTICR 路由

F103 只有 **19 条 EXTI 线**，却要服务几十个引脚，所以每条线只能从 PA/PB/PC/PD（F103 还有 PE）的**同名引脚**中选一个：EXTI14 只能接 PA14 或 PB14 或 PC14……，由 `AFIO->EXTICR4` 的相应 4 位选。`GPIO_EXTILineConfig` 写的就是这个寄存器。注意两条：

- **不能同时**让 PA14 和 PB14 都进 EXTI14（这是硬件约束，也是"为什么 ST 要发明 AFIO"的答案）；
- EXTI 线的编号来自引脚号，**不是任意映射**——面试题"能不能把 PB3 配到 EXTI7？"答案是**不能**，只能 EXTI3。

## 2.5 共线中断：5-9、10-15 必须手动分流

NVIC 只给 EXTI 分配了 7 个通道：EXTI0、1、2、3、4 各一个，**EXTI5–9 共用一个 `EXTI9_5_IRQn`，EXTI10–15 共用一个 `EXTI15_10_IRQn`**。所以你的 `EXTI15_10_IRQHandler` 里必须：

```c
if (EXTI_GetITStatus(EXTI_Line14) == SET)   // 逐线判断：这次是谁？
{ …; EXTI_ClearITPendingBit(EXTI_Line14); } // 清线挂起位，不清 = 出 ISR 立刻重进
```

`EXTI_GetITStatus` 与 `EXTI_GetFlagStatus` 的区别（面试常考）：前者要求 **PR 和 IMR 同时为 1**（"确实是我使能了中断的那条线"），后者只看 PR。中断服务里用 GetITStatus 是防御式标准写法。

## 2.6 工程体检：你的 Encoder.c 为什么"一个方向不计数"

`5-2旋转编码器计次\hardware\Encoder.c` 有三处问题，层层递进：

```c
// L49：EXTI0 的中断里
if(EXTI_GetITStatus(EXTI_Line0==SET ))     // ① bug 语法
// L61：EXTI1 的中断里
if(EXTI_GetITStatus(EXTI_Line1==SET ))     // ② bug 语法 + 语义
{
    if(GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_0)==0)  // ③ 判断方向读的还是 PB0
        Encoder_Count ++;
    EXTI_ClearITPendingBit (EXTI_Line1);
}
```

逐条拆解：

1. **`EXTI_Line0==SET` 是表达式**，先算出来：`0x0001==0x0001` → 1（恰好等于 EXTI_Line0 的值），所以 L49 **碰巧能用**；
2. **L61 的 `EXTI_Line1==SET`** → `0x0002==0x0001` → **0**，于是 `EXTI_GetITStatus(0)` 检查"第 0 条线"（参数 0 无对应位）→ **永远返回 RESET** → 编码器朝一个方向转时 `Encoder_Count++` 永远不执行。**症状：一个方向计数，反方向没反应**；
3. **L63 判断方向读的是 PB0 而不是 PB1**：EXTI1 中断里应该读"另一相 PB1"来判断旋转方向，读 PB0 方向逻辑颠倒或失效。

正确写法（两路 EXTI 方案）：

```c
void EXTI0_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_Line0) == SET)
    {
        if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_1) == 0) Encoder_Count--;
        else                                               Encoder_Count++;
        EXTI_ClearITPendingBit(EXTI_Line0);
    }
}
```

更深一层：**两路 EXTI 方案本身有缺陷**——A 相边沿触发时读 B 相电平判断方向，高速旋转下 A 相中断处理不及时会丢步。**正确做法是 TIM 编码器接口模式**（05 篇 6-8 就是），硬件直接对两相四沿计数，零丢失。这也回答了面试官爱问的"EXTI 计编码器和 TIM 编码器模式差在哪"。

另外 L39-L44 的 `Encoder_Get()` 是"读出并清零"，main 与中断并发读写 `Encoder_Count`（int16_t 读改写），存在临界区竞态（内核系列 07 篇）。

## 2.7 你的 CountSensor 好习惯

- 配置里 `EXTI_Mode_Interrupt` + NVIC pre=1：对射计次是人机交互速率，优先级不必最高，pre=1 合理；
- ISR 只做"计数 +1 + 清标志"，轻到 2µs 内完成——符合"ISR 要短"的工程铁律；
- 触发沿用**下降沿**：对射管被遮挡时输出翻转，选下降沿对应当前模块电平逻辑——**触发沿必须对着硬件模块的 datasheet 选**，这是移植代码时第一个要查的点。

## 2.8 面试官视角

- "EXTI 的触发条件配置在哪个寄存器？边沿检测和电平检测有什么区别？"→ RTSR/FTSR；EXTI 只有边沿（事件模式也是边沿，但 EMR 路径不经 IMR）。F1 无电平触发 EXTI——低功耗唤醒要电平检测得用 PVD/WKUP 引脚。
- "为什么 EXTI5-9 共用一个中断？ISR 里怎么处理？"→ 通道资源复用；GetITStatus 逐线分流 + 清 PR。
- "EXTI 有硬件滤波吗？毛刺怎么办？"→ 没有；软件去抖或换 TIM 输入捕获（带数字滤波）。
- "事件模式和中断模式的区别？"→ 事件不打断 CPU，可用于 ADC 触发、定时器同步、睡眠唤醒。
- "EXTI 线和 GPIO 引脚什么关系？"→ AFIO_EXTICR 同名引脚多选一。

## 2.9 本篇自测题

1. EXTI_Init 改了哪四个寄存器？各自的位是什么语义？
2. 为什么 `EXTI_GetITStatus(EXTI_Line1==SET)` 恒为假？把等号优先级讲清楚。
3. 忘了清 PR 会发生什么？PR 怎么清零（写 0 还是写 1）？
4. 能不能把 PB3 映射到 EXTI7？为什么？
5. 两路 EXTI 计编码器 vs TIM 编码器模式，各有什么缺点？
