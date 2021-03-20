#include<stdio.h>
#include<stdlib.h>
#include<ctype.h>
#include<unistd.h>
#include<sys/ioctl.h>
#include<errno.h>
#include<termio.h>
/* Masking 3 bits from MSB
*    ex. ASCII Code a = 11000001 (97)
*    =====>  C1 & 1F = 01
*/
#define CTRL_KEY(x) (x & 0x1f)

typedef struct Editor{
    struct termios terminal_orig;
    int screenrows;
    int screencols;
}Editor;
Editor E;

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
char readCharFromTerminal(){
    char c;
    if(read(STDIN_FILENO, &c, 1) == -1) returnError("Error in Read");
    return c;
}
/*  Function Processes Key Input   */
void processKeyInput(){
    char c = readCharFromTerminal();
    switch (c){
    case CTRL_KEY('q'):
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        exit(0);
        break;
    }
}
/*
 * Function prints out Text in PieceTable onto the screen
 */
void displayRows(){
    for(int i = 0; i < E.screenrows; i++){
        if(i != E.screenrows - 1)
            write(STDOUT_FILENO, "~\r\n", 3);
        else
            write(STDOUT_FILENO, "~", 1);
    }
}

/*
 *   Function gives Window Size
 */
int getWindowSize(int *rows, int* cols){
    struct winsize ws;
    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
        return -1;
    else{
        *rows = ws.ws_row;
        *cols = ws.ws_col;
        return 0;
    }
}
/*   Function  Refreshes the Screen     */
void refreshScreen(){
    // Escape Sequence for clearing whole screen
    write(STDOUT_FILENO, "\x1b[2J", 4);
    // Escape Sequence for moving cursor to initial position
    write(STDOUT_FILENO, "\x1b[H", 3);
    
    displayRows();
    
    write(STDOUT_FILENO, "\x1b[H", 3);
}

/*
 *      Function Initializes struct Editor
 */
void initEditor(){
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