#include<stdio.h>
#include<stdlib.h>
#include<ctype.h>
#include<unistd.h>
#include<errno.h>
#include<termio.h>
struct termios terminal_orig;

void returnError(char* s){
    perror(s);
    exit(1);
}
// Disabling Raw Mode
void rawModeOFF(){
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &terminal_orig);
}
// Enabling Raw Mode
void rawModeON(){  

    // Getting terminal attributes 
    if(tcgetattr(STDIN_FILENO, &terminal_orig) == -1) returnError("Error in tcgetattr");
    // Calling rawModeOFF function on Exit
    atexit(rawModeOFF);
    struct termios raw_terminal = terminal_orig;
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

int main(){
    rawModeON();
    // Read returns 0 on reading nothing(indicate end of file)
    while(1){
        char c;
        if(read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) returnError("Error in Read");
        // IF control character
        if(iscntrl(c))
            printf("%d\r\n", c);
        else    
            printf("%d %c\r\n", c ,c);
        if(c == CTRL_KEY('q'))    break;
    }
    return 0;
}