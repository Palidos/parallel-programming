#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <mpi.h>
#include <time.h>
using namespace std;

int main(int argc, char * argv[])
{
    int procNum = 0, procRank = 0;
    int len = atoi(argv[1]);
    char letter = argv[2][0];
    char *str = new char[len+1];
    double startTime = 0.0, endTime = 0.0;
    double linearTime = 0.0, parallelTime = 0.0;
    int parallelResult = 0, linearResult = 0;
    int partSize = 0, partRemainder = 0, lastPartSize = 0;
    int count = 0;
    MPI_Status status;

    srand(time(NULL));
    for (int i = 0; i < len; i++)
        str[i] = rand() % 26 + 97;
    str[len] = '\0';

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &procNum);
    MPI_Comm_rank(MPI_COMM_WORLD, &procRank);

    partSize = len / procNum;
    partRemainder = len % procNum;
    lastPartSize = partSize + partRemainder;

    if (procRank == 0)
    {
        /*****************LINEAR*****************/
        startTime = MPI_Wtime();
        for (int i = 0; i < len; i++)
            if (str[i] == letter)
                linearResult++;
        endTime = MPI_Wtime();
        linearTime = endTime - startTime;
        if (len <= 80)
            cout << str << endl << endl;
        cout << "Letter '" << letter << "' frequency = " << (static_cast<double>(linearResult) / static_cast<double>(len)) * 100 << "%" << endl;
        cout << "Linear time = " << fixed << setprecision(6) << linearTime << endl << endl;
        /*****************************************/

        /****************PARALLEL*****************/
        startTime = MPI_Wtime();
        for (int i = 1; i < procNum; i++)
        {
            if (i == procNum - 1)
                MPI_Send(&str[partSize * i], lastPartSize, MPI_CHAR, i, 0, MPI_COMM_WORLD);
            else
                MPI_Send(&str[partSize * i], partSize, MPI_CHAR, i, 0, MPI_COMM_WORLD);

        }
    }

    if (procRank != 0 && procRank != (procNum - 1))
    {
        char* buf = new char[partSize];
        MPI_Recv(buf, partSize, MPI_CHAR, 0, 0, MPI_COMM_WORLD, &status);
        for (int j = 0; j < partSize; j++)
            if (buf[j] == letter)
                count++;
        delete[] buf;
    }
    else if (procRank == (procNum - 1) && procNum != 1)
    {
        char* bufLast = new char[lastPartSize];
        MPI_Recv(bufLast, lastPartSize, MPI_CHAR, 0, 0, MPI_COMM_WORLD, &status);
        for (int j = 0; j < lastPartSize; j++)
            if (bufLast[j] == letter)
                count++;
        delete[] bufLast;
    }
    else
    {
        for (int j = 0; j < partSize; j++)
            if (str[j] == letter)
                count++;
    }

    MPI_Reduce(&count, &parallelResult, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

    if (procRank == 0)
    {
        endTime = MPI_Wtime();
        parallelTime = endTime - startTime;
        cout << "Letter '" << letter << "' frequency = " << (static_cast<double>(parallelResult) / static_cast<double>(len)) * 100 << "%" << endl;
        cout << "Parallel time = " << fixed << setprecision(6) << parallelTime << endl << endl;
        /*****************************************/
        if (parallelResult == linearResult)
            cout << "Answers are equal" << endl;
        else
            cout << "Answers are different" << endl;
    }

    MPI_Finalize();

    delete[] str;
    return 0;
}
