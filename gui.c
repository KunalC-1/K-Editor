#include<stdio.h>
#include<stdlib.h>
#include<ctype.h>
#include<unistd.h>
#include<sys/ioctl.h>
#include<errno.h>
#include<termio.h>
#include<string.h>
#include "piecetable.h"
#include "gui.h"
/* Masking 3 bits from MSB
*    ex. ASCII Code a = 11000001 (97)
*    =====>  C1 & 1F = 01
*/
#define CTRL_KEY(x) (x & 0x1f)
char* added = NULL;
int addedIndex = -1;

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


// Function paste content of piece table into writebuffer
void printLinesToWriteBuffer(writeBuffer* wb,pieceTable PT, int firstLine, int lastLine){
    // Initialize Current Line Number as 1
    int currLine = 1;
    pieceNode*  currNode = PT.head->next;

    // Traverse Through the PT until Required Node found
    while(currNode != PT.tail && currLine + currNode->lineCount < firstLine){
        currLine += currNode->lineCount;
        currNode = currNode->next;
    }
    // rowLen will store lenght of each row from first to last line
    int* rowLen = (int* )calloc((lastLine - firstLine) + 1 ,sizeof(int));
    int rowIndex = 0;
    // position index in rowLen array
    int irow = 0;
    while(currNode != PT.tail && currLine <= lastLine){
        int offset = 0;
        int count = firstLine - currLine > 0 ? firstLine - currLine : 0;
        // calculate offset from where line starts
        for(int i = 0; i < count; i++)
            offset += currNode->lineBreak[i];
        for(int i = currNode->start + offset; i <= currNode->end; i++){
            // if all rows apppended into write buffer
            if(irow == lastLine - firstLine + 1) break;
            char* buffer = currNode->buffer;
            rowIndex++;
            if(buffer[i] == '\n'){
                appendToWriteBuffer(wb, "\r\n", 2);
                // printf("%d", rowIndex);
                rowLen[irow] = rowIndex;
                irow++;
                rowIndex = 0;
            }
            else{
                if(rowIndex > E.colOffset && rowIndex <= E.colOffset + E.screencols){
                    if(isdigit(buffer[i])){
                        // to set red color
                        appendToWriteBuffer(wb, "\x1b[31m", 5);
                        appendToWriteBuffer(wb, &buffer[i], 1);
                        // to reser color
                        appendToWriteBuffer(wb, "\x1b[39m", 5);
                    }else
                        appendToWriteBuffer(wb, &buffer[i], 1);
                }
            }
        }
        currLine += currNode->lineCount;
        currNode = currNode->next;
    }
    rowLen[irow] = rowIndex;
    free(E.rowLen);
    E.rowLen = rowLen;
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
            if (E.x != 0) {
                E.x--;
            } else if (E.y > 0) {
                E.y--;
                E.x = E.rowLen[E.y - E.rowOffset] > 1 ? E.rowLen[E.y - E.rowOffset] - 1 : 0;
            }
            break;
        case ARROW_RIGHT:
            if(E.rowLen[E.y - E.rowOffset] < 1)
                break;
            if (E.y < E.numrows && E.rowLen[E.y - E.rowOffset] - 1 > E.x){
                E.x++;
            }else if (E.y + 1 < E.numrows){
                E.y++;
                E.x = 0;
            }
            break;
        case ARROW_UP:
            if(E.y != 0){
                E.y--;
            }
            if(E.rowLen[E.y - E.rowOffset] < 1)
                E.x = 0;
            else if(E.x > E.rowLen[E.y - E.rowOffset] - 1) 
                E.x = E.rowLen[E.y - E.rowOffset] - 1;
            break;
        case ARROW_DOWN:
            if(E.y + 1 < E.numrows){
                E.y++;
            }
            if(E.rowLen[E.y - E.rowOffset] < 1)
                E.x = 0;
            else if(E.x > E.rowLen[E.y - E.rowOffset] - 1) 
                E.x = E.rowLen[E.y - E.rowOffset] - 1;
            
            break;
    }
}
//   Adjust rowOffset according to scrolling done 
void scrollScreen(){
    if(E.x < E.colOffset)
        E.colOffset = E.x;
    if(E.x >= E.colOffset + E.screencols)
        E.colOffset = E.x - E.screencols + 1;
    if(E.y < E.rowOffset)
        E.rowOffset = E.y;
    if(E.y >= E.rowOffset + E.screenrows)
        E.rowOffset = E.y - E.screenrows + 1;

}
int bufferIndex = 0;
/*  Function Processes Key Input   */
void processKeyInput(){
    int repeat;
    int c = readCharFromTerminal();
    // printf("%c" , c);
    switch (c){
        case '\r':

            break;
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
            E.y = (c == PAGE_UP) ? E.rowOffset : E.rowOffset + E.screenrows - 1;
            if(c == PAGE_DOWN && E.y > E.numrows) E.y = E.numrows;
            while(repeat--) cursorMovement(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
            break;
        case HOME_KEY :
            E.x = 0;
            break;
        case END_KEY :
            if(E.y < E.numrows)
                E.x = E.rowLen[E.y] - 1;
            break;
        case CTRL_KEY('l'):
        case '\x1b':    
            break;

        default:
            added[++addedIndex] = c;
            insertCharAt(E.PT, E.y+1,E.x);
    }
}
/*
 * Function prints out Text in PieceTable onto the screen
 */
void displayRows(writeBuffer* wb){
    if(E.numrows == 0){
        for(int i = 0; i < E.screenrows; i++){
            int actualRowPosition = i + E.rowOffset;
            if(actualRowPosition >= E.numrows ){
                // Printing Welcome message in middle of screen
                if(E.numrows == 0 && i == E.screenrows /3){
                    char msg[50];
                    int msglen = sprintf(msg , "Editor By Kunal\r\n");
                    if(msglen > E.screencols) msglen = E.screencols;
                    int space = (E.screencols - msglen)/2;
                    if(space){
                        appendToWriteBuffer(wb, "~", 1);
                        space--;
                    }
                    while(space--) appendToWriteBuffer(wb, " ", 1);
                    appendToWriteBuffer(wb, msg, msglen);
                }
                else
                    appendToWriteBuffer(wb, "~", 1);
            }
            // appendToWriteBuffer(wb, "\x1b[K", 3);
            if(i < E.screenrows - 1){
                appendToWriteBuffer(wb, "\r\n", 2);
            }
        }
    }
    else{

        int lastline = E.rowOffset + E.screenrows > E.numrows ? E.numrows : E.rowOffset + E.screenrows;
        printLinesToWriteBuffer(wb, E.PT, E.rowOffset + 1, lastline );
        appendToWriteBuffer(wb, "\r\n", 2);
        for(int i = lastline ; i < E.screenrows - 2; i++){
            appendToWriteBuffer(wb, "~\r\n", 3);
        }
        appendToWriteBuffer(wb, "~", 1);
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
    scrollScreen();
    char position[20];
    writeBuffer wb;
    initWriteBuffer(&wb);

    // Escape Sequence for hiding cursor
    appendToWriteBuffer(&wb, "\x1b[?25l", 6);
    appendToWriteBuffer(&wb, "\x1b[2J", 4);
    // Escape Sequence for moving cursor to initial position
    appendToWriteBuffer(&wb, "\x1b[H", 3);    

    displayRows(&wb);

    sprintf(position, "\x1b[%d;%dH", E.y - E.rowOffset + 1, E.x - E.colOffset + 1);
    appendToWriteBuffer(&wb, position, strlen(position));
    appendToWriteBuffer(&wb, "\x1b[?25h", 6);

    write(STDOUT_FILENO, wb.buf, wb.size);
    destroyWriteBuffer(&wb);
    // printPieceTable(E.PT);
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

// Function opens provided filename as argument and stores content in piece table
void openEditor(char *filename){
    FILE *fp = fopen(filename, "rb");
    fseek(fp, 0, SEEK_END);
    long fileSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if(fileSize != 0){
        original =(char*) malloc(fileSize + 1);
        fread(original, 1, fileSize, fp);
    }else
        original = NULL;

    if(original){
        pieceNode* newNode = newPieceNode(original, 0, fileSize - 1, 0);
        newNode->next = (E.PT.head)->next;
        newNode->prev = E.PT.head;

        (E.PT.tail)->prev = newNode;
        (E.PT.head)->next = newNode;
        E.numrows = newNode->lineCount + 1;                               ///  //// //// See Later
    }
    fclose(fp);
    // printPieceTable(E.PT);
}



/*
 *      Function Initializes struct Editor
 */
void initEditor(){
    E.x = E.y = 0;
    E.screencols = E.screenrows = 0;
    E.numrows = 0;
    E.rowOffset = E.colOffset =  0;
    E.rowLen = (int*)calloc(E.screenrows + 1 , sizeof(int));
    initPieceTable(&E.PT);
    if(getWindowSize(&E.screenrows, &E.screencols) == -1)
        returnError("Error in getWindowSize");
}



int main(int argc, char* argv[]){
    added = (char* )malloc(sizeof(char) * 100);
   
    rawModeON();
    initEditor();
    openEditor(argv[1]);
    // for(int i = 0; i < 3; i++)
    //     printf("%d",E.rowLen[i]);
    while(1){
        refreshScreen();
        
        processKeyInput();        
    }
    return 0;
}



