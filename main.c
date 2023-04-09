/*
 * This example demonstrates using I2C with SSD1306
 * oled controller
 */

#include "stm32f0xx_ll_rcc.h"
#include "stm32f0xx_ll_system.h"
#include "stm32f0xx_ll_bus.h"
#include "stm32f0xx_ll_gpio.h"
#include "stm32f0xx_ll_exti.h"                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          
#include "stm32f0xx_ll_utils.h"
#include "stm32f0xx_ll_cortex.h"
#include "stm32f0xx_ll_tim.h"


#include "xprintf.h"
#include "oled_driver.h"

/*
 * This array contains grayscale photo of Donov
//  */

// #define  action1 LL_GPIO_PIN_0
// #define  UP      LL_GPIO_PIN_1
// #define  DOWN    LL_GPIO_PIN_2
// #define  LEFT    LL_GPIO_PIN_3
// #define  RIGHT   LL_GPIO_PIN_4
// #define  action2_3 LL_GPIO_PIN_5
uint8_t flags = 1;
#define flag_menu (flags>>0)&1
#define flag_game (flags>>1)&1
#define flag_inp (flags>>2)&1
#define resetflag(bit) flags = (1<<bit)



uint8_t x = 128/2, y = 32, menu = 0, speed = 5;
uint16_t ms_old = 0, ms;
uint32_t tick = 0, last_tick = 0, sec = 0, S = 18, col = 0;

//GAME
uint32_t board[128 * 64/32] = {};
uint32_t new_board[128 * 64/32] = {};

uint8_t get_bit(unsigned long int * a, int x, int y){
    return (a[4*y+x/32] >> (x%32) ) & 1;
}
void set_bit(unsigned long int * a, int x, int y, int value){
    if (get_bit(a, x, y) == value){
        return;
    }
    a[4*y+x/32] = a[4*y+x/32] ^ 1<<(x%32);
}

uint32_t LIVE_GET_CUA(){
    col = 0;
    for (uint16_t y = 0; y < GMEM_HEIGHT; y++) {
        for (uint16_t x = 0; x < GMEM_WIDTH; x++) {
            col += get_bit(board, x, y);
        }
    }
    return col;
}
void LIVE_DELITE(){
    for (uint16_t i = 0; i < 256; i++){
        new_board[i] = 0;
    }for (uint16_t i = 0; i < 256; i++){
        board[i] = 0;
    }
}
uint8_t LIVE_GET_NEIGHBOUR(uint8_t x, uint8_t y){
    int8_t corx, cory, nei = 0;
    for (int8_t x_p = -1; x_p < 2; x_p++)
    {
        for (int8_t y_p = -1; y_p < 2; y_p++)
        {
            corx = x + x_p;
            cory = y + y_p;
            if (corx >= 0 && corx < 128 && cory >= 0 && cory < 64){
                nei += get_bit(board, corx, cory);
            }
        }
    }
    return nei - get_bit(board, x, y);
}
uint8_t LFSR_Fibonacci (){
  S = ((((S >> 31) ^ (S >> 30) ^ (S >> 29) ^ (S >> 27) ^ (S >> 25) ^ S ) & 0x00000001 ) << 31 ) | (S >> 1);
  return S & 0x00000001;
}
void LIVE_INIT_RAND(){
    S = sec;
    for (uint8_t y = 0; y < GMEM_HEIGHT; y++) {
        for (uint8_t x = 0; x < GMEM_WIDTH; x++) {
            if (LFSR_Fibonacci () & LFSR_Fibonacci ()){
            set_bit(board, x, y, LFSR_Fibonacci ());
            }else {
                set_bit(board, x, y, 0);
            }

        }
    }
}
void oled_board(){
    uint8_t x, y;

    for (y = 0; y < GMEM_HEIGHT; y++) {
        for (x = 0; x < GMEM_WIDTH; x++) {
            if (get_bit(board, x, y) == 1){
                oled_set_pix(x, y, clWhite);
            }
            else{
                oled_set_pix(x, y, clBlack);
            }
        }
    }
}
void LIVE_STEP(){
    for (uint16_t i = 0; i < 256; i++){
        new_board[i] = board[i];
    }

    uint8_t nei = 0;
    for (uint16_t y = 0; y < GMEM_HEIGHT; y++) {
        for (uint16_t x = 0; x < GMEM_WIDTH; x++) {
            nei = LIVE_GET_NEIGHBOUR(x, y); 
            if(get_bit(board, x ,y) == 1){
                if (nei > 3 || nei < 2){
                    set_bit(new_board, x, y, 0);
                }
            }
            if(get_bit(board, x ,y) == 0 && nei == 3){
                    set_bit(new_board, x, y, 1);
            }
        }
    }

    for (uint16_t i = 0; i < 256; i++){
        board[i] = new_board[i];
    }
}


static void rcc_config()
{
    /* Set FLASH latency */
    LL_FLASH_SetLatency(LL_FLASH_LATENCY_1);

    /* Enable HSI and wait for activation*/
    LL_RCC_HSI_Enable();
    while (LL_RCC_HSI_IsReady() != 1);

    /* Main PLL configuration and activation */
    LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSI_DIV_2,
                                LL_RCC_PLL_MUL_12);

    LL_RCC_PLL_Enable();
    while (LL_RCC_PLL_IsReady() != 1);

    /* Sysclk activation on the main PLL */
    LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
    LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);
    while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL);

    /* Set APB1 prescaler */
    LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);

    /* Update CMSIS variable (which can be updated also
     * through SystemCoreClockUpdate function) */
    SystemCoreClock = 48000000;
}
static void gpio_config(void)
{
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOC);
    LL_GPIO_SetPinMode(GPIOC, LL_GPIO_PIN_9, LL_GPIO_MODE_OUTPUT);
    LL_GPIO_SetPinMode(GPIOC, LL_GPIO_PIN_8, LL_GPIO_MODE_OUTPUT);
    LL_GPIO_ResetOutputPin(GPIOC, LL_GPIO_PIN_8);
    LL_GPIO_ResetOutputPin(GPIOC, LL_GPIO_PIN_9);

    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_0, LL_GPIO_MODE_INPUT);
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_1, LL_GPIO_MODE_INPUT);
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_2, LL_GPIO_MODE_INPUT);
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_3, LL_GPIO_MODE_INPUT);
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_4, LL_GPIO_MODE_INPUT);
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_5, LL_GPIO_MODE_INPUT);

    LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_1, LL_GPIO_PULL_UP);
    LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_2, LL_GPIO_PULL_UP);
    LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_3, LL_GPIO_PULL_UP);
    LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_4, LL_GPIO_PULL_UP);
    LL_GPIO_SetPinPull(GPIOA, LL_GPIO_PIN_5, LL_GPIO_PULL_UP);
    return;
}
static void exti_config(void)
{
    LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_SYSCFG);

    LL_SYSCFG_SetEXTISource(LL_SYSCFG_EXTI_PORTA, LL_SYSCFG_EXTI_LINE0);
    LL_SYSCFG_SetEXTISource(LL_SYSCFG_EXTI_PORTA, LL_SYSCFG_EXTI_LINE1);
    LL_SYSCFG_SetEXTISource(LL_SYSCFG_EXTI_PORTA, LL_SYSCFG_EXTI_LINE2);
    LL_SYSCFG_SetEXTISource(LL_SYSCFG_EXTI_PORTA, LL_SYSCFG_EXTI_LINE3);
    LL_SYSCFG_SetEXTISource(LL_SYSCFG_EXTI_PORTA, LL_SYSCFG_EXTI_LINE4);
    LL_SYSCFG_SetEXTISource(LL_SYSCFG_EXTI_PORTA, LL_SYSCFG_EXTI_LINE5);

    LL_EXTI_EnableIT_0_31(LL_EXTI_LINE_0);
    LL_EXTI_EnableIT_0_31(LL_EXTI_LINE_1);
    LL_EXTI_EnableIT_0_31(LL_EXTI_LINE_2);
    LL_EXTI_EnableIT_0_31(LL_EXTI_LINE_3);
    LL_EXTI_EnableIT_0_31(LL_EXTI_LINE_4);
    LL_EXTI_EnableIT_0_31(LL_EXTI_LINE_5);

    LL_EXTI_EnableRisingTrig_0_31(LL_EXTI_LINE_0);
    LL_EXTI_EnableRisingTrig_0_31(LL_EXTI_LINE_1);
    LL_EXTI_EnableRisingTrig_0_31(LL_EXTI_LINE_4);
    LL_EXTI_EnableRisingTrig_0_31(LL_EXTI_LINE_3);
    LL_EXTI_EnableRisingTrig_0_31(LL_EXTI_LINE_2);
    LL_EXTI_EnableRisingTrig_0_31(LL_EXTI_LINE_5);
    /*
     * Setting interrupts
     */
    NVIC_EnableIRQ(EXTI0_1_IRQn);
    NVIC_SetPriority(EXTI0_1_IRQn, 0);
    NVIC_EnableIRQ(EXTI2_3_IRQn);
    NVIC_SetPriority(EXTI2_3_IRQn, 0);
    NVIC_EnableIRQ(EXTI4_15_IRQn);
    NVIC_SetPriority(EXTI4_15_IRQn, 0);
    return;
}
static void systick_config(void){
    /*
    * Частота 1 КГц (выставляются регистр-перезагрузка SYST_RVR и производится
    * включение таймера в SYST_CSR)
    * Входные параметры для функции это текущая частота МК и желаемая частота таймера
    */
    LL_InitTick(48000000, 1000);
    /*
    * Разрешение прерываний в модуле таймера в SYST_CSR
    */
    LL_SYSTICK_EnableIT();

    /*
    * В данном случае приоритет будет выставлен через регистр SCB, а не регистры NVIC, так как
    * номер прерываний -1 (системное прерывание)
    */
    NVIC_SetPriority(SysTick_IRQn, 31);
}
static void timers_config(void)
{
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM2);
    LL_TIM_SetPrescaler(TIM2, 47999);
    LL_TIM_SetAutoReload(TIM2, 199);
    LL_TIM_SetCounterMode(TIM2, LL_TIM_COUNTERMODE_UP);

    LL_TIM_EnableIT_UPDATE(TIM2);
    LL_TIM_EnableCounter(TIM2);

    NVIC_EnableIRQ(TIM2_IRQn);
    NVIC_SetPriority(TIM2_IRQn, 16);
    return;
}
static void printf_config(void)
{
    xdev_out(oled_putc);
    return;
}

/*
 * Init all periphs and print out something
 */

uint8_t mar = 1;
void SysTick_Handler(void)//прерывание таймера
{
    // if (sec % 150 == 0){
        
    // }
    sec++;
}
void TIM2_IRQHandler(void)
{
    oled_set_cursor(0, 0);
    if (flag_game){
        if (sec % (11-speed) == 0){
        LIVE_STEP();
        oled_board();
        xprintf("%d", LIVE_GET_CUA());
        }
    }else if (flag_menu){
        xprintf("(%s)continue game\n", (menu == 0)?"*":"");
        xprintf("(%s)input mode\n", (menu == 1)?"*":"");
        xprintf("(%s)delete all\n", (menu == 2)?"*":"");
        xprintf("(%s)fill random\n", (menu == 3)?"*":"");
        xprintf("(%s)set speed(%d)\n", (menu == 4)?"*":"", speed);
        xprintf("(%s)exit\n", (menu == 5)?"*":"");
        xprintf("N = %d", LIVE_GET_CUA());
    }else if (flag_inp){
        oled_board();
        xprintf(" x, y = %d, %d\n", x, y);
        if (mar==3){
            oled_set_pix(x, y, clWhite);
            mar = 0;
        } else{
            oled_set_pix(x, y, clBlack);
            mar++;
        }
    }
    oled_update();
    LL_TIM_ClearFlag_UPDATE(TIM2);
}
void EXTI4_15_IRQHandler(void){
    ms = sec;
    LL_GPIO_TogglePin(GPIOC, LL_GPIO_PIN_8);
    if (ms - ms_old > 15 && LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_4)){
        if (x < 127 && flag_inp)
            x++;
        else if(menu == 4 && speed <= 9)
            speed++;
    }  

    if (ms - ms_old > 15 && LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_5)){
        if(flag_menu){
            oled_clr(clBlack);
            switch (menu){
            case 0://продолжить
                oled_board();
                oled_update();
                resetflag(1);
                break;
            case 1://изменение
                x = 64;
                y = 32;
                resetflag(2);
                break;
            case 2://удаление
                LIVE_DELITE();
                break;
            case 3://random
                LIVE_DELITE();
                LIVE_INIT_RAND();
                break;
            case 5://exit
                NVIC_SystemReset();
                break;
            }  
            oled_update();
        }else if (!flag_inp){
            oled_clr(clBlack);
            oled_update();
            resetflag(0);//вызов меню
            menu = 0;
        }else if (flag_inp){
            set_bit(board, x, y, (get_bit(board, x, y) == 0)?1:0);
        }else{
            resetflag(0);
        }
    }  


    ms_old = ms;
    LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_4);
    LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_5);
}
void EXTI2_3_IRQHandler(void){
    ms = sec;
    LL_GPIO_TogglePin(GPIOC, LL_GPIO_PIN_8);
    if (ms - ms_old > 15 && LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_2)){
        if (y < 127 && flag_inp)
            y++;
        else if (menu < 4 && flag_menu)
            menu++;
    }

    
    if (ms - ms_old > 15 && LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_3)){
        if (x > 0 && flag_inp)
            x--;
        else if(menu == 4 && speed >= 2)
            speed--;
    }  

    ms_old = ms;
    LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_2);
    LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_3);
}
void EXTI0_1_IRQHandler(void){
    ms = sec;
    LL_GPIO_TogglePin(GPIOC, LL_GPIO_PIN_8);
    if (ms - ms_old > 40 && LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_0)){
        if (flag_inp){
            oled_clr(clBlack);
            oled_update();
            resetflag(0);
        }
        
    }


    if (ms - ms_old > 15 && LL_EXTI_IsActiveFlag_0_31(LL_EXTI_LINE_1)){
        if (y > 0 && flag_inp)
            y--;
        else if (menu > 0 && flag_menu)
            menu--;
    }
    

    ms_old = ms;
     
    LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_0);
    LL_EXTI_ClearFlag_0_31(LL_EXTI_LINE_1);
    return;
}


int main(void)
{
    rcc_config();
    gpio_config();
    oled_config();
    exti_config();
    timers_config();
    printf_config();
    systick_config();
    while (1){
    }
    return 0;
}