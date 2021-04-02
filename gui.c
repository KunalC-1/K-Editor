#include<stdio.h>
#include<stdlib.h>
#include<ctype.h>
#include<unistd.h>
#include<sys/ioctl.h>
#include<errno.h>
#include<termio.h>
#include<time.h>
#include <stdarg.h>
#include<string.h>
#include "piecetable.h"
#include "gui.h"
/* Masking 3 bits from MSB
*    ex. ASCII Code a = 11000001 (97)
*    =====>  C1 & 1F = 01
*/

#define CTRL_KEY(x) (x & 0x1f)
char* original = NULL;
char* added = NULL;
int addedIndex = -1;
int addedSize;
Editor E;

// typedef struct Editor{
//     struct termios terminal_orig;
//     int screenrows;
//     int screencols;
//     pieceTable PT;
//     int numrows;
//     int rowOffset;              // Stores offset by which rows are scolled(used for vertical scrolling)
//     int colOffset;              // Stores offset by which cols are scolled(used for horizontal scrolling)
//     int x, y;
//     int* rowLen;                // store lenght of each row displayed on screen(start from 0)
//     char* filename; 
//     char message[100];
//     time_t message_time;
//     char* copyBuff;
//     int copyBuffLen;
//     int selectingText;
//     int selectStartRow;
//     int selectStartCol;
// }Editor;


enum keyboardKeys
{
    ARROW_LEFT = 500,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    PAGE_UP,
    PAGE_DOWN,
    HOME_KEY,
    END_KEY,
    DELETE_KEY,
    SHIFT_LEFT,
    SHIFT_RIGHT,
    SHIFT_UP,
    SHIFT_DOWN,
    BACKSPACE = 127,
    
};
/* Debug Section */
void debugRowLen(){
    FILE* fp = fopen("log.txt", "w");
    for(int i = 0 ; i < E.numrows; i++){
        fprintf(fp, "%d ", E.rowLen[i]);        
    }
    fclose(fp);
}
void debugPieceTable(){
    FILE* fp = fopen("log.txt", "w");
    printPieceTable(E.PT, fp);
    fclose(fp);
}

void debugCopy(){
    FILE* fp = fopen("log.txt", "w");
    fprintf(fp,"%s" ,E.copyBuff);
    fclose(fp);
}

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
// Function returns selected text lenght
int selectedSize(int startRow, int startCol, int endRow, int endCol){
    int copiedSize = 0;
    while(1){
        if(startRow == endRow){
            copiedSize += endCol - startCol;
            break;
        }else{
            copiedSize += E.rowLen[startRow - E.rowOffset] - startCol;
            startRow++;
            startCol = 0;
        }
    }
    return copiedSize;
}
// Function deletes selected text by user
void deleteSelectedText(){
    int startRow,startCol;
    if(E.selectStartRow > E.y || (E.y == E.selectStartRow && E.x < E.selectStartCol)){
        startRow = E.y;
        startCol = E.x;
        E.y = E.selectStartRow;
        E.x = E.selectStartCol;
    }else{
        startRow = E.selectStartRow;
        startCol = E.selectStartCol;
    }
    while(!(E.x == startCol && E.y == startRow)){
        deleteCharAt(E.PT, E.y + 1, E.x);
        if(E.x > 0) E.x--;
    }
}
// Function copies text selected into copyBuff
void editorCopySelectedText(){
    
    free(E.copyBuff);
    int startRow,startCol, endRow, endCol, copiedSize = 0;
    if(E.selectStartRow > E.y || (E.y == E.selectStartRow && E.x < E.selectStartCol)){
        startRow = E.y;
        startCol = E.x;
        endRow = E.selectStartRow;
        endCol = E.selectStartCol;
    }else{
        startRow = E.selectStartRow;
        startCol = E.selectStartCol;
        endRow = E.y;
        endCol = E.x;
    }
    
    E.copyBuffLen = selectedSize(startRow, startCol, endRow, endCol);
    if(E.copyBuffLen == 0) return;
    E.copyBuff = copyLineFrom(E.PT, startRow + 1, startCol, E.copyBuffLen);
    #ifdef SELECTEDLEN
    FILE* fp = fopen("log3.txt", "w");
    fprintf(fp, "%d %s", E.copyBuffLen, E.copyBuff);        
    fclose(fp);
    #endif
}
// Function paste content of piece table into writebuffer
void printLinesToWriteBuffer(writeBuffer* wb,pieceTable PT, int firstLine, int lastLine,int selectStartRow, int selectStartCol){
    int count1 = 0, count2 = 0, count = 0, selectStart = 0, selectEnd = 0;
    if(E.selectingText){
        selectStart = getIndexInNode(PT, selectStartRow, selectStartCol, &count1);
        selectEnd = getIndexInNode(PT, E.y + 1, E.x, &count2);
        if(selectStartRow - 1 > E.y){
            int temp = selectEnd;
            selectEnd = selectStart;
            selectStart = temp;
            temp = count1;
            count1 = count2;
            count2 = temp;
        }
        if(selectStartRow - 1 == E.y){
            if(E.x < selectStartCol){
                int temp = selectEnd;
                selectEnd = selectStart;
                selectStart = temp;
                temp = count1;
                count1 = count2;
                count2 = temp;
            }
        }
    }
    // FILE* fp = fopen("log2.txt", "w");
    // fprintf(fp, "%d %d %d %d", selectStart, selectEnd, count1, count2);        
    // fclose(fp);

    // Initialize Current Line Number as 1
    int currLine = 1;
    pieceNode*  currNode = PT.head->next;

    // Traverse Through the PT until Required Node found
    while(currNode != PT.tail && currLine + currNode->lineCount < firstLine){
        currLine += currNode->lineCount;
        currNode = currNode->next;
        count++;
    }
    // rowLen will store lenght of each row from first to last line
    int* rowLen = (int* )calloc((lastLine - firstLine) + 1 ,sizeof(int));
    int rowIndex = 0;
    // position index in rowLen array
    int irow = 0;
    while(currNode != PT.tail && currLine <= lastLine){
        int offset = 0;
        int countline = firstLine - currLine > 0 ? firstLine - currLine : 0;
        // calculate offset from where line starts
        for(int i = 0; i < countline; i++){
            offset += currNode->lineBreak[i];
            currLine++;
        }
        for(int i = currNode->start + offset; i <= currNode->end; i++){
            // if all rows apppended into write buffer
            if(irow == lastLine - firstLine + 1) break;
            char* buffer = currNode->buffer;
            rowIndex++;
            if(i == selectStart + currNode->start && E.selectingText && count == count1){
                appendToWriteBuffer(wb, "\x1b[7m", 4);
            }
            if(i == selectEnd + currNode->start && E.selectingText && count == count2)
                appendToWriteBuffer(wb, "\x1b[m", 3);
            if(buffer[i] == '\n'){
                if(currLine == lastLine){
                    rowLen[irow] = rowIndex;
                    irow++;
                    rowIndex = 0;
                    break;
                }
                currLine++;
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
                    }else if(buffer[i] == '\t'){
                        appendToWriteBuffer(wb, " ", 1);
                       
                    }
                    else
                        appendToWriteBuffer(wb, &buffer[i], 1);
                }
            }
        }
        // currLine += currNode->lineCount;
        count++;
        currNode = currNode->next;
    }
    appendToWriteBuffer(wb, "\x1b[m", 3);
    rowLen[irow] = rowIndex+ 1;
    free(E.rowLen);
    E.rowLen = rowLen;
}

/*  Function Reads a character and Returns it   */
int readCharFromTerminal(){
    int nread;
    char c;
    while((nread = read(STDIN_FILENO, &c, 1)) != 1){
        if(nread == -1 && errno != EAGAIN) returnError("Error in Read");
    } 
    if(c == '\x1b'){
        char escapeSeq[5];
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
                }else if(escapeSeq[1] == '1'){
                    if(escapeSeq[2] == ';'){
                        if(read(STDIN_FILENO, &escapeSeq[3], 1) != 1) return c;
                        if(escapeSeq[3] == '2'){
                            if(read(STDIN_FILENO, &escapeSeq[4], 1) != 1) return c;
                            if(escapeSeq[4] >= 'A' && escapeSeq[4] <= 'D'){
                                switch (escapeSeq[4])
                                {
                                case 'A':
                                    return SHIFT_UP;
                                case 'B':
                                    return SHIFT_DOWN;
                                case 'C':
                                    return SHIFT_RIGHT;
                                case 'D':
                                    return SHIFT_LEFT;
                                }
                            }
                        }
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
    if(E.x == E.colOffset + 5){
        E.colOffset -= 5;
        if(E.colOffset < 0) E.colOffset = 0;
    }
    if(E.y == E.rowOffset + 5){
        E.rowOffset -= 5;
        if(E.rowOffset < 0) E.rowOffset = 0;
    }
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
    int repeat, index;
    int c = readCharFromTerminal();
    #ifdef DEBUG
        FILE *fp = fopen("log.txt", "w");
        fprintf(fp, "%c ", c);
        fclose(fp);
    #endif
    switch (c){
        
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
        case CTRL_KEY('x'):
            if(E.selectingText){
                E.selectingText = 0;
                E.selectStartRow = 0;
                E.selectStartCol = 0;
                setStatusMessage("Selection of Text : OFF");
            }
            free(E.copyBuff);
            E.copyBuff = copyLine(E.PT, E.y + 1);
            E.copyBuffLen = E.rowLen[E.y - E.rowOffset] - 1;
            #ifdef COPYLINE
                FILE *fp = fopen("log2.txt", "w");
                fprintf(fp, "%d ", E.copyBuffLen);
                fclose(fp);
                debugCopy();
            #endif
            break;
        case CTRL_KEY('c'):
            if(E.selectingText)
                editorCopySelectedText();
            break;
        case CTRL_KEY('v'):
            if(E.selectingText && E.copyBuffLen != 0){
                deleteSelectedText();
                E.selectingText = 0;
                E.selectStartRow = 0;
                E.selectStartCol = 0;
                setStatusMessage("Selection of Text : OFF");
            }
            index = addedIndex++;
            for(int i = 0; i < E.copyBuffLen; i++){
                if(index >= addedSize - 1){
                    addedSize *= 2;
                    added = (char*)realloc(added, addedSize * sizeof(char));
                }
                if(E.copyBuff[i] == '\n') E.numrows++;
                added[++index] = E.copyBuff[i];
            }
            insertLineAt(E.PT, E.copyBuff, E.copyBuffLen, E.y+1, E.x);
            addedIndex = index;
            refreshScreen();
            break;
        case CTRL_KEY('s'):
            if(E.selectingText){
                E.selectingText = 0;
                E.selectStartRow = 0;
                E.selectStartCol = 0;
                setStatusMessage("Selection of Text : OFF");
            }
            saveFile();
            break;
        case CTRL_KEY('u'):
            if(E.selectingText){
                E.selectingText = 0;
                E.selectStartRow = 0;
                E.selectStartCol = 0;
                setStatusMessage("Selection of Text : OFF");
            }
            // TODO undo
            break;
        case CTRL_KEY('r'):
            if(E.selectingText){
                E.selectingText = 0;
                E.selectStartRow = 0;
                E.selectStartCol = 0;
                setStatusMessage("Selection of Text : OFF");
            }
            // TODO redo

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
            E.y = (c == PAGE_UP) ? E.rowOffset - 6 : E.rowOffset + E.screenrows + 5;
            if(PAGE_UP && E.y < 0) E.y = 0;
            if(c == PAGE_DOWN && E.y >= E.numrows) E.y = E.numrows - 1;
            E.x = 0;
            // while(repeat--) cursorMovement(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
            break;
        case HOME_KEY :
            E.x = 0;
            break;
        case END_KEY :
            if(E.y < E.numrows)
                E.x = E.rowLen[E.y - E.rowOffset] - 1;
            break;
        // CTRL-L refreshes the screen but we dont need
        case CTRL_KEY('l'):
        case CTRL_KEY('j'):
        case CTRL_KEY('h'):
        case CTRL_KEY('g'):
        // case 9:
            break;
        // Escape Character "\x1b"
        case 27:
            break;
        case BACKSPACE:
           
            if(E.selectingText){
                deleteSelectedText();
            }else{
                deleteCharAt(E.PT, E.y + 1, E.x);
                if(E.x > 0) E.x--;
            }
            if(E.selectingText){
                E.selectingText = 0;
                E.selectStartRow = 0;
                E.selectStartCol = 0;
                setStatusMessage("Selection of Text : OFF");
            }
            break;
        case SHIFT_LEFT:
        case SHIFT_UP:
        case SHIFT_DOWN:
        case SHIFT_RIGHT:
            if(E.selectingText){
                E.selectingText = 0;
                E.selectStartRow = 0;
                E.selectStartCol = 0;
                setStatusMessage("Selection of Text : OFF");
            }else{
                E.selectingText = 1;
                setStatusMessage("Selection of Text : ON");
            }
            E.selectStartRow = E.y;
            E.selectStartCol = E.x;
            break;
        default:
            if(E.selectingText){
                deleteSelectedText();
                E.selectingText = 0;
                E.selectStartRow = 0;
                E.selectStartCol = 0;
                setStatusMessage("Selection of Text : OFF");
            }
            // If Enter Key pressed it returns 13
            c =  (c == 13) ? '\n' : c ;
            if(addedIndex >= addedSize - 1){
                addedSize *= 2;
                added = (char*)realloc(added, addedSize * sizeof(char));
            }
            added[++addedIndex] = c;
            insertCharAt(E.PT, E.y+1,E.x);
            if(c == '\n'){
                E.x = 0;
                E.y++;
                E.numrows++;

            }else   
                E.x++;
            break;
    }
}
/*
 * Function prints out Text in PieceTable onto the screen
 */
void displayRows(writeBuffer* wb){
    if(E.filename == NULL && E.numrows == 0){
        for(int i = 0; i < E.screenrows; i++){
            int actualRowPosition = i + E.rowOffset;
            if(actualRowPosition >= E.numrows){
                // Printing Welcome message in middle of screen
                if(i == E.screenrows /3){
                    char msg[50];
                    int msglen = sprintf(msg , "Editor By Kunal");
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
            appendToWriteBuffer(wb, "\r\n", 2);
        }
    }
    else{

        int lastline = E.rowOffset + E.screenrows > E.numrows ? E.numrows : E.rowOffset + E.screenrows;
        #ifdef DISPLAYROW
            FILE *fp = fopen("log2.txt", "w");
            fprintf(fp, "%d %d", lastline, E.rowOffset + 1);
            fclose(fp);
         #endif
        if(E.selectingText == 1)
            printLinesToWriteBuffer(wb, E.PT, E.rowOffset + 1, lastline , E.selectStartRow + 1, E.selectStartCol);
        else{
            printLinesToWriteBuffer(wb, E.PT, E.rowOffset + 1, lastline , 1, 0);
        }
        // printLinesToWriteBuffer(wb, E.PT, E.rowOffset + 1, lastline);
        appendToWriteBuffer(wb, "\r\n", 2);
        for(int i = lastline ; i < E.screenrows + E.rowOffset; i++){
            if(i == lastline) appendToWriteBuffer(wb, "\x1b[36m", 5);
            appendToWriteBuffer(wb, "~\r\n", 3);
            if(i == E.screenrows + E.rowOffset - 1) appendToWriteBuffer(wb, "\x1b[39m", 5);
        }
    }
}

// Function Displays Status Bar
void drawStatusBar(writeBuffer* wb){
    // Enabling Inversion of color
    appendToWriteBuffer(wb, "\x1b[7m", 4);

    char status[100], rstatus[100];
    int len = sprintf(status, "%.20s - %d lines %s", E.filename ? E.filename : "[No Name]", E.numrows, "[Modified]");
    int rightLen = sprintf(rstatus, "[%d, %d]", E.y + 1, E.x);
    if(len > E.screencols) len = E.screencols;
    appendToWriteBuffer(wb, status, len);
    while(len < E.screencols){
        if(E.screencols - len == rightLen){
            appendToWriteBuffer(wb, rstatus, rightLen);
            break;
        }
        else{
            appendToWriteBuffer(wb, " ", 1);
            len++;
        }
    }
    // Disable Inversion of color
    appendToWriteBuffer(wb, "\x1b[m", 3);
    appendToWriteBuffer(wb, "\r\n", 2);

}
void drawMessageBar(writeBuffer* ab) {
    // Clear Current Line
    appendToWriteBuffer(ab, "\x1b[K", 3);

    int msglen = strlen(E.message);
    if (msglen > E.screencols) msglen = E.screencols;
    if (msglen && time(NULL) - E.message_time < 5)
        appendToWriteBuffer(ab, E.message, msglen);
}
// Variadic Function
void setStatusMessage(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(E.message, sizeof(E.message), fmt, ap);
  va_end(ap);
  E.message_time = time(NULL);
}
// Function takes prompt to display on status message and take input from user and returns
char *prompt(char* prompt){
    int size = 100;
    // Allocating memory for buffer
    char* buff = (char*)malloc(size);
    int len = 0;
    buff[0] = '\0';
    while(1){
        // display on screen as user types
        setStatusMessage(prompt, buff);
        refreshScreen();
        int c = readCharFromTerminal();
        if(c == DELETE_KEY || c == BACKSPACE){
            if(len != 0) buff[--len] = '\0';
        }else if(c == 27){ // ESC key
            setStatusMessage("");
            free(buff);
            return NULL;
        }else if(c == '\r'){ // ENTER key
            setStatusMessage("");
            return buff;
        }else if(!iscntrl(c) && c < 128){
            if(len == size - 1){
                size *= 2;
                buff = realloc(buff, size);
            }
            buff[len++] = c;
            buff[len] = '\0';
        }
    }
}

void saveFile(){
    if(E.filename == NULL){
        E.filename = prompt("Save as : %s (Press ESC to Cancel)");
        if(E.filename == NULL){
            setStatusMessage("Attempt to Save Failed");
            return;
        }
    }
    FILE *fp = fopen(E.filename, "w+");
    if(fp){
        int fileSize = writeToFile(E.PT, fp);
        setStatusMessage("%d Bytes written to File", fileSize);
        fclose(fp);
        return;
    }
    fclose(fp);
    setStatusMessage("Unable to Save! I/O Error : %s", strerror(errno));
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
    drawStatusBar(&wb);
    drawMessageBar(&wb);
    sprintf(position, "\x1b[%d;%dH", E.y - E.rowOffset + 1, E.x - E.colOffset + 1);
    appendToWriteBuffer(&wb, position, strlen(position));
    appendToWriteBuffer(&wb, "\x1b[?25h", 6);

    write(STDOUT_FILENO, wb.buf, wb.size);
    #ifdef DEBUGROWLEN
        debugRowLen();
    #endif
    #ifdef DEBUGPT
       debugPieceTable();
    #endif
    #ifdef WRITEBUF
        FILE *fp = fopen("writebuff.txt", "w");
        fprintf(fp, "%s ", wb.buf);
        fclose(fp);
    #endif
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

// Function opens provided filename as argument and stores content in piece table
void openEditor(char *filename){
    E.filename = strdup(filename);
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
    if(fileSize == 0)
        E.numrows = 1;
    fclose(fp);
}



/*
 *      Function Initializes struct Editor
 */
void initEditor(){
    E.x = E.y = 0;
    E.screencols = E.screenrows = 0;
    E.numrows = 0;
    E.rowOffset = E.colOffset =  0;
    E.filename = NULL;
    E.message[0] = '\0';
    E.message_time = 0;
    addedIndex = -1;
    addedSize = 1000;
    initPieceTable(&E.PT);
    added = (char*)malloc(addedSize * sizeof(char));
    if(getWindowSize(&E.screenrows, &E.screencols) == -1)
        returnError("Error in getWindowSize");
    E.screenrows -= 2;
    E.rowLen = (int*)calloc(E.screenrows - 1, sizeof(int));
    E.selectingText = 0;
    E.selectStartRow = 0;
    E.selectStartCol = 0;
}



int main(int argc, char* argv[]){
   
    rawModeON();
    initEditor();
    if(argc >= 2)
        openEditor(argv[1]);
    
    setStatusMessage("HELP: Ctrl-Q = quit");
    while(1){
        refreshScreen();
        processKeyInput();        

    }
    return 0;
}


