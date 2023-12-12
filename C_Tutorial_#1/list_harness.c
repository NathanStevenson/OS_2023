#include "list.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Driver Code to test our List Module Interface written inside list.h
int main(int argc, char* argv[]){
    if(argc != 3){
        printf("Insufficient argument count\n");
        return 1;
    }

    // Intializing Data
    char *filename = (char *)malloc(strlen(argv[1]));
    char *keyword = (char *)malloc(strlen(argv[2]));
    
    strcpy(filename, argv[1]);
    strcpy(keyword, argv[2]);
    
    char* str = malloc(sizeof(char)*42);
    FILE *file_text;
   
    // Make a List object and initialize it. anytime use pointer to a struct must use malloc
    list_t *list = malloc(sizeof(list_t));
    list_init(list, list->compare, list->datum_delete);  // need to be passing aparameter of type int(*) like list.compare() or something like that

    // Reading the File
    file_text = fopen(filename, "r");
    if (file_text == NULL){
        printf("File does not exist\n");
        return 1;
    }

    // checking if file is empty
    // how to tell if this file the file is empty found documentation on: https://stackoverflow.com/questions/13566082/how-to-check-if-a-file-has-content-or-not-using-c
    fseek(file_text, 0, SEEK_END);
    long size = ftell(file_text);
    if (0 == size) {
        printf("<EMPTY>\n");
    }
    // comes back to the beginning of the file
    fseek(file_text, 0, SEEK_SET);

    // KEYWORD CASE: ECHO
    if (strcmp(keyword, "echo") == 0){
        // TODO print <EMPTY> if file is completely empty
        while (fgets(str, 42, file_text) != NULL) { //strcspn(text, "\n")
            if (strcspn(str, "\n") != 0){
                printf("%s", str);
            }
        }
        fclose(file_text);
    }
    // KEYWORD CASE: TAIL
    else if (strcmp(keyword, "tail") == 0){
        while (fgets(str, 42, file_text) != NULL) {
            if (strcspn(str, "\n") != 0){
                char *line_copy = malloc(strlen(str));
                /*** Very Important: if we do not make a copy here all nodes end up pointing to same 'str' buffer and when we 
                * print out the list all the nodes are pointing to the same 'str' buffer which now reads the last line of input bc str gets modified
                * Essentially it will look like we are adding the correct nodes, but all nodes will change to be the new value of string for example:
                * dog, cat, human would become: 1. dog (str="dog")  2. cat, cat  (str="cat")  3. human, human, human  (str="human")
                * 
                * Documentation found for strcpy and why to use it: https://stackoverflow.com/questions/8021904/c-copy-buffer-to-string-is-it-even-possible
                * as per syllabus we can use online sources for basic helper functions not related to the main point of assignment
                */
                strcpy(line_copy, str);
                list_insert_tail(list, line_copy);
            }
        }
        fclose(file_text);
        list_visit_items(list, print_list);
    }
    // KEYWORD CASE: TAIL-REMOVE
    else if (strcmp(keyword, "tail-remove") == 0){
        // reading in the file and insert the values at the end of the tail
        while (fgets(str, 42, file_text) != NULL) {
            if (strcspn(str, "\n") != 0){
                char *line_copy = malloc(strlen(str));
                strcpy(line_copy, str);
                list_insert_tail(list, line_copy);
            }
        }
        fclose(file_text);

        // repeatedly calling list_remove_head to remove three items from the list and then printing
        while(list->length > 0){
            for(int i=0; i < 3; i++){ list_remove_head(list); }
            list_visit_items(list, print_list);
            if(list->length == 0){ printf("EMPTY\n"); }
            printf("---\n");
        }
    }
    else{
        printf("Invalid command\n");
        return 1;
    }
    return 0;
}
