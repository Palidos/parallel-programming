#include <mpi.h>
#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <time.h>
using namespace std;


void InitializeMatrix(int *matrix, const int matrixSize)
{
    for (int i = 0; i < matrixSize; i++)
        matrix[i] = rand() % 100;
}

void MultiplyMatrices(int *matrixA, int *matrixB, int *resultMatrix, const int l, const int m, const int n)
{
    for (int i = 0; i < l; i++)
        for (int j = 0; j < n; j++)
        {
            resultMatrix[i * n + j] = 0;
            for (int k = 0; k < m; k++)
                resultMatrix[i * n + j] += matrixA[i * m + k] * matrixB[k * n + j];
        }
}


void CheckResults(int *linearResultingMatrix, int *parallelResultingMatrix, const int matrixSize)
{
    for (int i = 0; i < matrixSize; i++)
        if (linearResultingMatrix[i] != parallelResultingMatrix[i])
        {
            cout << "Error! Linear and parallel results are not equal" << endl;
            return;
        }
    cout << "Matrices are equal" << endl;
}


void PrintMatrix(int *matrix, const int size, const int columns)
{
    for (int i = 0; i < size; i++)
    {
        if (i % columns == 0)
            cout << endl;
        cout << matrix[i];
    }
    cout << endl << endl;
}

int main(int argc, char **argv)
{
    int procNum = 0, procRank = 0;
    int aRows = 0, aColumns = 0, bRows = 0, bColumns = 0;
    int aSize = 0, bSize = 0;
    int rowsPerProc = 0, remainingRows = 0;
    int *matrixA = nullptr, *matrixB = nullptr;
    int *linearResultingMatrix = nullptr, *parallelResultingMatrix = nullptr;
    int *elementsPerProcScatter = nullptr, *shiftsScatter = nullptr;
    int *elementsPerProcGather = nullptr, *shiftsGather = nullptr;
    int *bufferA = nullptr, *resultBuffer = nullptr;
    double startTime = 0.0, endTime = 0.0;
    double linearTime = 0.0, parallelTime = 0.0;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &procNum);
    MPI_Comm_rank(MPI_COMM_WORLD, &procRank);

    if (procRank == 0)
    {
        if (argc == 5)
        {
            aRows = atoi(argv[1]);
            aColumns = atoi(argv[2]);
            bRows = atoi(argv[3]);
            bColumns = atoi(argv[4]);

            if (aColumns != bRows)
            {
                cout << "Error! Number of columns in first matrix and number of rows in second must be equal" << endl;
                MPI_Finalize();
                return 1;
            }
        }
        else
        {
            aRows = aColumns = 1000;
            bRows = bColumns = 1000;
        }
        aSize = aRows * aColumns;
        bSize = bRows * bColumns;

        rowsPerProc = aRows / procNum;
        remainingRows = aRows - (procNum - 1) * rowsPerProc;

        matrixA = new int[aSize];
    }

    /***********************Parallel***********************/
    startTime = MPI_Wtime();
    MPI_Bcast(&aRows, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&aColumns, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&bColumns, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&aSize, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&bSize, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&rowsPerProc, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&remainingRows, 1, MPI_INT, 0, MPI_COMM_WORLD);

    matrixB = new int[bSize];
    elementsPerProcScatter = new int[procNum];
    elementsPerProcGather = new int[procNum];
    shiftsScatter = new int[procNum];
    shiftsGather = new int[procNum];

    if (procRank == 0)
    {
        srand(time(nullptr));
        InitializeMatrix(matrixA, aSize);
        InitializeMatrix(matrixB, bSize);

        for (int i = 0; i < procNum; i++)
        {
            elementsPerProcScatter[i] = rowsPerProc * aColumns;
            elementsPerProcGather[i] = rowsPerProc * bColumns;
            shiftsScatter[i] = i * rowsPerProc * aColumns;
            shiftsGather[i] = i * rowsPerProc * bColumns;
        }
        elementsPerProcScatter[procNum - 1] = remainingRows * aColumns;
        elementsPerProcGather[procNum - 1] = remainingRows * bColumns;

        parallelResultingMatrix = new int[aRows * bColumns];
    }

    MPI_Bcast(matrixB, bSize, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(elementsPerProcScatter, procNum, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(elementsPerProcGather, procNum, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(shiftsScatter, procNum, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(shiftsGather, procNum, MPI_INT, 0, MPI_COMM_WORLD);

    if (procRank == procNum - 1)
        rowsPerProc = remainingRows;

    bufferA = new int[rowsPerProc * aColumns];
    resultBuffer = new int[rowsPerProc * bColumns];

    MPI_Scatterv(matrixA, elementsPerProcScatter, shiftsScatter, MPI_INT, bufferA,
        elementsPerProcScatter[procRank], MPI_INT, 0, MPI_COMM_WORLD);

    MultiplyMatrices(bufferA, matrixB, resultBuffer, rowsPerProc, aColumns, bColumns);

    MPI_Gatherv(resultBuffer, elementsPerProcGather[procRank], MPI_INT,
        parallelResultingMatrix, elementsPerProcGather, shiftsGather, MPI_INT, 0, MPI_COMM_WORLD);

    if (procRank == 0)
    {
        endTime = MPI_Wtime();
        parallelTime = endTime - startTime;
        cout << "Parallel time = " << setprecision(6) << parallelTime << endl;
        /****************************************************************/

        /***********************Linear***********************/
        linearResultingMatrix = new int[aRows * bColumns];

        startTime = MPI_Wtime();
        MultiplyMatrices(matrixA, matrixB, linearResultingMatrix, aRows, aColumns, bColumns);
        endTime = MPI_Wtime();
        linearTime = endTime - startTime;
        cout << "Linear time = " << setprecision(6) << linearTime << endl << endl;
        /****************************************************************/

        CheckResults(linearResultingMatrix, parallelResultingMatrix, aRows * bColumns);

        delete[] matrixA;
        delete[] linearResultingMatrix;
        delete[] parallelResultingMatrix;
    }

    MPI_Finalize();

    delete[] matrixB;
    delete[] bufferA;
    delete[] resultBuffer;
    delete[] elementsPerProcScatter;
    delete[] shiftsScatter;
    delete[] elementsPerProcGather;
    delete[] shiftsGather;

    return 0;
}
