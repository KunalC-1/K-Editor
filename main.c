#include<stdio.h>
#include<stdlib.h>
#include "piecetable.h"

char* original = NULL;
char* added = NULL;
int addedIndex = -1;
int main(int argc, char* argv[]){
    FILE *fp = fopen(argv[1], "rb");
    if(!fp){
        printf("hello\n");
        exit(-1);
    }     
    fseek(fp, 0, SEEK_END);
    long fileSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    // printf("%ld\n", fileSize);
    
    if(fileSize != 0){
        original =(char*) malloc(fileSize + 1);
        fread(original, 1, fileSize, fp);
    }else{
        original = NULL;
    }
    fclose(fp);

    // printf("%s", original);
    pieceTable PT;
    initPieceTable(&PT);
    if(original){
        pieceNode* newNode = newPieceNode(original, 0, fileSize - 1, 0);

        // printf("%d %d\n", PT.head->start,PT.head->end);
        // printf("%d %d\n", PT.tail->start, PT.tail->end);

        newNode->next = (PT.head)->next;
        newNode->prev = PT.head;

        (PT.tail)->prev = newNode;
        (PT.head)->next = newNode;

    }
    added = (char* )malloc(sizeof(char) * 100);

    // printf("\nInsertion 1");
    // added[++addedIndex] = 'c';
    // insertCharAt(PT, 3, 1);
    // printPieceTable(PT);


    // printf("\nInsertion 2");
    // added[++addedIndex] = 'A';
    // insertCharAt(PT, 1, 0);
    // printPieceTable(PT);
    
    // printf("Insertion 3");
    // added[++addedIndex] = 'K';
    // insertCharAt(PT, 1, 4);
    // printPieceTable(PT);
        
    // printf("\nDeletion 1");
    // deleteCharAt(PT, 1, 13);
    // printPieceTable(PT);
    
    // printf("\nDeletion 2");
    // deleteCharAt(PT, 1, 0);
    // printPieceTable(PT);
    
    // printf("\nDeletion 3");
    // deleteCharAt(PT, 1, 1);
    // printPieceTable(PT);


    // printf("\nInsertion 4");
    // added[++addedIndex] = '\n';
    // insertCharAt(PT, 4, 65);
    // printPieceTable(PT);

    // printf("\nInsertion 5");
    // added[++addedIndex] = 'L';
    // insertCharAt(PT, 5, 0);
    // printPieceTable(PT);
    
    

    // writeToFile(PT, fp);
    // fclose(fp);
    int choice, lineNo, position, flag = 1; 
    char character;
    while(flag){
        getc(stdin);
        
        printf("1. Insert Character.\n");
        printf("2. Delete Character.\n");
        printf("3. Display Piece Table.\n");
        printf("Enter Choice :\n");
        scanf("%d", &choice);

        if(choice > 3){
            break;
        }
        if(choice != 3){
            printf("Enter Line Number and Position : ");
            scanf("%d%d", &lineNo, &position);
        }getc(stdin);
        switch (choice){
        case 1:
            printf("Character to Insert : ");
            scanf("%c%*c", &character);
            added[++addedIndex] = character;
            insertCharAt(PT, lineNo, position);
            printPieceTable(PT);
            break;
        case 2:
            deleteCharAt(PT,lineNo, position);
            printPieceTable(PT);
            break;
        case 3:
            printPieceTable(PT);
            break;
        }
        fp = fopen(argv[1], "w");
        writeToFile(PT,fp);
        fclose(fp);
    }
    return 0;
}