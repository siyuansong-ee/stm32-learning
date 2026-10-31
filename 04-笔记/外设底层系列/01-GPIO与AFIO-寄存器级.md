# 01 · GPIO 与 AFIO 寄存器级

> 你的代码：`3-4按键控制LED\hardware\LED.c`、`key.c`、`3-5…\Buzzer.c`、`LightSensor.c`、`4-1OLED\hardware\OLED.c`。
> 手册：RM0008 §8（GPIO and AFIO）、§7.3（APB2 时钟使能寄存器 APB2ENR）。

## 1.1 你的代码长这样

```c
// LED.c L8-L11：PA1/PA2 推挽输出，驱动低电平点亮的 LED
GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;   // 通用推挽输出
GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_1 | GPIO_Pin_2;
GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
GPIO_Init(GPIOA, &GPIO_InitStructure);

// key.c L7：PB1/PB11 上拉输入，按键按下接地 → 读到 0
GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;

// OLED.c L14：PB8/PB9 开漏输出——软件 I2C 的"线与"基础
GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
```

问题来了：`GPIO_Init` 这个函数内部到底改了什么寄存器？`GPIO_Mode_IPU` 这个枚举值又对应哪些位？

## 1.2 引脚的结构：输入通路与输出通路

F1 每个 GPIO 引脚内部有两条独立通路（RM0008 §8.1，引脚结构图必须能默画）：

```
引脚 ──┬──► [保护二极管] ──► [施密特触发器] ──► 输入数据寄存器 IDR
       │
       └── [输出驱动器: 推挽(P-MOS+N-MOS) 或 开漏(仅N-MOS)]
              ▲
              │
        输出数据寄存器 ODR / 置位复位寄存器 BSRR、BRR
```

- **输入通路**：施密特触发器带**迟滞**（两个翻转阈值），输入 0→1 和 1→0 的阈值不同——按键抖动信号经过它不会在阈值附近反复翻转，这是硬件级抗噪的第一道防线；
- **输出通路**：推挽模式 P-MOS/N-MOS 轮流导通，能主动输出高低电平（驱动 LED 用这个）；**开漏模式只有 N-MOS**，输出 0 = 拉低，输出 1 = 高阻"释放"（配合外部上拉电阻，让线上其他设备也能拉低——I2C 总线"线与"的物理基础，你的 OLED.c 用开漏就是这个原因）；
- **模拟输入模式**：施密特触发器被关掉（省电 + 防止数字噪声），引脚直连 ADC——你 AD.c 里的 `GPIO_Mode_AIN`。

## 1.3 CRL/CRH：每个引脚 4 位配置字

GPIO 的配置寄存器是一对 32 位寄存器：**CRL 管引脚 0–7，CRH 管引脚 8–15**，每 4 位一个引脚（RM0008 §8.2.1/8.2.2）：

| CNF[1:0] | 输入模式（MODE=00 时） | 输出模式（MODE≠00 时） |
|---|---|---|
| 00 | 模拟输入 | 通用推挽输出 |
| 01 | 浮空输入 | 通用开漏输出 |
| 10 | **上拉/下拉输入**（上还是下由 ODR 位决定） | **复用推挽**（AF_PP，引脚交给外设） |
| 11 | 保留 | **复用开漏**（AF_OD） |

MODE[1:0]：00=输入，01/10/11=输出最大翻转速率 **2/10/50MHz**。

三个被误解最多的点（面试官挖坑处）：

1. **`GPIO_Speed_50MHz` 不是信号频率**，是**输出驱动器的压摆率档位**（RM0008 表格里叫 "Maximum output speed"）。50MHz 档 = 输出管翻转更陡、驱动更强，同时 **EMI 噪声更大**。工程上 LED/按键这种低速信号应选 2MHz 档——你全部选 50MHz 是"默认值习惯"，能跑但低功耗/低噪声设计里要改。
2. **上拉还是下拉由 ODR 决定**：CNF=10 且 MODE=00 时，ODR 位=1 接上拉电阻，=0 接下拉。`GPIO_Init` 内部会先写 ODR 再写 CRL/CRH（stm32f10x_gpio.c L219/L252 正是用 BSRR/BRR 预置 ODR）。所以 `GPIO_Mode_IPU` 的完整语义 = "CNF=10 + ODR=1"。
3. **复用输出（AF_PP/AF_OD）下，ODR 不再控制引脚**——引脚被外设模块接管（TIM 的 PWM 输出、USART 的 TX、SPI 的 SCK 都是这样）。你的 PWM.c L13 就是 `GPIO_Mode_AF_PP`。什么时候用 AF_OD？外设信号需要"线与"时，比如硬件 I2C 的 SCL/SDA（你 10-2 的 `GPIO_Mode_AF_OD`）。

## 1.4 数据寄存器：ODR vs IDR vs BSRR vs BRR

| 寄存器 | 偏移 | 语义 |
|---|---|---|
| IDR | 0x08 | 引脚**实际电平**（输入态读它；输出态也能读回实际电平） |
| ODR | 0x0C | 输出数据锁存。**读-改-写有竞态**（内核系列 07 篇讲过） |
| BSRR | 0x10 | 写 1 置位 bit[15:0]，写 1 复位 bit[31:16]，**写 0 无效果** |
| BRR | 0x14 | 写 1 复位 bit[15:0]，写 0 无效果 |
| LCKR | 0x18 | 引脚配置锁定（配合 16 位密钥序列，防跑飞误改） |

你的 LED.c L12 `GPIO_SetBits(GPIOA, GPIO_Pin_1|GPIO_Pin_2)` 落地就是一句 `GPIOA->BSRR = 0x0006`——**单条 32 位写，原子**，没有读改写竞态（对比 `GPIOA->ODR |= 0x6` 是读-改-写三步）。

`LED1_Turn()`（L22-L32）里 `GPIO_ReadOutputDataBit` 读的是 **ODR 而不是 IDR**：读 ODR 得到的是"我想输出的值"，读 IDR 得到的是"引脚真实电平"。对开漏引脚这两者可能不同（外部把线拉低时 IDR=0 而 ODR=1）——这个区别在 I2C 读应答时至关重要（09 篇）。

## 1.5 AFIO：一个"路由器"，不是 GPIO

AFIO（RM0008 §8.4）自己不碰数据，它干两件事：

1. **EXTI 引脚选择**：`GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource14)` 写 `AFIO->EXTICR4`，决定"PB14 接到 EXTI14"（02 篇展开）。F1 每个 EXTI 线只能在 PA/PB/PC/PD 同名引脚中选一个。
2. **外设引脚重映射**：`GPIO_PinRemapConfig()` 写 `AFIO->MAPR`，把外设功能从默认引脚换到备用引脚（例如 TIM2 CH1 从 PA0 换到 PA15）。

你仓库笔记里自己纠正过的那条铁律再强调一次：**AFIO 时钟只在"用 EXTI 选引脚"或"重映射"时才需要开**，普通 GPIO 和外设默认复用引脚不需要 `RCC_APB2Periph_AFIO`。你 PWM.c 里被注释掉的 L7-L10（重映射配置）就是备用的——TIM2 的 PWM 引脚默认 PA0/PA1/PA2 够用就不开 AFIO。

## 1.6 按键消抖：你的 20ms 延时在物理上消的是什么

key.c L17-L19：

```c
if(GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_1)==0)  // 检测到按下
{ delay_ms(20);                                  // 消抖延时
  while(GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_1)==0);  // 等松开
  delay_ms(20);                                  // 松开消抖
  KeyNum=1; }
```

按键按下瞬间，金属触点会**弹跳 5–20ms**，产生一串 0/1 毛刺（数十 kHz 量级）。你的方案是**延时消抖**：第一次读到 0 后等 20ms 跳过弹跳期再确认。工程视角的三个升级点（面试加分项）：

1. **阻塞式消抖会卡住主循环**：`while(等松开)` 期间 CPU 干不了别的。若系统里有串口接收/OLED 刷新，应该改成"状态机 + 定时器轮询"或 EXTI+定时器确认，非阻塞；
2. 这里只有"按下消抖"，`while` 等松开后**松开弹跳**靠第二个 20ms 吸收——你的写法是对的，但注意**长按 3 秒期间主循环完全冻结**；
3. 硬件方案：RC 滤波 + 施密特触发器，把消抖交给电路（板级设计常识）。

另外注意你 key.c 的返回值语义：**一次按键只返回一个数**，而且两个按键同时按下时只有 KeyNum=1 生效（if 顺序优先）——这是简化设计，正式产品要能同时检测。

## 1.7 你的 LED/Buzzer/LightSensor 电路为什么"反着写"

- `LED1_ON()` 里是 `GPIO_ResetBits`（**低电平点亮**）：板子 LED 一端接 3.3V、一端接 PA1，引脚拉低才有电流。**看代码先问硬件拓扑**——面试官给一段 ResetBits 点亮的代码问"为什么反着"，考的就是这个；
- `Buzzer_ON()` 同样是 ResetBits（PB12）：蜂鸣器模块多为低电平有效（NPN 驱动）；
- `LightSensor_Get()` 读 PB13 上拉输入：光敏模块 DO 输出数字量，遮挡时模块输出低电平。上拉输入的**高阻态容忍**：模块输出悬空时线上保持高，不误触发。

## 1.8 OLED 的 GPIO 用法：开漏 + 上拉 = 软件 I2C

OLED.c L14 用 `GPIO_Mode_Out_OD` 做 SCL/SDA：写 1 时 N-MOS 关断、引脚高阻，靠模块上的上拉电阻回到高电平；写 0 时拉低。**主机永远只拉低、不主动拉高**——这样从机才能把 SDA 拉低做应答，这就是"线与"。如果误用推挽：从机拉低时主机的 P-MOS 还硬往上顶 → 短路电流 + 数据读错。这是 I2C 物理层最经典的一道面试题，09 篇会从协议层再讲一遍。

## 1.9 面试官视角

- "GPIO 的 8 种模式分别是什么？上拉/下拉由哪个寄存器决定？"→ 4 输入 4 输出；CNF=10 时 ODR 决定上下拉。
- "GPIO_Speed_50MHz 是什么意思？"→ 压摆率档位，不是信号频率；高速度档 EMI 大。
- "为什么点 LED 用推挽、I2C 用开漏？"→ 推挽驱动能力强双向主动；开漏支持线与、电平由外部上拉决定。
- "ODR 和 BSRR 什么区别？为什么库函数用 BSRR？"→ 读改写竞态（内核系列 07 篇联动）。
- "IDR 和 ODR 在开漏模式下会不一样吗？"→ 会，外部拉低时 IDR=0、ODR=1；读引脚电平必须 IDR。

## 1.10 本篇自测题

1. 写出 PA0 配成"上拉输入"后 CRL 的 [3:0] 与 ODR 的 bit0 各是什么值。
2. 为什么 I2C 引脚不能用推挽？画出开漏"线与"的电流路径。
3. 按键消抖的 20ms 是从哪来的物理量？阻塞式消抖有什么代价？
4. AFIO 时钟什么时候必须开？什么时候开了也没用？
5. 复用推挽和通用推挽，在寄存器上差在哪一位？引脚被谁控制？
