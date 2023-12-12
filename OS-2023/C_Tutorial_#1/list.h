// List.h File written by Morgan Kinne and Nathan Stevenson 8/28/2023

/***                                                        DESCRIPTION
 * Header File for the List Module. This will provide an Interface to the List Module written below, and all code will be included
 * within this .h header file, such that other users can write their own list implementatio that uses our list type and functions
 * Think of this Interface as a library of functions that includes the list itself as well as functions for managing the list,
 * and that this should be separated and could be called from any other program with full functionality by including "list.h"
*/

#include <stdio.h>
#include <stdlib.h>

// Struct for the Individual ListNode
typedef struct list_item { 
    struct list_item *pred, *next;
    void *datum;
} list_item_t;

// Struct for the List as a whole
typedef struct list { //list of nodes
    list_item_t *head, *tail;
    unsigned length;
    // Function pointers
    int (*compare)(const void *key, const void *with); 
    void (*datum_delete)(void *);
} list_t;

// C++ Equivalent of a Constructor. Should be called to ensure no uninitialzed structs
void list_init(list_t *l, int (*compare)(const void *key, const void *with), void (*datum_delete)(void *datum)){
    // Setting up a dummy head and tail value
    list_item_t *dummyHead = malloc(sizeof(list_item_t));
    list_item_t *dummyTail= malloc(sizeof(list_item_t));
    dummyHead->datum = "DH";
    dummyTail->datum = "DT";
    dummyHead->pred = NULL;
    dummyHead->next = dummyTail;
    dummyTail->pred = dummyHead;
    dummyTail->next = NULL;
    
    l->head = dummyHead;
    l->tail = dummyTail;

    l->length = 0;
    // Function's name without () evaluates to its address so we can initialize the list's compare and delete methods
    // with those passed in as arguments to the list_init function
    l->compare = compare;
    l->datum_delete = datum_delete;
}

// This takes a pointer to a function, visitor, and calls it on each member of l from head to tail
// Use this to print and debug the list, which means visitor needs to be the addr of a function that displays the ListNode contents (we need to write this helper function)
void list_visit_items(list_t *l, void (*visitor)(void *v)){
    // start at the head and call visitor on each member of l 
    list_item_t *iterator;
    iterator = l->head->next;
    while(iterator != l->tail){
        visitor(iterator);
        iterator = iterator->next;
    }
}

// Helper function to print the listNode values for calling list_visit_items
// Intent is for the list_visit_items function to be totally generic and the visitor function being passed should cast
// to the desired argument, and then for different functionality you must write a new visitor function
void print_list(void *listNode){
    printf("%s", ((list_item_t*)listNode)->datum);
}

// Insert Tail Functionality (if 2nd keyword == "tail"). Read the input from the file and add the input to the list
// after the input has been added we should call list_visit_items to print the list
void list_insert_tail(list_t *l, void *v){
    // creating a new list node
    list_item_t *temp = malloc(sizeof(list_item_t));
    temp->datum = v;
    // setting the new nodes predecessor to be the tails old pred
    temp->pred = l->tail->pred;
    // setting the temp next to be tail
    temp->next = l->tail;
    // setting the tail's predecessor's next node to be the new one we insert
    l->tail->pred->next = temp;
    // setting tails pred to be the new node
    l->tail->pred = temp;
    // increase length
    l->length++;
}

// List Remove Head Functionality (if 2nd keyword == "tail-remove"). 
void list_remove_head(list_t *l){
    if (l->length > 0){ // if list not empty
        list_item_t *removedNode = malloc(sizeof(list_item_t));
        removedNode = l->head->next;
        l->head->next = removedNode->next; // shift had down the list one
        removedNode->next->pred = l->head;
        free(removedNode);
        l->length--;
    } // else do nothing, list is empty (void so no returns)
}
