#include<stdio.h>
#include<stdlib.h>
#include<ctype.h>
#include<unistd.h>
#include<sys/ioctl.h>
#include<errno.h>
#include<termio.h>
#include<string.h>
#include "gui.h"
/* Masking 3 bits from MSB
*    ex. ASCII Code a = 11000001 (97)
*    =====>  C1 & 1F = 01
*/
#define CTRL_KEY(x) (x & 0x1f)

typedef struct Editor{
    struct termios terminal_orig;
    int screenrows;
    int screencols;
    int x, y;
}Editor;
Editor E;

enum keyboardKeys{ARROW_LEFT = 500, ARROW_RIGHT, ARROW_UP, ARROW_DOWN, PAGE_UP, PAGE_DOWN, HOME_KEY, END_KEY, DELETE_KEY};
void returnError(char* s){
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    perror(s);
    exit(1);
}
// Disabling Raw Mode
void rawModeOFF(){
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.terminal_orig) == -1)
        returnError("Error in tcsetattr");
}
// Enabling Raw Mode
void rawModeON(){  

    // Getting terminal attributes 
    if(tcgetattr(STDIN_FILENO, &E.terminal_orig) == -1) returnError("Error in tcgetattr");
    // Calling rawModeOFF function on Exit
    atexit(rawModeOFF);
    struct termios raw_terminal = E.terminal_orig;
    // Setting Flags required for RAW Mode
    raw_terminal.c_iflag &= ~(BRKINT | ICRNL | IXON | INPCK | ISTRIP);
    raw_terminal.c_oflag &= ~(OPOST);
    raw_terminal.c_cflag |= (CS8); 
    raw_terminal.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw_terminal.c_cc[VMIN] = 0;
    raw_terminal.c_cc[VTIME] = 1;
    // Setting terminal attributes
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_terminal) == -1) returnError("Error in tcsetattr");
}
/*  Function Reads a character and Returns it   */
int readCharFromTerminal(){
    char c;
    if(read(STDIN_FILENO, &c, 1) == -1) returnError("Error in Read");
    if(c == '\x1b'){
        char escapeSeq[3];
        if(read(STDIN_FILENO, &escapeSeq[0], 1) != 1)   return c;
        if(read(STDIN_FILENO, &escapeSeq[1], 1) != 1)   return c;
           
        if(escapeSeq[0] == '['){
            if(escapeSeq[1] >= '0' && escapeSeq[1] <= '9'){
                if(read(STDIN_FILENO, &escapeSeq[2], 1) != 1) return c;
                if(escapeSeq[2] == '~'){
                    switch(escapeSeq[1]){
                        case '3' :
                            return DELETE_KEY;
                        case '5' :
                            return PAGE_UP;
                        case '6' :
                            return PAGE_DOWN;
                    }
                }
            }
            else{
                switch(escapeSeq[1]){
                    case 'A' : 
                        return ARROW_UP;
                    case 'B' :
                        return ARROW_DOWN;
                    case 'C' :
                        return ARROW_RIGHT;
                    case 'D' :
                        return ARROW_LEFT;
                    case 'H' :
                        return HOME_KEY;
                    case 'F' :
                        return END_KEY;
                }
            }
        }return c;
    }else
        return c;
}
void cursorMovement(int key){
    switch(key){
        case ARROW_LEFT:
            if(E.x != 0) E.x--;
            break;
        case ARROW_RIGHT:
            if(E.x != E.screencols - 1) E.x++;
            break;
        case ARROW_UP:
            if(E.y != 0) E.y--;
            break;
        case ARROW_DOWN:
        if(E.y != E.screenrows - 1) E.y++;
            break;
    }
}

/*  Function Processes Key Input   */
void processKeyInput(){
    int repeat;
    int c = readCharFromTerminal();
    printf("%c" , c);
    switch (c){
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            cursorMovement(c);
            break; 
        case PAGE_UP:
        case PAGE_DOWN:
            repeat = E.screenrows;
            c = (c == PAGE_UP) ? ARROW_UP : ARROW_DOWN;
            while(repeat--) cursorMovement(c);
            break;
        case HOME_KEY :
            E.x = 0;
            break;
        case END_KEY :
            E.x = E.screencols - 1;
            break;
    }
}
/*
 * Function prints out Text in PieceTable onto the screen
 */
void displayRows(writeBuffer* wb){
    for(int i = 0; i < E.screenrows; i++){
        // Printing Welcome message in middle of screen
        if(i == E.screenrows/3){
            char msg[50];
            int msglen = sprintf(msg , "Editor By Kunal\r\n");
            if(msglen > E.screencols) msglen = E.screencols;
            int space = (E.screencols - msglen)/2;
            if(space){
                appendToWriteBuffer(wb, "~\x1b[K", 4);
                space--;
            }
            while(space--) appendToWriteBuffer(wb, " ", 1);
            appendToWriteBuffer(wb, msg, msglen);
        }
        else if(i != E.screenrows - 1)
            appendToWriteBuffer(wb, "~\x1b[K\r\n", 6);
        else
            appendToWriteBuffer(wb, "~\x1b[K", 4);
    }
}

/*
 *   Function gives Window Size
 */
int getWindowSize(int *rows, int* cols){
    struct winsize ws;
    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0 || ws.ws_row == 0)
        return -1;
    else{
        *rows = ws.ws_row;
        *cols = ws.ws_col;
        return 0;
    }
}
/*   Function  Refreshes the Screen     */
void refreshScreen(){
    char position[20];
    writeBuffer wb;
    initWriteBuffer(&wb);

    // Escape Sequence for hiding cursor
    // appendToWriteBuffer(&wb, "\x1b[?25l", 6);
  
    // Escape Sequence for moving cursor to initial position
    appendToWriteBuffer(&wb, "\x1b[H", 3);    

    displayRows(&wb);

    sprintf(position, "\x1b[%d;%dH", E.y+1, E.x+1);
    appendToWriteBuffer(&wb, position, strlen(position));
    // appendToWriteBuffer(&wb, "\x1b[?25h", 6);

    write(STDOUT_FILENO, wb.buf, wb.size);
    destroyWriteBuffer(&wb);
}

/*
 *      Write Buffer Section
 */

// Function Initializes writeBuffer ADT
void initWriteBuffer(writeBuffer* wb){
    wb->buf = NULL;
    wb->size = 0;
}
// Function Apppend String s to writeBuffer
void appendToWriteBuffer(writeBuffer* wb, char* s, int size){
    char* temp = realloc(wb->buf, wb->size + size);
    if(!temp) return;
    memcpy(temp + wb->size, s, size);
    wb->buf = temp;
    wb->size += size;
}
// Destroys writeBuffer ADT
void destroyWriteBuffer(writeBuffer* wb){
    free(wb->buf);
    wb->size = 0;
}





/*
 *      Function Initializes struct Editor
 */
void initEditor(){
    E.x = E.y = 0;
    if(getWindowSize(&E.screenrows, &E.screencols) == -1)
        returnError("Error in getWindowSize");
}



int main(){
    rawModeON();
    initEditor();
    while(1){
        refreshScreen();
        processKeyInput();        
    }
    return 0;
}