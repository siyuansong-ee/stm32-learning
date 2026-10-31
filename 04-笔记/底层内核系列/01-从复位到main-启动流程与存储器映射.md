# 01 · 从复位到 main()：启动流程与存储器映射

> 本篇对应你仓库里每个工程的 `start\startup_stm32f10x_md.s` 和 `start\system_stm32f10x.c`。
> 一行汇编都不用改，但你必须能**逐行讲出它在干什么**——这是面试里区分"会点灯"和"懂单片机"的第一道题。
>
> 手册：RM0008 §2（Memory and bus architecture）、§2.4（Boot configuration）、§3（Flash 接口）；PM0056 §2.1/§2.3（存储器模型与异常模型）。

## 1.1 你的启动文件里到底有什么

打开 `01-标准库学习\6-1定时器中断\start\startup_stm32f10x_md.s`（每个工程内容相同），它只有五段：

| 行号 | 段落 | 作用 |
|---|---|---|
| L33–L37 | `Stack_Size EQU 0x400` + `AREA STACK` | 划 1KB 栈空间，`__initial_sp` 标记栈顶 |
| L44–L49 | `Heap_Size EQU 0x200` + `AREA HEAP` | 划 512B 堆空间 |
| L61–L122 | `__Vectors` 向量表 | **上电后 CPU 第一个读的数据结构** |
| L129–L137 | `Reset_Handler` | 复位后第一条指令：先 `SystemInit` 再 `__main` |
| L141–L275 | 默认异常/中断处理函数 | 全部 `B .`（原地死循环），带 `[WEAK]` |

两个关键汇编细节先记住：

- `L35 ALIGN=3`：栈区 **8 字节对齐**（2³=8）。ARM 的 AAPCS 调用约定要求 SP 在函数调用边界保持 8 字节对齐，违反会导致部分库函数（如 printf 里用 LDRD 双字加载的地方）行为异常。
- `L51 PRESERVE8`：要求链接器保持 8 字节对齐；`L52 THUMB`：告诉汇编器用 Thumb-2 指令集（M3 不支持 ARM 32 位指令，只支持 Thumb/Thumb-2）。

## 1.2 复位那一刻：CPU 只做两件事

Cortex-M3 复位时（PM0056 §2.3.1，DDI0403 异常模型），**硬件自动**：

1. 从地址 `0x00000000` 读 4 字节 → 写入 **MSP**（主栈指针）；
2. 从地址 `0x00000004` 读 4 字节 → 作为 **PC** 初值（同时清 bit0 装 T 位）。

对应启动文件 L61–L62：

```asm
__Vectors       DCD     __initial_sp               ; Top of Stack
                DCD     Reset_Handler              ; Reset Handler
```

所以**向量表的前两项根本不是"代码"，是数据**：第 0 项是初始栈顶、第 1 项是复位入口。这就是面试题"为什么向量表第一项是 SP"的答案：复位后 CPU 要压栈才能调用第一个 C 函数，而复位后没人用汇编给 SP 赋值，只能靠硬件从向量表拿。

**关键问题：0x00000000 上真的有内容吗？** 你的程序烧在 Flash `0x08000000`。答案是 **boot 别名映射**（RM0008 §2.4 Boot configuration）：

| BOOT1 | BOOT0 | 启动介质 | 被映射到 0x00000000 的区域 |
|---|---|---|---|
| x | 0 | 主 Flash（0x08000000） | Flash，程序从这里启动（你板子默认） |
| 0 | 1 | 系统存储器（0x1FFFF000） | 内置 BootLoader，出厂串口烧录走这里 |
| 1 | 1 | 内置 SRAM（0x20000000） | RAM 启动（调试用） |

所以复位瞬间 CPU 读 `0x0/0x4`，实际读到的物理介质是 Flash 的头部——向量表的两份"地址"（0x00000000 别名 与 0x08000000 本体）内容一致。稍后 `SystemInit` 把 `SCB->VTOR` 指到 `0x08000000`（见 1.4），从此中断向量不再依赖别名。

## 1.3 Reset_Handler：三行汇编逐行读

```asm
; L129-L137
Reset_Handler    PROC
                 EXPORT  Reset_Handler             [WEAK]
     IMPORT  __main
     IMPORT  SystemInit
                 LDR     R0, =SystemInit
                 BLX     R0
                 LDR     R0, =__main
                 BX      R0
                 ENDP
```

- `IMPORT __main`：注意 **不是 `main`，是 `__main`**（两条下划线）。这是 ARM 编译器 C 运行库的入口符号。链接器把 C 库初始化代码挂在这个符号后面。
- `BLX R0` 调用 `SystemInit()`（C 函数，配置时钟，见 1.4）。
- `BX R0` 跳进 `__main`——注意是 `BX` 不是 `BL`：**这一跳不再回来**，LR 不需要保存。

`__main` 内部做了什么（这是第二个高频面试题）：

```
__main → __rt_entry → __scatterload → 把 RW 段从 Flash 搬到 RAM
                                     → 把 ZI 段在 RAM 里清零
                   → __rt_lib_init  → 初始化 C 库（若用 printf 重定向等）
                   → main()         → 终于进入你的代码
```

为什么要有这一步？你写的代码里有：

```c
uint16_t Num;                          // 未初始化全局变量 → ZI 段
uint8_t Serial_TxPacket[4];            // 同上
```

ZI 段只记"需要多大空间"，不占 Flash；RW 段（初值非零的全局变量，如 `int x = 5;`）的**初值存在 Flash 里**。复位后 RAM 内容是随机的，必须有代码把初值搬进 RAM、把 ZI 清成 0——这就是 `__main` 的事。**如果你的程序里全局变量初值"莫名其妙不对"，先怀疑 scatterload 没跑或链接配置错误。**

> 工程视角：Keil 里 `Use MicroLIB` 勾上后，`__main` 走的是 MicroLIB 的精简流程（启动文件 L282 的 `IF :DEF:__MICROLIB` 分支、L293 的 `__user_initial_stackheap` 就是给非 MicroLIB 的标准库用的）。MicroLIB 启动更快、代码更小，但不支持部分 C99 特性。

### 栈顶到底是多少？

`__initial_sp = Stack_Mem + 0x400`，链接器默认把 STACK 段放在 RAM 最顶端。C8T6 有 20KB SRAM（0x20000000–0x20004FFF），所以初始 SP = **0x20005000**。栈向低地址生长（SP 递减）。1KB 栈够吗？后面 04 篇讲栈溢出的排查。

## 1.4 SystemInit：复位后的第一条 C 代码

`start\system_stm32f10x.c` L212–L269。复位后 CPU 跑在内部 **HSI 8MHz** 上，`SystemInit` 的任务是把系统切到 **72MHz**：

```c
void SystemInit (void)
{
  RCC->CR |= 0x1;                        // 开 HSI（L216）
  RCC->CFGR &= 0xF8FF0000;               // 复位 SW/HPRE/PPRE1/PPRE2/ADCPRE/MCO（L220）
  RCC->CR &= 0xFEF6FFFF;                 // 关 HSEON/CSSON/PLLON（L226）
  ...                                     // 复位 PLLSRC/PLLXTPRE/PLLMUL（L232）
  RCC->CIR = 0x009F0000;                 // 清所有时钟中断标志（L251）
  SetSysClock();                         // → SetSysClockTo72()（L262）
  SCB->VTOR = FLASH_BASE | VECT_TAB_OFFSET;  // 向量表指向 0x08000000（L267）
}
```

`SetSysClockTo72()`（L987–L1080）的关键顺序，**顺序本身就是考点**：

1. 开 HSE，轮询 `HSERDY`（带超时计数，防晶振不起振卡死）；
2. **先配 Flash 等待周期再提频**：`FLASH->ACR = PRFTBE | LATENCY_2`（L1014–L1018）。72MHz 下 Flash 读一次要 2 个等待周期（RM0008 §3，ACR 的 LATENCY 位），**顺序反了 CPU 会从 Flash 读出乱码**；
3. 配分频：AHB ÷1、APB2 ÷1、APB1 ÷2（L1022–L1028）；
4. 配 PLL：`PLLSRC_HSE | PLLMULL9` → 8MHz×9 = 72MHz（L1054–L1056）；
5. 开 PLL，等 `PLLRDY`；
6. **最后才切开关**：`SW = PLL`，等 `SWS == 0x08`（L1068–L1073）。切换时钟源必须在 PLL 稳定之后，且切换是原子的一拍完成。

完整时钟树、每个外设挂在哪条总线上、为什么 TIM2 在 APB1 上却有 72MHz——放到 05 篇展开。

## 1.5 向量表剩余 57 项：中断的"花名册"

L63–L122 是完整的异常向量表：16 个系统异常 + 43 个外设中断（中等容量 F103 的全部中断源）。每个 `DCD 名字` 占 4 字节，按**异常编号**排死，硬件只认位置不认名字：

| 异常编号 | 名称 | 说明 | 是否可配优先级 |
|---|---|---|---|
| 1 | Reset | 复位 | 固定 -3（最高） |
| 2 | NMI | 不可屏蔽中断 | 固定 -2 |
| 3 | HardFault | 硬件错误 | 固定 -1 |
| 4–6 | MemManage/BusFault/UsageFault | 可配置故障（默认关闭→并入 HardFault） | 可配 |
| 11 | SVCall | 系统调用（RTOS 用） | 可配 |
| 14 | PendSV | 可挂起的系统调用（上下文切换用） | 可配 |
| 15 | SysTick | 系统节拍 | 可配 |
| 16+ | 43 个外设中断 | WWDG…USBWakeUp | 可配 |

**外设中断号 = 异常编号 − 16**。比如启动文件 L85 的 `EXTI0_IRQHandler` 是第 7 个外设项，异常编号 16+6=22，IRQ 号 6——和你 `stm32f10x.h` 里 `EXTI0_IRQn = 6` 对得上。NVIC 内部就是按这个号寻址寄存器的（03 篇细讲）。

### [WEAK]：为什么你在 it.c 里写个同名函数就能接管中断？

启动文件 L185–L227 把 43 个外设函数名全部 `EXPORT ... [WEAK]`，默认实现是 `B .`（死循环）。**WEAK = 弱符号**：链接时如果别处有同名的强符号（普通函数），强符号覆盖弱符号；没有就用弱符号。所以：

- 你在 `Timer.c` L28 写了 `void TIM2_IRQHandler(void)` → 覆盖启动文件里的死循环版本；
- 你如果**写错函数名**（比如 `TIM2_Handler`）→ 没人覆盖弱符号 → 中断来了进死循环，表现就是"程序卡死"。这是每个新手都踩过的坑，根源就是这条链接规则。

同理，`stm32f10x_it.c` 里那些 `while(1)` 的 HardFault_Handler 是强符号，覆盖了启动文件的弱版本——所以你把打印代码写进 it.c 就能生效，不用动 .s 文件。

## 1.6 存储器映射：为什么寄存器能当变量访问

Cortex-M3 是 **4GB 统一编址**（von Neumann 意义上的统一地址空间，PM0056 §2.2），所有东西——Flash、RAM、外设寄存器、内核私有外设——都在一张地址表上。F103 的布局（RM0008 §2.3）：

| 地址区间 | 内容 | 你的代码里的例子 |
|---|---|---|
| 0x00000000–0x07FFFFFF | Flash 别名区（boot 决定指向谁） | 复位取向量 |
| 0x08000000–0x0800FFFF | 主 Flash（64KB，C8T6） | 程序、RW 段初值 |
| 0x1FFFF000–0x1FFFF7FF | 系统存储器（BootLoader） | BOOT1=0/BOOT0=1 时启动 |
| 0x1FFFF800–0x1FFFF80F | 选项字节 | 读保护 RDP、看门狗配置 |
| 0x20000000–0x20004FFF | SRAM（20KB） | 变量、栈、堆 |
| 0x40000000–0x4001FFFF 区间 | **外设寄存器**（APB1/APB2/AHB） | GPIOA–GPIOD、TIM、USART… |
| 0x42000000 | 外设位带别名区 | 位带操作（07 篇） |
| 0xE000E000–0xE000EFFF | **内核私有外设 SCS**（NVIC/SCB/SysTick/调试） | 你天天用但从没"打开过时钟"的部件 |

最后一行值得停一下：`SCS_BASE = 0xE000E000`（你仓库 `core_cm3.h` L715），NVIC 在 +0x100、SysTick 在 +0x10、SCB 在 +0xD00（L718–L720）。**内核外设没有时钟使能**——你从没见过 `RCC_APB2PeriphClockCmd(RCC_APB2Periph_NVIC)`，因为 NVIC/SCB/SysTick 由 HCLK 直接驱动，永远在线。这也是"内核外设"和"芯片外设"最本质的区别：前者在 ARM 设计里，后者在 ST 设计里，两者的手册都是分开的（PM0056 vs RM0008）。

### 寄存器 = 结构体指针的魔法

`stm32f10x.h` 里：

```c
#define GPIOA   ((GPIO_TypeDef *) GPIOA_BASE)   // GPIOA_BASE = 0x40010800
typedef struct {
  __IO uint32_t CRL, CRH, IDR, ODR, BSRR, BRR, LCKR;
} GPIO_TypeDef;
```

编译器把 `GPIOA->BSRR = 0x0001` 翻译成"向地址 0x40010810 写 0x0001"。**外设寄存器就是内存**，所以 C 语言指针天然能访问它——这就是"寄存器映射"的全部秘密。`__IO` = `volatile`（core_cm3.h 定义），保证每次访问都真实发生、不被编译器优化掉（07 篇展开）。

## 1.7 三个必须能画出来的图（面试现场白板题）

1. **复位时序图**：复位释放 → 硬件读 0x0 得 SP、读 0x4 得 PC → Reset_Handler → SystemInit（HSI→72MHz，VTOR=0x08000000）→ __main（scatterload：RW 搬运/ZI 清零）→ main() → while(1)。
2. **向量表布局图**：0x08000000 起，第 0 项 SP、第 1 项 Reset、第 3 项 HardFault、第 15 项 SysTick、第 16+43 项外设，每项 4 字节，共 59×4=236 字节。
3. **存储映射图**：Flash/SRAM/外设/PPB 四大地带 + boot 别名。

## 1.8 本篇自测题

1. 复位后 CPU 从哪两个地址取什么？为什么向量表第一项是栈顶不是代码？
2. `__main` 和 `main` 的区别？ZI/RW 段各自存放在哪、谁搬运谁？
3. `[WEAK]` 是什么？解释"中断函数名写错程序就卡死"的机理。
4. 为什么 NVIC 不需要开时钟，GPIO 需要？
5. C8T6 的 SRAM 顶地址是多少？你的栈初始指向哪里、往哪个方向生长？
