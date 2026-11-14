# 12-5 标识符过滤器：16 位屏蔽模式

## 功能

按键 1 依次发送 16 帧，环回模式下经过两个 16 位**屏蔽模式**过滤器：

| ID 范围 | 是否接收 |
|---|---|
| 0x100 ~ 0x1FF | 否 |
| 0x200 ~ 0x2FF | 是 |
| 0x310 ~ 0x31F | 否 |
| 0x320 ~ 0x32F | 是 |

## 过滤器配置

```c
// 过滤器 1：0x200~0x2FF，标准数据帧
CAN_FilterIdHigh      = 0x200 << 5;
CAN_FilterMaskIdHigh  = (0x700 << 5) | 0x10 | 0x08;

// 过滤器 2：0x320~0x32F，标准数据帧
CAN_FilterIdLow       = 0x320 << 5;
CAN_FilterMaskIdLow   = (0x7F0 << 5) | 0x10 | 0x08;
```

- `0x700 << 5`：只比较 ID 的 bit10:8；
- `0x7F0 << 5`：只比较 ID 的 bit10:4；
- `0x10`：IDE=0，只要标准帧；
- `0x08`：RTR=0，只要数据帧。

## 说明

- 模式：`CAN_Mode_LoopBack`；
- 位时序：`Prescaler=48, BS1=2tq, BS2=3tq`（36 MHz 下 125 kbps）；
- 16 位屏蔽模式一个 bank 放 2 个 ID+掩码。
