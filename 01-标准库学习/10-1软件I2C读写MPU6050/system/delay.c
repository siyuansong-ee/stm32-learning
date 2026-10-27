#include "delay.h"

// 72M系统时钟，SysTick 时钟源 HCLK/8 = 9MHz
void delay_us(uint32_t us)
{
    uint32_t temp;
    SysTick->LOAD = 9 * us;        // 装载计数值
    SysTick->VAL = 0x00;           // 清空计数器
    SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk; // 开启定时器
    do
    {
        temp = SysTick->CTRL;
    } while((temp & 0x01) && !(temp & (1 << 16))); //等待时间到达
    SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;    //关闭计数器
    SysTick->VAL = 0x00;                           //清空计数器
}

void delay_ms(uint32_t ms)
{
    uint32_t temp;
    SysTick->LOAD = 9000 * ms;
    SysTick->VAL = 0x00;
    SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;
    do
    {
        temp = SysTick->CTRL;
    } while((temp & 0x01) && !(temp & (1 << 16)));
    SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
    SysTick->VAL = 0x00;
}
