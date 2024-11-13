#include "stm32f0xx.h"
#include <stdint.h>
#include <math.h>   // for M_PI
#include <stdio.h>
#include <stdlib.h>
#include "ff.h"   // FatFS library
#include "diskio.h" // SD card driver
#include <string.h>


void internal_clock();


//AUSTINS PART:
//==========================================================================================================================================================
//==========================================================================================================================================================
//==========================================================================================================================================================
// float cNotes[9] = {16.351999, 32.703999, 65.407997, 130.815994, 261.631989, 523.263977, 1046.527954, 2093.055908, 4186.111816};
// float dNotes[9] = {18.354000, 36.708000, 73.416000, 146.832001, 293.664001, 587.328003, 1174.656006, 2349.312012, 4698.624023};
// float eNotes[9] = {20.601999, 41.203999, 82.407997, 164.815994, 329.631989, 659.263977, 1318.527954, 2637.055908, 5274.111816};
// float fNotes[9] = {21.827000, 43.653999, 87.307999, 174.615997, 349.231995, 698.463989, 1396.927979, 2793.855957, 5587.711914};
// float gNotes[9] = {24.500000, 49.000000, 98.000000, 196.000000, 392.000000, 784.000000, 1568.000000, 3136.000000, 6272.000000};
// float aNotes[9] = {27.500000, 55.000000, 110.000000, 220.000000, 440.000000, 880.000000, 1760.000000, 3520.000000, 7040.000000};
// float bNotes[9] = {30.868000, 61.736000, 123.472000, 246.944000, 493.888000, 987.776001, 1975.552002, 3951.104004, 7902.208008};

float noteMatrix[7][9] = {
                        {27.500000, 55.000000, 110.000000, 220.000000, 440.000000, 880.000000, 1760.000000, 3520.000000}, //A note
                        {30.868000, 61.736000, 123.472000, 246.944000, 493.888000, 987.776001, 1975.552002, 3951.104004}, //B note
                        {16.351999, 32.703999, 65.407997, 130.815994, 261.631989, 523.263977, 1046.527954, 2093.055908},  //C note
                        {18.354000, 36.708000, 73.416000, 146.832001, 293.664001, 587.328003, 1174.656006, 2349.312012},  //D note
                        {20.601999, 41.203999, 82.407997, 164.815994, 329.631989, 659.263977, 1318.527954, 2637.055908},  //E note
                        {21.827000, 43.653999, 87.307999, 174.615997, 349.231995, 698.463989, 1396.927979, 2793.855957},  //F note
                        {24.500000, 49.000000, 98.000000, 196.000000, 392.000000, 784.000000, 1568.000000, 3136.000000}}; //G note



//=============================================================================
// Part 2: Debounced keypad scanning.
//=============================================================================

typedef struct noteDuration {
    char note;
    float duration;
    struct noteDuration* next; 
} noteDuration;

int VOLUME = 64;
int OCTAVE = 4;

noteDuration* headOfNotes = NULL;
uint8_t buffer[600] = {0};  // Buffer to store the MIDI data




void freeList(noteDuration* head) {
    noteDuration* current = head;
    noteDuration* nextNode;
    
    while (current != NULL) {
        nextNode = current->next;
        free(current);
        current = nextNode;
    }
}

noteDuration* createNode(char note, float duration) {
    noteDuration* newNode = (noteDuration*)malloc(sizeof(noteDuration));
    if (newNode == NULL) {
        return NULL;
    }
    newNode->note = note;
    newNode->duration = duration;
    newNode->next = NULL;
    return newNode;
}

void pushBack(noteDuration* head, char note, float duration)
{
    noteDuration* iter = head;

    if(head == NULL)
    {
        return;
    }

    while(iter->next != NULL)
    {
        iter = iter->next;
    }

    iter->next = createNode(note, duration);

}



void write_variable_length(uint8_t *buffer, uint32_t value, int *index) {
    do {
        buffer[*index] = value & 0x7F;  // Keep the lower 7 bits
        if (*index > 0) {
            buffer[*index] |= 0x80;  // Set the MSB for continuation
        }
        value >>= 7;  // Shift right by 7 bits
        (*index)++;
    } while (value > 0);
}

void write_tempo_meta_event(uint8_t *buffer, uint32_t tempo, int *index) {
    // Tempo meta-event format: 0xFF 0x51 0x03 t1 t2 t3
    buffer[*index] = 0x00; (*index)++;  // Delta time
    buffer[*index] = 0xFF; (*index)++;  // Meta-event
    buffer[*index] = 0x51; (*index)++;  // Tempo event type
    buffer[*index] = 0x03; (*index)++;  // Length of the data (3 bytes)

    // Split the 24-bit tempo value into three bytes (big-endian)
    buffer[*index] = (tempo >> 16) & 0xFF; (*index)++;  // Byte 1
    buffer[*index] = (tempo >> 8) & 0xFF;  (*index)++;  // Byte 2
    buffer[*index] = tempo & 0xFF;         (*index)++;  // Byte 3
}

void write_note_on(uint8_t *buffer, uint8_t channel, uint8_t note, uint8_t velocity, int *index) {
    buffer[*index] = 0x00; (*index)++;  // Delta time
    buffer[*index] = 0x90 | (channel & 0x0F); (*index)++;  // Note-on event for the given channel
    buffer[*index] = note; (*index)++;  // Note number (e.g., 60 for middle C)
    buffer[*index] = velocity; (*index)++;  // Velocity (e.g., 64 for medium)
}

void write_note_off(uint8_t *buffer, uint32_t delta_time, uint8_t channel, uint8_t note, uint8_t velocity, int *index) {
    write_variable_length(buffer, delta_time, index);  // Delta time before note-off
    buffer[*index] = 0x80 | (channel & 0x0F); (*index)++;  // Note-off event for the given channel
    buffer[*index] = note; (*index)++;  // Note number
    buffer[*index] = velocity; (*index)++;  // Velocity (usually 0)
}

// void write_variable_length(FILE *file, uint32_t value) {
//     uint8_t buffer[4];
//     int i = 0;

//     do {
//         buffer[i] = value & 0x7F; // Keep the lower 7 bits
//         if (i > 0) {
//             buffer[i] |= 0x80; // Set the MSB for continuation
//         }
//         value >>= 7; // Shift right by 7 bits
//         i++;
//     } while (value > 0);

//     // Write the bytes in reverse order (most significant byte first)
//     for (int j = i - 1; j >= 0; j--) {
//         fputc(buffer[j], file);
//     }
// }

// void write_tempo_meta_event(FILE *file, uint32_t tempo) {
//     // Tempo meta-event format: 0xFF 0x51 0x03 t1 t2 t3
//     fputc(0x00, file);    // Delta time (0 for immediate effect)
//     fputc(0xFF, file);    // Meta-event
//     fputc(0x51, file);    // Tempo event type
//     fputc(0x03, file);    // Length of the data (3 bytes)

//     // Split the 24-bit tempo value into three bytes (big-endian)
//     fputc((tempo >> 16) & 0xFF, file);  // Byte 1
//     fputc((tempo >> 8) & 0xFF, file);   // Byte 2
//     fputc(tempo & 0xFF, file);          // Byte 3
// }

// void write_note_on(FILE *file, uint8_t channel, uint8_t note, uint8_t velocity) {
//     fputc(0x00, file);                  // Delta time (0 for immediate effect)
//     fputc(0x90 | (channel & 0x0F), file); // Note-on event for the given channel
//     fputc(note, file);                  // Note number (e.g., 60 for middle C)
//     fputc(velocity, file);              // Velocity (e.g., 64 for medium)
// }

// void write_note_off(FILE *file, uint32_t delta_time, uint8_t channel, uint8_t note, uint8_t velocity) {
//     write_variable_length(file, delta_time); // Delta time before note-off
//     fputc(0x80 | (channel & 0x0F), file);    // Note-off event for the given channel
//     fputc(note, file);                       // Note number
//     fputc(velocity, file);                   // Velocity (usually 0)
// }

long calcTime(float sec)
{
    float microsec = sec * 1000000;
    float microSecPerTick = 500000 / 480 ; //micro-s per tick

    long ticks = microsec / microSecPerTick;

    return ticks;

}

int note2Offset(char note)
{
    switch (note)
    {
        // C = 0
        // D = 2
        // E = 4
        // F = 5
        // G = 7
        // A = 9
        // B = 11
        case 'C':
            return 0;    
        case 'D':
            return 2; 
        case 'E':
            return 4; 
        case 'F':
            return 5; 
        case 'G':
            return 7; 
        case 'A':
            return 9; 
        case 'B':
            return 11;     
        default:
            return 0;
    }
}

int getNoteNum(char note)
{
    int number = (12 * OCTAVE) + note2Offset(note);
    return number;
}


void writeNotes(noteDuration* head, uint8_t *buffer) {
    noteDuration* iter = head;
    int index = 0;

    if (head == NULL) {
        return;
    }

    while (iter != NULL) {
        // Duration
        long duration = calcTime(iter->duration);
        int noteNum = getNoteNum(iter->note);

        // Write note-on event
        write_note_on(buffer, 0, noteNum, VOLUME, &index);

        // Write note-off event
        write_note_off(buffer, duration, 0, noteNum, 0, &index);

        iter = iter->next;
    }
}


void write2Array(noteDuration* notes) {
    int index = 0;

    // Write MIDI header chunk
    memcpy(&buffer[index], "MThd", 4); index += 4;
    buffer[index++] = 0x00;
    buffer[index++] = 0x00;
    buffer[index++] = 0x00;
    buffer[index++] = 0x06;  // Header length (6 bytes)
    buffer[index++] = 0x00;
    buffer[index++] = 0x01;  // Format type (1 for multi-track)
    buffer[index++] = 0x00;
    buffer[index++] = 0x01;  // Number of tracks (1)
    buffer[index++] = 0x01;
    buffer[index++] = 0xE0;  // Division (480 ticks per quarter note)

    // Write track chunk header
    memcpy(&buffer[index], "MTrk", 4); index += 4;
    buffer[index++] = 0x00;
    buffer[index++] = 0x00;
    buffer[index++] = 0x00;
    buffer[index++] = 0x00;  // Placeholder for track length

    // Write tempo meta-event (e.g., 500,000 μs/qn for 120 BPM)
    write_tempo_meta_event(buffer, 500000, &index);

    // Write all notes to array
    writeNotes(notes, buffer);

    // Write end-of-track meta-event
    buffer[index++] = 0x00;  // Delta time (immediately after last event)
    buffer[index++] = 0xFF;  // Meta-event
    buffer[index++] = 0x2F;  // End-of-track event type
    buffer[index++] = 0x00;  // Length (0 bytes)

    // Go back and write the actual track length (subtract 8 bytes for "MTrk" and length placeholder)
    long track_length = index - 22;  // Header (14 bytes) + "MTrk" (4 bytes) + length field (4 bytes)

    // Update the track length in the array
    buffer[18] = (track_length >> 24) & 0xFF;
    buffer[19] = (track_length >> 16) & 0xFF;
    buffer[20] = (track_length >> 8) & 0xFF;
    buffer[21] = track_length & 0xFF;

    // Now the buffer contains the MIDI data

    // You can now do something with the buffer, such as sending it over UART, or saving it to a different storage
}


// void writeNotes(noteDuration* head, FILE* file)
// {
//     noteDuration* iter = head;

//     if(head == NULL)
//     {
//         return;
//     }

//     while(iter != NULL)
//     {     
//         //Duration
//         long duration = calcTime(iter->duration);

//         int noteNum = getNoteNum(iter->note);

//         //TODO: implement volume 

//         // Write note-on event
//         write_note_on(file, 0, noteNum, VOLUME);

//         //Write note off event
//         write_note_off(file, duration, 0, noteNum, 0);

//         iter = iter->next;
//     }
// }

// void write2File(noteDuration* notes)
// {
//     FILE *file = fopen("output.mid", "wb");

//     if (!file) {
//         perror("Failed to open file");
//         return;
//     }

//     // Write MIDI header chunk
//     fwrite("MThd", 1, 4, file);
//     fputc(0x00, file); fputc(0x00, file); fputc(0x00, file); fputc(0x06, file); // Header length (6 bytes)
//     fputc(0x00, file); fputc(0x01, file); // Format type (1 for multi-track)
//     fputc(0x00, file); fputc(0x01, file); // Number of tracks (1)
//     fputc(0x01, file); fputc(0xE0, file); // Division (480 ticks per quarter note)

//     // Write track chunk header
//     fwrite("MTrk", 1, 4, file);
//     fputc(0x00, file); fputc(0x00, file); fputc(0x00, file); fputc(0x00, file); // Placeholder for track length

//     // Write tempo meta-event (e.g., 500,000 μs/qn for 120 BPM)
//     write_tempo_meta_event(file, 500000);

//     //Write all notes to file
//     writeNotes(notes, file);

//     // Write end-of-track meta-event
//     fputc(0x00, file);    // Delta time (immediately after last event)
//     fputc(0xFF, file);    // Meta-event
//     fputc(0x2F, file);    // End-of-track event type
//     fputc(0x00, file);    // Length (0 bytes)

//     // Go back and write the actual track length (subtract 8 bytes for "MTrk" and length placeholder)
//     long end_position = ftell(file);
//     long track_length = end_position - 22; // Header (14 bytes) + "MTrk" (4 bytes) + length field (4 bytes)
//     fseek(file, 18, SEEK_SET); // Move to the length field position
//     fputc((track_length >> 24) & 0xFF, file);
//     fputc((track_length >> 16) & 0xFF, file);
//     fputc((track_length >> 8) & 0xFF, file);
//     fputc(track_length & 0xFF, file);
//     fseek(file, end_position, SEEK_SET); // Move back to the end of the file

//     freeList(notes);
//     fclose(file);
//     return;
// }



uint8_t col; // the column being scanned
char hist[4][8] = {{'\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0'},{'\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0'},
                   {'\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0'},{'\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0'}};
// char queue[2];  // A two-entry queue of button press/release events.
const char keymap[] = "DCBA#9630852*741";
char pressed = '4';


char update_pressed(int column, int rows)
{
    for(int i = 0; i < 4; i++)
    {
        if(((rows>>i)&1))
        {
            return keymap[4*column + i];
        }
    }

    return '\0';
    
}

void update_history(int c, int rows)
{
    
    //Shift and add new value
    for(int i = 6; i >= 0; i--) 
    {
        hist[c][i + 1] = hist[c][i];
    }

    hist[c][0] = update_pressed(c, rows);
    
}

char getKey()
{
    for(;;)
    {
        for(int column = 0; column < 4; column++)
        { 
            if(hist[column][0] == hist[column][1] && hist[column][1] == hist[column][2] && hist[column][2] == hist[column][3] 
            && hist[column][3] == hist[column][4] && hist[column][4] == hist[column][5] && hist[column][5] == hist[column][6]
            && hist[column][6] == hist[column][7]) //History is all same to remove debouncing
            {
                if(hist[col][0] != '\0')
                {    if(headOfNotes == NULL)
                    {
                        headOfNotes = createNode(hist[col][0], 1.5f);
                    }
                    else
                    {
                        pushBack(headOfNotes, hist[col][0], 1.5f);
                    }
                }
               
                return hist[col][0];
            }
        }
    }
}

void drive_column(int c)
{
    GPIOC->BSRR = 0xf00000 | ~(1 << (c + 4 + 4));
}

int read_rows()
{
    return (((~GPIOC->IDR) & 0xf0) >> 4);
}

int key2Index(char key)
{
    switch(key)
    {
        case 'A': //A note
            return 0;
        case 'B': //B note
            return 1;
        case 'C': //C note
            return 2;
        case 'D': //D note
            return 3;
        case '*': //E note
            return 4;
        case '0': //F note
            return 5;
        case '#': //G note
            return 6;
        
        default:
            return 0;
    }
}

int setOctave(char key)
{
    switch(key)
    {
        case '1': 
            return 0;
        case '2':
            return 1;
        case '3':
            return 2;
        case '4': 
            return 3;
        case '5': 
            return 4;
        case '6': 
            return 5;
        case '7': 
            return 6;
        case '8': 
            return 7;
        
        default:
            return 4;
    }
}

float getNote(char key)
{
    return noteMatrix[key2Index(key)][OCTAVE];
}


//============================================================================
// GPIOC enable
//============================================================================
void enable_c(void) {
    //Enable RCC clock to GPIOC
    RCC->AHBENR |= RCC_AHBENR_GPIOCEN;

    //PC8-11 as outputs
    GPIOC->MODER &= ~0x0ff0000; //Clear
    GPIOC->MODER |= 0x0550000; //Output

    //PC8-11 output open-drain type
    GPIOC->OTYPER |= 0x0f000;

    //PC4-7 as inputs
    GPIOC->MODER &= ~0x0ff00; //Input

    //PC4-7 pull up
    GPIOC->PUPDR &= ~0x0ff00; //Clear
    GPIOC->PUPDR |= 0x05500; //Pull down
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

    enable_c();

    setup_adc();
    init_tim7();
    init_tim2();
    init_wavetable();
    setup_dac();
    init_tim6();

    // float f = 600; //getfloat();
    // for(;;)
    // {
        
    //     set_freq(0,f);
    //     f += .0001;
    // }
    set_freq(0,0);

    freeList(headOfNotes);

    for(;;) {
        char key = getKey();
        //Change note
        if(key == 'A' || key == 'B' || key == 'C' || key == 'D' || key == '*' || key == '0' || key == '#')
        {
            
            set_freq(0,getNote(key));
        }
        //Change octave
        else if((key == '1' || key == '2' || key == '3' || key == '4' || key == '5' || key == '6' || key == '7' || key == '8'))
        {
            OCTAVE = setOctave(key);
        }
        //Save to buffer
        else if(key == '9')
        {
            write2Array(headOfNotes);
            set_freq(0,0);
            //headOfNotes = NULL;
            return 0;
        }
    }

    freeList(headOfNotes);

    
    
}


