#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include "inode.h"

// pass in the bitmap tracking free blocks as well as the total number of blocks with n
int get_free_block(char* bitmap, int n, int m)
{
    // we want to start and iterate over the blocks from m to n since the first m blocks are dedictaed for only inodes
    int i = m;
    while (i < n){
        // if the bitmap is zero therefore the block is free
        if(*(bitmap+i) == 0){
            // set block to being used and break out
            *(bitmap+i) = 1;
            break;
        }
        i++;
    }
    if(i == n){
        printf("No more free blocks left to allocate!\n");
        exit(-1);
    }
    return i;
}

void write_int(int pos, int val, char* rawdata)
{
  int *ptr = (int *)&rawdata[pos];
  *ptr = val;
}

int read_int(int pos, char* rawdata)
{
  int *ptr = (int *)&rawdata[pos];
  return *ptr;
}


// if there is an inode at the offset in the rawdata return 1, else return 0
// this works for insert because we know which blocks are dedicated for inodes, in extract it seems we do not will need to add more code to check for that
int is_inode(int offset, char* rawdata){
    for(int i=0; i < 25; i+=4){
        int contents = read_int((offset+i), rawdata);
        // inode exists
        if (contents != 0){
            return 1;
        }
    }
    // inode does not exist
    return 0;
}


// 1. Disk Image is N total blocks, first M blocks are dedicated for indoes
// 2. Zeroes out N*1024 bytes, then places a INPUT FILE in disk image using inode that is placed in block D at position I (counting in units of inodes NOT bytes w/in block D) 
// 3. Need to check that D < M (inode is put in an inode part of the disk image) and that the inode will fit in the block if it starts at position I

// Inode have a size of 100 bytes. Each block is 1024 bytes therefore we can store 10 inodes per block (check if I is less than 10). Place in block offset 10*I

void place_inode(int d, int i, int m, char* rawdata, struct inode* ip){
    if(d >= m){
        printf("D >= M. The first M=%d blocks are reserved for inodes, trying to place inode in block=%d NOT ALLOWED!\n", m, d);
        exit(-1);
    }

    if(i >= 10){
        printf("Each inode is 100 bytes and each block is 1024. Each block holds 10 inodes, trying to place your %dth inode NOT ALLOWED!\n", i);
        exit(-1);
    }

    // if make it to here then the inode is allowed to be placed in the location specified
    int offset = d*BLOCK_SZ + i*100;
    char* inode_pos = rawdata + offset;

    // check here that the next 100 bytes (25 integers) are all 0. If so then inode has not been placed here
    int inode_exists = is_inode(offset, rawdata);

    if(inode_exists){
        printf("You are currently attempting to place an inode in a location already filled. NOT ALLOWED!\n");
        exit(-1);
    }

    memcpy(inode_pos, &*ip, sizeof(struct inode));
}


struct inode* extract_inode_from_data(int offset, char* rawdata){
    struct inode *file_inode;
    // at offset in rawdata array we have an inode. need to point file_inode to there to extract
    file_inode = (struct inode*)(rawdata+offset);
    
    return file_inode;
}


// walks an indirect block and populates bitmap
void extract_walk_indirect_block(FILE* outfile, char* rawdata, int block_no, int max_block, struct inode* file_inode) {
    for (int i = 0; i < BLOCK_SZ; i += 4) {
        int data_block_number = read_int((block_no * BLOCK_SZ) + i, rawdata);

        // if the block number is negative or higher than the max block then we know this cannot be a legit inode (filter out the false positives)
        if(data_block_number < 0 || data_block_number > max_block){
            printf("False Positive. Inode does not exist. Data Block Number: %d\n", data_block_number);
            return;
        }
        char* temp = rawdata + (data_block_number*1024);
        
        if(ftell(outfile) + BLOCK_SZ > file_inode->size){
            fwrite(temp, 1, (file_inode->size % BLOCK_SZ), outfile);
        }
        else{
            fwrite(temp, 1, BLOCK_SZ, outfile);
        }
    }
}

void extract_walk_doubly_indirect_block(FILE* outfile, char* rawdata, int block_no, int max_block, struct inode* file_inode) {
    for (int i = 0; i < BLOCK_SZ; i += 4) {
        int indirect_block_number = read_int((block_no * BLOCK_SZ) + i, rawdata);

        // if the block number is negative or higher than the max block then we know this cannot be a legit inode (filter out the false positives)
        if(indirect_block_number < 0 || indirect_block_number > max_block){
            printf("False Positive. Inode does not exist. Indirect Block Number: %d\n", indirect_block_number);
            return;
        }
        
        extract_walk_indirect_block(outfile, rawdata, indirect_block_number, max_block, file_inode);
    }
}

void extract_walk_triply_indirect_block(FILE* outfile, char* rawdata, int block_no, int max_block, struct inode* file_inode) {
    for (int i = 0; i < BLOCK_SZ; i += 4) {
        int doubly_indirect_block_number = read_int((block_no * BLOCK_SZ) + i, rawdata);

        // if the block number is negative or higher than the max block then we know this cannot be a legit inode (filter out the false positives)
        if(doubly_indirect_block_number < 0 || doubly_indirect_block_number > max_block){
            printf("False Positive. Inode does not exist. Doubly Indirect Block Number: %d\n", doubly_indirect_block_number);
            return;
        }

        extract_walk_doubly_indirect_block(outfile, rawdata, doubly_indirect_block_number, max_block, file_inode);
    }
}


// walks an indirect block and populates bitmap
void walk_indirect_block(char* bitmap, int block_no, char* rawdata){
    // set the indirect block to be used in the new bitmap
    bitmap[block_no] = 1;

    // each indirect block holds 256 integers of block numbers being used
    int block_in_bytes = block_no*BLOCK_SZ;

    for(int i=0; i < BLOCK_SZ; i+=4){
        int direct_block_number = read_int((block_in_bytes + i), rawdata);
        // if the indirect block has a value of zero then break out no more needed
        if (direct_block_number == 0){ break; }

        // set bitmap at the direct block to be 1
        bitmap[direct_block_number] = 1;
    }
}


// walks a doubly indirect block and populates the bitmap
void walk_doubly_indirect_block(char* bitmap, int block_no, char* rawdata){
    // set the doubly indirect block to be used in the new bitmap
    bitmap[block_no] = 1;

    // each doubly indirect block holds 256 integers of indirect blocks
    int block_in_bytes = block_no*BLOCK_SZ;

    for(int i=0; i < BLOCK_SZ; i+=4){
        int indirect_block_number = read_int((block_in_bytes + i), rawdata);
        // if the indirect block has a value of zero then break out no more needed
        if (indirect_block_number == 0){ break; }

        // call walk indirect block to appropriately update the bitmap for indirect blocks
        walk_indirect_block(bitmap, indirect_block_number, rawdata);
    }
}


// walks a triply indirect block and populates the bitmap
void walk_triply_indirect_block(char* bitmap, int block_no, char* rawdata){
    // set the triply indirect block to be used in the new bitmap
    bitmap[block_no] = 1;

    // each triply indirect block holds 256 integers of doubly indirect blocks
    int block_in_bytes = block_no*BLOCK_SZ;

    for(int i=0; i < BLOCK_SZ; i+=4){
        int doubly_indirect_block_number = read_int((block_in_bytes + i), rawdata);
        // if the indirect block has a value of zero then break out no more needed
        if (doubly_indirect_block_number == 0){ break; }

        // call walk doubly indirect block to appropriately update the bitmap for doubly indirect blocks
        walk_doubly_indirect_block(bitmap, doubly_indirect_block_number, rawdata);
    }
}


// given a pointer to a file inode and the rawdata of the image, this function will populate the bitmap with blocks that are already being used
void populate_bitmap_inode(struct inode* file_inode, char* bitmap, char* rawdata){
    // walk direct pointer to data blocks
    for(int i=0; i < N_DBLOCKS; i++){
        int block_no = file_inode->dblocks[i];

        // if the block number is 0 then this direct data block never got written to. We can return out of the function as the file is too small
        if (block_no == 0){
            return;
        }
        else{
            bitmap[block_no] = 1;
        }
    }
    // each of the indexes in iblocks is a data block which is an indirect block mark those as used
    for(int i=0; i < N_IBLOCKS; i++){
        int block_no = file_inode->iblocks[i];
        // if the block number is 0 then this direct data block never got written to. We can return out of the function as the file is too small
        if (block_no == 0){
            return;
        }
        else{
            // walk inodes indirect blocks
            walk_indirect_block(bitmap, block_no, rawdata);
        }
    }

    // i2block is a block being used as a doubly indirect 
    if(file_inode->i2block != 0){
        // walk doubly indirect blocks
        walk_doubly_indirect_block(bitmap, file_inode->i2block, rawdata);
    }

    if(file_inode->i3block != 0){
        // walk triply indirect blocks
        walk_triply_indirect_block(bitmap, file_inode->i3block, rawdata);
    }
}    


// walk the inodes for insert where they are located in the first 0-M blocks. Each block has space for 10 inodes
// if there is an inode there walk thru it and populate the bitmap so we can place the file correctly
void walk_inodes_insert(char* rawdata, char* bitmap, int m){
    // iterate thru all the blocks that could possibly hold inodes (i is block #, j is inode pos in block)
    for(int i=0; i < m; i++){
        // check each of the 10 offset that it could be at each block
        for(int j=0; j < 10; j++){
            int offset = (i*BLOCK_SZ) + (j*100);

            // check if there is an inode at that offset
            int inode_exists = is_inode(offset, rawdata);

            // if the inode exists then we are going to extract the inode from rawdata and walk the inode to populate bitmap with used blocks
            if(inode_exists){
                printf("Inode found at block: %d and offset: %d\n", i, j);
                // pointer to the inode will be stored in file_inode (populate bitmap from inode)
                struct inode* file_inode = extract_inode_from_data(offset, rawdata);

                populate_bitmap_inode(file_inode, bitmap, rawdata);    
            }
        }
    }
}

int is_inode_extract(int offset, char* rawdata, int uid, int gid){
    // extract the inode from the data and see if the uid and the gid match
    struct inode* extract_ptr = extract_inode_from_data(offset, rawdata);
    if (extract_ptr->uid == uid && extract_ptr->gid == gid) {
        // if they match then inode might exist, could be a faulty match
        return 1;
    }
    // if they dont match we know for sure an inode does not exist at this location
    else{
        return 0;
    }
}


// file_size is the size of the image drive (poorly named variable)
// file_inode->size has the size we need
void extract_file(struct inode* file_inode, char* rawdata, FILE* fp, int file_count, int file_size){
    // get the highest possible block we can have
    int max_block = file_size / BLOCK_SZ;

    // walk direct pointer to data blocks
    for(int i=0; i < N_DBLOCKS; i++){
        int block_no = file_inode->dblocks[i];
        
        // if the block number is negative or higher than the max block then we know this cannot be a legit inode (filter out the false positives)
        if(block_no < 0 || block_no > max_block){
            printf("False Positive. Inode does not exist. Block Number: %d\n", block_no);
            return;
        }

        char* temp = rawdata + (block_no*1024);

        // if ftell(fp) + BLOCK_SZ over shoots the file size defined by the inode then we only write to file_size % inode
        if(ftell(fp) + BLOCK_SZ > file_inode->size){
            fwrite(temp, 1, (file_inode->size % BLOCK_SZ), fp);
        }
        else{
            fwrite(temp, 1, BLOCK_SZ, fp);
        }
    }

    //now we need to walk the indirect blocks
    for(int i=0; i < N_IBLOCKS; i++){
        int block_no = file_inode->iblocks[i];

        // if the block number is negative or higher than the max block then we know this cannot be a legit inode (filter out the false positives)
        if(block_no < 0 || block_no > max_block){
            printf("False Positive. Inode does not exist. Indirect Block Number 2: %d\n", block_no);
            return;
        }
        extract_walk_indirect_block(fp, rawdata, block_no, max_block, file_inode);
    }

    //we need to walk the doubly indirect blocks 
    if(file_inode->i2block >= 0 && file_inode->i2block <= max_block){
        extract_walk_doubly_indirect_block(fp, rawdata, file_inode->i2block, max_block, file_inode);
    }

    //now we need to walk the triply indirect blocks
    if(file_inode->i3block >= 0 && file_inode->i3block <= max_block){
        extract_walk_triply_indirect_block(fp, rawdata, file_inode->i3block, max_block, file_inode);
    }
}


int is_block_empty(int offset, char* rawdata, char* bitmap){
    // iterate thru the block starting at the offset and return 1 if empty, 0 otherwise
    for(int i=0; i < 256; i+=4){
        // offset is the start of the block and we are gonna look at the values
        int value = read_int((offset+i), rawdata);
        // if the value does not equal 0 then we know that this block is being used
        if(value != 0){
            // offset is equal to block number (i) times BLOCK_SZ (populates the bitmap as we walk thru the drive - tracks unused blocks)
            bitmap[(offset / BLOCK_SZ)] = 1;
            return 0;
        }
    }
    // if all of the values return 0 then this block is empty and return 1, block emty therefore bitmap is 0
    bitmap[(offset / BLOCK_SZ)] = 0;
    return 1;
}


// get the unused blocks and print them out into the UNUSED_BLOCKS file inside the path directory specified
void get_unused_blocks(char* bitmap, int number_blocks, char* output_path){
    // the file name will be the output_path + "\UNUSED_BLOCKS"
    char* file_name = (char*)malloc(strlen(output_path)+15);

    // file_name length can handle output_path + \UNUSED_BLOCKS, copy output path to the start and then append UNUSED BLOCKS
    strcpy(file_name, output_path);
    strcat(file_name, "/UNUSED_BLOCKS");
    
    FILE* unused_blocks = fopen(file_name, "w");
    // Go through the bitmap to find unused blocks
    for(int i = 0; i < number_blocks; i++){
        // if bitmap is 0 then the block is unused
        if(bitmap[i] == 0){
            fprintf(unused_blocks, "%d\n", i);
        }
    }
    fclose(unused_blocks);
}


// same overall logic as walk_inodes_insert() but instead of populating bitmap we are going to extract the files from the drive and write them to the path
void walk_inodes_extract(char* rawdata, char* output_path, int file_size, int uid, int gid, char* bitmap){
    // get the highest possible block we can have
    int max_block = file_size / BLOCK_SZ;

    // keep track of which file we are extracting at this point for export purposes
    int file_count = 0;

    // go thru the drive stepping thru each block. checking if the block is empty. update bitmap accordingly, if non-empty check for inodes
    for(int i=0; i < max_block; i++){
        // i*block_sz is the block position offset we are at in the rawdata
        int block_empty = is_block_empty((i*BLOCK_SZ), rawdata, bitmap);

        // if the block is not empty then check for inodes via pattern matching (step thru every 100 bytes of block)
        if (!block_empty){
            for(int j=0; j < 10; j++){
                // offset will be i*BLOCK_SZ + j*100
                int offset = i*BLOCK_SZ + j*100;
                // check if this may be a real inode (if it returns 0 then it definitely is not, if 1 then it probably is but we need to ensure its not random chance and garbage)
                int inode_exists = is_inode_extract(offset, rawdata, uid, gid);

                // if the inode exists then we are going to walk the inode and gather the data from the data blocks to write to the corresponding file
                if(inode_exists){
                    file_count += 1;
                    // pointer to the inode will be stored in file_inode
                    struct inode* file_inode = extract_inode_from_data(offset, rawdata);

                    // first set up the file names and directory locations
                    char str[5];
                    // sprintf converts file count into a string
                    sprintf(str, "%d", file_count);
                    // file num is a string that will store "file{number}" (filenum combined string of str "\file" and str)
                    char* file_num = (char*)malloc(10);    
                    // file_name will store combined string of output_path and filenum
                    char* file_name = (char*)malloc(strlen(output_path)+10);
                    // copy the first string into the combined string and then concat second string to combine (dest, src)
                    strcpy(file_num, "/file");
                    strcat(file_num, str);
                    strcpy(file_name, output_path);
                    strcat(file_name, file_num);

                    // create the file
                    FILE* fp;
                    fp = fopen(file_name, "a");

                    // walk the inode and extract the file w a new extract method
                    extract_file(file_inode, rawdata, fp, file_count, file_size);

                    // see where fp is in the file and then close it 
                    fclose(fp);

                    // need to create an extracted file size variable
                    printf("File found at inode in block %d, file size %d\n", i, file_inode->size); //instead of file size we need to print the output_file size);
                }
            }
        }
    }
}


int place_indirect_block(FILE* fpr, char* rawdata, char* buf, char* bitmap, int n, int m, int num_indirect, struct inode* ip, int* nbytes, int file_size){
    // have to get one block to hold pointers to data blocks
    int indirect_blockno = get_free_block(bitmap, n, m);
    printf("Indirect Block placed at block: %d\n", indirect_blockno);

    // if num_indirect is 21 then this function is being called from doubly indirect block we do not want to set this value
    if(num_indirect != 21){
        ip->iblocks[num_indirect] = indirect_blockno;
    }

    int indirect_block_offset = 0;
    // fill up the indirect block (1024KB) each int is 4B therefore holds 256 block # (indirect stores 256KB)
    // or when we have read the entire file
    while(indirect_block_offset < 256 && *nbytes < file_size){
        int start = ftell(fpr);
        // position in the indirect block where the block # for the data block will be written
        int indirect_offset = (indirect_blockno * BLOCK_SZ) + (indirect_block_offset * 4);

        // get the value for the data block and store it in the indirect block at the offset
        int data_blockno = get_free_block(bitmap, n, m);
        write_int(indirect_offset, data_blockno, rawdata);

        // read next 1024 bytes of the file into the buffer
        size_t num_bytes_read = fread(buf, sizeof(char), BLOCK_SZ, fpr);

        // increment the number of bytes read
        *nbytes += num_bytes_read;
        
        // copy the buffer contents into memory at the appropriate block
        char* inode_pos = rawdata + (data_blockno*1024);
        memcpy(inode_pos, buf, 1024);

        indirect_block_offset++;

        // write which parts of the file you are reading into each block for debugging purposes
        int end = ftell(fpr);
        printf("Writing blocks %d - %d to block %d\n", start, end, data_blockno);

        if(*nbytes >= file_size){
            return indirect_blockno;
        }
    }

    // if we are calling from doubly indirect return the indirect blockno after setting up the indirect block to store in the doubly indirect
    printf("Number Indirect: %d\n", num_indirect);
    if(num_indirect == 21){
        return indirect_blockno;
    }
    else{
        return 0;
    }
}

// doubly indirect needs to grab its own data block and then it will point to up to 256 more indirect blocks
int place_doubly_indirect_block(FILE* fpr, char* rawdata, char* buf, char* bitmap, int n, int m, struct inode* ip, int* nbytes, int file_size, int triple_flag){
    // have to get one block to hold pointers to data blocks
    int doubly_indirect_blockno = get_free_block(bitmap, n, m);
    printf("Doubly Indirect Block placed at block: %d\n", doubly_indirect_blockno);
    
    if(triple_flag == 0){
        ip->i2block = doubly_indirect_blockno;
    }

    int doubly_indirect_block_offset = 0;
    // fill up the doubly indirect block (1024KB) each int is 4B therefore holds 256 indirect block # (indirect stores 256KB)
    // or when we have read the entire file
    while(doubly_indirect_block_offset < 256 && *nbytes < file_size){
        // position in the doubly indirect block where the block # for the data block will be written
        int doubly_indirect_offset = (doubly_indirect_blockno * BLOCK_SZ) + (doubly_indirect_block_offset * 4);

        // place the indirect block and attach it to data blocks
        int indirect_blockno = place_indirect_block(fpr, rawdata, buf, bitmap, n, m, 21, ip, nbytes, file_size);

        // map the doubly indirect index to the indirect block and write it to rawdata
        write_int(doubly_indirect_offset, indirect_blockno, rawdata);

        // increment the offset
        doubly_indirect_block_offset++;

        if(*nbytes >= file_size){
            return doubly_indirect_blockno;
        }
    }
    
    // if this function is being called from the triple block
    if (triple_flag == 1){
        return doubly_indirect_blockno;
    }else{
        return 0;
    }
}


// place the final triply indirect block when creating/inserting a file into the drive
void place_triply_indirect_block(FILE* fpr, char* rawdata, char* buf, char* bitmap, int n, int m, struct inode* ip, int* nbytes, int file_size){
    // have to get one block to hold pointers to data blocks
    int triply_indirect_blockno = get_free_block(bitmap, n, m);
    printf("Triply Indirect Block placed at block: %d\n", triply_indirect_blockno);
    
    ip->i3block = triply_indirect_blockno;
    int triply_indirect_block_offset = 0;
    // fill up the doubly indirect block (1024KB) each int is 4B therefore holds 256 indirect block # (indirect stores 256KB)
    // or when we have read the entire file
    while(triply_indirect_block_offset < 256 && *nbytes < file_size){
        // position in the doubly indirect block where the block # for the data block will be written
        int triply_indirect_offset = (triply_indirect_blockno * BLOCK_SZ) + (triply_indirect_block_offset * 4);

        // place the indirect block and attach it to data blocks
        int doubly_indirect_blockno = place_doubly_indirect_block(fpr, rawdata, buf, bitmap, n, m, ip, nbytes, file_size, 1);

        // map the doubly indirect index to the indirect block and write it to rawdata
        write_int(triply_indirect_offset, doubly_indirect_blockno, rawdata);

        // increment the offset
        triply_indirect_block_offset++;

        if(*nbytes >= file_size){
            return;
        }
    }

}

void place_file(char *file, int uid, int gid, int d, int i, int m, char* rawdata, char* bitmap, int n){
    
    int iterator, nbytes = 0;
    int i2block_index, i3block_index;
    struct inode *ip = (struct inode*)malloc(sizeof(struct inode));
    FILE *fpr;
    unsigned char buf[BLOCK_SZ];

    ip->mode = 0;
    ip->nlink = 1;
    ip->uid = uid;
    ip->gid = gid;
    ip->ctime = random();
    ip->mtime = random();
    ip->atime = random();

    // open the specific file that was passed to be inserted
    fpr = fopen(file, "rb");
    if (!fpr) {
        printf("File not found\n");
        exit(-1);
    }

    // get the total size of the file which needs to be read
    fseek(fpr, 0, SEEK_END);
    long file_size = ftell(fpr); 
    fseek(fpr, 0, SEEK_SET);
    
    // data blocks are each 1KB and there are 12 direct pointers (fill in first 12KB of file into data blocks)
    for (iterator = 0; iterator < N_DBLOCKS; iterator++) {
        // get the start of where the file is
        int start = ftell(fpr);
        // get free block number
        int blockno = get_free_block(bitmap, n, m);
        ip->dblocks[iterator] = blockno;
        // each block is 1024 bytes (get blockno and multiply by 1024 to get offset to write to in rawdata)
        int offset = blockno * 1024;
        char* inode_pos = rawdata + offset;

        // read the ith 1024 byte chunk from the binary file into the buffer (if nothing read in then file is done being read)
        size_t num_bytes_read = fread(buf, sizeof(char), BLOCK_SZ, fpr);
        
        // increment the nbytes read by num_bytes read from the file
        nbytes += num_bytes_read;

        // copy the buffer contents into memory at the appropriate blocks
        memcpy(inode_pos, buf, 1024);

        // get where fp is after the reading and print out which bytes it is reading into each block for debugging
        int end = ftell(fpr);
        printf("Writing blocks %d - %d to block %d\n", start, end, blockno);

        // if we have read all of the file then break out of it
        if(nbytes >= file_size){
            break;
        }
    }

    // fill in here if IBLOCKS needed
    // if so, you will first need to get an empty block to use for your IBLOCK

    // // keep repeating these steps until indirect blocks are full
    int num_indirect = 0;
    while (num_indirect < 3 && nbytes < file_size){
        place_indirect_block(fpr, rawdata, buf, bitmap, n, m, num_indirect, ip, &nbytes, file_size);
        num_indirect++;
    }

    // one doubly indirect block if needed
    if (nbytes < file_size){
        place_doubly_indirect_block(fpr, rawdata, buf, bitmap, n, m, ip, &nbytes, file_size, 0);
    }

    // one triply indirect block if needed
    if (nbytes < file_size){
        place_triply_indirect_block(fpr, rawdata, buf, bitmap, n, m, ip, &nbytes, file_size);
    }

    // if after all of this nbytes is still less then file size our drive cannot support that big of a file
    if(nbytes < file_size){
        printf("The file you supplied is too large for one inode to handle it. System cannot support this file!\n");
        exit(-1);
    }

    ip->size = nbytes;  // total number of data bytes written for file
    printf("successfully wrote %d bytes of file %s\n", nbytes, file);
    
    // once Inode is written we need to place it in the correct place in rawdata
    place_inode(d, i, m, rawdata, ip);
}



// Phase 1 need to support creating a new disk image
// COMMAND SYNTAX: disk_image -create -image IMAGE_FILE -nblocks N -iblocks M -inputfile FILE -u UID -g GID -block D -inodepos I
// number of arguments (17)

// Phase 2 need to extend phase 1 to insert new file into a given disk image
// COMMAND SYNTAX: disk_image -insert -image IMAGE_FILE -nblocks N -iblocks M -inputfile FILE -u UID -g GID -block D -inodepos I
// number of arguments (17)

// Phase 3 need to add new functionality to reconstruct any files found in given disk image
// COMMAND SYNTAX: disk_image -extract -image IMAGE_FILE -u UID -g GID -o PATH
// number of arguments (9)


void main(int argc, char* argv[]){ 
    //                                  ADDED ARGUMENT HANDLING FOR ALL OF THE TASKS
    if(argc < 10){
        printf("Malformed Command. Exiting!\n");
        exit(-1);
    }
    // determine which task the user is attempting to perform (create, insert, extract)
    // in all tasks IMAGE_FILE is at CLP number 3
    char* output_filename = argv[3];
    // declare and instantiate all of the potential integer values to 0
    int n = 0, m = 0, uid = 0, gid = 0, d = 0, i = 0;
    // declare all of the string values to empty
    char* input_filename;
    char* path;

    // if it is an extract task there will only be 9 command line arguments
    if (strcmp(argv[1], "-extract") == 0){
        uid  = atoi(argv[5]);
        gid  = atoi(argv[7]);
        path = argv[9];
    }
    // if it is either insert or create the number of command line arguments is going to be 17
    else if(strcmp(argv[1], "-insert") == 0 || strcmp(argv[1], "-create") == 0){
        n              = atoi(argv[5]);
        m              = atoi(argv[7]);
        input_filename = argv[9];
        uid            = atoi(argv[11]);
        gid            = atoi(argv[13]);
        d              = atoi(argv[15]);
        i              = atoi(argv[17]);
    }
    // if it is not one of the expected output an error msg
    else{
        printf("Task must be either -create, -insert, or -extract! You entered: %s\n", argv[1]);
        exit(-1);
    }

    // CREATE IS DONE AND HAS BEEN TESTED ON TEST SUITE

    // if the command is create this code will create the drive image
    if(strcmp(argv[1], "-create") == 0){
        // raw data contains exact byte contents of the disk image we are creating / using
        char* rawdata = (char*)malloc((n*BLOCK_SZ)*sizeof(char));
        // bitmap is an array of bytes that is equivalent to the # of total blocks. If bitmap[i] == 1, then ith block in use. Else it is free
        char* bitmap = (char*)malloc((n)*sizeof(char));

        // first m blocks are dedicated for inodes therefore mark them as blocks in use
        // set these to one before calling place file so that it knows
        char* temp = bitmap;
        for(int i=0; i < m; i++){
            *(temp + i) = 1;
        }

        int check;
        FILE *outfile;

        outfile = fopen(output_filename, "wb");
        if (!outfile) {
            perror("datafile open");
            exit(-1);
        }

        // fill in here to place file (after file is placed write the inode to the desired place in memory)
        place_file(input_filename, uid, gid, d, i, m, rawdata, bitmap, n);

        check = fwrite(rawdata, 1, n*BLOCK_SZ, outfile);
        if (check != n*BLOCK_SZ) {
            perror("fwrite");
            exit(-1);
        }

        check = fclose(outfile);
        if (check) {
            perror("datafile close");
            exit(-1);
        }

        printf("Done.\n");
        return;
    }

    // fill in this block to handle the insert logic
    // read a disk image output_filename, with n total blocks, first m are inodes, to place a file with UID and GID,
    // new files inode placed in block D at position I where I is counting in inodes (check D < M, I < 10, and inode position not already used)
    else if(strcmp(argv[1], "-insert") == 0){
        // raw data contains exact byte contents of the disk image we are creating / using
        char* rawdata = (char*)malloc((n*BLOCK_SZ)*sizeof(char));
        // bitmap is an array of bytes that is equivalent to the # of total blocks. If bitmap[i] == 1, then ith block in use. Else it is free
        char* bitmap = (char*)malloc((n)*sizeof(char));
        int check;
        FILE *outfile;

        // read the output_filename (the image drive passed in into rawdata)
        FILE* fp = fopen(output_filename, "rb");
        fread(rawdata, 1, (n*BLOCK_SZ), fp);
        fclose(fp);
        
        // we need to walk thru the first M blocks detecting inodes so we can go thru the inodes and populate bitmap with the filled in blocks so
        // get_free_block() does not allocate blocks that were being used by a different file to the new one we are creating
        walk_inodes_insert(rawdata, bitmap, m);

        // place_file will place the file appropriately as well as write the inode to the specific place
        place_file(input_filename, uid, gid, d, i, m, rawdata, bitmap, n);

        // open the output file we will write to
        outfile = fopen(output_filename, "wb");
        if (!outfile) {
            perror("datafile open");
            exit(-1);
        }

        check = fwrite(rawdata, 1, n*BLOCK_SZ, outfile);
        if (check != n*BLOCK_SZ) {
            perror("fwrite");
            exit(-1);
        }

        check = fclose(outfile);
        if (check) {
            perror("datafile close");
            exit(-1);
        }
        printf("Done.\n");
        return;
    }

    // fill in this block to handle the extract logic
    else if(strcmp(argv[1], "-extract") == 0){
        // open the image drive the user passed in thru command line arguments
        FILE* fp = fopen(output_filename, "rb");

        // determine how long the drive is
        fseek(fp, 0, SEEK_END);
        long file_size = ftell(fp); 
        fseek(fp, 0, SEEK_SET);
        
        // read the drive file into rawdata
        char* rawdata = (char*)malloc(file_size*sizeof(char));
        fread(rawdata, 1, (file_size), fp);
        fclose(fp);

        // get the max number of blocks inside the file
        int max_block = file_size / BLOCK_SZ;
        char* bitmap = (char*)malloc(max_block*sizeof(char));

        // walk through inodes and extract files
        walk_inodes_extract(rawdata, path, file_size, uid, gid, bitmap);

        // write the unused blocks to the specific file via looking at the bitmap
        get_unused_blocks(bitmap, max_block, path);

        printf("Done.\n");
        return;
    }

    // error handling
    else{
        printf("Invalid Command Entered! Program can only handle create, extract, and insert.\n");
        exit(-1);
    }
}
