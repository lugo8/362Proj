/**
  ******************************************************************************
  * @file    main.c
  * @author  Weili An, Niraj Menon
  * @date    Feb 7, 2024
  * @brief   ECE 362 Lab 7 student template
  ******************************************************************************
*/

/*******************************************************************************/

// Fill out your username!  Even though we're not using an autotest, 
// it should be a habit to fill out your username in this field now.
const char* username = "lugo8";

/*******************************************************************************/ 

#include "stm32f0xx.h"
#include <stdint.h>

void internal_clock();

// Uncomment only one of the following to test each step
#define STEP1
//#define STEP2
//#define STEP3
//#define STEP4

void init_usart5() {
    //enable gpioc/d
    RCC->AHBENR |= RCC_AHBENR_GPIOCEN;
    RCC->AHBENR |= RCC_AHBENR_GPIODEN;

    //PC12 and PD2 in alternate mode
    GPIOC->MODER |= (0x02 << GPIO_MODER_MODER12_Pos);
    GPIOD->MODER |= (0x02 << GPIO_MODER_MODER2_Pos);

    //set PC12 to USART5 TX
    GPIOC->AFR[1] &= ~(0xf << GPIO_AFRH_AFRH4_Pos);
    GPIOC->AFR[1] |= (0x2 << GPIO_AFRH_AFRH4_Pos);

    //set PD2 to USART5 RX
    GPIOD->AFR[0] &= ~(0xf << GPIO_AFRL_AFRL2_Pos);
    GPIOD->AFR[0] |= (0x2 << GPIO_AFRL_AFRL2_Pos);

    //enable clock to usart5
    RCC->APB1ENR |= RCC_APB1ENR_USART5EN;

    //Disable usart5
    USART5->CR1 &= ~1;

    //Word size is 8 bits
    USART5->CR1 &= ~USART_CR1_M1;
    USART5->CR1 &= ~USART_CR1_M0;

    //One stop bit
    USART5->CR2 &= ~USART_CR2_STOP;

    //No parity bit
    USART5->CR1 &= ~USART_CR1_PCE;

    //16x oversampling
    USART5->CR1 &= ~USART_CR1_OVER8;

    //Baud rate of 115.2 kbaud
    USART5->BRR = 0x1A1;

    //Enable transmit and receive pin
    USART5->CR1 |= USART_CR1_TE;
    USART5->CR1 |= USART_CR1_RE;

    //Enable USART5
    USART5->CR1 |= 1;

    //Wait for usart to be ready to transmit/receive
    while(!(USART5->ISR & USART_ISR_TEACK) && !(USART5->ISR & USART_ISR_REACK))
    {}

}

#ifdef STEP1
int main(void){
    internal_clock();
    init_usart5();
    for(;;) {
        while (!(USART5->ISR & USART_ISR_RXNE)) { }
        char c = USART5->RDR;
        while(!(USART5->ISR & USART_ISR_TXE)) { }
        USART5->TDR = c;
    }
}
#endif

#ifdef STEP2
#include <stdio.h>

// TODO Resolve the echo and carriage-return problem

int __io_putchar(int c) {
    // TODO
    if(c == '\n')
    {
        while(!(USART5->ISR & USART_ISR_TXE));
        USART5->TDR = '\r';
    }

    while(!(USART5->ISR & USART_ISR_TXE));
    USART5->TDR = c;
    return c;
}

int __io_getchar(void) {
    while (!(USART5->ISR & USART_ISR_RXNE));
    char c = USART5->RDR;
    if(c == '\r')
    {
        c = '\n';
    }
    __io_putchar(c);
    return c;
}

int main() {
    internal_clock();
    init_usart5();
    setbuf(stdin,0);
    setbuf(stdout,0);
    setbuf(stderr,0);
    printf("Enter your name: ");
    char name[80];
    fgets(name, 80, stdin);
    printf("Your name is %s", name);
    printf("Type any characters.\n");
    for(;;) {
        char c = getchar();
        putchar(c);
    }
}
#endif

#ifdef STEP3
#include <stdio.h>
#include "fifo.h"
#include "tty.h"
int __io_putchar(int c) {
    // TODO Copy from your STEP2
    if(c == '\n')
    {
        while(!(USART5->ISR & USART_ISR_TXE));
        USART5->TDR = '\r';
    }

    while(!(USART5->ISR & USART_ISR_TXE));
    USART5->TDR = c;
    return c;
}

int __io_getchar(void) {
    // TODO
    return line_buffer_getchar();
}

int main() {
    internal_clock();
    init_usart5();
    setbuf(stdin,0);
    setbuf(stdout,0);
    setbuf(stderr,0);
    printf("Enter your name: ");
    char name[80];
    fgets(name, 80, stdin);
    printf("Your name is %s", name);
    printf("Type any characters.\n");
    for(;;) {
        char c = getchar();
        putchar(c);
    }
}
#endif

#ifdef STEP4

#include <stdio.h>
#include "fifo.h"
#include "tty.h"

// TODO DMA data structures
#define FIFOSIZE 16
char serfifo[FIFOSIZE];
int seroffset = 0;

void enable_tty_interrupt(void) {

    // Enable the USART5 RXNE interrupt and DMA mode 
    USART5->CR1 |= USART_CR1_RXNEIE; 
    USART5->CR3 |= USART_CR3_DMAR;    

    // Enable the NVIC interrupt for USART5
    NVIC->ISER[0] = (1 << (USART3_8_IRQn));  

    RCC->AHBENR |= RCC_AHBENR_DMA2EN;
    DMA2->CSELR |= DMA2_CSELR_CH2_USART5_RX;
    DMA2_Channel2->CCR &= ~DMA_CCR_EN;  // First make sure DMA is turned off
    
    // The DMA channel 2 configuration goes here

    //CMAR is equal to the address of serfifo
    DMA2_Channel2->CMAR = (uint32_t)serfifo;

    //CPAR set to usart5 rdr
    DMA2_Channel2->CPAR = (uint32_t)&USART5->RDR;

    DMA2_Channel2->CNDTR = FIFOSIZE;

    //peripheral to memory
    DMA2_Channel2->CCR &= ~DMA_CCR_DIR;

    //disable half transfer interrupt
    DMA2_Channel2->CCR &= ~DMA_CCR_HTIE;

    //disable transfer complete interrupt
    DMA2_Channel2->CCR &= ~DMA_CCR_TCIE;

    //M and p size for 8 bits
    DMA2_Channel2->CCR &= ~DMA_CCR_MSIZE;
    DMA2_Channel2->CCR &= ~DMA_CCR_PSIZE;

    //MINC increment
    DMA2_Channel2->CCR |= DMA_CCR_MINC;

    //PINC not set
    DMA2_Channel2->CCR &= ~DMA_CCR_PINC;

    //CIRC en
    DMA2_Channel2->CCR |= DMA_CCR_CIRC;

    //Disable mem2mem
    DMA2_Channel2->CCR &= ~DMA_CCR_MEM2MEM;

    //Channel priority highest
    DMA2_Channel2->CCR &= ~DMA_CCR_PL;
    DMA2_Channel2->CCR |= 0x3 << DMA_CCR_PL_Pos;

    //Enable
    DMA2_Channel2->CCR |= DMA_CCR_EN;
}

// Works like line_buffer_getchar(), but does not check or clear ORE nor wait on new characters in USART
char interrupt_getchar() {

    // Wait for a newline to complete the buffer.
    while(fifo_newline(&input_fifo) == 0) {
        asm volatile ("wfi"); // wait for an interrupt
    }
    // Return a character from the line buffer.
    char ch = fifo_remove(&input_fifo);
    return ch;
}

int __io_putchar(int c) {
    if(c == '\n')
    {
        while(!(USART5->ISR & USART_ISR_TXE));
        USART5->TDR = '\r';
    }

    while(!(USART5->ISR & USART_ISR_TXE));
    USART5->TDR = c;
    return c;

}

int __io_getchar(void) {
    return interrupt_getchar();
}

// TODO Copy the content for the USART5 ISR here
// TODO Remember to look up for the proper name of the ISR function

void USART3_8_IRQHandler(void) {
    while(DMA2_Channel2->CNDTR != sizeof serfifo - seroffset) {
        if (!fifo_full(&input_fifo))
            insert_echo_char(serfifo[seroffset]);
        seroffset = (seroffset + 1) % sizeof serfifo;
    }
}

int main() {
    internal_clock();
    init_usart5();
    enable_tty_interrupt();

    setbuf(stdin,0); // These turn off buffering; more efficient, but makes it hard to explain why first 1023 characters not dispalyed
    setbuf(stdout,0);
    setbuf(stderr,0);
    printf("Enter your name: "); // Types name but shouldn't echo the characters; USE CTRL-J to finish
    char name[80];
    fgets(name, 80, stdin);
    printf("Your name is %s", name);
    printf("Type any characters.\n"); // After, will type TWO instead of ONE
    for(;;) {
        char c = getchar();
        putchar(c);
    }
}
#endif