# 05 · 时钟树与 SysTick

> 72MHz 到底从哪来、怎么分发到每个外设、你的 delay_us 为什么写 `LOAD = 9*us`——
> 本篇把"时钟"这件事在寄存器层面闭环。素材：`system_stm32f10x.c` 的 SystemInit/SetSysClockTo72、
> `system\delay.c`、`Timer.c` 的 PSC/ARR、`AD.c` 的 `RCC_ADCCLKConfig`。
>
> 手册：RM0008 §7（Reset and clock control，含全时钟树图 Figure）、§3（Flash 接口等待周期）。

## 5.1 一张图先立起来（RM0008 §7 的 Clock tree）

F103 时钟树四层结构，**面试白板必须画得出**：

```
                ┌─ HSI 8MHz（内部 RC，复位默认）
时钟源：        ├─ HSE 8MHz（外部晶振，你板子上的）
                └─ PLL  ×9 ───────► SYSCLK = 72MHz（最终系统时钟）
                                    │
                    ┌───────────────┼──────────────────┐
                    ▼               ▼                  ▼
                 AHB 预分频 ÷1    AHB = HCLK 72MHz ──► Flash 接口(2WS)
                    │               │
        ┌───────────┼───────────────┼───────────────┐
        ▼           ▼               ▼               ▼
     APB1 ÷2     APB2 ÷1         DMA/SDIO      Cortex 内核(直接 HCLK)
     PCLK1=36   PCLK2=72                       → SysTick、NVIC
        │           │
   TIM2~7(×2)   TIM1、GPIO、
   USART2/3    USART1、SPI1、
   I2C、SPI2   ADC(再÷6=12MHz)
```

三个最反直觉、也最常考的点：

1. **TIM2~TIM7 挂在 APB1（PCLK1=36MHz）上，但定时器时钟是 72MHz**：RM0008 §7 明确，**当 APB1 预分频 ≠ 1 时，TIMx 时钟 = PCLK1 × 2**。原因：万一用户把 APB1 分到很慢（比如 ÷16 给低功耗），定时器不该跟着变慢。你 Timer.c 的推导就依赖这条：`72MHz / (PSC+1=7200) / (ARR+1=10000) = 1Hz`——若误以为 TIM2 只有 36MHz，1s 定时会变 2s。
2. **ADC 最高 14MHz**：PCLK2=72MHz 直接喂给 ADC 会超规格。ADC 有自己的预分频器（ADCPRE），你的 `AD.c` L7 `RCC_ADCCLKConfig(RCC_PCLK2_Div6)` → 72/6 = **12MHz** < 14MHz ✓。**注意 SystemInit 里没配 ADCPRE**（复位值 00 = PCLK2/2 = 36MHz 超频！），所以这行必须自己写——你已经写了，非常好，但面试时要能讲出"为什么必须写"。
3. **内核外设（NVIC/SCB/SysTick）没有时钟开关**，直接吃 HCLK，永远在线（01 篇提过）。所以 SysTick 的默认时钟源是 **HCLK/8 = 9MHz**——下一步的所有计算都从这来。

## 5.2 SystemInit 的时钟切换为什么是这个顺序（逐行回放）

01 篇列过代码，这里讲"为什么"（`system_stm32f10x.c` L987–L1080）：

| 步骤 | 代码 | 为什么必须这个顺序 |
|---|---|---|
| 1 | 开 HSE、等 HSERDY（带超时） | 晶振起振需要毫秒级稳定时间，超时防卡死 |
| 2 | `FLASH->ACR = PRFTBE \| LATENCY_2` | **先降速（加等待）再提速**：72MHz 时 Flash 最快 2 周期出数，先配好 ACR，之后切 72MHz 才不会取错指令 |
| 3 | HPRE÷1、PPRE2÷1、PPRE1÷2 | 总线分频先定死，外设时钟立刻稳定 |
| 4 | PLLSRC=HSE、PLLMULL=9，开 PLL，等 PLLRDY | PLL 锁定也需要时间 |
| 5 | `SW=PLL`，等 SWS=0x08 | **最后一切换**：SYSCLK 源切换是原子动作，之前的 HSI 始终在兜底 |

Flash 等待周期表（RM0008 §3 ACR 寄存器，必须背）：

| SYSCLK | LATENCY | 你的代码里 |
|---|---|---|
| 0–24MHz | 0 WS | — |
| 24–48MHz | 1 WS | SetSysClockTo36/48 用 LATENCY_1 |
| 48–72MHz | 2 WS | SetSysClockTo72 用 LATENCY_2（L1018） |

`PRFTBE`（预取缓冲，L1014）：F1 的 Flash 接口有 2×64 位的预取缓冲，顺序执行时提前把下两条 64 位指令读进来，**把 2 等待周期的平均代价摊薄到接近 0**——所以 72MHz 下实际执行速度远好于"每条指令 2 等待"。这也是为什么建议**代码顺序执行、少跳转**（跳转会清空预取缓冲）。

**工程对照**：如果你做低功耗/降频方案，把 SYSCLK 降到 24MHz 以下，记得把 LATENCY 降回 0——顺序和提速相反：先切时钟（降频），再减等待周期。调错顺序的后果是偶发 HardFault 或 Flash 读错，极难查（04 篇的"随机故障"）。

## 5.3 SysTick：内核自带的 24 位节拍器

寄存器（core_cm3.h L365–370，PM0056 §4.5）：

| 寄存器 | 位 | 含义 |
|---|---|---|
| CTRL | 0 | ENABLE 使能 |
| CTRL | 1 | TICKINT：计数到 0 时触发 SysTick_Handler |
| CTRL | 2 | CLKSOURCE：**0 = HCLK/8（9MHz），1 = HCLK（72MHz）** |
| CTRL | 16 | COUNTFLAG：上次读 CTRL 之后计数是否到过 0（读后自动清） |
| LOAD | 23:0 | 重装载值，24 位（最大 0xFFFFFF，9MHz 下最长 1.86s） |
| VAL | 23:0 | 当前计数值，**写任意值立即清零** |

### 逐行解剖你的 delay.c

```c
// 72M系统时钟，SysTick 时钟源 HCLK/8 = 9MHz     ← 你没写 CLKSOURCE 位，默认 0 = HCLK/8
void delay_us(uint32_t us) {
    SysTick->LOAD = 9 * us;      // 9 个计数 = 1µs（9MHz）
    SysTick->VAL = 0x00;         // 写 VAL 清零计数器
    SysTick->CTRL |= ENABLE;     // 开跑
    do { temp = SysTick->CTRL; }
    while((temp & 0x01) && !(temp & (1<<16)));  // ENABLE 还开着 且 没到 0 → 继续等
    SysTick->CTRL &= ~ENABLE;    // 关
    SysTick->VAL = 0x00;
}
```

- `LOAD = 9*us`：计数 9 次 = 9 × (1/9MHz) = 1µs，所以 us 微秒 × 9。
- 轮询 COUNTFLAG（bit16）而不是开 TICKINT 中断——**读 CTRL 这个动作会顺带清 COUNTFLAG**（注意这行代码每次循环都读 CTRL，读一下就把标志清了，所以逻辑成立的前提就是"读即清"）。
- `delay_ms` 的 `9000 * ms` 同理：9000 计数 = 1ms。
- 条件里的 `temp & 0x01` 是防御：万一别处关了 SysTick，不至于死等（虽然这里没别处用）。

### 三个工程级注意点（面试官视角）

1. **24 位上限**：`9 * us` 最大 0xFFFFFF，单次 delay 上限 ≈ 1.86s；`delay_ms(2000)` 会**溢出 LOAD** → 定时完全错误。工程上长延时要用循环多次调用或换 TIM。
2. **SysTick 被 RTOS 占用**：FreeRTOS 一上，SysTick 就是调度器的心脏，你的 `delay_ms` 轮询 CTRL 会和调度器打架。所以 RTOS 工程的延时改用 **DWT 周期计数器**（CYCCNT，HCLK 频率计数，F1 支持：`CoreDebug->DEMCR |= TRCENA; DWT->CYCCNT=0; DWT->CTRL|=1;` 后读 `DWT->CYCCNT` 即可做 µs 级延时，不占用任何中断）。这是"裸机代码 → RTOS 代码"迁移的第一个坑。
3. **COUNTFLAG 读即清**的设计很特殊：如果你开了 TICKINT 中断，ISR 里读 CTRL 会把标志清了——通常无碍，但写"检查是否超时"逻辑时要知道。

## 5.4 从 72MHz 反推你每个外设的时钟参数（查账练习）

| 工程 | 外设 | 挂在哪 | 时钟 | 关键参数推导 |
|---|---|---|---|---|
| 6-1 Timer.c | TIM2 | APB1(36)×2 | **72MHz** | PSC=7200-1, ARR=10000-1 → 1Hz |
| 6-3 PWM | TIM3? | APB1×2 | 72MHz | PSC/ARR 决定 PWM 频率与分辨率 |
| 7-1/8-2 AD.c | ADC1 | APB2 ÷6 | **12MHz** | 采样时间周期换算（RM0008 §11） |
| 9-x Serial.c | USART1 | APB2 | 72MHz | BRR=72e6/(16×9600)=468.75→0x1D4C |
| 11-2 SPI | SPI1 | APB2 | 72MHz | 预分频 ÷4 → 18MHz SCLK |
| 10-2 I2C | I2C1 | APB1 | 36MHz | CCR 计算（RM0008 §17） |
| 所有工程 delay | SysTick | HCLK/8 | **9MHz** | LOAD=9×µs |

面试时随便报一个"为什么 PSC 写 7200-1 而不是 7200"：寄存器存的是 **(PSC+1)** 作为分频值，写 7199 才得到 ÷7200。同理 ARR+1。"-1 魔咒"贯穿整个标准库（预分频、重装载、CCR 全是），这是读 RM0008 才会注意到的细节。

## 5.5 本篇自测题

1. 画出 F103 时钟树并标出：72/36/72/12/9 五个 MHz 各自是谁。
2. TIM2 在 APB1 上为什么是 72MHz？这条规则在 RM0008 哪一章？
3. SystemInit 里 Flash 等待周期为什么必须在切 PLL 之前配？
4. `delay_ms(3000)` 会出什么问题？为什么？怎么改？
5. ADC 时钟为什么必须自己调成 ÷6？不调的后果？
