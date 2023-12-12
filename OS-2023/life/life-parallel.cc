#define _XOPEN_SOURCE 600
#include "life.h"
#include <pthread.h>
#include <iostream>
using namespace std;

//holds args to be passed to each thread
struct ThreadData { //https://stackoverflow.com/questions/34387051/pthread-barriers-in-c
    int id;
    int steps;
    int start_cell;
    int end_cell;
    LifeBoard* state;
    LifeBoard* next_state;
    pthread_barrier_t* barrier;
};

// this is the func executed by each thread
void* thread_simulate_life(void* arg) {
    // each thread is created with a void pointer to the struct ThreadData, and the start routine of this function
    // when each thread gets started they will start running this function and have any of the data that was passed to them (the reading/writing should be done here)
    struct ThreadData* data = (struct ThreadData*) arg;
    //int id = data->id;
    int steps = data->steps;
    int start_cell = data->start_cell;
    int end_cell = data->end_cell;
    pthread_barrier_t* barrier = data->barrier;
    LifeBoard* state = data->state;
    LifeBoard* next_state = data->next_state;
    
    // Have steps inside thread function bc we must use same threads for all steps. Iterate over the given number of steps
    for(int i=0; i < steps; i++){
        // iterate over all the cells within that range and then determine whether the next state should have a 1 or 0 there
        for(int ith_cell=start_cell; ith_cell <= end_cell; ith_cell++){

            int x = ith_cell % state->width();
            int y = ith_cell / state->width();
            
            // this will ensure only valid coords get processed
            if(x != 0 && y != 0 && x != (state->width()-1) && y !=(state->height()-1)){
                int live_in_window = 0;
                /* For each cell, examine a 3x3 "window" of cells around it,
                    * and count the number of live (true) cells in the window. */
                for (int y_offset = -1; y_offset <= 1; ++y_offset) {
                    for (int x_offset = -1; x_offset <= 1; ++x_offset) {
                        if (state->at(x + x_offset, y + y_offset)) {
                            ++live_in_window;
                        }
                    }
                }
                /* Cells with 3 live neighbors remain or become live.
                    Live cells with 2 live neighbors remain live. */
                next_state->at(x, y) = (
                    live_in_window == 3 /* dead cell with 3 neighbors or live cell with 2 */ ||
                    (live_in_window == 4 && state->at(x, y)) /* live cell with 3 neighbors */
                );
            }
        }
        pthread_barrier_wait(barrier);

        // each thread has pointer to board. swap is swapping the pointers for each thread so we want to swap ALL pointers
        swap(state, next_state);

        // do not need second barrier due to how we constructed this. each thread has pointer to the state. swap will then
        // swap the two pointers. we discovered this by trying to only have one thread swap and that would result in only that threads
        // portion of the board being correct. since each thread has a pointer to the board each thread will need to swap it to next state.

        /* at the point of the barrier next state has been finalized and state so after any thread swaps they can immediately begin doing work
        on their section. dont need to wait for other threads as swap is jsut pointer and will not start accessing board until swap finishes for each thread.
        */
    }
    return 0;
}

void simulate_life_parallel(int threads, LifeBoard &state, int steps) {
    // make a copy of the first board. Threads should be reading from current state board and writing to next state
    // No thread should start reading from next state board until all threads have written to it
    LifeBoard* next_state = new LifeBoard(state); 
    LifeBoard* ptr_to_state = &state;

    // set up the barrier here. must ensure that all threads stop accessing the boards before swap and dont start up until after board swapped
    pthread_barrier_t swap_state_barrier;
    // initialize the barrier with the number of threads needed to break down the barrier
    pthread_barrier_init(&swap_state_barrier, NULL, threads);
    pthread_barrier_t* ptr_to_swap_state_barrier = &swap_state_barrier;

    // each thread when created should point to data[i], where i is the thread ID from (0 to (threads-1))
    struct ThreadData *data = (struct ThreadData*)malloc(threads * sizeof(struct ThreadData));
    // we are only worried about the cells not on the outside so total number of cells that need to be calculated is height-2 and width-2
    int viable_cols = state.width();
    int viable_rows = state.height();

    // num_cells per thread as well as any extras
    int num_cells_per_thread = viable_cols*viable_rows / threads;
    int extra_cells = viable_cols*viable_rows % threads;

    // create the data for each thread
    for(int i=0; i < threads; i++){
        data[i].id = i;
        data[i].steps = steps;
        data[i].state = ptr_to_state;
        data[i].next_state = next_state;
        data[i].barrier = ptr_to_swap_state_barrier;
        // we want each thread to get an even-ish # of cells to process (best speedup is if these cells are contiguous memory (rows))
        // load a vector of x-coords and y-coords that each thread is responsible for processing across states
        if (i < extra_cells) {
            data[i].start_cell = i * (num_cells_per_thread + 1);
        } else {
            data[i].start_cell = i * num_cells_per_thread + extra_cells;
        }
        if (i < extra_cells) {
            data[i].end_cell = data[i].start_cell + num_cells_per_thread;
        } else {
            data[i].end_cell = data[i].start_cell + num_cells_per_thread - 1;
        }
    }

    // once data has been properly formatted dynamically allocate an array of threads
    pthread_t *thread_handles = (pthread_t*)malloc(threads*sizeof(pthread_t));

    // go thru all of the threads and create them with the corresponding data
    for (int i = 0; i < threads; i++) {
        pthread_create(&thread_handles[i], NULL, thread_simulate_life, (void*)&data[i]);
    }

    // wait for each of the threads to end (cleanup for threads)
    for (int i = 0; i < threads; i++) {
        pthread_join(thread_handles[i], NULL);
    }

    // if odd iteration we want to return the value in next state. if even then value in state is correct
    if(steps % 2 == 1){
        state = *next_state;
    }

    //free all the loose data
    pthread_barrier_destroy(&swap_state_barrier);
    free(data);
    free(thread_handles);
    delete next_state;
}
