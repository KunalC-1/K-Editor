#ifndef GUI_H_INCLUDED
#define GUI_H_INCLUDED
#include<termios.h>
#include "piecetable.h"
typedef struct writeBuffer{
    int size;
    char* buf;
}writeBuffer;

typedef struct Editor{
    struct termios terminal_orig;
    int screenrows;
    int screencols;
    pieceTable PT;
    int numrows;
    int rowOffset;              // Stores offset by which rows are scolled(used for vertical scrolling)
    int colOffset;              // Stores offset by which cols are scolled(used for horizontal scrolling)
    int x, y;
    int* rowLen;                // store lenght of each row displayed on screen(start from 0)
    char* filename; 
    char message[100];
    time_t message_time;
    char* copyBuff;
    int copyBuffLen;
    int selectingText;
    int selectStartRow;
    int selectStartCol;
    int searchEnable;
    int foundLength;
}Editor;

extern Editor E;

#endif