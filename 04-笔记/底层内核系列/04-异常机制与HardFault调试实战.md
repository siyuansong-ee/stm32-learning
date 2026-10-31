# 04 · 异常机制与 HardFault 调试实战

> 程序跑飞进 HardFault，是嵌入式工程师的分水岭：新手看门狗一开万事大吉，老手会问
> "哪条指令、哪个地址、哪个栈"。本篇给你一套**能现场抓到凶手**的完整方法，
> 所有代码可直接粘进你仓库的 `user\stm32f10x_it.c`。
>
> 手册：PM0056 §2.3（异常模型）、§4.4（SCB 故障寄存器）；DDI0403 B1.5（异常进入/返回）；
> ST 应用笔记 **AN3216**（HardFault 排查）；RM0008 §9.1。

## 4.1 Cortex-M3 的异常全景图

| # | 异常 | 触发条件（举例） | 优先级 | 能否屏蔽 |
|---|---|---|---|---|
| 1 | Reset | 上电/复位引脚/看门狗/软件复位 | -3（最高） | 否 |
| 2 | NMI | 不可屏蔽中断（F1 上由时钟安全系统 CSS 触发） | -2 | 否 |
| 3 | **HardFault** | 所有未被处理的故障的"总兜底" | -1 | 否 |
| 4 | MemManage | 违反 MPU/存储保护（**F1 无 MPU，罕见**） | 可配，默认 0 | 可 |
| 5 | BusFault | 取指/数据访问总线错误：**访问不存在的外设地址、越界到 0x40000000 以外乱读** | 可配 | 可 |
| 6 | UsageFault | 未定义指令、未对齐访问、除零、非法状态切换 | 可配 | 可 |
| 11 | SVCall | 执行 SVC 指令（RTOS 系统调用） | 可配 | — |
| 14 | PendSV | 软件挂起的系统调用（上下文切换） | 可配 | — |
| 15 | SysTick | 系统节拍定时器 | 可配 | — |

**关键设计：故障升级（escalation）**。MemManage/BusFault/UsageFault 三个"可配置故障"默认被 **SCB->SHCSR 关着**（SHCSR bit16/17/18 = MEMFAULTENA/BUSFAULTENA/USGFAULTENA，默认 0）。关着时，这三个故障**不会进自己的 Handler，而是升级成 HardFault**。所以你在 it.c 里看到的那四个 `while(1)`（HardFault/MemManage/BusFault/UsageFault），实际运行时 90% 的故障都落在 HardFault 里——MemManage/BusFault/UsageFault 三个函数平时是"死代码"。想要更细的故障信息，第一步就是打开这三个使能位，让故障各回各家。

## 4.2 三个故障状态寄存器（SCB，地址在 core_cm3.h L164–L169）

| 寄存器 | 地址 | 内容 |
|---|---|---|
| CFSR | 0xE000ED28 | **三合一**：MMFSR（bit7:0）+ BFSR（bit15:8）+ UFSR（bit31:16），可分别按字节读 |
| HFSR | 0xE000ED2C | HardFault 状态：bit30 FORCED（=1 表示是升级来的）、bit1 VECTTBL（向量表读错，常见于 VTOR 指空/IAP） |
| MMFAR | 0xE000ED34 | MemManage 出错地址（MMFSR.MMARVALID 有效时） |
| BFAR | 0xE000ED38 | BusFault 出错地址（BFSR.BFARVALID 有效时） |

最常用的几位（面试问到能报出名字）：

- UFSR bit16 **UNDEFINSTR**：执行了未定义指令——典型原因：**函数指针指飞**、栈溢出后 PC 被冲成垃圾、跳到了数据区；
- UFSR bit17 **INVSTATE**：跳进了一个"Thumb 位=0"的地址——典型原因：**函数指针没带 T 位**（bit0 清 0 的指针直接 BX）；
- UFSR bit25 **DIVBYZERO**：除零（整数除法，需使能除法陷阱后才进 UsageFault，M3 上默认 SDK 没开则结果钳位为 0）；
- UFSR bit24 **UNALIGNED**：未对齐访问（如 `*(uint32_t*)0x20000002`）；
- BFSR bit9 **PRECISERR**：数据访问总线错误且**地址已知**（看 BFAR）——访问越界地址的"头号嫌疑"；
- BFSR bit10 **IMPRECISERR**：写缓冲延迟报错，地址不可知（BFAR 无效）——排查更费劲；
- HFSR bit30 **FORCED** + bit1 **VECTTBL**：前者说明是升级来的（去看 CFSR 找真凶），后者直接怀疑 VTOR/向量表。

## 4.3 现场勘查：从栈里把"案发现场"挖出来

进异常时硬件压了 8 个寄存器（02 篇），**从栈顶往上依次是**：

```
低地址 ──► [SP+0]=R0  [SP+4]=R1  [SP+8]=R2  [SP+12]=R3
          [SP+16]=R12 [SP+20]=LR [SP+24]=PC(返回地址!) [SP+28]=xPSR ──► 高地址
```

所以只要拿到异常发生时的 SP，就能读出**跑飞前 PC 在哪**（SP+24）、调用者的 LR（SP+20）。SP 用哪个栈？看进 HardFault 时 LR 的 bit2：=0 用 MSP，=1 用 PSP。完整勘查代码（把这段替换掉 it.c 里的 `while(1)` 空壳，AN3216 官方推荐做法）：

```c
/* stm32f10x_it.c */
#include <stdio.h>
extern int fputc(int ch, FILE *f);   // 你 9-1 已重定向 printf 到串口

void HardFault_Handler(void)
{
    __asm volatile(
        "TST   LR, #4      \n"   // 检查 EXC_RETURN bit2：用哪个栈
        "ITE   EQ          \n"
        "MRSEQ R0, MSP     \n"
        "MRSNE R0, PSP     \n"
        "B     HardFault_Handler_C \n");
}

void HardFault_Handler_C(uint32_t *sp)
{
    printf("=== HARDFAULT ===\r\n");
    printf("PC    = 0x%08X\r\n", sp[6]);   // 跑飞点（取出后去掉 bit0 即真实地址）
    printf("LR    = 0x%08X\r\n", sp[5]);   // 调用者返回地址
    printf("R0-R3 = 0x%08X 0x%08X 0x%08X 0x%08X\r\n", sp[0], sp[1], sp[2], sp[3]);
    printf("xPSR  = 0x%08X\r\n", sp[7]);
    printf("CFSR  = 0x%08X (MM=0x%02X BF=0x%02X UF=0x%04X)\r\n",
           SCB->CFSR, (uint8_t)SCB->CFSR, (uint8_t)(SCB->CFSR >> 8), (uint16_t)(SCB->CFSR >> 16));
    printf("HFSR  = 0x%08X\r\n", SCB->HFSR);
    printf("BFAR  = 0x%08X\r\n", SCB->BFAR);
    printf("MSP   = 0x%08X\r\n", __get_MSP());
    while (1);
}
```

拿到 PC 后，Keil 里 **反汇编窗口 → 右键 → Go To Address** 输入 PC 值，直接定位跑飞的那条 C/汇编语句。查地址归属的快速判断：0x08xxxxxx=Flash 代码、0x20xxxxxx=RAM、0x40xxxxxx=外设（PC 落在这两个区基本就是野指针跳转）、0x00/0xFF…=栈被冲烂或指针悬空。

**实战演练（强烈建议亲手做一次）**：把下面代码临时放 main 里，让上面的打印跑一遍：

```c
uint8_t  buf[4];
for (int i = 0; i <= 4; i++) buf[i] = i;   // 越界写 → 可能踩坏 LR/栈 → HardFault
uint32_t *p = (uint32_t *)0x40018000;      // 不存在的地址 → BusFault(PRECISERR)
(void)*p;
void (*fp)(void) = (void (*)(void))0x20000000;  // 跳数据区 → UsageFault(INVSTATE/UNDEFINSTR)
fp();
```

逐个单测，看 CFSR 不同位被点亮，你就再也不会"害怕"HardFault。

## 4.4 你的工程里最可能的六个 HardFault 成因

对照你仓库代码逐条自查：

1. **数组越界**：你 Serial.c L102 已经用 `pRxPacket >= SERIAL_PACKET_SIZE` 防了写入越界（还留了 ★ 注释），这是标准做法；但凡少了这一行，连续收包就会把相邻的 `RxState`、`pRxPacket`、甚至栈上返回地址冲掉 → 随机 HardFault。
2. **栈溢出**：你的栈只有 1KB（启动文件 `Stack_Size EQU 0x400`）。深度递归、大局部数组（`uint8_t frame[1024]` 放函数里）、中断里开大缓冲、OLED 字库操作，都可能悄悄冲破栈顶，症状是"跑一会儿就 HardFault/重启，且 PC 每次都不一样"。**排查：把数组改 static/全局，或在 map 文件看栈使用**。
3. **外设时钟没开就操作**：比如忘了 `RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB)` 就去写 GPIOB——写读一个"没上电"的总线外设 → BusFault（地址错误）。你所有工程 Init 开头都开了时钟，好习惯，但集成时最容易漏。
4. **函数指针/回调配错**：野函数指针 → INVSTATE/UNDEFINSTR，4.2 的表直接对号入座。
5. **中断里重入/共享数据被撕裂**：main 和 ISR 同时改一个多字节变量（07 篇临界区），F1 是单核不会同时改，但 main 的"读-改-写"被打断会产生中间态——多数不触发 HardFault，但会引发逻辑 bug 和更诡异的异常。
6. **IAP/OTA 后向量表没重定位**：App 里忘写 `SCB->VTOR = 0x08002000`，中断一来 NVIC 去 0x08000000 的旧向量表取地址 → 跳飞 → HardFault（HFSR.VECTTBL=1 直接暴露）。这是 BootLoader 项目第一高频故障。

## 4.5 面试官视角

- "程序跑飞了你怎么查？"→ 别答"看门狗复位"，先答：读 CFSR/HFSR 定故障类型 → 从栈里取 PC/LR → 反汇编定位 → 结合 BFAR/MMFAR 找坏地址 → 再谈根因分类（越界/野指针/时钟/栈）。**能背出这条流程，比会配十个外设都值钱。**
- "HardFault 和 BusFault 什么关系？"→ 升级机制：可配置故障默认关闭，统一升级 HardFault；打开 SHCSR 使能位后各回各家，信息更细。
- "IMPRECISERR 为什么拿不到地址？"→ 写操作进写缓冲，CPU 已往后执行，报错时现场已不在原指令，需要缩小范围复现（或用内存屏障强制同步把 imprecise 转 precise）。

## 4.6 本篇自测题

1. 背出异常压栈顺序与 SP+24 的含义；EXC_RETURN bit2 怎么用？
2. 数组越界为什么会随机 HardFault？为什么"随机"比"必现"更危险？
3. F1 上 BusFault 默认是独立异常还是并入 HardFault？怎么分开？
4. HFSR.FORCED=1 且 CFSR.UFSR 的 INVSTATE=1，最可能是什么代码导致的？
5. 写出让 Fault 各回各家的三行配置（SHCSR 三个使能位）。
