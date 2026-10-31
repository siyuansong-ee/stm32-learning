# 02 · Cortex-M3 内核架构：寄存器、模式与执行

> 本篇把你天天写的 C 语句翻译回 CPU 视角：它有什么寄存器、什么工作模式、怎么调用函数、
> 怎么执行一条 Thumb-2 指令。素材：你仓库 `core_cm3.h` 的寄存器结构体、Keil 反汇编窗口。
>
> 手册：PM0056 §2（About the Cortex-M3 processor）、§3.2（指令集总览）；DDI0403 A2（应用级编程模型）；DDI0337（M3 TRM）。

## 2.1 先定位：F103 的内核是什么"配置"

打开数据手册或读 `core_cm3.h` 里 `__CM3_REV` 可知：F103 = **Cortex-M3 r1p1**（读 CPUID 寄存器 0xE000ED00 得 0x411FC231）。这句话的潜台词：

- **没有 FPU**（浮点运算全软件模拟，别在中断里算 float，见 07 篇性能话题）；
- **没有 MPU**（F1 全系无内存保护单元，MemManage 异常基本不触发，见 04 篇）；
- **没有 Cache/TCM**（Flash 靠预取缓冲提速，见 05 篇）；
- **有**：NVIC、SysTick、DWT/ITM 调试组件、位带（bit-band）、Thumb-2 指令集。

M3 是 3 级流水线（取指→译码→执行），指令为 **Thumb-2 混合编码**：16 位与 32 位指令混用，密度高、无需状态切换（老 ARM7 要 ARM/Thumb 状态切换，M3 不用）。这解释了为什么 M3 上 `BX` 到任何地址都行——bit0 只是 T 位标记（恒为 1），真正地址永远是半字对齐。

## 2.2 寄存器全集（PM0056 §2.1.3 / DDI0403 A2.3）

**通用寄存器 13 个 + 3 个特殊功能"寄存器"：**

| 寄存器 | 汇编名 | 角色 |
|---|---|---|
| R0–R3 | r0–r3 | **参数/返回值**寄存器，AAPCS 里调用者保存（callee 可随意破坏） |
| R4–R11 | r4–r11 | 通用，**被调用者保存**（callee 用完必须恢复原值） |
| R12 | ip | 过程内调用暂存（intra-procedure-call scratch） |
| R13 | sp | 栈指针。**M3 物理上有两个：MSP/PSP**，同一时刻用哪个由 CONTROL[1] 决定 |
| R14 | lr | 链接寄存器：`BL` 指令把返回地址写进 LR。进异常时 LR 会被改写成 EXC_RETURN！ |
| R15 | pc | 程序计数器。**写 PC = 跳转**；bit0 是 T 位（读出来可能是奇数） |
| xPSR | — | 状态字：APSR（N/Z/C/V 条件码）+ IPSR（当前异常号）+ EPSR（T 位等），三合一 |

**内核专用寄存器（必须用 MSR/MRS 访问，C 语言里经 CMSIS 内联函数）：**

| 寄存器 | 作用 | CMSIS 接口（core_cm3.h） |
|---|---|---|
| PRIMASK | bit0=1 屏蔽所有可配置优先级中断（除 NMI/HardFault） | `__disable_irq()` → `cpsid i`（L1204–L1205） |
| FAULTMASK | 连 HardFault 也屏蔽（调试/NMI 处理用） | `__set_FAULTMASK()` |
| BASEPRI | 屏蔽"优先级值 ≥ 某值"的中断，比 PRIMASK 精细 | `__set_BASEPRI(x<<4)` |
| CONTROL | bit0 nPRIV：特权/非特权；bit1 SPSEL：MSP/PSP | `__set_CONTROL()` |

看一遍你的代码：`__disable_irq()` 在哪个工程里都没直接用，但标准库内部大量依赖它（如 Flash 编程、RTC 写保护期间）；RTOS 里它无处不在（06 篇）。

### 面试官爱追问的三个点

1. **LR 的双重身份**：普通函数调用后 LR=返回地址；**进入异常后 LR 被硬件覆写成 EXC_RETURN**（0xFFFFFFF9/0xFFFFFFFD 这类"看起来是地址、其实是控制字"的值）。所以在 ISR 里直接 `BX LR` 就能"返回"——CPU 看到 EXC_RETURN 就执行出栈+恢复现场+回到被打断的代码。这就是为什么**中断函数不能用普通 return 语义来理解**。
2. **PC 的 bit0**：取指永远按半字对齐取，bit0 只在写 PC 时作为"目标代码是 Thumb"的标记（M3 上必须为 1）。所以 Keil 里函数指针打印出来往往是奇数，比如 `0x0800012D`——真正入口是 `0x0800012C`。
3. **SP 的双栈**：MSP 用于复位/异常/特权代码，PSP 用于任务代码。RTOS 里每个任务有独立 PSP 栈空间，切换任务 = 切换 PSP 指向（06 篇细讲）。你裸机程序从头到尾只用 MSP。

## 2.3 操作模式与特权级：一张 2×2 表

（PM0056 §2.1.4，DDI0403 B1.5）

| | Thread 模式（跑主程序/任务） | Handler 模式（跑异常/中断） |
|---|---|---|
| 特权级 Privileged | **复位后的默认状态**，可访问一切 | 永远特权 |
| 非特权级 Unprivileged | 可选（写 CONTROL[0] 降级），不能碰 NVIC/SCB 等内核寄存器 | 不存在 |

裸机程序永远"Thread+特权+MSP"三角。降级到非特权 + PSP 是 RTOS 才做的事，但面试官会问：**为什么要有非特权级？**——防止任务代码误改内核配置（如关了中断、改了 VTOR），这是"隔离"思想的最小实现。F1 没有 MPU，特权/非特权的保护力度有限（非特权不能访问 0xE000E000 以外的？其实非特权模式下内核寄存器和部分系统区访问会 fault），真正完整的隔离要 M3+MPU 或 M4/M7。

**模式切换机制**（画图题）：复位→Thread 特权；中断来了→Handler（自动，不可拒绝）；返回→看 EXC_RETURN 与 CONTROL 恢复 Thread。**从 Thread 进 Handler 是硬件行为，从 Handler 直接回 Thread 也是硬件行为，C 程序唯一能主动改的是 CONTROL**（且必须在特权态，改完要插 ISB 指令屏障）。

## 2.4 一条 C 语句的 CPU 旅程 + AAPCS 调用约定

以你 `main.c`（9-3）里 `OLED_ShowHexNum(2, 1, Serial_TxPacket[0], 2);` 为例，Keil 反汇编大概长这样：

```asm
LDR     r0, [sp, #xx]     ; 参数1: 2 → r0
MOVS    r1, #1            ; 参数2: 1 → r1
LDRB    r2, [...packet]   ; 参数3: Serial_TxPacket[0] → r2
MOVS    r3, #2            ; 参数4: 2 → r3
BL      OLED_ShowHexNum   ; 跳转，LR=下一条地址
```

这就是 **AAPCS（ARM 过程调用标准，DDI0403 A2.6）**，每个 STM32 工程师必须能默写：

1. **前 4 个参数走 R0–R3**，第 5 个开始压栈；64 位返回值用 R0:R1 两个寄存器；
2. **R0–R3、R12、LR 是调用者保存**：被调函数可以随便破坏，调用者若还要用必须自己先压栈；
3. **R4–R11 是被调用者保存**：被调函数如果要用，必须在函数头 `PUSH {R4, LR}`、函数尾 `POP {R4, PC}` 恢复——这就是你反汇编里最常见的一对指令；
4. **SP 在公开接口处保持 8 字节对齐**（呼应 01 篇的 `PRESERVE8`/`ALIGN=3`）。

面试高频："函数结尾的 `BX LR` 和 `POP {PC}` 有什么区别？"——`BX LR` 是叶子函数（没压过 R4 也没嵌套调用）的廉价返回；一旦函数压过栈（LR 被破坏）就必须 `POP {..., PC}` 把原 LR 弹进 PC 完成返回。看懂这一对，你就看得懂 90% 的反汇编骨架。

## 2.5 内核寄存器结构体：core_cm3.h 就是"内核版 stm32f10x.h"

你早就熟悉外设映射（`stm32f10x.h` 的 `GPIO_TypeDef`），内核映射同款套路（core_cm3.h）：

```c
// L715-720
#define SCS_BASE      (0xE000E000)   // System Control Space
#define SysTick_BASE  (SCS_BASE + 0x0010)
#define NVIC_BASE     (SCS_BASE + 0x0100)
#define SCB_BASE      (SCS_BASE + 0x0D00)

// L132-152: NVIC_Type（节选）
typedef struct {
  __IO uint32_t ISER[8];   // 0x000 中断使能置位寄存器（写1使能，写0无效）
  ...
  __IO uint8_t  IP[240];   // 0x300 每中断 8 位优先级寄存器（F1 只用高 4 位）
  __O  uint32_t STIR;      // 软件触发中断
} NVIC_Type;

// L155-169: SCB_Type（节选）
typedef struct {
  __IO uint32_t CPUID;     // 0x00 内核 ID（F103: 0x411FC231）
  __IO uint32_t ICSR;      // 0x04 中断控制状态（含 PENDSVSET）
  __IO uint32_t VTOR;      // 0x08 向量表偏移
  __IO uint32_t AIRCR;     // 0x0C 应用中断/复位控制（含 PRIGROUP、SYSRESETREQ）
  __IO uint32_t SCR;       // 0x10 系统控制（睡眠模式）
  __IO uint32_t CCR;       // 0x14 配置控制（栈对齐控制等）
  __IO uint32_t SHP[12];   // 0x18 系统异常优先级
  __IO uint32_t SHCSR;     // 0x24 系统异常使能/状态（打开 BusFault 用）
  __IO uint32_t CFSR;      // 0x28 故障状态（MMFSR+BFSR+UFSR 三合一）
  __IO uint32_t HFSR;      // 0x2C HardFault 状态
  ...
  __IO uint32_t MMFAR;     // 0x34 MemManage 出错地址
  __IO uint32_t BFAR;      // 0x38 BusFault 出错地址
} SCB_Type;

// L365-370: SysTick
typedef struct {
  __IO uint32_t CTRL;      // 0x00 控制（ENABLE/TICKINT/CLKSOURCE/COUNTFLAG）
  __IO uint32_t LOAD;      // 0x04 重装载值（24 位）
  __IO uint32_t VAL;       // 0x08 当前值（写任意值清零）
  __IO uint32_t CALIB;     // 0x0C 校准值
} SysTick_Type;
```

`SCB->AIRCR`、`NVIC->IP[]`、`SysTick->LOAD` 这些你在代码里天天写的符号，物理地址全在上面这张表里。**能默写 SCS 布局 + 三个关键寄存器地址（AIRCR=0xE000ED0C、VTOR=0xE000ED08、HFSR=0xE000ED2C）是面试加分项**——它证明你不是只会调库。

> 顺带记住：`__IO`/`__I`/`__O` = volatile 修饰，`__ASM volatile("cpsie i")` 这类内联汇编就是 PRIMASK 的操作封装。07 篇会讲为什么这些必须 volatile。

## 2.6 中断进入/返回的"硬件流水线"（预热）

03 篇会展开，这里先建立骨架（PM0056 §2.3.5，DDI0403 B1.5.6）：

1. **硬件压栈**：自动把 xPSR、PC、LR、R12、R3–R0 共 8 个寄存器压入当前栈（32 字节）；
2. **取向量**：按异常号从向量表（VTOR 指向处）取 ISR 地址装入 PC；
3. **改 LR**：LR 写入 EXC_RETURN；
4. 执行 ISR；结尾 `BX LR`（或 POP {PC}）触发**硬件出栈**恢复现场。

全程**没有一条汇编指令是程序员写的**——M3 的异常响应是纯硬件行为，所以 ISR 可以写成普通 C 函数（参数返回值都不需要，因为现场已经在栈里了）。对比 51/AVR：那里进中断要手工 PUSH 一堆寄存器。这是 Cortex-M 内核设计里最漂亮的机制，面试官讲到这里基本都在点头。

## 2.7 本篇自测题

1. F103 的 CPUID 是多少？r1p1 意味着没有什么？（MPU/FPU/Cache）
2. 为什么 Keil 里函数指针值是奇数？`BX LR` 返回时 CPU 怎么知道回哪？
3. AAPCS 三条铁律各是什么？`PUSH {R4, LR}` / `POP {R4, PC}` 这对称呼的语义？
4. MSP 和 PSP 的区别？裸机用哪个？
5. 进中断时硬件自动压了哪 8 个寄存器？顺序是什么（从栈顶往下）？LR 变成什么？
