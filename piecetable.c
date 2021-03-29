#include<stdio.h>
#include<stdlib.h>
#include "piecetable.h"
#include "gui.h"
char* added;
char* original;
int addedIndex;


void initPieceTable(pieceTable* PT){
    (*PT).head = newPieceNode(NULL, -1, -2, 0);            // Dummy Node
    (*PT).tail = newPieceNode(NULL, -1, -2, 0);            // Dummy Node
    (*PT).head->next = (*PT).tail;
    (*PT).tail->prev = (*PT).head;
}

pieceNode* newPieceNode(char* buffer, int start, int end, int bufferType){
    // Assigning Values to newly created pieceNode
    pieceNode* newNode = (pieceNode*)malloc(sizeof(pieceNode));
    newNode->buffer = buffer;
    newNode->start = start;
    newNode->end = end;
    newNode->bufferType = bufferType;
    newNode->lineBreak = NULL;
    int count = 0, offset =  1;                 // Offset in LineBreak array specifies from which index newline start
    for(int i = start; i <= end; i++){
        // Newline character occurs then store its offset into an LineBreak Array ( Offset from previous line break )
        if(buffer[i] == '\n'){
            newNode->lineBreak = (int*)realloc(newNode->lineBreak, (count + 1)*sizeof(int));
            newNode->lineBreak[count++] = offset;
            offset = 0;
        }offset++;
    }
    // Storing Count of line starting from 1
    newNode->lineCount = count;
    newNode->next = newNode->prev = NULL;
    return newNode;
}
/* Split Node Function will the split node into three parts provided splitting index :
 *      First Part : Contains Left Side information from the splitting index
 *      Second Part: Contains Information about new text inserted
 *      Third Part : Contains Right Side Information from the splitting index  
 */
void splitNodeForInsert(pieceNode* node, int splitIndex){
    pieceNode* previous = node->prev;
    pieceNode* next = node->next;

    pieceNode* leftPiece = newPieceNode(node->buffer, node->start, node->start + splitIndex - 1, node->bufferType);
    pieceNode* midPiece = newPieceNode(added, addedIndex, addedIndex, 1);
    pieceNode* rightPiece = newPieceNode(node->buffer, node->start + splitIndex, node->end, node->bufferType);
    // Connecting this three pieces
    leftPiece->next = midPiece;
    midPiece->next = rightPiece;
    midPiece->prev = leftPiece;
    rightPiece->prev = midPiece;

    // Connecting above created to the Piece Table
    previous->next = leftPiece;
    leftPiece->prev = previous;

    next->prev = rightPiece;
    rightPiece->next = next;

    free(node);  // Optional

    // Pushing node which was disconnected from Piece Table for undo redo operations
    /* Code */
}
void splitNodeForDelete(pieceNode* node, int splitIndex){
    pieceNode* previous = node->prev;
    pieceNode* next = node->next;
    // If deletion at position 1
    if(splitIndex == 0){
        return;
    }
    else{
        pieceNode* leftPiece = newPieceNode(node->buffer, node->start, node->start + splitIndex - 2, node->bufferType);
        pieceNode* rightPiece = newPieceNode(node->buffer, node->start + splitIndex , node->end, node->bufferType);

        leftPiece->next = rightPiece;
        rightPiece->prev = leftPiece;
        rightPiece->next = next;
        leftPiece->prev = previous;

        previous->next = leftPiece;
        next->prev = rightPiece;
        free(node);        // Optional

    }
}


/*
 *  Function inserts character into Piece Table by one of the two ways :
 *      1) Either by splitting nodes and Inseting New node 
 *      2) Extending Already Existing Node
 */

void insertCharAt(pieceTable PT, int lineNo, int position){
    // Initialize Current Line Number as 1
    int currLine = 1;
    pieceNode*  currNode = PT.head->next;

    // Traverse Through the PT until Required Node found
    while(currNode != PT.tail && currLine + currNode->lineCount < lineNo){
        currLine += currNode->lineCount;
        currNode = currNode->next;
    }
    // Offset will store starting index of the required line
    
    int offset = 0, splitIndex;
    
    // If node contain more lines then offset will change according to required line

    for(int i = 0; i < lineNo - currLine; i++)
        offset += currNode->lineBreak[i];
    
    if(offset == 0 && position == 0){                     // Inseting at start position pf line 1
        pieceNode* newNode = newPieceNode(added, addedIndex, addedIndex, 1);
        newNode->next = currNode;
        newNode->prev = currNode->prev;
        currNode->prev->next = newNode;
        currNode->prev = newNode;
    }

    // If Characters in Line are Not in single piecenode i.e. they are extended into series of nodes
    else if(currLine + currNode->lineCount == lineNo){
        
        // This variable contains required insert position in piecenode from ending
        int offset_from_end = currNode->end - currNode->start + 1 - offset - position;
        
        int flag = 0;
        
        // If calculated position not lies in current piecenode then go to nextnode
        while(currNode != PT.tail &&  offset_from_end < 0){ 
            flag = 1;
            currNode = currNode->next;
            if(currNode->lineCount != 0)    
                offset_from_end += currNode->lineBreak[0];
            else
                offset_from_end += currNode->end - currNode->start + 1;

        }// Splitting Index calculation
        
        if(!currNode->lineCount || !flag)
            splitIndex = currNode->end - currNode->start + 1 - offset_from_end;
        else    
            splitIndex = currNode->lineBreak[0] - offset_from_end;
        
        // If we are inserting in added buffer at last position
        if(currNode->bufferType == 1 && offset_from_end == 0){
            // If newline character inserted
            if(currNode->buffer[addedIndex] == '\n'){
                int lastLineOffset = 0;
                currNode->lineCount++;
                currNode->lineBreak = (int*)realloc(currNode->lineBreak, currNode->lineCount * sizeof(int));
                currNode->end++;
                for(int i = 0; i < currNode->lineCount - 1; i++)    lastLineOffset += currNode->lineBreak[i];
                currNode->lineBreak[currNode->lineCount - 1] = currNode->end - currNode->start + 1 - lastLineOffset;
            }else   
                currNode->end++;
        }
        // inserting at last but buffer type is original
        else if(offset_from_end == 0){
            pieceNode* newNode = newPieceNode(added, addedIndex, addedIndex, 1);
            newNode->next = currNode->next;
            currNode->next->prev = newNode;
            currNode->next = newNode;
            newNode->prev = currNode;
        }
        else{
            splitNodeForInsert(currNode, splitIndex);       
        }    
    }
    else{
        splitIndex = offset + position;
        splitNodeForInsert(currNode, splitIndex);
    }

}
/*  Function Deletes character at position and line number given as parameter
 *  It either 1) Split Node into two parts
 *            2) Decrementing End member or Incrementing Start member in PieceNode Struct
 */
void deleteCharAt(pieceTable PT, int lineNo, int position){
    // Initialize Current Line Number as 1
    int currLine = 1;
    pieceNode*  currNode = PT.head->next;
    // Traverse Through the PT until Required Node found
    while(currNode != PT.tail && currLine + currNode->lineCount < lineNo){
        currLine += currNode->lineCount;
        currNode = currNode->next;
    }
    // Offset will store starting index of the required line
    
    int offset = 0 , splitIndex;
    
    // If node contain more lines then offset will change according to required line

    for(int i = 0; i < lineNo - currLine; i++)
        offset += currNode->lineBreak[i];
    
    // Condition for Line No. 1
    if(offset == 0 && position == 0){
        return;
    }
    // If Characters in Line are Not in single piecenode i.e. they are extended into series of nodes
    else if(currLine + currNode->lineCount == lineNo){
        // This variable contains required insert position in piecenode from ending
        int offset_from_end = currNode->end - currNode->start + 1 - offset - position;
        
        int flag = 0;

        // printf("%d\n", offset_from_end);
        
        // If calculated position not lies in current piecenode then go to nextnode
        while(currNode != PT.tail &&  offset_from_end < 0){ 
            flag = 1;
            currNode = currNode->next;
            if(currNode->lineCount != 0)    
                offset_from_end += currNode->lineBreak[0];
            else
                offset_from_end += currNode->end - currNode->start + 1;

        }// Splitting Index calculation
        if(!currNode->lineCount || !flag)
            splitIndex = currNode->end - currNode->start + 1 - offset_from_end;
        else    
            splitIndex = currNode->lineBreak[0] - offset_from_end;

        // Deleting character from end of node
        if(offset_from_end == 0){
            if(currNode->start - currNode->end == 0){           // Node contain single character
                currNode->prev->next = currNode->next;
                currNode->next->prev = currNode->prev;
                free(currNode);  // 
            }else{
                if(currNode->buffer[currNode->end] == '\n'){
                    currNode->lineCount--;
                    currNode->lineBreak = (int*)realloc(currNode->lineBreak, currNode->lineCount * sizeof(int));
                }currNode->end--;
            }
        }
        else if(splitIndex == 1){
            if(currNode->buffer[currNode->start] == '\n'){
                reverseArray(currNode->lineBreak,currNode->lineCount);
                currNode->lineCount--;
                currNode->lineBreak = (int*)realloc(currNode->lineBreak, currNode->lineCount * sizeof(int));
                reverseArray(currNode->lineBreak, currNode->lineCount);
                
            }else{
                if(currNode->lineCount) currNode->lineBreak[0]--;
                
            }currNode->start++;
        }
        else{
            splitNodeForDelete(currNode, splitIndex);
        }
    }
    else{
        splitIndex = offset + position;
        // SplitIndex 1 should be considered separately
        if(splitIndex == 1){
            if(currNode->buffer[currNode->start] == '\n'){
                reverseArray(currNode->lineBreak,currNode->lineCount);
                currNode->lineCount--;
                currNode->lineBreak = (int*)realloc(currNode->lineBreak, currNode->lineCount * sizeof(int));
                reverseArray(currNode->lineBreak, currNode->lineCount);
                
            }else{
                if(currNode->lineCount) currNode->lineBreak[0]--;  
            }currNode->start++;
        }
        // printf("Split : %d", splitIndex);
        else
            splitNodeForDelete(currNode, splitIndex);
    }
}

void writeToFile(pieceTable PT, FILE *fp){
    pieceNode* currNode = PT.head;
    while(currNode != PT.tail){
        fprintf(fp, "%.*s", currNode->end - currNode->start + 1, currNode->start + currNode->buffer);
        // fwrite(currNode->buffer + currNode->start, currNode->end - currNode->start + 1, 1, fp);
        currNode = currNode->next;
    }
}

void printPieceTable(pieceTable PT){
    pieceNode* currNode = PT.head->next;
    int count = 0;
    while(currNode != PT.tail){
        count++;
        printf("\n--------------------------------------------------------\n");
        fprintf(stdout, "Node %d: \n%.*s\n", count, currNode->end - currNode->start + 1, currNode->start + currNode->buffer);
        fprintf(stdout, "Buffer Type : %d\nLine Count : %d\n", currNode->bufferType, currNode->lineCount);
        fprintf(stdout, "Start : %d\nEnd : %d\n", currNode->start, currNode->end);
        printf("--------------------------------------------------------\n");

        currNode = currNode->next;
    }
}


void reverseArray(int *arr, int size){
    int temp;
    for(int i = 0, j = size - 1; i < j ; i++, j--){
        temp = arr[i];
        arr[i] = arr[j];
        arr[j] = temp;
    }
}