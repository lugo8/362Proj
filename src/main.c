#include "stm32f0xx.h"
#include <stdint.h>
#include <math.h>   // for M_PI
#include <stdio.h>
#include <stdlib.h>
#include "ff.h"   // FatFS library
#include "diskio.h" // SD card driver
#include <string.h>
#include "commands.h"
//#include "commands.c"
#include "tty.h"
#include <stdio.h>
#include "fifo.h"
#include "tty.h"
#include "lcd.h"


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
    int duration;
    int octave;
    struct noteDuration* next; 
} noteDuration;

int VOLUME = 64;
int OCTAVE = 4;

noteDuration* headOfNotes = NULL;
uint8_t buffer[600] = {0};  // Buffer to store the MIDI data
int sizeOfBuffer = 0;




void freeList(noteDuration* head) {
    noteDuration* current = head;
    noteDuration* nextNode;
    
    while (current != NULL) {
        nextNode = current->next;
        free(current);
        current = nextNode;
    }
}

char char2Note(char input)
{
    //key == '*' || key == '0' || key == '#'
    // case '*': //E note
    //         return 4;
    //     case '0': //F note
    //         return 5;
    //     case '#': //G note
    //         return 6;
    if(input == '*')
    {
        return 'E';
    }
    else if(input == '0')
    {
        return 'F';
    }
    else if(input == '#')
    {
        return 'G';
    }
    else
    {
        return input;
    }
}

char note2char(char note)
{
    if(note == 'E')
    {
        return '*';
    }
    else if(note == 'F')
    {
        return '0';
    }
    else if(note == 'G')
    {
        return '#';
    }
    else
    {
        return note;
    }
}

noteDuration* createNode(char note, int duration) {
    noteDuration* newNode = (noteDuration*)malloc(sizeof(noteDuration));
    if (newNode == NULL) {
        return NULL;
    }
    newNode->note = char2Note(note);
    newNode->duration = duration;
    newNode->octave = OCTAVE;
    newNode->next = NULL;
    return newNode;
}

void pushBack(noteDuration* head, char note, int duration)
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

char getLastNote()
{
    if(headOfNotes == NULL)
    {
        return '\0';
    }

    noteDuration* iter = headOfNotes;

    while(iter->next != NULL)
    {
        iter = iter->next;
    }

    return note2char(iter->note);
}



void write_variable_length(uint8_t *buffer, uint32_t value, int *index) {

    // do {
    //     buffer[*index] = value & 0x7F;  // Keep the lower 7 bits
    //     if (*index > 0) {
    //         buffer[*index] |= 0x80;  // Set the MSB for continuation
    //     }
    //     value >>= 7;  // Shift right by 7 bits
    //     (*index)++;
    // } while (value > 0);

    uint8_t temp[4];
    int i = 0;

    do {
        temp[i] = value & 0x7F; // Keep the lower 7 bits
        if (i > 0) {
            temp[i] |= 0x80; // Set the MSB for continuation
        }
        value >>= 7; // Shift right by 7 bits
        i++;
    } while (value > 0);

    // Write the bytes in reverse order (most significant byte first)
    for (int j = i - 1; j >= 0; j--) {
        //fputc(temp[j], file);
        buffer[*index] = temp[j];
        (*index)++;
    }
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

long calcTime(int sec)
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

int getNoteNum(char note, int octave)
{
    int number = (12 * octave) + note2Offset(note);
    return number;
}


void writeNotes(noteDuration* head, uint8_t *buffer, int* index) {
    noteDuration* iter = head;

    if (head == NULL) {
        return;
    }

    while (iter != NULL) {
        // Duration
        long duration = calcTime(iter->duration);
        int noteNum = getNoteNum(iter->note, iter->octave);

        // Write note-on event
        write_note_on(buffer, 0, noteNum, VOLUME, index);

        // Write note-off event
        write_note_off(buffer, duration, 0, noteNum, 0, index);

        iter = iter->next;
    }
}


void write2Array(noteDuration* notes) {
    int index = 0;

    // Write MIDI header chunk
    //memcpy(&buffer[index], "MThd", 4); index += 4;
    buffer[index++] = 'M';
    buffer[index++] = 'T';
    buffer[index++] = 'h';
    buffer[index++] = 'd';
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
    //memcpy(&buffer[index], "MTrk", 4); index += 4;
    buffer[index++] = 'M';
    buffer[index++] = 'T';
    buffer[index++] = 'r';
    buffer[index++] = 'k';
    buffer[index++] = 0x00;
    buffer[index++] = 0x00;
    buffer[index++] = 0x00;
    buffer[index++] = 0x00;  // Placeholder for track length

    // Write tempo meta-event (e.g., 500,000 μs/qn for 120 BPM)
    write_tempo_meta_event(buffer, 500000, &index);

    // Write all notes to array
    writeNotes(notes, buffer, &index);

    // Write end-of-track meta-event
    buffer[index++] = 0x00;  // Delta time (immediately after last event)
    buffer[index++] = 0xFF;  // Meta-event
    buffer[index++] = 0x2F;  // End-of-track event type
    buffer[index++] = 0x00;  // Length (0 bytes)

    // Go back and write the actual track length (subtract 8 bytes for "MTrk" and length placeholder)
    int track_length = index - 22;  // Header (14 bytes) + "MTrk" (4 bytes) + length field (4 bytes)

    // Update the track length in the array
    buffer[18] = (track_length >> 24) & 0xFF;
    buffer[19] = (track_length >> 16) & 0xFF;
    buffer[20] = (track_length >> 8) & 0xFF;
    buffer[21] = track_length & 0xFF;

    sizeOfBuffer = index;
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





//THE OTHER PART:
//==========================================================================================================================================================
//==========================================================================================================================================================
//==========================================================================================================================================================

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

void clearHist()
{
    for(int i = 0; i < 4; i++)
    {
        for(int j = 0; j < 8; j++)
        {
            hist[i][j] = '\0';
        }
    }

    return;
}


char getKey()
{
    // for(;;)
    // {
    for(int column = 0; column < 4; column++)
    { 
        if(hist[column][0] == hist[column][1] && hist[column][1] == hist[column][2] && hist[column][2] == hist[column][3] 
        && hist[column][3] == hist[column][4] && hist[column][4] == hist[column][5] && hist[column][5] == hist[column][6]
        && hist[column][6] == hist[column][7]) //History is all same to remove debouncing
        {
            // char key = hist[col][0];
            // if(key != '\0' && (key == 'A' || key == 'B' || key == 'C' || key == 'D' || key == '*' || key == '0' || key == '#'))
            // {   if(headOfNotes == NULL)
            //     {
            //         headOfNotes = createNode(hist[col][0], 1.5f);
            //     }
            //     else
            //     {
            //         pushBack(headOfNotes, hist[col][0], 1.5f);
            //     }

            //     //char answer = hist[col][0];
            //     clearHist();


            //     return key;
                
            // }

            return hist[col][0];
            
            
        }
    }

    return '\0';
//}
}

void drive_column(int c)
{
    GPIOA->BSRR = 0xf00000 | ~(1 << (c + 4 + 5));
}

int read_rows()
{
    return (((~GPIOA->IDR) & 0x1f0) >> 5);
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

//JOSH'S PART:
//==========================================================================================================================================================
//==========================================================================================================================================================
//==========================================================================================================================================================


#define FIFOSIZE 16
char serfifo[FIFOSIZE];
int seroffset = 0;
const unsigned int asc3_3232[7][32]={
    {0x0,0x0,0x0,0x01F98000,0x03FBC000,0x073FE000,0x061FE000,0x00076000,0x000FF000,0x007DF000,0x00FBF800,0x01DFF800,0x019FB800,0X00077800,0X000E7800,0x001C7800,0X00187800,0X00387800,0X00387800,0X00701E00,0X00701E00,0X00FFFE00,0X00FFFE00,0X01C01E00,0X01801E00,0X01800780,0X01800F80,0X0FC01F98,0X1FF01FB8,0X1FF807F0,0X38FC07E0,0X0}, /*Note A*/
    {0x0,0x0,0x3C00FC00,0X7E01FE00,0XE73B1F00,0XC3BF0F80,0X01FC07C0,0x00DC03C0,0X01BCC3C0,0X033CC3C0,0X033CC3C0,0X073CC3C0,0X073CC380,0X0F3CC300,0X033C0E00,0X033C0E00,0X0F3CC300,0X073CC380,0X073CC3C0,0X033CC3C0,0X0338C3C0,0X07B0C3C0,0X0FF0C3C0,0X0C70C3C0,0X003003C0,0X001803C0,0X03FC0F80,0X07FFFF00,0X0E1FFC00,0XC0FF800,0X0,0X0}, /*Note B*/
    {0x0,0x0,0x1ff800,0x3fff00,0x7fff80,0xfc03c0,0x1f000e0,0x1b00020,0x3200130,0x36000f0,0x6600060,0x6600000,0x6600000,0x6600000,0x6600000,0x6600000,0x6600000,0x6e00000,0x6e00000,0xf800040,0x1f800020,0x3f000060,0x3800040,0x1f001c0,0xf00180,0x7c1f00,0x3ffe00,0xff000,0x0,0x0,0x0,0x0}, /*Note C*/
    {0x0,0x0,0x41ff800,0x33ffc00,0x3fffe00,0x1ffff00,0x1f80,0xf80,0x900780,0x3980780,0x7980780,0xe980780,0x1e99e780,0x1e93f780,0x4900780,0x5800780,0x1d800780,0x1d900780,0x593f780,0x799e780,0x7180780,0x6180780,0xc180780,0x10380780,0x700f00,0xf01e00,0x7fc3c00,0xffff800,0xc1ff000,0x101fe000,0x0,0x0}, /*Note D*/
    {0x0,0xe0000,0xffc000,0x3ffffc00,0x7ffffe00,0xdef01f00,0x1dc00180,0x19800080,0x19000000,0x19000000,0x19000000,0x19100000,0x193c0000,0x193ff800,0x197fff80,0x197f0000,0x19c00000,0x1b800000,0x1b000000,0x1e000000,0x1c000000,0x30000000,0x27fffe00,0xfffff00,0x7fc00380,0x78000080,0x60000000,0x4c000000,0x66000000,0x3f000000,0xe000000,0x0}, /*Note E*/
    {0x0,0x0,0x10000000,0xf000000,0x3ff0008,0x3ff8018,0x17ff078,0x37ffff0,0x75fff80,0xcc1f800,0xdc00000,0xd800000,0xd800000,0xd8c0000,0xd8f0020,0xd8fc0e0,0xdcfffc0,0x6cffe00,0x6ce0000,0x2e80000,0x3e00000,0x3c00000,0x3c00000,0x23800000,0x47800000,0x6f000000,0x3e000000,0x1c000000,0x0,0x0,0x0,0x0},/*Note F*/
    {0x0,0x1ff000,0x7ffe00,0xffffc0,0x1ffffe0,0x3be00f0,0x7700030,0xe600018,0x3ee00018,0xee00070,0xee00080,0xee00000,0xee00000,0xee03f00,0xee1ffe0,0xee1fc78,0xee3c1bc,0xec601cc,0xec000f8,0xec000f0,0x7c001e0,0x7c001c0,0xf8007c0,0x1e200f80,0x3c783f00,0x207ffe00,0x1ff000,0x7c000,0x0,0x0,0x0,0x0} /*Note G*/


};

void enable_tty_interrupt(void) {
    NVIC->ISER[0] |=  1 << 29;    //set NVIC ISER bit related to USART5
    USART5->CR1 |=  USART_CR1_RXNEIE;  //raise interrupt data register becomes not empty
    USART5->CR3 |= USART_CR3_DMAR;  //Trigger DMA when recieve data register becomes not empty

    RCC->AHBENR |= RCC_AHBENR_DMA2EN;
    DMA2->CSELR |= DMA2_CSELR_CH2_USART5_RX;
    DMA2_Channel2->CCR &= ~DMA_CCR_EN;

    DMA2_Channel2->CMAR = &serfifo; //Assign Memory to Serfifo
    DMA2_Channel2->CPAR = &(USART5->RDR); //Assign Peripheral to USART
    DMA2_Channel2->CNDTR = FIFOSIZE; //amount of data is FIFOSIZE
    DMA2_Channel2->CCR &= ~DMA_CCR_DIR; //read from peripheral
    DMA2_Channel2->CCR &= ~(DMA_CCR_MSIZE & DMA_CCR_PSIZE); //set M/P size to 8 bit
    DMA2_Channel2->CCR |= DMA_CCR_MINC; //increment Memory
    DMA2_Channel2->CCR &= ~DMA_CCR_PINC; //do not increment usart peripheral
    DMA2_Channel2->CCR |= DMA_CCR_CIRC; //enable circular mode
    DMA2_Channel2->CCR &= ~DMA_CCR_MEM2MEM; //disable mem2mem mode
    DMA2_Channel2->CCR |= DMA_CCR_PL; //set both bit of priority to highest

    DMA2_Channel2->CCR |= DMA_CCR_EN; //En channel


}

int __io_putchar(int c) {
    // TODO copy from STEP2
    if (c == 10)
    {
        while(!(USART5->ISR & USART_ISR_TXE));
        USART5->TDR = 13;
    }

    while(!(USART5->ISR & USART_ISR_TXE));
    USART5->TDR = c;
    return c;
}

char interrupt_getchar() {
    // TODO
    if (!fifo_newline(&input_fifo))
    {
        asm volatile ("wfi");
    }
    char ch = fifo_remove(&input_fifo);
    return ch;
    
}

int __io_getchar(void) {
    // TODO Use interrupt_getchar() instead of line_buffer_getchar()
   return interrupt_getchar();
}

void USART3_8_IRQHandler(void) { //Hopefully is the correct name
    while(DMA2_Channel2->CNDTR != sizeof serfifo - seroffset) {
        if (!fifo_full(&input_fifo))
            insert_echo_char(serfifo[seroffset]);
        seroffset = (seroffset + 1) % sizeof serfifo;
    }
}

void init_spi1_slow() {
    RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    GPIOB->MODER &= ~0xFC0;
    GPIOB->MODER |= 0xA80;
    GPIOB->AFR[0] &= ~0x00FFF000;

    SPI1->CR1 &= ~SPI_CR1_SPE;

    SPI1->CR1 |= SPI_CR1_BR;
    SPI1->CR1 |= SPI_CR1_MSTR;
    SPI1->CR1 |= SPI_CR1_SSM;
    SPI1->CR1 |= SPI_CR1_SSI;
    SPI1->CR2 |= SPI_CR2_DS;
    SPI1->CR2 |= SPI_CR2_FRXTH;
    SPI1->CR2 &= ~SPI_CR2_DS_3; //turn 8 bit

    SPI1->CR1 |= SPI_CR1_SPE;
}


void init_sdcard_io() {
    RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
    init_spi1_slow();
    GPIOB->MODER &= ~ 0x30;
    GPIOB->MODER |= 0x10;
    disable_sdcard();
}

void sdcard_io_high_speed() {
    SPI1->CR1 &= ~SPI_CR1_SPE;

    SPI1->CR1 &= ~SPI_CR1_BR;
    SPI1->CR1 |= SPI_CR1_BR_0;

    SPI1->CR1 |= SPI_CR1_SPE;
}



void init_lcd_spi() {
    RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
    GPIOB->MODER &= ~0x30C30000;
    GPIOB->MODER |= 0x10410000;
    init_spi1_slow();
    sdcard_io_high_speed();
}

void lcd_start_setup() {
    LCD_Setup();
    LCD_Clear(0xFFFF);
    LCD_DrawString(0,96,0x0000,0xFFFF, "The octave is now: 5", 16, 0);
    

}

void _LCD_DrawSpecialChar(u16 x, u16 y, u16 fc, u16 bc, char num, u16 res) {
    int temp, tempres, tempres1;
    u8 pos, t;

    num = (char)(key2Index(num));
    int size = 32 * res;
    if((num < 0) || (num > 6))
    {
        LCD_DrawFillRectangle(x, y, x + size, y + size, bc);
        return;
    }

    LCD_SetWindow(x, y, x + size - 1, y + size - 1);
    LCD_WriteData16_Prepare();
    for(pos = 0; pos < 32; pos++)
    {
        for(tempres = 0; tempres < res; tempres++)
        {
            temp = asc3_3232[(int)num][pos];
            for(t=0;t<32;t++)
            {
                for(tempres1 = 0; tempres1 < res; tempres1++)
                {
                    if(temp&0x80000000)
                        LCD_WriteData16(fc);
                    else
                        LCD_WriteData16(bc);

                }
                temp<<=1;
            }


        }
        
    }
    LCD_WriteData16_End();
}

void LCD_DrawSpecialChar(u16 x, u16 y, u16 fc, u16 bc, char num, u16 res){
    lcddev.select(1);
    _LCD_DrawSpecialChar(x,y,fc,bc,num, res);
    lcddev.select(0);
}





//========================================================================
// SD card init setup stuff

void enable_sdcard(void) {
    GPIOC->ODR &= ~(1 << 11);  // Set PC11 low to enable SD card (CS low)
}

void disable_sdcard(void) {
    GPIOC->ODR |= (1 << 11);   // Set PC11 high to disable SD card (CS high)
}


void init_sdcard_spi() {

    RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
    RCC->AHBENR |= RCC_AHBENR_GPIOCEN;

    GPIOB->MODER &= ~(0x00300000); // clear pb10
    GPIOB->MODER |= 0x00200000; // set pb10 to alternate function

    GPIOC->MODER &= ~(0x000000F0);
    GPIOC->MODER |= 0x000000A0;

    GPIOB->AFR[1] |= (5 << GPIO_AFRH_AFRH2_Pos); // set af5 pb10
    GPIOC->AFR[0] |= (5 << GPIO_AFRL_AFRL2_Pos) | (5 << GPIO_AFRL_AFRL3_Pos); // af5 for pc2, 3

    disable_sdcard();

    RCC->APB1ENR |= RCC_APB1ENR_SPI2EN; // set up spi2
    SPI2->CR1 &= ~SPI_CR1_SPE;

    SPI2->CR1 |= SPI_CR1_BR; //lowest baud rate (highest divisor)
    SPI2->CR1 |= SPI_CR1_MSTR;

    SPI2->CR2 &= ~(SPI_CR2_DS_0);
    SPI2->CR2 |= SPI_CR2_DS_1 | SPI_CR2_DS_2 | SPI_CR2_DS_3; // word size 8

    SPI2->CR1 |= SPI_CR1_SSM; // software slave mgmt
    SPI2->CR1 |= SPI_CR1_SSI; // internal slave select

    SPI2->CR2 |= SPI_CR2_FRXTH; // fifo reception release

    SPI2->CR1 |= SPI_CR1_SPE; // enable



}

void sdcard_high_speed () {

    SPI2->CR1 &= ~(SPI_CR1_SPE); // disable

    SPI2->CR2 &= ~(SPI_CR1_BR);
    //SPI2->CR2 &= ~(SPI_CR1_BR_1);
    //SPI2->CR2 |= SPI_CR1_BR_2; // 001 for /4. 48/4 = 12 MHz

    SPI2->CR1 |= SPI_CR1_SPE; // reenable

}



// writing to SD card

void save_midi_to_sd(uint8_t *midi_data, uint32_t midi_data_size) {
    // mount the SD card
    //   // file system object
    FRESULT res;
    mount(1, NULL);
    // FRESULT res = f_mount(&fs, "", 1);  // mount
    // if (res != FR_OK) {
    //     // if mounting fails
    //     printf("Failed to mount SD card: %d\n", res);
    //     return;
    // }

    // create or open the MIDI file
    FIL midi_file;  // file object
    res = f_open(&midi_file, "wee.mid", FA_WRITE | FA_CREATE_ALWAYS);
    if (res != FR_OK) {
        // error handling
        //printf("Failed to open file: %d\n", res);
        //f_mount(NULL, "", 1);  // unmount
        return;
    }

    // write the MIDI data to the file
    UINT bytes_written;
    res = f_write(&midi_file, midi_data, midi_data_size, &bytes_written);
    if (res != FR_OK || bytes_written != midi_data_size) {
        // error handling if writing fails
        //printf("Failed to write data to file: %d\n", res);
    } else {
        //printf("MIDI file written successfully, bytes: %d\n", bytes_written);
    }

    // close the file and unmount the SD card
    f_close(&midi_file);
    //f_mount(NULL, "", 1);  // Unmount SD card
}

//=======================================================================


//============================================================================
// GPIOA enable
//============================================================================
void enable_a(void) {
    //Enable RCC clock to GPIOA
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;

    //PA9-12 as outputs
    GPIOA->MODER &= ~0x3fc0000; //Clear
    GPIOA->MODER |= 0x1540000; //Output

    //PA9-12 output open-drain type
    GPIOA->OTYPER |= 0x1e000;

    //PA5-8 as inputs
    GPIOA->MODER &= ~0x3fc00; //Input

    //PA5-8 pull up
    GPIOA->PUPDR &= ~0x3fc00; //Clear
    GPIOA->PUPDR |= 0x15400; //Pull down
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

    enable_a();

    setup_adc();
    init_tim7();
    init_tim2();
    init_wavetable();
    setup_dac();
    init_tim6();
    init_sdcard_spi();
    sdcard_high_speed();


    //init_usart5();
    // enable_tty_interrupt();
    // setbuf(stdin,0);
    // setbuf(stdout,0);
    // setbuf(stderr,0);
    //command_shell();
    lcd_start_setup();

    set_freq(0,0);

    freeList(headOfNotes);

    for(;;) {
        char key = getKey();
        //Change note
        if(key == 'A' || key == 'B' || key == 'C' || key == 'D' || key == '*' || key == '0' || key == '#')
        {
            //Only add if new key
            char lastNote = getLastNote();
            if((getLastNote() != key)) //|| (getLastNote() == 'E' && key == '*') || (getLastNote() == 'F' && key == '0') || (getLastNote() == 'G' && key == '#')))
            {
                set_freq(0,getNote(key));
                LCD_DrawSpecialChar(0,0, 0x0000, 0xffff, key, 3);
                LCD_DrawFillRectangle(0, 112, 56, 128, 0xFFFF);

                if(headOfNotes == NULL)
                {
                    headOfNotes = createNode(key, 1);
                }
                else
                {
                    pushBack(headOfNotes, key, 1);
                }

                
            }
            //nano_wait(50000000);
            
            
        }
        //Change octave
        else if((key == '1' || key == '2' || key == '3' || key == '4' || key == '5' || key == '6' || key == '7' || key == '8'))
        {
            OCTAVE = setOctave(key);
            LCD_DrawChar(152, 96, 0x0000, 0xFFFF, key, 16, 0);
            LCD_DrawFillRectangle(0, 112, 56, 128, 0xFFFF);
            
            
        }
        //Save to buffer
        else if(key == '9')
        {
            write2Array(headOfNotes);
            save_midi_to_sd(buffer, sizeOfBuffer);
            set_freq(0,0);
            //headOfNotes = NULL;
            LCD_DrawString(0, 112, 0x0000, 0xFFFF, "Saved!", 16, 0);
            //return 0;
        }

    
    }

    freeList(headOfNotes);

    
    
}


