#ifndef PIECETABLE_H_INCLUDED
#define PIECETABLE_H_INCLUDED


extern char* added;
extern char* original;
extern int addedIndex;

// Struct for piece node
typedef struct pieceNode{
    int start;
    int end;
    int lineCount;
    int *lineBreak;
    char* buffer;
    int bufferType;
    struct pieceNode* next;
    struct pieceNode* prev;
}pieceNode;
// typedef pieceNode* piecePointer;

typedef struct pieceTable{
    pieceNode* head;
    pieceNode* tail;
}pieceTable;
typedef struct cursorPosition{
    int lineNo;
    int col;
}cursorPosition;


void initPieceTable(pieceTable* PT);
pieceNode* newPieceNode(char* buffer, int start, int end, int bufferType);
void splitNodeForInsert(pieceNode* node, int splitIndex, int midNodelen);
void splitNodeForDelete(pieceNode* node, int splitIndex);
void insertCharAt(pieceTable PT, int lineNo, int position);
void deleteCharAt(pieceTable PT, int lineNo, int position);

int writeToFile(pieceTable PT, FILE* fp);
void printPieceTable(pieceTable PT, FILE* fp);
char* copyLine(pieceTable PT, int lineNo);
void reverseArray(int *arr, int size);
int getIndexInNode(pieceTable PT, int lineNo, int position, int* count);
void insertLineAt(pieceTable PT, char* line,int lineLen, int lineNo, int position);
char* copyLineFrom(pieceTable PT, int startLine, int startCol,int copiedSize);
cursorPosition* searchInPT(pieceTable PT, char* str);
int* computeLPSArray(char* pat, int patternSize);
#endif