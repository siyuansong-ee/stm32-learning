# 07 · 编译器底层：volatile、临界区、位带

> 你写的 C 和 CPU 执行的机器码之间，隔着一个会"自作主张"的编译器。中断共享变量、
> DMA 缓冲、寄存器访问——凡是硬件和 CPU 并行读写的地方，都是编译器陷阱的高发区。
> 素材：`Serial.c` L6 的 `volatile Serial_RxFlag`、`8-2AD+DMA多通道\hardware\AD.c` L2 的
> `AD_Value[4]`（没有 volatile！）、`stm32f10x_gpio.c` 的 BSRR/BRR 用法。
>
> 手册：RM0008 §2.3.4（Bit banding）、§8.2（GPIO 寄存器，BSRR/BRR）；PM0056 §2.2.5（位带）。

## 7.1 volatile：给编译器的"此变量会被暗改"声明

你的 `Serial.c` L6：

```c
volatile uint8_t Serial_RxFlag;   // 中断里置 1，main 里清 0
```

如果去掉 volatile，main 的 `while(Serial_GetRxFlag() == 0);` 在 -O2 优化下很可能被编译成：

```asm
LDR  r0, =Serial_RxFlag   ; 只读一次内存！
LDRB r1, [r0]
loop:
CMP  r1, #1
BNE  loop                 ; 循环体里全是寄存器比较，再也不碰内存
```

编译器推理："这个循环体内没有任何语句写 Serial_RxFlag（写的人在 ISR 里，而编译器**不假设中断会打断**），所以它的值不变，读一次就够了。"——优化完全符合 C 标准，但把你坑到死：**中断永远"失效"**。volatile 的语义就是打断这条推理：**每次访问都必须真的读写内存，不得缓存、不得重排、不得删减**。

同样的规则覆盖三类场景（工程清单）：

1. **中断共享变量**：ISR 与 main 之间传递的标志/数据（你的 Serial_RxFlag ✓）；
2. **DMA 缓冲**：DMA 控制器是另一个"写者"，CPU 读它时必须 volatile——**你的 `AD.c` L2 `uint16_t AD_Value[4];` 没加 volatile 是隐患**：在优化全开 + 编译器激进时，`OLED_ShowNum(AD_Value[0])` 可能被优化成读一次旧值反复显示。目前 -O0/Keil 默认没炸只是运气，规范写法是 `volatile uint16_t AD_Value[4];`；
3. **寄存器映射**：`__IO` 就是 volatile（core_cm3.h），所以 `while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);`（你 Serial.c L56）不会死循环——TXE 是硬件改的，编译器必须每次重读。

### 面试官必挖的三个 volatile 边界

1. **volatile ≠ 原子**：`volatile uint64_t x` 的读写仍是两条 32 位指令，中断插在中间照样撕裂。volatile 只保证"访问真实发生"，不保证"一次访问不可分割"。
2. **volatile ≠ 同步**：它不提供任何内存屏障语义；多核/多主（DMA 强一致性场景）需要 DSB/DMB（F1 单核 + 简单 DMA 基本够用，但概念要分得清）。
3. **const volatile 组合**（经典送分题）：只读**寄存器**（如 GPIO 的 IDR、状态寄存器 SR）就是 `__I`（= const volatile）：硬件会改 → volatile；软件不能写 → const。问"什么变量要 const volatile"答"输入寄存器/状态寄存器"。

## 7.2 临界区：单核也有竞态

F1 是单核，main 和 ISR 不会"同时"改一个变量，但**main 的多步操作会被 ISR 打断**，中间态可见。你的 `Serial_GetRxFlag()`：

```c
if (Serial_RxFlag == 1) { Serial_RxFlag = 0; return 1; }
```

"读=1"与"写=0"之间，USART1 中断可能又收满一包把 flag 置 1——然后被你写 0 吞掉 → **丢一包**。这类"检查-清除"必须做成原子操作（06 篇的临界区武器）：

```c
uint8_t Serial_GetRxFlag(void) {
    uint8_t r = 0;
    __disable_irq();            // 或 __set_BASEPRI(...) 更精细
    if (Serial_RxFlag == 1) { Serial_RxFlag = 0; r = 1; }
    __enable_irq();
    return r;
}
```

更优的裸机替代：用**原子的位操作**绕开读改写。Cortex-M3 提供 **LDREX/STREX** 独占访问指令对（PM0056 §3.6），ARMCC/GCC 的 `__LDREXB/__STREXB` 内联函数可以直接用；或者干脆让 flag 是单字节且"只有 ISR 写、只有 main 清"，再接受"最多丢一包"的业务语义（你的状态机其实可以设计成容忍丢包——工程上要**先定义竞态是否可接受，再决定要不要关中断**）。

## 7.3 位带（Bit-band）：把一个 bit 变成一个地址

Cortex-M3 支持位带（RM0008 §2.3.4）：把"字"区里的每个 bit 映射成别名区的一个**完整 32 位字**，写别名 = 原子地改那一位，**硬件自动完成读-改-写，不会被中断撕裂**。

| 区域 | 本体区 | 别名区 |
|---|---|---|
| SRAM | 0x20000000–0x200FFFFF（1MB） | 0x22000000 |
| 外设 | 0x40000000–0x400FFFFF（1MB） | 0x42000000 |

换算公式（必须能默写）：

```c
// SRAM 位带
bit_word_addr = 0x22000000 + (addr - 0x20000000) * 32 + bit * 4
// 外设位带
bit_word_addr = 0x42000000 + (addr - 0x40000000) * 32 + bit * 4

#define BITBAND(addr, bitnum) ((addr & 0xF0000000) + 0x02000000 + \
                               ((addr & 0x000FFFFF) << 5) + ((bitnum) << 2))
#define PAout(n)  (*(volatile uint32_t *)BITBAND(&GPIOA->ODR, n))  // 你课程里 PAout 宏的真相
```

验证：PA0 → `0x42000000 + (0x4001080C-0x40000000)*32 + 0` = 0x4221_081C*?…算一下：0x1080C*32 = 0x210180，加 0x42000000 = 0x42210180。有兴趣在 Keil 里 `*(uint32_t*)0x42210180 = 1` 试试，效果和 `GPIOA->BSRR=1` 一样。

**注意陷阱**：① 别名区访问必须是**字（32 位）**访问，字节/半字访问会进 HardFault；② 位带写是原子读改写，但语义上等价"改 1 位"；③ **ST 的标准库 GPIO 没有用位带，而是用 BSRR/BRR**，原因见下。

## 7.4 为什么标准库写引脚用 BSRR 而不是 ODR？

`stm32f10x_gpio.c` L358–L403：

```c
void GPIO_SetBits(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin)   { GPIOx->BSRR = GPIO_Pin; }
void GPIO_ResetBits(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin) { GPIOx->BRR  = GPIO_Pin; }
void GPIO_WriteBit(...) { BitVal ? (GPIOx->BSRR=pin) : (GPIOx->BRR=pin); }
```

如果写成 `GPIOx->ODR |= pin;`，编译出来是"读 ODR → 或运算 → 写回 ODR"三步。若此刻一个中断也改了 ODR 的其它位，中断里写入的值会被 main 的"写回"覆盖——**读-改-写丢失更新**。BSRR/BRR 的解法（RM0008 §8.2）：

- BSRR：bit[15:0] 写 1 = 置位对应引脚；bit[31:16] 写 1 = 复位对应引脚；**写 0 无效果**；
- BRR：bit[15:0] 写 1 = 复位；写 0 无效果。

于是"置位 PA0、复位 PA1"都变成**单次写寄存器**，天然原子、不需要读旧值——写 0 无效的设计让"我不关心的位"永远不会被误伤。**这就是"写 1 清零/置位"寄存器模式存在的根本原因**：消灭读改写竞态（NVIC 的 ISER/ICPR 同理，03 篇）。面试把"BSRR vs ODR"和"ISER 写 1 置位"串起来讲，是内核理解力的直接证据。

## 7.5 把整篇串起来的工程审查清单（对照你仓库）

| 位置 | 现状 | 内核层面的评价 |
|---|---|---|
| `Serial.c` L6 `volatile Serial_RxFlag` | ✓ 正确 | 中断共享标志，教科书级 |
| `Serial.c` L77 `GetRxFlag` 读-清 | △ 有丢失唤醒竞态 | 见 7.2，定义好丢包容忍度或加临界区 |
| `AD.c` L2 `uint16_t AD_Value[4]` | ✗ 建议加 volatile | DMA 缓冲被硬件写，编译器看不到 |
| `delay.c` 轮询 SysTick CTRL | ✓ 正确 | 读寄存器本身就是 volatile 访问 |
| `Timer.c` ISR 里 `Num++` 后清标志 | ✓ 正确 | 清标志放在 ISR 尾部，避免重复进入 |
| `stm32f10x_it.c` HardFault 空壳 | △ 建议升级 | 换成 04 篇的现场打印版 |

## 7.6 本篇自测题

1. 什么场景必须 volatile？举出你仓库里一个"没加 volatile 但有风险"的例子。
2. volatile 为什么不能保证原子性？uint64_t 变量在 F1 上读写几条指令？
3. 什么是 const volatile？标准库/内核头文件里哪个类型就是它？
4. 位带换算公式默写；写别名区必须用什么宽度访问？
5. 为什么 GPIO 用 BSRR 而不用 ODR |= ？同样的思想还出现在哪个内核寄存器组？
