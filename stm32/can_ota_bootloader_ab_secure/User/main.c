/**
 ****************************************************************************************************
 * @file        main.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2020-05-12
 * @brief       串口IAP 实验-IAP Bootloader V1.0
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 *
 * 实验平台:正点原子 STM32F103开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 *
 ****************************************************************************************************
 */

#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/usart/usart.h"
#include "./SYSTEM/delay/delay.h"
#include "./USMART/usmart.h"
#include "./BSP/LED/led.h"
#include "./BSP/LCD/lcd.h"
#include "./BSP/KEY/key.h"
#include "./BSP/STMFLASH/stmflash.h"
#include "./IAP/iap.h"


int main(void)
{
    uint8_t t;
    uint8_t key;
    uint32_t oldcount = 0;      /* 老的串口接收数据值 */
    uint32_t applenth = 0;      /* 接收到的app代码长度 */
    uint8_t clearflag = 0;

    HAL_Init();                                 /* 初始化HAL库 */
    sys_stm32_clock_init(RCC_PLL_MUL9);         /* 设置时钟, 72Mhz */
    delay_init(72);                             /* 延时初始化 */
    usart_init(115200);                         /* 串口初始化为115200 */
    led_init();                                 /* 初始化LED */
    lcd_init();                                 /* 初始化LCD */
    key_init();                                 /* 初始化按键 */

    lcd_show_string(30,  50, 200, 16, 16, "STM32", RED);
    lcd_show_string(30,  70, 200, 16, 16, "IAP TEST", RED);
    lcd_show_string(30,  90, 200, 16, 16, "ATOM@ALIENTEK", RED);
    lcd_show_string(30, 110, 200, 16, 16, "KEY_UP: Copy APP2FLASH!", RED);
    lcd_show_string(30, 130, 200, 16, 16, "KEY1: Run FLASH APP", RED);
    lcd_show_string(30, 150, 200, 16, 16, "KEY0: Run SRAM APP", RED);

    while (1)
    {
        if (g_usart_rx_cnt)
        {
            if (oldcount == g_usart_rx_cnt)   /* 新周期内,没有收到任何数据,认为本次数据接收完成 */
            {
                applenth = g_usart_rx_cnt;
                oldcount = 0;
                g_usart_rx_cnt = 0;
                printf("用户程序接收完成!\r\n");
                printf("代码长度:%dBytes\r\n", applenth);
            }
            else oldcount = g_usart_rx_cnt;
        }

        t++;
        delay_ms(100);

        if (t == 3)
        {
            LED0_TOGGLE();
            t = 0;

            if (clearflag)
            {
                clearflag--;

                if (clearflag == 0)
                {
                    lcd_fill(30, 190, 240, 210 + 16, WHITE);    /* 清除显示 */
                }
            }
        }

        key = key_scan(0);

        if (key == WKUP_PRES)   /* WKUP按下,更新固件到FLASH */
        {
            if (applenth)
            {
                printf("开始更新固件...\r\n");
                lcd_show_string(30, 190, 200, 16, 16, "Copying APP2FLASH...", BLUE);

                if (((*(volatile uint32_t *)(0X20001000 + 4)) & 0xFF000000) == 0x08000000)  /* 判断是否为0X08XXXXXX */
                {
                    iap_write_appbin(FLASH_APP1_ADDR, g_usart_rx_buf, applenth);            /* 更新FLASH代码 */
                    lcd_show_string(30, 190, 200, 16, 16, "Copy APP Successed!!", BLUE);
                    printf("固件更新完成!\r\n");
                }
                else
                {
                    lcd_show_string(30, 190, 200, 16, 16, "Illegal FLASH APP!  ", BLUE);
                    printf("非FLASH应用程序!\r\n");
                }
            }
            else
            {
                printf("没有可以更新的固件!\r\n");
                lcd_show_string(30, 190, 200, 16, 16, "No APP!", BLUE);
            }

            clearflag = 7; /* 标志更新了显示,并且设置7*300ms后清除显示 */
        }

        if (key == KEY1_PRES)   /* KEY1按键按下, 运行FLASH APP代码 */
        {
            if (((*(volatile uint32_t *)(FLASH_APP1_ADDR + 4)) & 0xFF000000) == 0x08000000) /* 判断FLASH里面是否有APP,有的话执行 */
            {
                printf("开始执行FLASH用户代码!!\r\n\r\n");
                delay_ms(10);
                iap_load_app(FLASH_APP1_ADDR);/* 执行FLASH APP代码 */

            }
            else
            {
                printf("没有可以运行的固件!\r\n");
                lcd_show_string(30, 190, 200, 16, 16, "No APP!", BLUE);
            }

            clearflag = 7; /* 标志更新了显示,并且设置7*300ms后清除显示 */
        }

        if (key == KEY0_PRES)   /* KEY0按下 */
        {
            printf("开始执行SRAM用户代码!!\r\n\r\n");
            delay_ms(10);

            if (((*(volatile uint32_t *)(0x20001000 + 4)) & 0xFF000000) == 0x20000000)   /* 判断是否为0X20XXXXXX */
            {
                iap_load_app(0x20001000);   /* SRAM地址 */
            }
            else
            {
                printf("非SRAM应用程序,无法执行!\r\n");
                lcd_show_string(30, 190, 200, 16, 16, "Illegal SRAM APP!", BLUE);
            }

            clearflag = 7; /* 标志更新了显示,并且设置7*300ms后清除显示 */
        }

    }
}























