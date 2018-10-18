#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char **argv)
{
    srand(time(NULL));
    const long long MAX_STRING_SIZE = 10000000; //rand() % 100000000;
    char *str = (char *)malloc(sizeof(char) * (MAX_STRING_SIZE + 1));
    int linear_result = 0, parallel_result = 0, char_count = 0;
    double start_time = 0, end_time = 0;
    double linear_time = 0, parallel_time = 0;
    int procNum, procRank;
    int i;

    // Initialize MPI
    MPI_Init(&argc, &argv);
    // Set the number of processors
    MPI_Comm_size(MPI_COMM_WORLD, &procNum);
    // Set rank of current process
    MPI_Comm_rank(MPI_COMM_WORLD, &procRank);

    start_time = MPI_Wtime();
    // Random fill of string
    for (i = 0; i < MAX_STRING_SIZE; i++)
        str[i] = 'A' + rand() % ('Z' - 'A' + 1) + (rand() % 2) * ('a' - 'A');
    str[MAX_STRING_SIZE] = '\0';
    end_time = MPI_Wtime();
    printf("Randomization time: %f\n", end_time - start_time);

    // Linear
    if (procRank == 0)
    {
        start_time = MPI_Wtime();
        for (i = 0; i < MAX_STRING_SIZE; i++)
            linear_result++;
        end_time = MPI_Wtime();
        linear_time = end_time - start_time;

        printf("Linear time = %.3f\n", end_time - start_time);
        printf("Linear result = %d\n", linear_result);
    }

    // Parallel
    if (procRank == 0)
        start_time = MPI_Wtime();

    // Broadcast string to each processor
    MPI_Bcast(str, MAX_STRING_SIZE, MPI_CHAR, 0, MPI_COMM_WORLD);

    for (i = procRank; i < MAX_STRING_SIZE; i += procNum)
        char_count++;

    // Collect the results from each processor and sum them in main processor
    MPI_Reduce(&char_count, &parallel_result, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

    if (procRank == 0)
    {
        end_time = MPI_Wtime();
        parallel_time = end_time - start_time;
        printf("MPI time = %.3f\n", parallel_time);
        printf("MPI result = %d\n", parallel_result);
        printf("Performance difference: %.1f%\n", ((linear_time - parallel_time) / ((linear_time + parallel_time) / 2)) * 100);
    }

    MPI_Finalize();
    free(str);
    return 0;
}