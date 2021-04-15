#ifndef PIECETABLE_H_INCLUDED
#define PIECETABLE_H_INCLUDED

// extern variables to extend visibility
extern char* added;
extern char* original;
extern int addedIndex;

// Struct for piece node
typedef struct pieceNode{
    int start;                  // Starting Index of Content in Node of Original or Added
    int end;                    // Last Index of Content in Node of  Original or Added
    int lineCount;              // Number of Lines in Current Piece Node
    int *lineBreak;             // Stores Offset(int) from where Newline Starts in CurrNode
    char* buffer;               // Char Pointer Pointing either Added or Original
    int bufferType;             // Original(1)    Or    Added(0)
    struct pieceNode* next;     // Pointing to Next Piece Node
    struct pieceNode* prev;     // Pointing to Prev Piece Node
}pieceNode;

// ADT for PieceTable
typedef struct pieceTable{
    pieceNode* head;            // Pointing to Head Piece Node
    pieceNode* tail;            // Pointing to Tail Piece Node
}pieceTable;

// Special ADT to return cursorPosition
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

void insertLineAt(pieceTable PT, char* line,int lineLen, int lineNo, int position);

char* copyLine(pieceTable PT, int lineNo);

char* copyLineFrom(pieceTable PT, int startLine, int startCol,int copiedSize);

int writeToFile(pieceTable PT, FILE* fp);

int getIndexInNode(pieceTable PT, int lineNo, int position, int* count);

cursorPosition* searchInPT(pieceTable PT, char* str);

void printPieceTable(pieceTable PT, FILE* fp);

#endif