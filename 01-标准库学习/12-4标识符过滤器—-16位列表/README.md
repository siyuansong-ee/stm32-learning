# 12-4 标识符过滤器：16 位列表模式

## 功能

按键 1 依次发送 6 帧，环回模式下经过 16 位**列表模式**过滤器，只接收名单里的 ID：

| 发送 ID | 帧类型 | 是否接收 |
|---|---|---|
| 0x123 | 标准数据帧 | 否 |
| 0x234 | 标准遥控帧 | 是 |
| 0x345 | 标准数据帧 | 是 |
| 0x456 | 标准数据帧 | 否 |
| 0x567 | 标准数据帧 | 是 |
| 0x678 | 标准数据帧 | 否 |

## 过滤器配置

```c
CAN_FilterIdHigh      = (0x234 << 5) | 0x08;  // 0x08 = RTR=1，遥控帧
CAN_FilterIdLow       =  0x345 << 5;
CAN_FilterMaskIdHigh  =  0x567 << 5;
CAN_FilterMaskIdLow   =  0x000 << 5;
CAN_FilterMode        = CAN_FilterMode_IdList;
CAN_FilterScale       = CAN_FilterScale_16bit;
```

16 位列表模式一个 bank 放 4 个精确 ID；16 位单元里 `STID` 在 bit15:5，`IDE` 在 bit4，`RTR` 在 bit3。

## 说明

- 模式：`CAN_Mode_LoopBack`；
- 位时序：`Prescaler=48, BS1=2tq, BS2=3tq`（36 MHz 下 125 kbps）；
- 被过滤器拒绝的帧不会进 FIFO，OLED 不更新。
