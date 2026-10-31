# 06 · PendSV 与上下文切换（RTOS 前奏）

> 为什么任务切换要用一个叫 PendSV 的异常？MSP/PSP 怎么分工？FreeRTOS 移植到 F1 靠的
> 是什么？本篇是"裸机 → RTOS"的最后一块跳板。素材：你 `stm32f10x_it.c` L126 那个空壳
> `PendSV_Handler`、启动文件 L174–L177 的 PendSV 向量。
>
> 手册：PM0056 §2.3（异常模型）、§4.4.3（ICSR 的 PENDSVSET）；DDI0403 B1.5；
> 《The Definitive Guide to ARM Cortex-M3》(Joseph Yiu) Ch.8——RTOS 章节的圣经。

## 6.1 任务切换的难题：为什么不用普通函数调用？

假设 main 里有两个"任务"想轮流跑。朴素做法：任务 A 的循环里隔一会儿 `B();` 调一下任务 B——这叫**协作式轮询**（你的裸机程序本质如此）。它的死穴：任务 A 一旦卡死/忘调 B，B 永远没机会。要"抢占"，需要**定时中断里强行切换 PC 指向另一个任务**。

但如果直接在 SysTick 中断里保存现场、跳去任务 B，会遇到两个问题：

1. **SysTick 优先级不够低**：如果切换发生在某个外设 ISR（如 USART1）**执行到一半**时，切出去的"任务 A 现场"其实是"USART ISR 现场"——半截中断的现场被换走，恢复时 USART 的挂起状态、嵌套信息全乱套。
2. **切回来的时机不可控**：正在跑外设 ISR 时硬切任务，等于让外设中断"被暂停"。

**Cortex-M 的官方解法**：把"真正切现场"的动作放到一个**优先级压到最低**的异常里做——这就是 **PendSV**。流程：

```
SysTick 中断（高优先级，只做决定）
   └─ 发现时间片到了
       └─ 置位 ICSR.PENDSVSET（只"挂号"，几行指令就返回）
           └─ SysTick ISR 结束 → 所有未决的高优先级 ISR 都跑完
               └─ 内核最后才响应 PendSV（因为它优先级最低）
                   └─ 在 PendSV 里：保存当前任务现场 → 恢复下一任务现场 → 返回
```

这样上下文切换永远发生在"没有别的中断插进来"的干净时刻。**"挂起服务 PendSV = 可延迟的系统调用"**——名字就是这么来的。你 it.c 里那个空 `PendSV_Handler`，将来移植 FreeRTOS 时会被它的汇编实现覆盖（和 01 篇 [WEAK] 覆盖是同一套机制）。

## 6.2 双栈机制：为什么任务要用 PSP

- **MSP**：复位、异常处理、内核代码用。出异常时硬件自动切回 MSP（Handler 模式下 CONTROL.SPSEL 强制为 0）。
- **PSP**：每个任务自己的栈。任务 A 的 PSP 指向 A 栈顶，任务 B 的 PSP 指向 B 栈顶——**切换任务 = 换 PSP 指针**，任务之间天然栈隔离。

切换瞬间（CONTROL.SPSEL 从 0→1）必须在**特权级 Thread 模式**做，而 PendSV 是 Handler 模式——所以经典做法是**在 PendSV 返回时"顺手"修改**：把要恢复的任务栈内容排好，返回时用 `EXC_RETURN = 0xFFFFFFFD`（Thread+PSP），硬件出栈+模式切换一气呵成。

## 6.3 EXC_RETURN 全家福（背下来）

| EXC_RETURN 值 | 返回后模式 | 栈 |
|---|---|---|
| 0xFFFFFFF1 | Handler | MSP |
| 0xFFFFFFF9 | Thread | MSP |
| 0xFFFFFFFD | Thread | PSP |

bit0=0（Thumb 标记）、bit2 选栈（0=MSP/1=PSP）、bit3 选模式（0=Handler/1=Thread）。**普通中断返回是 0xFFFFFFF9（裸机只有 MSP）；RTOS 切换任务时手工构造 0xFFFFFFFD**。面试官问"为什么 ISR 的 LR 是个 0xFFFFFFxx 的值"——答案在 02 篇+这张表。

## 6.4 迷你上下文切换（能看懂 FreeRTOS port.c 就赢了）

FreeRTOS 的 F1 移植（`port.c` 的 `xPortPendSVHandler`）核心就是这个套路，手动版如下（看懂即可，不必背）：

```asm
; 触发切换：任何地方执行
;   SCB->ICSR |= (1<<28);   // PENDSVSET —— C 代码里就是 NVIC_SetPendingIRQ(PendSV_IRQn)

PendSV_Handler:
    CPSID   I                  ; 关中断，切换过程不可打断
    MRS     R0, PSP            ; 当前任务栈指针
    ; ---- 保存"软件负责的一半"：R4-R11（R0-R3/R12/LR/PC 硬件已自动压栈）----
    STMDB   R0!, {R4-R11}      ; 压入任务 A 栈
    ; ---- 找到下一任务（RTOS 调度器决定，这里假设 nextSP 已算好）----
    LDR     R1, =nextTaskSP
    LDR     R0, [R1]           ; R0 = 任务 B 的 PSP
    LDMIA   R0!, {R4-R11}      ; 恢复任务 B 的 R4-R11
    MSR     PSP, R0            ; PSP 指向 B 的栈
    MOV     LR, #0xFFFFFFFD    ; ★ 构造 EXC_RETURN：Thread + PSP
    CPSIE   I
    BX      LR                 ; 硬件出栈：恢复 B 的 R0-R3/R12/LR/PC/xPSR → 直接跑在 B 里
```

对应关系记牢：**异常进入时硬件负责压 R0–R3/R12/LR/PC/xPSR，软件负责压 R4–R11**——因为 AAPCS 规定 R4–R11 是被调用者保存，硬件按 C 调用约定只压"调用者保存"那组。这就是为什么任务现场"只有一半是硬件压的"，也是面试能讲出深度的点。

**SVC 异常**（启动文件 L165 `SVC_Handler`）在 RTOS 里的角色：**第一次启动第一个任务**。Thread 模式无权改 CONTROL/SP（非特权任务代码），所以第一个任务要靠一次 SVC 调用（特权 Handler 里）把 PSP 指到任务栈、置 CONTROL.SPSEL、再返回 Thread+PSP。**"SVC 建局、PendSV 换局"**一句话概括整个 RTOS 上下文切换机制。

## 6.5 临界区：PRIMASK 与 BASEPRI 的分工

上下文切换要"原子"，裸机里你同样需要：main 和 ISR 共享的变量，做"读-改-写"时不能让 ISR 插进来。两级武器：

```c
__disable_irq();        // = cpsid i → PRIMASK=1，屏蔽除 NMI/HardFault 外的一切
critical_section();
__enable_irq();         // = cpsie i

// 精细版：只屏蔽"优先级低于阈值"的中断（保留高优先级中断的实时性）
__set_BASEPRI(3 << 4);  // F1 只有 4 个优先级位，屏蔽 pre>=3 的中断
__set_BASEPRI(0);
```

（`__disable_irq` 定义在你 core_cm3.h L1204–L1205，本质就是 `cpsid i`/`cpsie i`。）

工程铁律：

1. **临界区越短越好**：关中断期间串口数据可能丢（RXNE 被覆盖）、定时器标志会累积；
2. **BASEPRI 优先于 PRIMASK**（FreeRTOS 就用 BASEPRI 实现"只锁调度器不锁高优先级中断"的临界区）；
3. `__disable_irq()/__enable_irq()` **必须成对且中途不 return**——写个宏或 RAII 风格包装防漏；
4. 临界区只保护"多字节/读改写"访问；单次对齐的 32 位读写天然原子（07 篇）。你的 `Serial_RxFlag` 是 uint8_t，main 里"判 1 清 0"两步之间中断可能再置 1 把它吞掉——这是**丢失唤醒**竞态（丢一包显示），要想严谨得用关中断包裹读-清操作（或换 RTOS 的信号量）。

## 6.6 面试官视角

- "PendSV 为什么优先级要最低？"→ 保证切换发生在所有 ISR 都完成之后，不会切走半个中断现场。
- "上下文切换时哪些寄存器硬件保存、哪些软件保存？"→ R0-R3/R12/LR/PC/xPSR 硬件，R4-R11 软件；原因 = AAPCS 的保存约定。
- "0xFFFFFFFD 是什么？"→ EXC_RETURN，返回 Thread 模式用 PSP；bit2/bit3 含义。
- "SVC 和 PendSV 分工？"→ SVC 建第一个任务环境（特权操作），PendSV 做日常切换（低优先级可挂起）。
- "关中断和关调度器有什么区别？"（进阶）→ PRIMASK 全关（影响所有中断），BASEPRI 只锁低优先级（调度器），高优先级 ISR 仍可跑——FreeRTOS 的 `taskENTER_CRITICAL` vs `taskENTER_CRITICAL_FROM_ISR` 的底层分野。

## 6.7 本篇自测题

1. 用三句话解释"为什么任务切换要放进 PendSV"。
2. 任务 A 的完整现场存在哪？切到任务 B 时哪些步骤是硬件做的？
3. 写出三种 EXC_RETURN 值及含义；ISR 里 LR=0xFFFFFFF9 说明什么？
4. `__disable_irq()` 底层是哪条指令、改哪个寄存器？
5. 你的 `Serial_GetRxFlag` 存在什么竞态？给出两种修复方案。
