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
#include<limits.h>
#include "piecetable.h"
#include "gui.h"


/* Masking 3 bits from MSB
*    ex. ASCII Code a = 11000001 (97)
*    =====>  C1 & 1F = 01
*/

#define CTRL_KEY(x) (x & 0x1f)

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

// Defination of extern variables
char* original = NULL;
char* added = NULL;
int addedIndex = -1;
int addedSize;
Editor E;

/* ========================= Debuging Section ============================ */
void debugRowLen(){
    FILE* fp = fopen("rowlen.txt", "w");
    for(int i = 0 ; i < E.numrows; i++){
        fprintf(fp, "%d ", E.rowLen[i]);        
    }
    fclose(fp);
}
void debugPieceTable(){
    FILE* fp = fopen("pt.txt", "w");
    printPieceTable(E.PT, fp);
    fclose(fp);
}

void debugCopy(){
    FILE* fp = fopen("copy.txt", "w");
    fprintf(fp,"%s" ,E.copyBuff);
    fclose(fp);
}

void returnError(char* s){
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    perror(s);
    exit(1);
}
/* ========================= Low level terminal handling ====================== */

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

    /* Input Modes : No break, No CR to NL, No parity check, No strip char,
    No start/ stop output control */
    raw_terminal.c_iflag &= ~(BRKINT | ICRNL | IXON | INPCK | ISTRIP);
    /* Ouput Modes : Disabling Post Processing */
    raw_terminal.c_oflag &= ~(OPOST);
    /* Control Modes : Set 8 bit chars */
    raw_terminal.c_cflag |= (CS8); 
    /* Local Modes : Echoing Off, Canonical Off, No extended functions,
    no signal chars line CTRL + Z or C*/
    raw_terminal.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw_terminal.c_cc[VMIN] = 0;
    raw_terminal.c_cc[VTIME] = 1;
    // Setting terminal attributes
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_terminal) == -1) returnError("Error in tcsetattr");
}

/*  Function Reads a character  from terminal in Raw Mode and return appropriate key 
 *  If invalid then return ESC key
 */
int readCharFromTerminal(){
    int nread;
    char c;
    // while((nread = read(STDIN_FILENO, &c, 1)) != 1){
    //     if(nread == -1 && errno != EAGAIN) returnError("Error in Read");
    // } 
    while((nread = read(STDIN_FILENO, &c, 1)) == 0);
    if(nread == -1) exit(1);
    if(c == '\x1b'){            // Escape Sequences
        char escapeSeq[5];
        /*If it's only ESC then return it */
        if(read(STDIN_FILENO, &escapeSeq[0], 1) == 0)   return c;
        if(escapeSeq[0] == '\x1b') return c;
        if(read(STDIN_FILENO, &escapeSeq[1], 1) == 0)   return c;
        // Escape Sequence ESC [
        if(escapeSeq[0] == '['){
            if(escapeSeq[1] >= '0' && escapeSeq[1] <= '9'){
                if(read(STDIN_FILENO, &escapeSeq[2], 1) == 0) return c;
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
                        if(read(STDIN_FILENO, &escapeSeq[3], 1) == 0) return c;
                        if(escapeSeq[3] == '2'){
                            if(read(STDIN_FILENO, &escapeSeq[4], 1) == 0) return c;
                            if(escapeSeq[4] >= 'A' && escapeSeq[4] <= 'D'){             // Shift + Arrow Key
                                switch (escapeSeq[4]){
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
            else{           // Arrow Keys
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

/*
 *   Function gives Window Size of Terminal
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

/* Function opens file and Stores it into PieceTable
 * Original buffer gets initialized
 */
void openEditor(char *filename){
    E.filename = strdup(filename);
    FILE *fp = fopen(filename, "rb");
    if(!fp){
        returnError("Error in Opening File");
    }
    // To get file size
    fseek(fp, 0, SEEK_END);
    long fileSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    // If filesize not zero then malloc original buffer
    if(fileSize != 0){
        original =(char*) malloc(fileSize + 1);
        fread(original, 1, fileSize, fp);
    }else
        original = NULL;
    // Modify PieceTable
    if(original){
        pieceNode* newNode = newPieceNode(original, 0, fileSize - 1, 0);
        newNode->next = (E.PT.head)->next;
        newNode->prev = E.PT.head;

        (E.PT.tail)->prev = newNode;
        (E.PT.head)->next = newNode;
        // Update Number of Rows
        E.numrows = newNode->lineCount + 1;                               ///  //// //// See Later
    }
    if(fileSize == 0)
        E.numrows = 1;
    // Calloc rowlen array
    E.rowLen = calloc(E.numrows, sizeof(int));
    E.fileChanged = 0;
    fclose(fp);
}

/* Saves the modified file by writting content of PieceTable
 * into the file
 */
void saveFile(){
    // If No filename provided, ask for filename
    if(E.filename == NULL){
        // Prompting user for filename
        E.filename = prompt("Save as : %s (Press ESC 2 Times to Cancel)", NULL);
        if(E.filename == NULL){
            // If error in getting input from user
            setStatusMessage("Attempt to Save Failed");
            return;
        }
    }
    FILE *fp = fopen(E.filename, "w+");
    if(fp){
        int fileSize = writeToFile(E.PT, fp);
        setStatusMessage("%d Bytes written to File", fileSize);
        fclose(fp);
        // As file saved reseting filechanged to zero
        E.fileChanged = 0;
        return;
    }
    fclose(fp);
    // If error occured then setting status message
    setStatusMessage("Unable to Save! I/O Error : %s", strerror(errno));
}


/* ============================= Updating Terminal View ============================ */

/* Defined a very simple "write buffer" structure, in which we append
 * different character to display on screen. 
 * This is useful in order to
 * write all the escape sequences in a buffer and flush them to the standard
 * output in a single call, to avoid flickering effects. 
 */


/*
 *      Write Buffer Section
 */

// Function Initializes writeBuffer Abstract Data type

void initWriteBuffer(writeBuffer* wb){
    wb->buf = NULL;
    wb->size = 0;
}

// Function Apppend String s to writeBuffer
void appendToWriteBuffer(writeBuffer* wb, char* s, int size){
    char* temp = realloc(wb->buf, wb->size + size);             // Reallocating 
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


/* Refreshing Screen : 
 *      Appends different ANSI Escape Sequence into write Buffer 
 *      Also Appends different contents to be displayed on screen (like all characters, message bar, status bar)
 *      Then flush it to STD_OUT
 */
void refreshScreen() {
    // Return -1 if error occured
    if (getWindowSize(&E.screenrows, &E.screencols) == -1) returnError("Error in getWindowSize");
    E.screenrows -= 2;          // Since last two rows are for message bar and status bar
    E.rowLen = (int *)realloc(E.rowLen, E.numrows * sizeof(int));

    scrollScreen();             // Update row and col offset if scrolling happened
    char position[20];
    writeBuffer wb;
    initWriteBuffer(&wb);       // Init write buffer

    // Escape Sequence for hiding cursor
    appendToWriteBuffer(&wb, "\x1b[?25l", 6);
    // Escape Sequence for erasing whole screen
    appendToWriteBuffer(&wb, "\x1b[2J", 4);
    // Escape Sequence for moving cursor to initial position
    appendToWriteBuffer(&wb, "\x1b[H", 3);    
    // Append file content along with different escape codes for highlighting
    displayRows(&wb);
    // Apppend Status Bar info
    drawStatusBar(&wb);
    // Append Messsage Bar info
    drawMessageBar(&wb);
    // Appending Cursor info with Escape Sequence to reposition it
    sprintf(position, "\x1b[%d;%dH", E.y - E.rowOffset + 1, E.x - E.colOffset + 1);
    appendToWriteBuffer(&wb, position, strlen(position));
    appendToWriteBuffer(&wb, "\x1b[?25h", 6);
    // Flushing Write Buffer to STD_OUT
    write(STDOUT_FILENO, wb.buf, wb.size);

        /*              DEBUG SECTION STARTS         */
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
        /*              DEBUG SECTION  ENDS       */

    destroyWriteBuffer(&wb);                
}

/*  Append Different Content into write buffer
 *  If Help Mode Enable then append content related to help
 *  otherwise ,
 *      append all PieceTable content with different styling and colors
 */
void displayRows(writeBuffer* wb){
    // If help mode ON
    if(E.helpModeEnable){
        helpDisplay(wb);
    }
    // If No filename provided at start of program

    else if(E.filename == NULL && E.fileChanged == 0){
        for(int i = 0; i < E.screenrows; i++){
            // Printing Basic Info message in middle of screen
            appendToWriteBuffer(wb, "\x1b[36m", 5);
            if(i == E.screenrows /3){
                char msg[50];
                int msglen = sprintf(msg , "Editor By Kunal");
                // If msg length exceeds screensize
                if(msglen > E.screencols) msglen = E.screencols;
                int space = (E.screencols - msglen)/2;
                if(space){
                    appendToWriteBuffer(wb, "~", 1);
                    space--;
                }
                while(space--) appendToWriteBuffer(wb, " ", 1);

                appendToWriteBuffer(wb, "\x1b[7m", 4);
                appendToWriteBuffer(wb, "\x1b[35m", 5);
                appendToWriteBuffer(wb, msg, msglen);
                appendToWriteBuffer(wb, "\x1b[39m", 5);
                appendToWriteBuffer(wb, "\x1b[m", 3);
            }
            else if(i == E.screenrows /3 + 1){
                char msg[50];
                int msglen = sprintf(msg , "Version 1.0.0");
                if(msglen > E.screencols) msglen = E.screencols;
                int space = (E.screencols - msglen)/2;
                if(space){
                    appendToWriteBuffer(wb, "~", 1);
                    space--;
                }
                while(space--) appendToWriteBuffer(wb, " ", 1);
                appendToWriteBuffer(wb, "\x1b[7m", 4);
                appendToWriteBuffer(wb, "\x1b[33m", 5);
                appendToWriteBuffer(wb, msg, msglen);
                appendToWriteBuffer(wb, "\x1b[39m", 5);
                appendToWriteBuffer(wb, "\x1b[m", 3);
            }
            else if( i != 0)
                appendToWriteBuffer(wb, "~", 1);
        appendToWriteBuffer(wb, "\r\n", 2);             // '\r' = Carriage Return ; '\n' = newline
        }
        appendToWriteBuffer(wb, "\x1b[39m", 5);
    }
    // Otherwise append PieceTable content
    else{
        // Lastline number that can be displayed on screen
        int lastline = E.rowOffset + E.screenrows > E.numrows ? E.numrows : E.rowOffset + E.screenrows;

                /*              DEBUG SECTION STARTS        */
        #ifdef DISPLAYROW
            FILE *fp = fopen("displayRow.txt", "w");
            fprintf(fp, "%d %d", lastline, E.rowOffset + 1);
            fclose(fp);
         #endif
                /*              DEBUG SECTION  ENDS       */

        // If we have to show selected text
        if(E.selectingText == 1)
            printLinesToWriteBuffer(wb, E.PT, E.rowOffset + 1, lastline , E.selectStartRow + 1, E.selectStartCol);
        // otherwise
        else{
            printLinesToWriteBuffer(wb, E.PT, E.rowOffset + 1, lastline , 1, 0);
        }
        
        appendToWriteBuffer(wb, "\r\n", 2);

        // Append '~' if otherlines on screen empty
        for(int i = lastline ; i < E.screenrows + E.rowOffset; i++){
            if(i == lastline) appendToWriteBuffer(wb, "\x1b[36m", 5);
            appendToWriteBuffer(wb, "~\r\n", 3);
            if(i == E.screenrows + E.rowOffset - 1) appendToWriteBuffer(wb, "\x1b[39m", 5);
        }
    }
}

/*  Apppends PieceTable Cotent into writeBuffer provided starting and ending line number
 *  Also add different styling and coloring depending on selection ON/OFF or Searching ON/OFF
 */
void printLinesToWriteBuffer(writeBuffer* wb,pieceTable PT, int firstLine, int lastLine,int selectStartRow, int selectStartCol){

    int count1 = 0, count2 = 0, count = 0, selectStart = 0, selectEnd = 0, flag = 0;
    int foundNodeNo1 = 0, foundNodeNo2 = 0;
    int foundStart = 0;
    int foundEnd = 0;
    // If selection ON 
    if(E.selectingText){
        // Finding position in piecetable to show Selection at that position (x, y coordinate provided)
        // Count will contain node number
        selectStart = getIndexInNode(PT, selectStartRow, selectStartCol, &count1);
        selectEnd = getIndexInNode(PT, E.y + 1, E.x, &count2);
        // Swapping selection start if selection end is before
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
    // If Searching Enable
    if(E.searchEnable){
        foundStart = getIndexInNode(PT, E.y + 1, E.x, &foundNodeNo1);
        foundEnd =  getIndexInNode(PT, E.y + 1, E.x + E.foundLength, &foundNodeNo2);
    }
    // Initialize Current Line Number as 1
    int currLine = 1;
    pieceNode*  currNode = PT.head->next;

    // Traverse Through the PT until Required Node found
    while(currNode != PT.tail && currLine + currNode->lineCount < firstLine){
        currLine += currNode->lineCount;
        currNode = currNode->next;
        count++;
    }

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
            if(E.selectingText){            // Adding Colors if selected something
                if(i == selectStart + currNode->start && count == count1){
                    appendToWriteBuffer(wb, "\x1b[7m", 4);
                    flag = 1;
                }
                if((count >= count1 && i > selectStart + currNode->start) && !flag){
                    appendToWriteBuffer(wb, "\x1b[7m", 4);
                    flag = 1;
                }
                if(i == selectEnd + currNode->start && count == count2)
                    appendToWriteBuffer(wb, "\x1b[m", 3);
            }
            if(E.searchEnable){             // Adding Colors if searching something
                if(i == foundStart + currNode->start && count == foundNodeNo1){
                    appendToWriteBuffer(wb, "\x1b[7m\x1b[33m", 9);                
                }
                if(i == foundEnd + currNode->start && count == foundNodeNo2){
                    appendToWriteBuffer(wb, "\x1b[39m\x1b[m", 8);
                }
            }
            if(buffer[i] == '\n'){          // If '\n ' occur then save line lenght into rowlen array
                if(currLine == lastLine){
                    E.rowLen[irow + firstLine - 1] = rowIndex;
                    irow++;
                    rowIndex = 0;
                    break;
                }
                currLine++;
                appendToWriteBuffer(wb, "\r\n", 2);
                E.rowLen[irow + firstLine - 1] = rowIndex;
                irow++;
                rowIndex = 0;
            }
            else{
                // Append according Screen size and scrolling
                if(rowIndex > E.colOffset && rowIndex <= E.colOffset + E.screencols){   
                    
                    if(isdigit(buffer[i])){
                        // to set red color
                        appendToWriteBuffer(wb, "\x1b[31m", 5);
                        appendToWriteBuffer(wb, &buffer[i], 1);
                        // to reset color
                        appendToWriteBuffer(wb, "\x1b[39m", 5);
                    }else if(buffer[i] == '\t'){                // Handled Tab 
                        appendToWriteBuffer(wb, " ", 1);
                       
                    }
                    else
                        appendToWriteBuffer(wb, &buffer[i], 1);
                }
            }
        }
        count++;            // Node Count
        currNode = currNode->next;
    }
    // Reseting attributes
    appendToWriteBuffer(wb, "\x1b[m", 3);
    E.rowLen[irow  + firstLine - 1] = rowIndex+ 1;  // For last line length
}

/*  Function Append Required Character into write buffer
 *  thus help in drawing status bar
 */
void drawStatusBar(writeBuffer* wb){
    // Enabling Inversion of color
    appendToWriteBuffer(wb, "\x1b[7m", 4);

    char status[100], rstatus[100];
    int len = sprintf(status, "%.20s - %d lines %s", E.filename ? E.filename : "[No Name]", E.numrows, E.fileChanged ? "[Modified]" : "");
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


/*  Function Append Required Character into write buffer
 *  thus help in drawing message bar
 *  This message is depending on time , only display for 5 sec after setting
 *  It keeps on changing depending on different modes
 */
void drawMessageBar(writeBuffer* ab) {
    // Clear Current Line
    appendToWriteBuffer(ab, "\x1b[K", 3);
    // Get message length
    int msglen = strlen(E.message);
    if (msglen > E.screencols) msglen = E.screencols;
    // If time not exceeded
    if (msglen && time(NULL) - E.message_time < 5)
        appendToWriteBuffer(ab, E.message, msglen);
}


/*  A Variadic Function helps in setting message bar with variable arguments
 */
void setStatusMessage(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  // copying message into message member of editor
  vsnprintf(E.message, sizeof(E.message), fmt, ap);
  va_end(ap);
  // Storing time when message set
  E.message_time = time(NULL);
}

/* Function creates prompt which display at message bar 
 * and take input from user and returns that char string
 * ex. if searching for string , taking filename from user to save etc.
 * Also takes function pointer as argument if required to call it otherwise pass NULL
 */
char *prompt(char* prompt, void (*callback)(char *, int)){
    int size = 100;
    // Allocating memory for buffer
    char* buff = (char*)malloc(size);
    int len = 0;
    buff[0] = '\0';
    while(1){
        // Display on screen as user types input using set status msg
        setStatusMessage(prompt, buff);
        // Refreshing to show changes
        refreshScreen();
        // Reading char
        int c = readCharFromTerminal();

        // Different keys handled
        if(c == DELETE_KEY || c == BACKSPACE){
            if(len != 0) buff[--len] = '\0';
        }else if(c == 27){          // ESC key
            setStatusMessage("");
            // If not null then call
            if(callback) callback(buff, c);
            free(buff);
            return NULL;
        }else if(c == '\r'){        // ENTER key
            setStatusMessage("");
            if(callback) callback(buff, c);
            if(buff[0] == '\0'){
                free(buff);
                return NULL;
            }
            return buff;
        }else if(!iscntrl(c) && c < 128){
            if(len == size - 1){
                size *= 2;
                buff = realloc(buff, size);
            }
            buff[len++] = c;
            buff[len] = '\0';
        }
        if(callback) callback(buff, c);
    }
}

/* ================================ Help Mode ================================ */

/* Apppend to write buffer content related to help section
 */
void helpDisplay(writeBuffer* wb){
    for(int i = 0; i < E.screenrows; i++){
        // Printing Info in middle of screen
        if(i == E.screenrows /4){
            char msg[55];
            int msglen = sprintf(msg , "=================== Editor By Kunal ==================");
            if(msglen > E.screencols) msglen = E.screencols;
            int space = (E.screencols - msglen)/2;
            if(space){
                appendToWriteBuffer(wb, "~", 1);
                space--;
            }
            while(space--) appendToWriteBuffer(wb, " ", 1);
            appendToWriteBuffer(wb, "\x1b[31m", 5);
            appendToWriteBuffer(wb, msg, msglen);
            appendToWriteBuffer(wb, "\x1b[39m", 5);
        }
        else if(i == E.screenrows /3 + 1){
            char msg[53];
            int msglen = sprintf(msg , "Ctrl-Q : Exit From Editor                          ");
            if(msglen > E.screencols) msglen = E.screencols;
            int space = (E.screencols - msglen)/2;
            if(space){
                appendToWriteBuffer(wb, "~", 1);
                space--;
            }
            while(space--) appendToWriteBuffer(wb, " ", 1);
            appendToWriteBuffer(wb, "\x1b[32m", 5);
            appendToWriteBuffer(wb, msg, msglen);
            appendToWriteBuffer(wb, "\x1b[39m", 5);
        }
        else if(i == E.screenrows /3 + 2){
            char msg[53];
            int msglen = sprintf(msg , "Ctrl-S : Save the Changes                          ");
            if(msglen > E.screencols) msglen = E.screencols;
            int space = (E.screencols - msglen)/2;
            if(space){
                appendToWriteBuffer(wb, "~", 1);
                space--;
            }
            while(space--) appendToWriteBuffer(wb, " ", 1);
            appendToWriteBuffer(wb, "\x1b[33m", 5);
            appendToWriteBuffer(wb, msg, msglen);
            appendToWriteBuffer(wb, "\x1b[39m", 5);
        }
        else if(i == E.screenrows /3 + 3){
            char msg[53];
            int msglen = sprintf(msg , "Ctrl-F : Search in File                            ");
            if(msglen > E.screencols) msglen = E.screencols;
            int space = (E.screencols - msglen)/2;
            if(space){
                appendToWriteBuffer(wb, "~", 1);
                space--;
            }
            while(space--) appendToWriteBuffer(wb, " ", 1);
            appendToWriteBuffer(wb, "\x1b[34m", 5);
            appendToWriteBuffer(wb, msg, msglen);
            appendToWriteBuffer(wb, "\x1b[39m", 5);
        }
        else if(i == E.screenrows /3 + 4){
            char msg[53];
            int msglen = sprintf(msg , "Ctrl-X : Copy Current Line                         ");
            if(msglen > E.screencols) msglen = E.screencols;
            int space = (E.screencols - msglen)/2;
            if(space){
                appendToWriteBuffer(wb, "~", 1);
                space--;
            }
            while(space--) appendToWriteBuffer(wb, " ", 1);
            appendToWriteBuffer(wb, "\x1b[35m", 5);
            appendToWriteBuffer(wb, msg, msglen);
            appendToWriteBuffer(wb, "\x1b[39m", 5);
        }
        else if(i == E.screenrows /3 + 5){
            char msg[53];
            int msglen = sprintf(msg , "Ctrl-C : Copy Selected Text                        ");
            if(msglen > E.screencols) msglen = E.screencols;
            int space = (E.screencols - msglen)/2;
            if(space){
                appendToWriteBuffer(wb, "~", 1);
                space--;
            }
            while(space--) appendToWriteBuffer(wb, " ", 1);
            appendToWriteBuffer(wb, "\x1b[36m", 5);
            appendToWriteBuffer(wb, msg, msglen);
            appendToWriteBuffer(wb, "\x1b[39m", 5);
        }
        else if(i == E.screenrows /3 + 6){
            char msg[53];
            int msglen = sprintf(msg , "Ctrl-V : Paste Copied Text                         ");
            if(msglen > E.screencols) msglen = E.screencols;
            int space = (E.screencols - msglen)/2;
            if(space){
                appendToWriteBuffer(wb, "~", 1);
                space--;
            }
            while(space--) appendToWriteBuffer(wb, " ", 1);
            appendToWriteBuffer(wb, "\x1b[37m", 5);
            appendToWriteBuffer(wb, msg, msglen);
            appendToWriteBuffer(wb, "\x1b[39m", 5);
        }
        else if(i == E.screenrows /3 + 7){
            char msg[53];
            int msglen = sprintf(msg , "Ctrl-G : Goto to Certain Line                      ");
            if(msglen > E.screencols) msglen = E.screencols;
            int space = (E.screencols - msglen)/2;
            if(space){
                appendToWriteBuffer(wb, "~", 1);
                space--;
            }
            while(space--) appendToWriteBuffer(wb, " ", 1);
            appendToWriteBuffer(wb, "\x1b[36m", 5);
            appendToWriteBuffer(wb, msg, msglen);
            appendToWriteBuffer(wb, "\x1b[39m", 5);
        }
        else if(i == E.screenrows /3 + 8){
            char msg[53];
            int msglen = sprintf(msg , "SHIFT + ARROW-KEY : Enable / Disable Text Selection");
            if(msglen > E.screencols) msglen = E.screencols;
            int space = (E.screencols - msglen)/2;
            if(space){
                appendToWriteBuffer(wb, "~", 1);
                space--;
            }
            while(space--) appendToWriteBuffer(wb, " ", 1);
            appendToWriteBuffer(wb, "\x1b[32m", 5);
            appendToWriteBuffer(wb, msg, msglen);
            appendToWriteBuffer(wb, "\x1b[39m", 5);
        }
        else if(i == E.screenrows /3 + 9){
            char msg[53];
            int msglen = sprintf(msg , "Ctrl-H : Enable / Disable Help Mode                ");
            if(msglen > E.screencols) msglen = E.screencols;
            int space = (E.screencols - msglen)/2;
            if(space){
                appendToWriteBuffer(wb, "~", 1);
                space--;
            }
            while(space--) appendToWriteBuffer(wb, " ", 1);
            appendToWriteBuffer(wb, "\x1b[33m", 5);
            appendToWriteBuffer(wb, msg, msglen);
            appendToWriteBuffer(wb, "\x1b[39m", 5);
        }
        else if(i == E.screenrows /3 + 10){
            char msg[53];
            int msglen = sprintf(msg , "HOME : Move Cursor at Start of Line                ");
            if(msglen > E.screencols) msglen = E.screencols;
            int space = (E.screencols - msglen)/2;
            if(space){
                appendToWriteBuffer(wb, "~", 1);
                space--;
            }
            while(space--) appendToWriteBuffer(wb, " ", 1);
            appendToWriteBuffer(wb, "\x1b[34m", 5);
            appendToWriteBuffer(wb, msg, msglen);
            appendToWriteBuffer(wb, "\x1b[39m", 5);
        }
        else if(i == E.screenrows /3 + 11){
            char msg[53];
            int msglen = sprintf(msg , "END : Move Cursor at End of Line                   ");
            if(msglen > E.screencols) msglen = E.screencols;
            int space = (E.screencols - msglen)/2;
            if(space){
                appendToWriteBuffer(wb, "~", 1);
                space--;
            }
            while(space--) appendToWriteBuffer(wb, " ", 1);
            appendToWriteBuffer(wb, "\x1b[35m", 5);
            appendToWriteBuffer(wb, msg, msglen);
            appendToWriteBuffer(wb, "\x1b[39m", 5);
        }
        else if(i == E.screenrows /3 + 12){
            char msg[53];
            int msglen = sprintf(msg , "PAGE-UP / PAGE-DOWN : To Scroll Up and Down        ");
            if(msglen > E.screencols) msglen = E.screencols;
            int space = (E.screencols - msglen)/2;
            if(space){
                appendToWriteBuffer(wb, "~", 1);
                space--;
            }
            while(space--) appendToWriteBuffer(wb, " ", 1);
            appendToWriteBuffer(wb, "\x1b[36m", 5);
            appendToWriteBuffer(wb, msg, msglen);
            appendToWriteBuffer(wb, "\x1b[39m", 5);
        }
        
        else
            appendToWriteBuffer(wb, "~", 1);
        appendToWriteBuffer(wb, "\r\n", 2);
    }
}


/* =============================== Editor events handling  =============================== */

/*  According to ARROW KEYS it sets x and y members of Editor
*/

void cursorMovement(int key){
    switch(key){
        case ARROW_LEFT:
            if (E.x != 0) {         
                E.x--;
            } else if (E.y > 0) {
                E.y--;
                E.x = E.rowLen[E.y] > 1 ? E.rowLen[E.y] - 1 : 0;    // Moving to end of above line
            }
            break;
        case ARROW_RIGHT:
            if(E.rowLen[E.y] < 1)
                break;
            if (E.y < E.numrows && E.rowLen[E.y] - 1 > E.x){ // Going ahead on same line
                E.x++;
            }else if (E.y + 1 < E.numrows){  // Jumping to next line
                E.y++;
                E.x = 0;
            }
            break;
        case ARROW_UP:
            if(E.y != 0){   // Decrement E.y if not at first line
                E.y--;
            }
            if(E.rowLen[E.y] < 1)
                E.x = 0;
            else if(E.x > E.rowLen[E.y] - 1)        // Adjust E.x to end of above line 
                E.x = E.rowLen[E.y] - 1;
            break;
        case ARROW_DOWN:
            if(E.y + 1 < E.numrows){                // If E.y within range then increment
                E.y++;
            }
            if(E.rowLen[E.y] < 1)
                E.x = 0;
            else if(E.x > E.rowLen[E.y] - 1)        // Adjust E.x to end of below line
                E.x = E.rowLen[E.y] - 1;
            break;
    }
}

/*   Adjust rowOffset and colOffset of Editor according to scrolling done 
 *   rowOffset : rows with which scrolled down or up
 *   colOffset : cols with which scrolled left or right 
 */
void scrollScreen(){
    // if this then scroll left by 5 lines
    if(E.x == E.colOffset + 5){
        E.colOffset -= 5;
        if(E.colOffset < 0) E.colOffset = 0;
    }
    // if this then scroll up by 5 lines
    if(E.y == E.rowOffset + 5){
        E.rowOffset -= 5;
        if(E.rowOffset < 0) E.rowOffset = 0;
    }
    // Set colOffset according to E.x
    if(E.x < E.colOffset)
        E.colOffset = E.x;
    if(E.x >= E.colOffset + E.screencols)
        E.colOffset = E.x - E.screencols + 1;
    // Set rowOffset according to E.y
    if(E.y < E.rowOffset)
        E.rowOffset = E.y;
    if(E.y >= E.rowOffset + E.screenrows)
        E.rowOffset = E.y - E.screenrows + 1;

    // If searching and searched word is out of screen then scroll to right
    if(E.searchEnable){
        if(E.x + E.foundLength > E.screencols + E.colOffset)
            E.colOffset += E.foundLength - (E.x - E.screencols - E.colOffset);
    }
}

/*  Function handles different Events
 *  Events happens when user inputs some keys. It read keyboard Input.
 *  According to different keys different functions are done by this function
 */
void processKeyInput(){
    int index;
    static int quit_no = 3;         // No of times CTRL-Q should be pressed to quit file having unsaved changes
    // Reading keyboard input
    int c = readCharFromTerminal();

        /*              DEBUG SECTION STARTS         */
    #ifdef DEBUG
        FILE *fp = fopen("inputchar.txt", "w");
        fprintf(fp, "%c ", c);
        fclose(fp);
    #endif
        /*              DEBUG SECTION  ENDS          */

    switch (c){
        
        case CTRL_KEY('q'):         // CTRL + Q

            // If file has unsaved changes
            if (E.fileChanged && quit_no > 0) {
                setStatusMessage("WARNING !!! File has some Unsaved Changes | Press Ctrl-Q %d more times to quit.", quit_no);
                quit_no--;
                return;
            }
            // Clearing Screen
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;

        case CTRL_KEY('l'):         // CTRL + L

            // If selection ON then disable it
            if(E.selectingText){
                E.selectingText = 0;
                E.selectStartRow = 0;
                E.selectStartCol = 0;
                setStatusMessage("Selection of Text : OFF");
            }
            free(E.copyBuff);       // Free Previous copied string

            E.copyBuff = copyLine(E.PT, E.y + 1);
            E.copyBuffLen = E.rowLen[E.y] - 1;              // Length of copied line

                /*              DEBUG SECTION STARTS         */
            #ifdef COPYLINE
                FILE *fp = fopen("copy.txt", "w");
                fprintf(fp, "%d ", E.copyBuffLen);
                fclose(fp);
                debugCopy();
            #endif
                /*              DEBUG SECTION  ENDS          */

            break;

        case CTRL_KEY('x'):     // CTRL + X

            // If selection ON then only cut selected text
            if(E.selectingText){
                editorCopySelectedText();           // Copy selected text into Editor member copyBuff
                deleteSelectedText();               // Delete selected text
                // Disable Selection Mode
                E.selectingText = 0;
                E.selectStartRow = 0;
                E.selectStartCol = 0;
                setStatusMessage("Selection of Text : OFF");
            }
            break;
            
        case CTRL_KEY('c'):     // CTRL + C

            // If selection On then only copy selected text
            if(E.selectingText)
                editorCopySelectedText();
            break;

        case CTRL_KEY('v'):     // CTRL + V

            // If selected something then delete it then paste there
            if(E.selectingText && E.copyBuffLen != 0){
                deleteSelectedText();
                E.selectingText = 0;
                E.selectStartRow = 0;
                E.selectStartCol = 0;
                setStatusMessage("Selection of Text : OFF");
            }

            // Append all content into ADDED buffer
            index = addedIndex++;
            for(int i = 0; i < E.copyBuffLen; i++){
                if(index >= addedSize - 1){
                    addedSize *= 2;
                    added = (char*)realloc(added, addedSize * sizeof(char));
                }
                if(E.copyBuff[i] == '\n') E.numrows++;
                added[++index] = E.copyBuff[i];
            }
            // Then insert it into Piece Table
            insertLineAt(E.PT, E.copyBuff, E.copyBuffLen, E.y+1, E.x);
            // set filechanged to true
            E.fileChanged = 1;
            // update addedIndex
            addedIndex = index;
            break;

        case CTRL_KEY('s'):     // CTRL + S

            // If selection ON then disable it
            if(E.selectingText){
                E.selectingText = 0;
                E.selectStartRow = 0;
                E.selectStartCol = 0;
                setStatusMessage("Selection of Text : OFF");
            }
            // Save file
            saveFile();
            break;

        case CTRL_KEY('j'):     // CTRL + J    or  CTRL + ENTER
            
            // If selection ON then disable it
            if(E.selectingText){
                E.selectingText = 0;
                E.selectStartRow = 0;
                E.selectStartCol = 0;
                setStatusMessage("Selection of Text : OFF");
            }
            // Insert newline below current cursor positon
            if(addedIndex >= addedSize - 1){
                addedSize *= 2;
                added = (char*)realloc(added, addedSize * sizeof(char));
            }
            added[++addedIndex] = '\n';
            insertCharAt(E.PT, E.y+1, E.rowLen[E.y] - 1);
            E.fileChanged = 1;
            // Update cursor position and numrows
            E.x = 0;
            E.y++;
            E.numrows++;
            break;

        case CTRL_KEY('u'):     // CTRL + U

            // If selection ON then disable it
            if(E.selectingText){
                E.selectingText = 0;
                E.selectStartRow = 0;
                E.selectStartCol = 0;
                setStatusMessage("Selection of Text : OFF");
            }
            // TODO undo
            break;
        case CTRL_KEY('r'):     // CTRL + R

            // If selection ON then disable it
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

            cursorMovement(c);          // Move cursor according to key
            break; 

        case PAGE_UP:
        case PAGE_DOWN:

            // Update E.x and E.y 
            E.y = (c == PAGE_UP) ? E.rowOffset - 6 : E.rowOffset + E.screenrows + 5;
            if(PAGE_UP && E.y < 0) E.y = 0;
            if(c == PAGE_DOWN && E.y >= E.numrows) E.y = E.numrows - 1;
            E.x = 0;
            break;

        case HOME_KEY :

            E.x = 0;    // Start of current line
            break;
        case END_KEY :

            if(E.y < E.numrows)
                E.x = E.rowLen[E.y] - 1;    // End of Current line
            break;
        
        case CTRL_KEY('g'):     // CTRL + G

            // If selection ON then disable it
            if(E.selectingText){
                E.selectingText = 0;
                E.selectStartRow = 0;
                E.selectStartCol = 0;
                setStatusMessage("Selection of Text : OFF");
            }
            editorGoToLine();       // Switch to Go To Line Mode
            break;

        case CTRL_KEY('f'):     // CTRL + F

            // If selection ON then disable it
            if(E.selectingText){
                E.selectingText = 0;
                E.selectStartRow = 0;
                E.selectStartCol = 0;
                setStatusMessage("Selection of Text : OFF");
            }
            // SearchMode ON
            E.searchEnable = 1;
            // Invoke search mode
            editorSearch();
            // Disable Search Mode
            E.searchEnable = 0;
            E.foundLength = 0;
            break;

            
        case CTRL_KEY('h'):     // CTRL + H
            
            // Enable Help Mode
            E.helpModeEnable = 1;
            // Refreshscreen to show it
            refreshScreen();
            // ON CTRL + H disable it
            int c1 = readCharFromTerminal();
            while(c1 != CTRL_KEY('h')){
                c1 = readCharFromTerminal();
            }
            E.helpModeEnable = 0;
            break;

        case '\x1b':        // Escape Character "\x1b"
            break;

        case BACKSPACE:
           
            if(E.selectingText){
                deleteSelectedText();   // If selected then delete all selected text
            }else{
                deleteCharAt(E.PT, E.y + 1, E.x);
                if(E.x > 0) E.x--;
            }
            E.fileChanged = 1;
            // If selection ON then disable it
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

            // Toggle selection Mode
            if(E.selectingText){
                E.selectingText = 0;
                E.selectStartRow = 0;
                E.selectStartCol = 0;
                setStatusMessage("Selection of Text : OFF");
            }else{
                E.selectingText = 1;
                setStatusMessage("Selection of Text : ON");
            }
            // Store from where selection started
            E.selectStartRow = E.y;
            E.selectStartCol = E.x;
            break;

        default:

            // If selection ON then disable it
            if(E.selectingText){
                deleteSelectedText();
                E.selectingText = 0;
                E.selectStartRow = 0;
                E.selectStartCol = 0;
                setStatusMessage("Selection of Text : OFF");
            }
            // If Enter Key pressed it returns 13
            c =  (c == 13) ? '\n' : c ;
            // Append character to ADDED buffer
            if(addedIndex >= addedSize - 1){
                addedSize *= 2;
                added = (char*)realloc(added, addedSize * sizeof(char));
            }
            added[++addedIndex] = c;
            // Update PieceTable
            insertCharAt(E.PT, E.y+1,E.x);
            E.fileChanged = 1;
            // IF newline then update cursor and numrows
            if(c == '\n'){
                E.x = 0;
                E.y++;
                E.numrows++;
            }else   
                E.x++;
            break;
    }
    // Set again no of quit times
    quit_no = 3;
}

/* ===================================== Selection mode ===================================== */


/* Function returns selected text lenght where
 * startRow , startCol : Represent from where selection started
 * endRow, endCol : Represent where selection ended
 */
int selectedSize(int startRow, int startCol, int endRow, int endCol){
    int copiedSize = 0;
    while(1){
        //if startRow and endRow matched then
        if(startRow == endRow){
            copiedSize += endCol - startCol;
            break;
        }else{
            copiedSize += E.rowLen[startRow] - startCol;
            startRow++;
            startCol = 0;
        }
    }
    return copiedSize;
}

/* If some text is selected then function helps in deleting that text
 * from PieceTable
 */
void deleteSelectedText(){
    int startRow,startCol;  // From here selection started
    // Swap if selection start is after selection ends
    if(E.selectStartRow > E.y || (E.y == E.selectStartRow && E.x < E.selectStartCol)){
        startRow = E.y;
        startCol = E.x;
        E.y = E.selectStartRow;
        E.x = E.selectStartCol;
    }else{
        startRow = E.selectStartRow;
        startCol = E.selectStartCol;
    }
    // Until start Position reached delete each char
    while(!(E.x == startCol && E.y == startRow)){
        deleteCharAt(E.PT, E.y + 1, E.x);
        if(E.x > 0) E.x--;
    }
}

/* ================================ Copy Selected Text ================================ */

/* Function copies selected text into copyBuff member of Editor and also
 * stores copied text size into copyBuffLen member
 */
void editorCopySelectedText(){
    // If previously copied something then free it
    free(E.copyBuff);

    int startRow,startCol, endRow, endCol, copiedSize = 0;

    // Swap if selection start is after selection ends
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
    // get selected text len
    E.copyBuffLen = selectedSize(startRow, startCol, endRow, endCol);
    // if its 0 return
    if(E.copyBuffLen == 0) return;
    // else copy that text
    E.copyBuff = copyLineFrom(E.PT, startRow + 1, startCol, E.copyBuffLen);

         /*              DEBUG SECTION STARTS         */
    #ifdef SELECTEDLEN
    FILE* fp = fopen("selection.txt", "w");
    fprintf(fp, "%d %s", E.copyBuffLen, E.copyBuff);        
    fclose(fp);
    #endif
         /*              DEBUG SECTION ENDS         */
         
}

/* =============================== Searching Section =============================== */

/* Function Find for query in Piecetable and as user is typing
 * found words displayed on screen with different style and color
 */
void editorSearch(){
    // Saving Editor properties before searching 
    int saved_Ex = E.x;
    int saved_Ey = E.y;
    int saved_colOffset = E.colOffset;
    int saved_rowOffset = E.rowOffset;

    // Calling prompt function with editorSearchCallBack function as argument
    char* query = prompt("Search: %s (Press ESC 2 Times to Cancel | ARROW KEYS : Navigate | ENTER)", editorSearchCallBack);   

    // If searching successful then free query
    if (query){
        free(query);
    }else{
        // Restoring Editor properties
        E.x = saved_Ex;
        E.y = saved_Ey;
        E.colOffset = saved_colOffset;
        E.rowOffset = saved_rowOffset;
    }
}

/* Helps in Seaching for query in PieceTable and used with prompt function
 * to handle different key pressed like ARROY KEYS etc.
 */
void editorSearchCallBack(char* query, int key){
    static char previous[100];
    static int i = 1;
    // Storing length of query
    E.foundLength = strlen(query);
    // query length = 0 or ESC/ENTER pressed then restore static variables and return
    if(strlen(query) == 0 ||key == '\r' || key == '\x1b'){
        i = 1;
        previous[0] = '\0';
        return;
    }
    // If previous query is not same as current then reset i
    if(strcmp(previous, query) != 0){
        i = 1;
    }
    // FoundAt will contain all lineNo and x values where query is found
    cursorPosition *foundAt = searchInPT(E.PT, query);
    // At index 0 of foundAt we have stored total lenght of array
    if(foundAt[0].lineNo == 1)  E.searchEnable = 0;                 // If length of array 1 means no matched found then display nothing on screen
    else E.searchEnable = 1;

    // if ARROW KEY pressed then jumping to that position of matched query using static int i
    if(key == ARROW_DOWN || key == ARROW_RIGHT){
        if(i < foundAt[0].lineNo - 1)
            i++;
        else
            i = 1;
    }
    if(key == ARROW_UP || key == ARROW_LEFT){
       if(i != 1)
            i--;
        else    
            i = foundAt[0].lineNo - 1;
    }
    // Update cursor to that matched string
    if(i < foundAt[0].lineNo){
        E.y = foundAt[i].lineNo - 1;
        E.x = foundAt[i].col; 
    }
    strcpy(previous, query);
    free(foundAt);
}

/* =============================== Go to Line Number Section =============================== */

/* Function take query which is line number and update cursor position
 * to that line Number
 * Function used with prompt function
 */
void editorGoToLineCallBack(char* query, int key){
    // query length = 0 or ESC/ENTER pressed then return
    if(strlen(query) == 0 ||key == '\r' || key == '\x1b')
        return;
    // Convert string to int
    int lineNumber = atoi(query);
    // If line Number not valid
    if(lineNumber > E.numrows)  return;
    // else update E.y and E.x
    if(lineNumber)
        E.y = lineNumber - 1;
    else    
        E.y = 0;
    E.x = 0;
}

/* Function move cursor to Line given by user in Go TO Line Mode
 */
void editorGoToLine(){
    // Saving Editor properties
    int saved_Ex = E.x;
    int saved_Ey = E.y;
    int saved_colOffset = E.colOffset;
    int saved_rowOffset = E.rowOffset;

    // Calling prompt function with editorGoTOLineCallBack function as argument
    char* query = prompt("Go To Line: %s (Press ESC 2 Times to Cancel | ENTER)", editorGoToLineCallBack);   
    
    if (query){
        free(query);
    }else{
        // Restoring Editor properties
        E.x = saved_Ex;
        E.y = saved_Ey;
        E.colOffset = saved_colOffset;
        E.rowOffset = saved_rowOffset;
    }
}


/*
 *      Function Initializes struct Editor
 */
void initEditor(){
    E.x = E.y = 0;
    E.screencols = E.screenrows = 0;
    E.numrows = 1;
    E.rowOffset = E.colOffset =  0;
    E.filename = NULL;
    E.message[0] = '\0';
    E.message_time = 0;
    addedIndex = -1;
    addedSize = 1000;
    initPieceTable(&E.PT);
    added = (char*)malloc(addedSize * sizeof(char));

    // If error in getting in window size
    if(getWindowSize(&E.screenrows, &E.screencols) == -1)
        returnError("Error in getWindowSize");

    E.screenrows -= 2;
    E.rowLen = NULL;
    E.selectingText = 0;
    E.selectStartRow = 0;
    E.selectStartCol = 0;
    E.searchEnable = 0;
    E.foundLength = 0;
    E.fileChanged = 0;
}