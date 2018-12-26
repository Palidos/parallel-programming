#include <iostream>
#include <ctime>
#include <iomanip>
#include <mpi.h>

using namespace std;

#define ROOT 0 // ������� ����� 0 - �������� �������

int procRank;   // ������� ���� ��������
int procNum; // ����� ���������

int *CreateArray(int size)
{
    int *arr;
    arr = new int[size];
    srand(time(nullptr));

    for (int i = 0; i < size; i++)
        arr[i] = rand() % 1000 - 500;
    return arr;
}

void PrintArray(int *arr, int size)
{
    for (int i = 0; i < size; i++)
        cout << arr[i] << " ";
    cout << endl;
}

void Radix(int byte, int N, int *source, int *dest)
{
    // *source - ������� ������
    // *dest - ���������������
    int count[256]; // ���������� ������������� ������� ����� �� ���� ��������� �������
    int offset[256]; // ������� ��������, �� ������� ����� ����������� ��������
    memset(count, 0, sizeof(count));

    for (int i = 0; i < N; i++)
    {
        if (byte == 3)
            count[((source[i] >> (byte * 8)) + 128) & 0xff]++;
        else
            count[((source[i]) >> (byte * 8)) & 0xff]++;
    }

    offset[0] = 0;
    for (int i = 1; i < 256; i++)
        offset[i] = offset[i - 1] + count[i - 1];

    for (int i = 0; i < N; i++)
    {
        if (byte == 3)
        {
            dest[offset[((source[i] >> (byte * 8)) + 128) & 0xff]++] = source[i];
            //PrintArray(dest, N);
        }
        else
        {
            dest[offset[((source[i]) >> (byte * 8)) & 0xff]++] = source[i];
            //PrintArray(dest, N);
        }
    }
}

void radixsort(int *source, int N)
{
    int *temp = new int[N];
    Radix(0, N, source, temp);
    Radix(1, N, temp, source);
    Radix(2, N, source, temp);
    Radix(3, N, temp, source);
    delete[] temp;
}

void Calc_work_and_displs(int *displs, int *send_num_work, int size)
{
    int mid_workload = size / procNum;
    int remainder = size % procNum;

    for (int i = 0; i < remainder; i++)
    {
        displs[i] = i * (mid_workload + 1);
        send_num_work[i] = mid_workload + 1;
    }

    for (int i = remainder; i < procNum; i++)
    {
        displs[i] = mid_workload * i + remainder;
        send_num_work[i] = mid_workload;
    }
}

int main(int argc, char *argv[])
{
    int *Array_Radix_Seq = nullptr;
    int *Array_Radix_Pp = nullptr;

    int *displs;        // ������ �������� ������������ ������ ������ Array
    int *sendNumWork; // ������ ���-�� ������ ��� ������� ��������
    int *rbuf;

    int size = 0;

    double sequentTime = 0;
    double parallelTime = 0;

    double totalSequentTime = 0;
    double totalParallelTime = 0;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &procNum);
    MPI_Comm_rank(MPI_COMM_WORLD, &procRank);

    if (procRank == ROOT)
    {
        /*cout << "Enter size of array: " << endl;
        cin >> size;*/
        size = atoi(argv[1]);
        //size = 5;

        Array_Radix_Seq = CreateArray(size);

        Array_Radix_Pp = new int[size];

        for (int i = 0; i < size; i++)
        {
            Array_Radix_Pp[i] = Array_Radix_Seq[i];
        }

        if (size < 500)
        {
            cout << "Unsorted array: " << endl;
            PrintArray(Array_Radix_Seq, size);
        }

        // ���������� lsd radix sort ���������������� ������
        sequentTime = MPI_Wtime();
        radixsort(Array_Radix_Seq, size);
        totalSequentTime = MPI_Wtime() - sequentTime;

        if (size < 500)
        {
            cout << "\nSorted array - sequential version - LSD Radix Sort: " << endl;
            PrintArray(Array_Radix_Seq, size);
        }
    }

    // Parallel
    if (procRank == ROOT)
        parallelTime = MPI_Wtime();

    sendNumWork = new int[procNum];
    displs = new int[procNum];

    // �������� ������ �� ��������� �������� ���� ���������
    MPI_Bcast(&size, 1, MPI_INT, ROOT, MPI_COMM_WORLD);

    // ������������ ������ �������� � ���-�� ������
    Calc_work_and_displs(displs, sendNumWork, size);

    rbuf = new int[sendNumWork[procRank]];

    int N = sendNumWork[procRank];

    int count[256];
    int offset[256];
    int GCount[256]; //Global count

    MPI_Status status;

    for (int byte = 0; byte < 4; byte++) // ��� ������� �� 4� ������ ����� �����������:
    {
        //	����� ������� �� �����
        MPI_Scatterv(Array_Radix_Pp, sendNumWork, displs, MPI_INT, rbuf, N, MPI_INT, ROOT, MPI_COMM_WORLD);

        for (int i = 0; i < 256; i++)
            count[i] = 0;

        for (int i = 0; i < N; ++i)
        {
            if (byte == 3)
                count[((rbuf[i] >> (byte * 8)) + 128) & 0xff]++;
            else
                count[((rbuf[i]) >> (byte * 8)) & 0xff]++;
        }

        MPI_Allreduce(count, GCount, 256, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
        // Combines values from all processes and distributes the result back to all processes

        offset[0] = 0;
        for (int i = 1; i < 256; ++i)
            offset[i] = offset[i - 1] + GCount[i - 1];

        int *sizesOfArrs = new int[procNum];

        // ���� offset, ����� ������� ������� ��� ���� ��������� ������������� �������
        int **newArr;
        int maxSizeArr; // ������������ ����� ��������� � ������� NewArr[i]
        newArr = new int *[procNum];
        for (int i = 0; i < procNum; i++)
        {
            maxSizeArr = 0;

            for (int j = i * 256 / procNum; j < (i + 1) * 256 / procNum; j++)
            {  
                maxSizeArr += GCount[j];
            }

            if (maxSizeArr > 0)
            {
                newArr[i] = new int[maxSizeArr];
                sizesOfArrs[i] = maxSizeArr;
            }

            else
            {
                newArr[i] = nullptr;
                sizesOfArrs[i] = 0;
            }
        }

        int *index = new int[procNum]; // �������� index[i] ���������� ��������� �������� rbuf[i] � ������� NewArr[j]
                                           
        for (int i = 0; i < procNum; i++)
            index[i] = 0;

        for (int i = 0; i < N; i++)
        {
            int ind; // ���� � �������� rbuf[i]
            if (byte == 3)
                ind = ((rbuf[i] >> (byte * 8)) + 128) & 0xff;
            else {
                ind = ((rbuf[i]) >> (byte * 8)) & 0xff;
                //cout <<"procRanc = "<< procRank << "    rbuf[i] = " << rbuf[i] << "    ind = " << ind << endl;
                //cout << endl;
            }

            //� ������� �� ������� ������� ��������
            int j = 0;
            for (; j < procNum; j++)
                if (j * 256 / procNum <= ind && ind < (j + 1) * 256 / procNum)
                    break;

            //���� �� � j ��������� ������� �������� �� ����������� �������� � j ����� ������
            // ����� ������� ��������� ������� � ��� ����� ������������ �� �������� ������� �� �������

            // �������� 33�� ����(����� n) - ��� 4� ��������� � ���������� ����� ���������� � ���� ���� = 5
            /* n 0 0 0       35��(m) ->   n 0 0 0        70��(k) ->  n 0 0 0 
               0 0 0 0                    m 0 0 0                    m 0 0 0 
               0 0 0 0                    0 0 0 0                    0 k 0 0 
               0 0 0 0                    0 0 0 0                    0 0 0 0 
               0 0 0 0                    0 0 0 0                    0 0 0 0 
            */
            if (sizesOfArrs[j] > 0)
            {
                newArr[j][index[j]] = rbuf[i];
                //cout <<"procRanc = " << procRank << "    rbuf[i] = " << rbuf[i] << "    ind = " << ind 
                 //   << "    j = " << j << "    index[j] = " <<index[j] << endl;
                index[j]++;
            }
        }

        // � ���� ������ ������� ��� ������� ������� ������ ��������� � ��������������� ����������
        int *Gindex = new int[procNum];
        MPI_Alltoall(index, 1, MPI_INT, Gindex, 1, MPI_INT, MPI_COMM_WORLD);
        // Sends data from all to all processes

        // ���������� ������. ��������, RecvBuffers[i] - �����, ���������� �� i-��� ��������
        int **RecvBuffers = new int *[procNum];
        for (int i = 0; i < procNum; i++)
        {
            if (Gindex[i] > 0)
                RecvBuffers[i] = new int[Gindex[i]];
            else
                RecvBuffers[i] = nullptr;
        }

        // ��������� ������� ������� ������ ���������, ������ ��� ��, ��� ��� �������� ������� ������ � ������������� ��������
        for (int i = 0; i < procNum; i++)
        {
            if (i != procRank)
            {
                MPI_Sendrecv(newArr[i], index[i], MPI_INT, i, 1, RecvBuffers[i],
                    Gindex[i], MPI_INT, i, 1, MPI_COMM_WORLD, &status);
            }
        }

        // i-�� ������� ���������� ��� ���� ���������� ����� ������� � ���� �����,
        // ������� �� ������(NewArr[i]) � 1 ������ tmpbuf � ��� ������������������, � ������� ��� ��� � Array_Radix_PP
        int *tmpbuf;
        if (sizesOfArrs[procRank] > 0)
        {
            tmpbuf = new int[sizesOfArrs[procRank]];
            for (int i = 0, k = 0; i < procNum; i++)
            {
                if (i != procRank)
                {
                    for (int j = 0; j < Gindex[i]; j++)
                        tmpbuf[k++] = RecvBuffers[i][j];
                }
                else
                {
                    for (int j = 0; j < index[i]; j++)
                        tmpbuf[k++] = newArr[i][j];
                }
            }
        }
        else
            tmpbuf = nullptr;

        /* ������������ �� ������ ������(���������)*/
        int lim = 0; // ��������, ����� �������� �� ������ ������� ������ �������
        for (int i = 0; i < procRank; i++) {
            lim += sizesOfArrs[i];
            /*cout << "procRanc = " << procRank << "    lim = " << lim 
                << "    Sizes_of_Arrs[i] = " << sizesOfArrs[i] << "   byte = " << byte << endl<<endl;*/
        }

        int *dest = new int[sizesOfArrs[procRank]];
        for (int i = 0; i < sizesOfArrs[procRank]; i++)
        {
            if (byte == 3)
                dest[offset[((tmpbuf[i] >> (byte * 8)) + 128) & 0xff]++ - lim] = tmpbuf[i];
            else
                dest[offset[((tmpbuf[i]) >> (byte * 8)) & 0xff]++ - lim] = tmpbuf[i];
        }

        // �������� ��� Gatherv
        int *displs_1 = new int[procNum];

        memset(displs_1, 0, sizeof(int) * procNum);
        displs_1[0] = 0;
        for (int i = 1; i < procNum; i++)
            displs_1[i] = displs_1[i - 1] + sizesOfArrs[i - 1];

        // �������� ��� � ���� ������. Array_Radix_Pp ����� ������������ �� ����� byte
        MPI_Gatherv(dest, sizesOfArrs[procRank], MPI_INT, Array_Radix_Pp, sizesOfArrs,
            displs_1, MPI_INT, ROOT, MPI_COMM_WORLD);

        delete[] displs_1;
        delete[] dest;
        delete[] tmpbuf;

        for (int i = 0; i < procNum; i++)
            delete[] RecvBuffers[i];

        delete[] RecvBuffers;
        delete[] Gindex;
        delete[] index;

        for (int i = 0; i < procNum; i++)
            delete[] newArr[i];

        delete[] newArr;
        delete[] sizesOfArrs;
    }

    if (procRank == ROOT)
    {
        totalParallelTime = MPI_Wtime() - parallelTime;

        if (size < 500)
        {
            cout << "Sorted array - parallel version - LSD Radix Sort:" << endl;
            PrintArray(Array_Radix_Pp, size);
            cout << endl;
        }

        cout << "Time of sequence version LSD Radix Sort: " << fixed << setprecision(5) << totalSequentTime << " sec." << endl;
        cout << "Time of parallel version LSD Radix Sort: " << setprecision(5) << totalParallelTime << " sec." << endl;

        cout << endl;

        // �������� �����������
        int check = true;
        for (int i = 0; i < size; i++)
        {
            if (Array_Radix_Pp[i] != Array_Radix_Seq[i])
            {
                cout << "Array_Radix_Pp !=  Array_Radix_Seq" << endl;
                check = false;
                break;
            }
        }

        if (check)
            cout << "Array_Radix_Pp ==  Array_Radix_Seq" << endl;

        delete[] Array_Radix_Seq;
    }

    delete[] Array_Radix_Pp;
    delete[] sendNumWork;
    delete[] displs;
    delete[] rbuf;

    MPI_Finalize();

    return 0;
}