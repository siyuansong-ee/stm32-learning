/**
 * GPIO 点灯（标准库版）— STM32F103
 *
 * 功能：板载 LED 闪烁（PC13，蓝板常用；若你的板子 LED 在别的引脚，改 GPIO_Pin_x）
 *
 * 学习要点：
 * 1. 用哪个外设，先开哪个时钟：RCC_APB2PeriphClockCmd（GPIO 挂在 APB2）
 * 2. 标准库通用套路：结构体 + Init 函数（GPIO_InitTypeDef + GPIO_Init）
 * 3. 位操作：GPIO_SetBits 置高 / GPIO_ResetBits 置低（底层是 BSRR/BRR，原子操作）
 * 4. 推挽输出（Out_PP）：既能输出高也能输出低，直接驱动 LED
 */
#include "stm32f10x.h"

/* 粗略软件延时（仅教学演示；工程中用 SysTick 或定时器延时更精确） */
void Delay_ms(uint32_t ms)
{
    while (ms--)
    {
        volatile uint32_t i = 7200;  /* 72MHz 主频下约 0.1ms，粗略值 */
        while (i--)
            ;
    }
}

int main(void)
{
    GPIO_InitTypeDef gpio;

    /* 1. 开 GPIOC 时钟（APB2 总线） */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

    /* 2. 配置 PC13：推挽输出，50MHz */
    gpio.GPIO_Pin   = GPIO_Pin_13;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &gpio);

    /* 3. 主循环：亮 500ms，灭 500ms */
    while (1)
    {
        GPIO_SetBits(GPIOC, GPIO_Pin_13);    /* LED 灭（低电平点亮的板子） */
        Delay_ms(500);
        GPIO_ResetBits(GPIOC, GPIO_Pin_13);  /* LED 亮 */
        Delay_ms(500);
    }
}
