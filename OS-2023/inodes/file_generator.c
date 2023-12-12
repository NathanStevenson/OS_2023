
// this is a testing suite used to generate files with repeating digits 0-9 to test that are drive is being created/inserted correctly
// to use this code when running the executable specific command given below

// "./a.out <output_filename> <file_size_inKB>" (1 KB is 1024 bytes the size of one block)

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {
    FILE *fp = fopen(argv[1], "w");

    // Check if the file is opened successfully
    if (fp == NULL) {
        printf("Error opening the file.\n");
        return 1; // Return an error code
    }

    char input_chars[10] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};

    int file_size = atoi(argv[2]) * 1024;
    char* output_string = (char*) malloc(sizeof(char)*file_size);

    int input_index = 0;
    for(int i=0; i < file_size; i++){
        // every block change the character we write
        if(i % 1024 == 0){
            input_index++;
        }
        output_string[i] = input_chars[(input_index % 10)];
    }

    fwrite(output_string, 1, file_size, fp);

    // Close the file
    fclose(fp);

    printf("Data written to the file successfully.\n");

    return 0; // Return success
}