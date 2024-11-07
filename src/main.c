#include "stm32f0xx.h"
#include <stdint.h>
#include <math.h>   // for M_PI

void internal_clock();


//AUSTINS PART:
//==========================================================================================================================================================
//==========================================================================================================================================================
//==========================================================================================================================================================

//=============================================================================
// Part 2: Debounced keypad scanning.
//=============================================================================
float cNotes[9] = {16.351999, 32.703999, 65.407997, 130.815994, 261.631989, 523.263977, 1046.527954, 2093.055908, 4186.111816};
float dNotes[9] = {18.354000, 36.708000, 73.416000, 146.832001, 293.664001, 587.328003, 1174.656006, 2349.312012, 4698.624023};
float eNotes[9] = {20.601999, 41.203999, 82.407997, 164.815994, 329.631989, 659.263977, 1318.527954, 2637.055908, 5274.111816};
float fNotes[9] = {21.827000, 43.653999, 87.307999, 174.615997, 349.231995, 698.463989, 1396.927979, 2793.855957, 5587.711914};
float gNotes[9] = {24.500000, 49.000000, 98.000000, 196.000000, 392.000000, 784.000000, 1568.000000, 3136.000000, 6272.000000};
float aNotes[9] = {27.500000, 55.000000, 110.000000, 220.000000, 440.000000, 880.000000, 1760.000000, 3520.000000, 7040.000000};
float bNotes[9] = {30.868000, 61.736000, 123.472000, 246.944000, 493.888000, 987.776001, 1975.552002, 3951.104004, 7902.208008};

int OCTAVE = 4;

uint8_t col; // the column being scanned
uint8_t hist[16];
char queue[2];  // A two-entry queue of button press/release events.
const char keymap[] = "DCBA#9630852*741";
int qin;        // Which queue entry is next for input
int qout;       // Which queue entry is next for output

void push_queue(int n) {
    queue[qin] = n;
    qin ^= 1;
}

void update_history(int c, int rows)
{
    // We used to make students do this in assembly language.
    for(int i = 0; i < 4; i++) {
        hist[4*c+i] = (hist[4*c+i]<<1) + ((rows>>i)&1);
        if (hist[4*c+i] == 0x01)
            push_queue(0x80 | keymap[4*c+i]);
        if (hist[4*c+i] == 0xfe)
            push_queue(keymap[4*c+i]);
    }
}

void drive_column(int c)
{
    GPIOC->BSRR = 0xf00000 | ~(1 << (c + 4));
}

int read_rows()
{
    return (~GPIOC->IDR) & 0xf;
}




//============================================================================
// The Timer 7 ISR
//============================================================================
// Write the Timer 7 ISR here.  Be sure to give it the right name.
void TIM7_IRQHandler()
{
    TIM7->SR &= ~TIM_SR_UIF; //Acknowledge interrupt
    int rows = read_rows();
    update_history(col, rows);
    col = (col + 1) & 3;
    drive_column(col);

}

//============================================================================
// init_tim7()
//============================================================================
void init_tim7(void) {
    RCC->APB1ENR |= (1 << 5); //Enable RCC clock
    TIM7->PSC = 479; //Prescale to 100 kHz
    TIM7->ARR = 99; //Timer to 100 -> 1kHz
    TIM7->DIER |= TIM_DIER_UIE; //Enable interrupt each time the timer is reset
    NVIC->ISER[0] |= (1 << TIM7_IRQn); //Enabled interrupt
    TIM7->CR1 |= 1; //Enabled CEN
}

//=============================================================================
// Part 3: Analog-to-digital conversion for a volume level.
//=============================================================================
uint32_t volume = 2048;

//============================================================================
// setup_adc()
//============================================================================
void setup_adc(void) {
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;

    //PA1 as analog
    GPIOA->MODER &= ~0x00c; //Clear
    GPIOA->MODER |= 0x00c; //Output

    //Emable clock to ADC peripheral
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;

    //Turn on high speed internal 14MHz clock and wait for it to be ready
    RCC->CR2 |= RCC_CR2_HSI14ON;
    while ((RCC->CR2 & RCC_CR2_HSI14RDY) == 0)
    {
    /* For robust implementation, add here time-out management */
    }

    

    //Enable ADC and wait till it is ready
    ADC1->CR |= ADC_CR_ADEN;
    while ((ADC1->ISR & ADC_ISR_ADRDY) == 0) 
    {
    /* For robust implementation, add here time-out management */
    } 

    ADC1->CHSELR = ADC_CHSELR_CHSEL1;
}

//============================================================================
// Varables for boxcar averaging.
//============================================================================
#define BCSIZE 32
int bcsum = 0;
int boxcar[BCSIZE];
int bcn = 0;

//============================================================================
// Timer 2 ISR
//============================================================================
// Write the Timer 2 ISR here.  Be sure to give it the right name.
void TIM2_IRQHandler()
{
    TIM2->SR &= ~TIM_SR_UIF; //Acknowledge interrupt
    ADC1->CR |= 0x04; //Start the ADC

    //Wait until EOC bit is set in register
    while ((ADC1->ISR & (1 << ADC_ISR_EOC_Pos)) == 0) 
    {
    /* For robust implementation, add here time-out management */
    } 

    bcsum -= boxcar[bcn];
    bcsum += boxcar[bcn] = ADC1->DR;
    bcn += 1;
    if (bcn >= BCSIZE)
        bcn = 0;
    volume = bcsum / BCSIZE;

}


//============================================================================
// init_tim2()
//============================================================================
void init_tim2(void) {
    RCC->APB1ENR |= 1; //Enable RCC clock
    TIM2->PSC = 479; //Prescale to 100 kHz
    TIM2->ARR = 9999; //Timer to 10 kHz -> 10 Hz
    TIM2->DIER |= TIM_DIER_UIE; //Enable interrupt each time the timer is reset
    NVIC->ISER[0] |= (1 << TIM2_IRQn); //Enabled interrupt
    TIM2->CR1 |= 1; //Enabled CEN    
    //NVIC_Set
}


//===========================================================================
// Part 4: Create an analog sine wave of a specified frequency
//===========================================================================
void dialer(void);

// Parameters for the wavetable size and expected synthesis rate.
#define N 1000
#define RATE 20000
short int wavetable[N];
int step0 = 0;
int offset0 = 0;
int step1 = 0;
int offset1 = 0;

//===========================================================================
// init_wavetable()
// Write the pattern for a complete cycle of a sine wave into the
// wavetable[] array.
//===========================================================================
void init_wavetable(void) {
    for(int i=0; i < N; i++)
        wavetable[i] = 32767 * sin(2 * M_PI * i / N);
}

//============================================================================
// set_freq()
//============================================================================
void set_freq(int chan, float f) {
    if (chan == 0) {
        if (f == 0.0) {
            step0 = 0;
            offset0 = 0;
        } else
            step0 = (f * N / RATE) * (1<<16);
    }
    if (chan == 1) {
        if (f == 0.0) {
            step1 = 0;
            offset1 = 0;
        } else
            step1 = (f * N / RATE) * (1<<16);
    }
}

//============================================================================
// setup_dac()
//============================================================================
void setup_dac(void) {
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN; //Enable port A clock

    GPIOA->MODER |= GPIO_MODER_MODER4; //Put PA4 into analog mode

    //Enable clock to DAC
    RCC->APB1ENR |= RCC_APB1ENR_DACEN;

    //Set the DAC trigger source to TIM6 TRGO
    DAC->CR &= ~DAC_CR_TSEL1;     
    DAC->CR |= DAC_CR_TEN1;

    //Enable trigger for DAC chnl 1
    DAC->CR |= DAC_CR_TEN1;

    //Enable DAC
    DAC->CR |= DAC_CR_EN1;



}

//============================================================================
// Timer 6 ISR
//============================================================================
// Write the Timer 6 ISR here.  Be sure to give it the right name.
void TIM6_DAC_IRQHandler()
{
    // Acknowledge the interrupt here first!
    TIM6->SR &= ~TIM_SR_UIF; //Acknowledge interrupt

    offset0 += step0;
    offset1 += step1;

    if(offset0 >= (N << 16))
    {
        offset0 -= (N << 16);
    }

    if(offset1 >= (N << 16))
    {
        offset1 -= (N << 16);
    }

    int samp = wavetable[offset0>>16] + wavetable[offset1>>16];
    samp = samp * volume;
    samp = (samp >> 17); //shift samp right by 17 bits to ensure it's in the right format for `DAC_DHR12R1` 
    samp += 2048;
    DAC->DHR12R1 = samp;
}


//============================================================================
// init_tim6()
//============================================================================
void init_tim6(void) {
    //TODO: Make this work

    RCC->APB1ENR |= RCC_APB1ENR_TIM6EN; //Enable RCC clock

    int divisor = 48000000 / (100 * RATE);

    TIM6->PSC = (divisor) - 1; //Prescale
    TIM6->ARR = 100 - 1; //Number to count to

    TIM6->DIER |= TIM_DIER_UIE; //Enable interrupt each time the timer is reset

    // Configure TRGO
    TIM6->CR2 &= ~TIM_CR2_MMS;        // Clear MMS
    TIM6->CR2 |= TIM_CR2_MMS_1;       // Set to TRGO (MMS = 010)

    NVIC->ISER[0] |= (1 << TIM6_DAC_IRQn); //Enabled interrupt

    TIM6->CR1 |= TIM_CR1_CEN; //Enabled CEN    


}

int main() {
    internal_clock();

    setup_adc();
    init_tim7();
    init_tim2();
    init_wavetable();
    setup_dac();
    init_tim6();

    set_freq(0,1000);

    
    
}


