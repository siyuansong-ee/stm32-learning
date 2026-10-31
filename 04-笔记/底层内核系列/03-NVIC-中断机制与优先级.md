# 03 · NVIC：中断机制与优先级

> 本篇把"一个引脚事件 → CPU 跳进你的 IRQHandler"整条链路拆到寄存器级。
> 素材：`Timer.c`（TIM2 中断）、`CountSensor.c`（EXTI15_10）、`Serial.c`（USART1 中断）、
> `libary\misc.c` 的 NVIC 实现、`start\core_cm3.h` 的 NVIC 结构体。
>
> 手册：PM0056 §2.3（异常模型）、§4.3（NVIC 寄存器描述）；RM0008 §9（Interrupts and events：NVIC+EXTI）；DDI0337 §7/§8（NVIC 与中断时序）。

## 3.1 先分清三层"中断开关"

你 Timer.c 里连续出现了三组开关，很多初学者分不清（L15、L17、L21）：

```c
TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);   // ① 外设自己的中断使能（TIM2->DIER）
NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2); // ② 全局优先级分组（SCB->AIRCR）
NVIC_Init(&NVIC_InitStructure);              // ③ 内核侧使能+优先级（NVIC->ISER/IP）
```

整条链路是**三级串联**：

```
事件源         外设层           内核层
── TIM2 计数溢出 ──► ① DIER 使能 ──► 状态挂到 TIM2->SR(UIF=1)
                    │
                    └─ 若 DIER 打开 ──► ②③ NVIC 使能该 IRQ 通道
                                        │
                                        └─ 按优先级仲裁 ──► CPU 跳 TIM2_IRQHandler
```

- **①不开**：外设产生事件但不上报，SR 里标志照样置位（所以能轮询）；
- **③不开**：外设上报了但 NVIC 不理会（你 Serial.c L47 注释的"★ 漏掉这一行，中断永远进不去"说的就是它）；
- 进了 ISR 之后：**外设的中断标志（SR 的 UIF/RXNE）必须软件清**（`TIM_ClearITPendingBit`、读 DR），而 NVIC 侧的挂起位由硬件在响应时自动清。两个"清标志"别混——这是面试官最常挖的坑之一。

## 3.2 NVIC 的五个寄存器组（core_cm3.h L132–152）

| 寄存器 | 偏移 | 读/写语义 |
|---|---|---|
| ISER[8] | 0x000 | 中断使能置位：**写 1 使能，写 0 无效**（所以天然原子，不用读改写） |
| ICER[8] | 0x080 | 中断使能清除：写 1 关闭 |
| ISPR[8] | 0x100 | 挂起置位：写 1 强制挂起（软件触发中断，RTOS 的 PendSV 就这么干） |
| ICPR[8] | 0x180 | 挂起清除：写 1 清挂起 |
| IABR[8] | 0x200 | 活动状态（只读，当前正在执行/嵌套中的中断） |
| IP[240] | 0x300 | **8 位/中断的优先级寄存器，F1 只实现高 4 位**（16 级） |

每个寄存器组 8 个 32 位字，每 bit 对应一个中断：`IRQn` 所在寄存器 = `IRQn >> 5`，所在位 = `IRQn & 0x1F`。看一眼 misc.c L135–L136 正是这么算的。ISER 的"写 1 置位"设计是 Cortex-M 的经典技巧：使能/挂起操作不需要"读-改-写"，也就**没有读改写竞态**（07 篇临界区话题的对照面）。

## 3.3 优先级分组：4 位怎么分

F1 的 NVIC 只实现 4 个优先级位（core_cm3.h L98 `__NVIC_PRIO_BITS 4`）→ 16 个优先级值（0 最高，15 最低）。这 4 位如何切成"抢占优先级"和"子优先级"，由 **SCB->AIRCR 的 PRIGROUP[10:8]** 全局决定（PM0056 §4.4.7）：

| NVIC_PriorityGroup | PRIGROUP 值 | 抢占位 | 子优先级位 | 抢占级数 |
|---|---|---|---|---|
| Group_0 | 7 (0x700) | 0 | 4 | 1（全部不能互相打断！） |
| Group_1 | 6 (0x600) | 1 | 3 | 2 |
| **Group_2** | **5 (0x500)** | **2** | **2** | **4** |
| Group_3 | 4 (0x400) | 3 | 1 | 8 |
| Group_4 | 3 (0x300) | 4 | 0 | 16 |

你的 Timer.c L17 选了 Group_2：抢占 2 位（0–3）、子优先级 2 位（0–3）。两条铁律：

1. **抢占优先级高的可以打断低的**（嵌套）；子优先级只影响"同时挂起时谁先响应"，**不能打断**。
2. **分组是全局的**，整个程序只能配一次——`NVIC_PriorityGroupConfig` 在多个外设 Init 里重复调用是常见坏味道（你的 CountSensor.c 和 Timer.c 各调了一次，值相同所以没事，但工程上应集中在 main 开头配一次）。

### 手算一遍：Group_2 下 pre=2, sub=1 写入 IPR 是什么值？

这是面试现场常考的"手算题"。看 misc.c L124–L132 的实现，逆推规则：

```c
tmppriority = (0x700 - (SCB->AIRCR & 0x700)) >> 8;   // Group2 → 0x700-0x500=0x200 >>8 = 2
tmppre  = 4 - 2 = 2;                                  // 抢占占高 2 位
tmpsub  = 0x0F >> 2 = 0x03;                           // 子优先级掩码
value   = (pre << 2) | (sub & 0x03);                  // (2<<2)|1 = 0b1001
IP[ch]  = value << 4;                                 // 0b1001_0000 = 0x90
```

即：**优先级字节 = [抢占位][子优先级位] 放在高 4 位**。Group_2 下 pre=2/sub=1 → `0b10 01 0000 = 0x90`。所以你可以在 Keil 寄存器窗口验证：TIM2 的 IP[28] 应显示 0x90。**会算这个，优先级相关面试题基本满分。**

### AIRCR 为什么要写 VECTKEY？

`NVIC_PriorityGroupConfig` 全句是 `SCB->AIRCR = 0x05FA0000 | group`（misc.c L102）。AIRCR 高 16 位是**写保护钥匙**：只有高半字 == 0x05FA 的写操作才生效，防止程序跑飞时误写复位/分组位（PM0056 §4.4.7）。读回来时高半字是 0xFA05（大小端反序显示）。顺带记住 AIRCR 里还有两个面试考点：`SYSRESETREQ`（软件复位，写它=调用 NVIC_SystemReset 的底层动作）、`PRIGROUP`。

## 3.4 中断响应时序：压栈、取向量、尾链（DDI0337 §8）

从外设把请求送进 NVIC 到 ISR 第一条指令，M3 用**硬件状态机**完成：

1. **压栈（stacking）**：把 xPSR、PC、LR、R12、R3、R2、R1、R0 共 8 字压入当前栈（MSP 或 PSP），SP−32；
2. **取向量**：从 VTOR 向量表读 ISR 入口写 PC；
3. **改寄存器**：LR←EXC_RETURN，IPSR←异常号，IABR 置活动位。

返回时（ISR 执行 `BX LR` 或 `POP {PC}`，发现 LR 是 EXC_RETURN）：

4. **出栈（unstacking）**：恢复那 8 个寄存器，PC 回到被打断处，返回被打断的模式。

**两个著名优化**（面试说出这两个词直接拉满）：

- **尾链 tail-chaining**：一个 ISR 刚结束、另一个中断已挂起时，跳过"出栈+压栈"（共 16 次栈访问），直接取下一个向量——后一个 ISR 约 **6 周期**就能开始。
- **晚到 late-arriving**：正在为低优先级中断压栈时来了个高优先级中断，压栈继续、向量改取高优先级——高优先级中断不额外付出压栈开销。

所以 M3 名义中断延迟约 **12 周期**（零等待存储、单周期取指下），尾链时更短（DDI0337 的官方数字，工程上再叠加你 F103 的 Flash 2 等待周期与总线占用）。对比 51 手工压栈动辄几十周期——这就是"嵌套向量中断控制器 NVIC"里"嵌套、向量"四个字的价值。

## 3.5 EXTI：引脚事件怎么变成 NVIC 请求（你的 CountSensor.c）

`CountSensor.c` 的完整链路（对应 RM0008 §9.2）：

```
PB14 引脚 ──(上升/下降沿检测)──► EXTI14 挂起寄存器 PR bit14
  ▲ 先 GPIO_EXTILineConfig(GPIOB, Pin14)：AFIO_EXTICR4 选"PB14 接 EXTI14"
  ▲ 要开 AFIO 时钟（L7，RM0008 §8 重映射章节）
EXTI_Init：模式=中断、触发=下降沿、LineCmd=ENABLE → 开放 EXTI→NVIC 的请求
  │
  ▼
EXTI15_10_IRQn = 40 ──► NVIC 通道 40（ISER/IP）──► CPU 跳 EXTI15_10_IRQHandler
```

两个工程要点：

1. **EXTI5–9 共用一个通道 EXTI9_5，EXTI10–15 共用一个 EXTI15_10**（F1 只有 7 个 EXTI 中断通道：0–4 各一个 + 5_9 + 15_10）。所以你 L39 的 ISR 里必须 `if(EXTI_GetITStatus(EXTI_Line14))` **逐线判断**是哪个引脚——共线中断的"分流"是程序员的活。同理 9_5。
2. **挂起位必须清**：L44 `EXTI_ClearITPendingBit(EXTI_Line14)` 写 PR 寄存器。忘了清 = 出 ISR 后立刻再进一次 = 卡死在中断里（或无限重入）。

> 你的 CountSensor.c 还有个漂亮细节：配置用 `EXTI_GetFlagStatus` 也行，但标准写法是 `GetITStatus`（要求 IM/MR 使能位也置位）——语义上"确实是我使能的中断"才处理，防御式编程，面试可以主动讲。

## 3.6 你的 Serial.c 中断：一个完美的"生产者-消费者"范例

`Serial.c` L6 `volatile uint8_t Serial_RxFlag;` + L88 `USART1_IRQHandler`（生产者）+ main.c L49 `Serial_GetRxFlag()`（消费者）。内核视角的三个知识点：

1. **RXNE 的中断标志是"读 DR 自动清"**（L95 注释）：`USART_ReceiveData` 读 `USART1->DR` 这一动作本身清 RXNE——硬件设计把"取走数据"和"清标志"绑定成原子动作，所以 ISR 里先读数据再处理，标志不会丢。对比 TIM 的 UIF 要写 `TIM_ClearITPendingBit`——**每个外设清标志方式不同，这是芯片手册才有的知识**（RM0008 §15/§14）。
2. **volatile 为什么必须**（07 篇展开）：main 里 `Serial_GetRxFlag()` 的读取在编译器的视角是一个"永远不变的全局变量"（因为 main 循环里没人写它——写的人在 ISR 里，编译器不知道中断会改），不加 volatile 可能被优化成"读一次、永远当 0"。
3. **NVIC 优先级选择**：L45 给 USART1 配了 pre=1/sub=1。9600 波特率下每字节 1.04ms 才来一次，中断并不紧急，pre=1 合理；如果换成 115200 且要做大量处理，就要掂量"中断里干活时长 vs 下一字节到达间隔"——**中断里只做"取数据+置标志"，重活丢给 main**，这正是你状态机的设计，工程上完全正确。

## 3.7 面试视角的"标准连环问"

1. "外设中断请求到 ISR 执行，经过哪几级开关？"→ 3.1 三级串联 + 标志位清除差异。
2. "抢占优先级和子优先级什么区别？Group 配了有什么影响？"→ 打断 vs 排队；分组全局唯一；手算 0x90。
3. "中断响应 CPU 做了什么？"→ 8 字压栈、取向量、EXC_RETURN；说出 tail-chaining/late-arriving 加分。
4. "为什么 ISR 要尽量短？"→ ①同优先级/低优先级中断被阻塞；②F1 无中断嵌套优先级分配会延迟紧急任务；③栈占用是叠加的（每层嵌套 32B+局部变量）；④共享数据竞态窗口变大。RTOS 术语叫"中断延迟抖动"。
5. "两个中断同时到达，谁的 ISR 先跑？"→ 先比抢占级；同抢占级比子优先级；再同则比异常编号（编号小的优先，这也是固定的硬件规则）。
6. "NVIC 里 ISPR 和 ICPR 什么时候用？"→ 软件触发中断（PendSV 就是这么干的，06 篇）；测试时人为挂起；清掉误挂起。

## 3.8 本篇自测题

1. 写出 EXTI0 与 TIM2 的 IRQn 与异常编号（提示：EXTI0_IRQn=6，TIM2_IRQn=28）。
2. Group_4 分组下 pre=5/sub=0 合法吗？写入 IPR 的值是多少？
3. 在 ISR 里忘了清 EXTI 挂起位会发生什么？忘了清 TIM 的 UIF 呢？
4. 为什么 ISER 设计成"写 1 置位"而不是"读写寄存器"？
5. 用一句话解释 tail-chaining 省掉了什么。
