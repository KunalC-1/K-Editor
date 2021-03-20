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


void initPieceTable(pieceTable* PT);
pieceNode* newPieceNode(char* buffer, int start, int end, int bufferType);
void splitNodeForInsert(pieceNode* node, int splitIndex);
void splitNodeForDelete(pieceNode* node, int splitIndex);
void insertCharAt(pieceTable PT, int lineNo, int position);
void deleteCharAt(pieceTable PT, int lineNo, int position);

void writeToFile(pieceTable PT, FILE* fp);
void printPieceTable(pieceTable PT);

void reverseArray(int *arr, int size);
#endif