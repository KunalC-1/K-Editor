#include<stdio.h>
#include<stdlib.h>
#include "gui.h"
int main(int argc, char* argv[]){
    // Enabling Raw Mode
    rawModeON();

    // Initialize Editor
    initEditor();

    // If file name provided open file and update PieceTable
    if(argc >= 2)
        openEditor(argv[1]);
        
    // Set Status Message
    setStatusMessage("HELP: Ctrl-Q = Quit | Ctrl-S = Save | Ctrl-H = Help");
    
    // While loop to process keys and refreshscreen
    while(1){
        refreshScreen();
        processKeyInput();        
    }
    return 0;
}