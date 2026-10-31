# 07 · DMA 总线搬运

> 你的代码：`8-1DMA数据转运\hardware\MyDMA.c`（内存到内存）、`8-2AD+DMA多通道\hardware\AD.c`（ADC→内存循环搬运）。
> 手册：RM0008 §10（DMA controller）、§10.3.3（通道请求映射表）、§10.4（寄存器）。

## 7.1 你的代码长这样

```c
// 8-1 MyDMA.c：DataA → DataB，内存到内存，4 字节
DMA_InitStructure.DMA_M2M = DMA_M2M_Enable;          // 内存到内存模式
DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;   // 源端自增
DMA_InitStructure.DMA_PeripheralInc = Enable;  DMA_MemoryInc = Enable;
DMA_InitStructure.DMA_BufferSize = 4;                // CNDTR
DMA_Init(DMA1_Channel1, &DMA_InitStructure);

// MyDMA_Transfer()：重传一次
DMA_Cmd(DISABLE);  DMA_SetCurrDataCounter(4);  DMA_Cmd(ENABLE);   // ★ 写 CNDTR 前必须先停
while(DMA_GetFlagStatus(DMA1_FLAG_TC1) == RESET);   // 等传输完成
DMA_ClearFlag(DMA1_FLAG_TC1);

// 8-2 AD.c：ADC1→AD_Value[4]，半字、循环
DMA_DIR=PeripheralSRC;  M2M=Disable;  PeripheralBaseAddr=&ADC1->DR;
MemoryBaseAddr=AD_Value;  HalfWord;  MemoryInc=Enable;  Mode=Circular;
DMA_Init(DMA1_Channel1, …);  DMA_Cmd(ENABLE);  ADC_DMACmd(ADC1, ENABLE);
```

## 7.2 DMA 是什么：总线上的"第二颗 CPU"

DMA 控制器是一个**专职搬运工**：它挂在内核的 AHB 总线上，不执行指令，只做"从地址 A 读 → 写到地址 B → 计数器减 1 → 指针按配置移动"这一个循环。价值在于（对照 8-2 想）：

- **ADC 每 5.67µs 出一个结果**。用中断：每 5.67µs 进一次 ISR（压栈/出栈/取向量 ≈ 1µs+），CPU 20% 时间全花在"进出门"上；用 DMA：**CPU 零参与**，结果自动出现在数组里；
- CPU 照常跑 main 里的 OLED 刷新、按键扫描——**"并行"的幻觉来自总线仲裁**：DMA 和 CPU 通过 AHB 总线矩阵轮流占用总线（RM0008 §2.1 框图），DMA 请求总线时 CPU 让一拍。所以 DMA 不是绝对免费：**搬运期间 CPU 访存会被插入等待**，搬得越凶 CPU 越慢——工程上叫"总线带宽竞争"。

## 7.3 通道与请求映射：为什么 ADC1 必须用 DMA1_Channel1

F1 的 DMA1 有 7 个通道，**每个通道的"外设请求线"是固定焊死的**（RM0008 §10.3.3 表，必背前五行）：

| 通道 | 外设请求 | 你的用法 |
|---|---|---|
| CH1 | **ADC1** | 8-2 的 ADC1→内存 ✓ |
| CH2 | SPI1_RX | — |
| CH3 | SPI1_TX | — |
| CH4 | USART1_TX | 扩展：串口 DMA 发送 |
| CH5 | USART1_RX | 扩展：串口 DMA 接收 |
| CH6/7 | USART2_TX/RX | — |

**配错通道 = 搬运永远不会被触发**（因为请求线没接对设备）。这是 DMA 调试第一坑。内存到内存模式（M2M=1）不占请求线，所以 8-1 随便用 CH1。

## 7.4 每通道的寄存器：CCR/CNDTR/CPAR/CMAR/ISR

| 寄存器 | 内容 | 关键位 |
|---|---|---|
| CCR | 配置 | EN、TCIE/HTIE/TEIE（三个中断使能）、DIR、**CIRC**、PINC/MINC、PSIZE/MSIZE、PL（优先级）、**MEM2MEM** |
| CNDTR | **剩余传输数**，每搬一次减 1 | 读它 = 已搬了多少（进度查询） |
| CPAR / CMAR | 外设地址 / 存储器地址 | 8-2 里分别 = &ADC1->DR / AD_Value |
| ISR / IFCR | 状态 / 清状态 | TCIF 传输完成、HTIF 半传输、TEIF 错误 |

三个工程要点：

1. **写 CNDTR 前必须关 EN**：你的 `MyDMA_Transfer` L27-L29 先 DISABLE 再 `DMA_SetCurrDataCounter` 再 ENABLE——**顺序正确**。运行中直接改 CNDTR 是未定义行为（普通模式跑完后 CNDTR=0，不重写就永远搬 0 个）；
2. **TCIF 要软件清**（写 1 清，`DMA_ClearFlag`），不清下次轮询立刻"假完成"；
3. **循环模式（CIRC）**：CNDTR 减到 0 自动重装初始值，周而复始——8-2 的 ADC 连续扫描 + 循环 DMA，`AD_Value` 里**永远是最新的 4 个转换结果**，main 随时读，读到的都是"最近一帧"。代价：没有"数据已更新"的通知——要通知就开 HTIE/TCIE 中断，或者对比数据变化。

## 7.5 半传输中断与乒乓缓冲（8-2 的升级路线）

循环模式的一个经典增强：开 **HTIE（半传输中断）**，把缓冲区一分为二：

```
AD_Value[0..3] 填满（前半）→ HTIE 中断：处理前半
AD_Value[4..7] 填满（后半）→ TCIE 中断：处理后半
```

DMA 永远在写"另一半"，CPU 永远在处理"这一半"——**零拷贝、零等待、数据永不撕裂**（对比你现在的方案：main 读 AD_Value 的瞬间 DMA 可能正在写，读到一个"新旧混搭"的帧——虽然 ADC 数据一帧只有 4 个字，撕裂概率和危害都小，但概念上这是"DMA 共享缓冲必须 volatile + 接受撕裂或换乒乓"的标准权衡）。你的 `AD_Value` 没加 volatile（内核系列 07 篇点过名），补上。

## 7.6 数据宽度与地址自增的搭配

- 8-1：两边都是 Byte + 都自增 → 标准的 memcpy 硬件版；
- 8-2：**外设端 HalfWord 且不自增**（ADC 的 DR 只有一个地址，每次读同一处）+ **内存端 HalfWord 自增**（AD_Value[0..3] 依次填）——这是"外设→数组"的固定套路；
- 宽度必须匹配外设寄存器实际位数：ADC DR 是 16 位（12 位右对齐），用 Byte 会搬两次漏数据，用 Word 会把下一次转换一起吞掉。**PSIZE/MSIZE 配错是 DMA 第二坑**。

## 7.7 面试官视角

- "DMA 搬运时 CPU 在干嘛？"→ 正常执行，通过总线矩阵仲裁分时用总线；带宽竞争时互相插等待。
- "为什么 ADC1 必须用 DMA1_CH1？"→ 请求线固定映射；配错通道永远不触发。
- "CNDTR 是什么？为什么重配前要先 DISABLE？"→ 剩余计数，减到 0 普通模式停；运行中改未定义。
- "循环模式 + 半传输中断能干什么？"→ 乒乓缓冲：处理一半、采集一半。
- "DMA 共享缓冲要不要 volatile？读到的数据会撕裂吗？"→ 要 volatile；会撕裂（除非乒乓或加临界保护）。
- "DMA 中断和普通外设中断的差别？"→ DMA 是搬运事件（半满/全满/出错），外设中断是数据事件（收到字节/转换完成）——两者解耦后 ISR 里不再搬数据。

## 7.8 本篇自测题

1. 画出 8-2 的数据流：ADC 转换完成 → ? → DMA → AD_Value。
2. USART1 发送想用 DMA，该选哪个通道？为什么？
3. 8-1 的 MyDMA_Transfer 为什么要先关 DMA？
4. 循环模式 CNDTR 到 0 后发生什么？main 怎么知道来了一帧新数据？
5. 乒乓缓冲比单缓冲好在哪？代价是什么？
