#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

//defining Sudoku grid size and the number of threads
#define size 9
#define threadNum 4

//global variables for sudoku grid validation
int sudoku[size][size];  
int row[size] = {0};       
int col[size] = {0};    
int sub[size] = {0};         
int counter = 0;               

//synchronization primitives
pthread_mutex_t mutex;   
pthread_cond_t done_cond;
int completed_threads = 0;   

//struct for thread data
typedef struct {
    int index;       //thread index
    int sleepDelay; 
} threadData;

//row validation method
void validate_row(int rowPos) {
    //(non critical section)
    int found[size + 1] = {0};  //tracker for numbers
    for (int j = 0; j < size; j++) {
        int num = sudoku[rowPos][j];
        //check for invalid or duplicate row
        if (num < 1 || num > 9 || found[num]) {
            return;  
        }
        found[num] = 1;
    }
    //mark the row as valid and increment the counter (critical section)
    pthread_mutex_lock(&mutex);
    row[rowPos] = 1;
    counter++;
    pthread_mutex_unlock(&mutex);
}

//column validation method
void validate_column(int colPos) {
    //(non critical section)
    int found[size + 1] = {0};
    for (int i = 0; i < size; i++) {
        int num = sudoku[i][colPos];
        //check for invalid or duplicate row
        if (num < 1 || num > 9 || found[num]) {
            return; 
        }
        found[num] = 1;
    }
    //mark the column as valid and increment the counter (critical section)
    pthread_mutex_lock(&mutex);
    col[colPos] = 1;
    counter++;
    pthread_mutex_unlock(&mutex);
}

//validate a 3x3 sub grid
void validate_subgrid(int initialRow, int initialCol, int subgrid) {
    //(non critical section)
    int found[size + 1] = {0};
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            int num = sudoku[initialRow + i][initialCol + j];
            //check for invalid subgrid
            if (num < 1 || num > 9 || found[num]) {
                return;  
            }
            found[num] = 1;
        }
    }
    //mark the sub grid as valid and increment the counter (critical section)
    pthread_mutex_lock(&mutex);
    sub[subgrid] = 1;
    counter++;
    pthread_mutex_unlock(&mutex);
}

//thread function for validating rows, columns, and sub grids
void *validate(void *param) {
    threadData *data = (threadData *)param;
    int threadID = data->index;
    int sleepDelay = data->sleepDelay;

    char *results = (char *)malloc(256 * sizeof(char));
    snprintf(results, 256, "Thread ID-%d: ", threadID + 1);

    char errors[128] = "";

    //validate rows and sub grids if ID < 3
    if (threadID < 3) {
        int rowPos = threadID * 3; //multiply thread index by 3 and then loop for the next 3 rows to be validated
        for (int i = 0; i < 3; i++) {
            if (row[rowPos + i] == 0) {
                validate_row(rowPos + i);
                //check for invalid row
                if (row[rowPos + i] == 0) {
                    char buffer[32];
                    //format error message and append
                    snprintf(buffer, sizeof(buffer), "row %d, ", rowPos + i + 1);
                    strcat(errors, buffer);
                }
            }
        }
        int subgridRow = threadID * 3; //multiply thread index by 3 and then loop for the next 3 rows to be validated
        for (int k = 0; k < 3; k++) {
            int subgridCol = k * 3;
            int subgridIndex = threadID * 3 + k;
            if (sub[subgridIndex] == 0) {
                validate_subgrid(subgridRow, subgridCol, subgridIndex);
                //check for invalid subgrid
                if (sub[subgridIndex] == 0) {
                    char buffer[32];
                    //format error message and append
                    snprintf(buffer, sizeof(buffer), "sub-grid %d, ", subgridIndex + 1);
                    strcat(errors, buffer);
                }
            }
        }
    } else {  //validate columns
        for (int i = 0; i < size; i++) {
            if (col[i] == 0) {
                validate_column(i);
                //check for invalid column
                if (col[i] == 0) {
                    char buffer[32];
                    //format error message and append
                    snprintf(buffer, sizeof(buffer), "column %d, ", i + 1);
                    strcat(errors, buffer);
                }
            }
        }
    }

    //check for errors
    if (strlen(errors) == 0) {
        strcat(results, "valid");
    } else {
        errors[strlen(errors) - 2] = '\0';  //remove trailing comma and space
        snprintf(results + strlen(results), 256 - strlen(results), "%s are invalid", errors);
    }

    //sleep for the user provided delay
    sleep(sleepDelay);


    pthread_mutex_lock(&mutex);
    completed_threads++;
    //signal the last thread and print
    if (completed_threads == threadNum) {
        printf("Thread ID-%d is the last thread.\n\n", threadID + 1);
        pthread_cond_signal(&done_cond);
    }
    pthread_mutex_unlock(&mutex);

    pthread_exit((void *)results);
}

int main(int argc, char *argv[]) {
    //error handle for command line argument
    if (argc != 3) {
        printf("Usage: %s <fileName> <sleepDelay>\n", argv[0]);
        return 1;
    }

    int sleepDelay = atoi(argv[2]);
    if (sleepDelay < 1 || sleepDelay > 10) {
        fprintf(stderr, "Error: Sleep seconds must be between 1 and 10.\n");
        return 1;
    }

    char *fileName = argv[1];
    //read Sudoku grid from a file
    FILE *file = fopen(fileName, "r");
    if (file == NULL) {
        perror("Error opening file");
        return 1;
    }
    int read_items;
    //looping through and reading file
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            read_items = fscanf(file, "%d", &sudoku[i][j]);
            if (read_items != 1) {
                fprintf(stderr, "Failed to read Sudoku grid properly.\n");
                fclose(file);
                return 1;
            }
        }
    }
    fclose(file);

    //initialize threads and thread data
    pthread_t threads[threadNum];
    threadData thread_data[threadNum];

    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&done_cond, NULL);

    //create threads
    for (int i = 0; i < threadNum; i++) {
        thread_data[i].index = i;
        thread_data[i].sleepDelay = sleepDelay;
        if (pthread_create(&threads[i], NULL, validate, (void *)&thread_data[i]) != 0) {
            perror("Failed to create thread");
            return 1;
        }
    }

    // Wait for all threads to complete
    pthread_mutex_lock(&mutex);
    while (completed_threads < threadNum) {
        pthread_cond_wait(&done_cond, &mutex);
    }
    pthread_mutex_unlock(&mutex);

    // Print the results of each thread
    for (int i = 0; i < threadNum; i++) {
        char *result;
        pthread_join(threads[i], (void **)&result);
        printf("%s\n", result);
        free(result);
    }

    //print sudoku result
    if (counter == size * 3) {
        printf("There are in total %d valid rows, columns, and sub-grids, thus the solution is valid.\n", counter);
    } else {
        printf("There are in total %d valid rows, columns, and sub-grids, thus the solution is invalid.\n", counter);
    }

    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&done_cond);

    return 0;
}
