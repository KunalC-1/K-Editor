#include<stdio.h>
#include<stdlib.h>
#include "piecetable.h"
#include "gui.h"
char* added;
char* original;
int addedIndex;
Editor E;

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
void splitNodeForInsert(pieceNode* node, int splitIndex, int midNodeLen){
    pieceNode* previous = node->prev;
    pieceNode* next = node->next;

    pieceNode* leftPiece = newPieceNode(node->buffer, node->start, node->start + splitIndex - 1, node->bufferType);
    pieceNode* midPiece = newPieceNode(added, addedIndex, addedIndex + midNodeLen - 1, 1);
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
        if(node->buffer[node->start + splitIndex - 1] == '\n'){
            if(E.y > 0) E.y--;
            E.numrows--;
            E.x = E.rowLen[E.y - E.rowOffset] > 1 ? E.rowLen[E.y - E.rowOffset] : 0;
        }
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
            // If it is possible to increment end
            if(currNode->end + 1 == addedIndex){
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
            }else{
                pieceNode* newNode = newPieceNode(added, addedIndex, addedIndex, 1);
                newNode->next = currNode->next;
                currNode->next->prev = newNode;
                currNode->next = newNode;
                newNode->prev = currNode;
            }
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
            splitNodeForInsert(currNode, splitIndex, 1);       
        }    
    }
    else{
        splitIndex = offset + position;
        splitNodeForInsert(currNode, splitIndex, 1);
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
                if(currNode->buffer[currNode->end] == '\n'){
                    if(E.y > 0) E.y--;
                    E.numrows--;
                    E.x = E.rowLen[E.y - E.rowOffset] > 1 ? E.rowLen[E.y - E.rowOffset] : 0;
                }
                currNode->prev->next = currNode->next;
                currNode->next->prev = currNode->prev;
                free(currNode);  // 
            }else{
                if(currNode->buffer[currNode->end] == '\n'){
                    if(E.y > 0) E.y--;
                    E.numrows--;
                    E.x = E.rowLen[E.y - E.rowOffset] > 1 ? E.rowLen[E.y - E.rowOffset] : 0;
                    currNode->lineCount--;
                    currNode->lineBreak = (int*)realloc(currNode->lineBreak, currNode->lineCount * sizeof(int));
                }currNode->end--;
            }
        }
        else if(splitIndex == 1){
            if(currNode->buffer[currNode->start] == '\n'){
                if(E.y > 0) E.y--;
                E.numrows--;
                E.x = E.rowLen[E.y - E.rowOffset] > 1 ? E.rowLen[E.y - E.rowOffset] : 0;
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
                if(E.y > 0) E.y--;
                E.numrows--;
                E.x = E.rowLen[E.y - E.rowOffset] > 1 ? E.rowLen[E.y - E.rowOffset] : 0;
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

int writeToFile(pieceTable PT, FILE *fp){
    pieceNode* currNode = PT.head;
    int fileSize = 0;
    while(currNode != PT.tail){
        fprintf(fp, "%.*s", currNode->end - currNode->start + 1, currNode->start + currNode->buffer);
        fileSize += currNode->end - currNode->start + 1;
        // fwrite(currNode->buffer + currNode->start, currNode->end - currNode->start + 1, 1, fp);
        currNode = currNode->next;
    }
    return fileSize;
}

void printPieceTable(pieceTable PT, FILE* fp){
    pieceNode* currNode = PT.head->next;
    int count = 0;
    while(currNode != PT.tail){
        count++;
        fprintf(fp,"\n--------------------------------------------------------\n");
        fprintf(fp, "Node %d: \n%.*s\n", count, currNode->end - currNode->start + 1, currNode->start + currNode->buffer);
        fprintf(fp, "Buffer Type : %d\nLine Count : %d\n", currNode->bufferType, currNode->lineCount);
        fprintf(fp, "Start : %d\nEnd : %d\n", currNode->start, currNode->end);
        fprintf(fp,"\n--------------------------------------------------------\n");
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

char* copyLine(pieceTable PT, int lineNo){
    // Initialize Current Line Number as 1
    int currLine = 1;
    pieceNode*  currNode = PT.head->next;

    // Traverse Through the PT until Required Node found
    while(currNode != PT.tail && currLine + currNode->lineCount < lineNo){
        currLine += currNode->lineCount;
        currNode = currNode->next;
    }
    // Offset will store starting index of the required line
    int offset = 0;
    // calculate offset from where line starts
        for(int i = 0; i < lineNo - currLine; i++)
            offset += currNode->lineBreak[i];

    // position index in copiedline array
    int index = 0;
    int size = 100;
    char* copiedLine = (char*)malloc(size);
    while(currNode != PT.tail){
        for(int i = currNode->start + offset; i <= currNode->end; i++){
            // if all rows apppended into write buffer
            char* buffer = currNode->buffer;
            if(buffer[i] == '\n'){
                copiedLine = realloc(copiedLine, index + 1);
                copiedLine[index] = '\0';
                return copiedLine;
            }
            else{
                if(index == size){
                    size *= 2;
                    copiedLine = realloc(copiedLine, size);
                }
                copiedLine[index++] = buffer[i];
            }
        }
        offset = 0;
        currNode = currNode->next;
    }
    copiedLine = realloc(copiedLine, index + 1);
    copiedLine[index] = '\0';
    return copiedLine;
}

char* copyLineFrom(pieceTable PT, int startLine, int startCol,int copiedSize){
    // Initialize Current Line Number as 1
    int currLine = 1;
    pieceNode*  currNode = PT.head->next;

    // Traverse Through the PT until Required Node found
    while(currNode != PT.tail && currLine + currNode->lineCount < startLine){
        currLine += currNode->lineCount;
        currNode = currNode->next;
    }
    
    // position index in copiedline array
    int index = 0;
    int size = 100;
    char* copiedLine = (char*)malloc(size);

    int offset = 0;
    int countline = startLine - currLine > 0 ? startLine - currLine : 0;
    // calculate offset from where line starts
    for(int i = 0; i < countline; i++)
        offset += currNode->lineBreak[i];

    int offset_from_end = currNode->end - currNode->start + 1 - offset - startCol;
    
    int flag = 0, splitIndex;
    
    // If calculated position not lies in current piecenode then go to nextnode
    while(currNode != PT.tail &&  offset_from_end < 0){ 
        flag = 1;
        currNode = currNode->next;
        if(currNode->lineCount != 0)    
            offset_from_end += currNode->lineBreak[0];
        else
            offset_from_end += currNode->end - currNode->start + 1;
    }
    if(!currNode->lineCount || !flag)
        splitIndex = currNode->end - currNode->start + 1 - offset_from_end;
    else    
        splitIndex = currNode->lineBreak[0] - offset_from_end;

    while(currNode != PT.tail){
        // Splitting Index calculation
        for(int i = currNode->start + splitIndex; i <= currNode->end; i++){
            // if all rows apppended into write buffer
            
            char* buffer = currNode->buffer;
            if(index == size){
                size *= 2;
                copiedLine = realloc(copiedLine, size);
            }
            copiedLine[index++] = buffer[i];
            if(index >= copiedSize) break;
        }
        if(index >= copiedSize) break;
        splitIndex = 0;
        currNode = currNode->next;
    }
    copiedLine = realloc(copiedLine, index + 1);
    copiedLine[index] = '\0';
    return copiedLine;
}
// Function converts provided lineNo and position into position relative to nodes and changes count to required node count
int getIndexInNode(pieceTable PT, int lineNo, int position, int* count){
    // Initialize Current Line Number as 1
    int currLine = 1;
    pieceNode*  currNode = PT.head->next;

    // Traverse Through the PT until Required Node found
    while(currNode != PT.tail && currLine + currNode->lineCount < lineNo){
        currLine += currNode->lineCount;
        currNode = currNode->next;
        (*count)++;
    }
    // Offset will store starting index of the required line
    
    int offset = 0, splitIndex = 0;
    
    // If node contain more lines then offset will change according to required line

    for(int i = 0; i < lineNo - currLine; i++)
        offset += currNode->lineBreak[i];
    

    // If Characters in Line are Not in single piecenode i.e. they are extended into series of nodes
    if(currLine + currNode->lineCount == lineNo){
        
        // This variable contains required insert position in piecenode from ending
        int offset_from_end = currNode->end - currNode->start + 1 - offset - position;
        
        int flag = 0;
        
        // If calculated position not lies in current piecenode then go to nextnode
        while(currNode != PT.tail &&  offset_from_end < 0){ 
            flag = 1;
            currNode = currNode->next;
            (*count)++;
            if(currNode->lineCount != 0)    
                offset_from_end += currNode->lineBreak[0];
            else
                offset_from_end += currNode->end - currNode->start + 1;

        }// Splitting Index calculation
       
        if(!currNode->lineCount || !flag)
            splitIndex = currNode->end - currNode->start + 1 - offset_from_end;
        else    
            splitIndex = currNode->lineBreak[0] - offset_from_end;
    }
    else{
        splitIndex = offset + position;
        
    }
    if(splitIndex == currNode->end - currNode->start + 1){
         splitIndex = 0;
         (*count)++;
    }   
    
    // FILE* fp = fopen("log3.txt", "w");
    // fprintf(fp, "%d ", splitIndex);        
    // fclose(fp);
    return splitIndex;

}


void insertLineAt(pieceTable PT, char* line,int lineLen, int lineNo, int position){
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
        pieceNode* newNode = newPieceNode(added, addedIndex, addedIndex + lineLen - 1, 1);
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
            // If it is possible to increment end
            if(currNode->end + 1 == addedIndex){
                pieceNode* newNode = newPieceNode(added, currNode->start, addedIndex + lineLen - 1, 1);
                newNode->next = currNode->next;
                newNode->prev = currNode->prev;
                currNode->next->prev = newNode;
                currNode->prev->next = newNode;
                free(currNode);
            }else{
                pieceNode* newNode = newPieceNode(added, addedIndex, addedIndex + lineLen - 1, 1);
                newNode->next = currNode->next;
                currNode->next->prev = newNode;
                currNode->next = newNode;
                newNode->prev = currNode;
            }
        }
        // inserting at last but buffer type is original
        else if(offset_from_end == 0){
            pieceNode* newNode = newPieceNode(added, addedIndex, addedIndex + lineLen - 1, 1);
            newNode->next = currNode->next;
            currNode->next->prev = newNode;
            currNode->next = newNode;
            newNode->prev = currNode;
        }
        else{
            splitNodeForInsert(currNode, splitIndex, lineLen);       
        }    
    }
    else{
        splitIndex = offset + position;
        splitNodeForInsert(currNode, splitIndex, lineLen);
    }
}