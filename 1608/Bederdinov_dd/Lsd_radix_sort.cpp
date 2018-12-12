#include <iostream>
#include <ctime>
#include <mpi.h>

using namespace std;

#define ROOT 0 // ������� ����� 0 - �������� �������

int curr_rank;   // ������� ���� ��������
int num_process; // ����� ���������

int *CreateArray(int size)
{
    int *arr;
    arr = new int[size];
    srand(time(NULL));

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

// ��������� � ����� ���������
void Swap(int &a1, int &a2)
{
    int tmp = a1;
    a1 = a2;
    a2 = tmp;
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

    for (int i = 0; i < N; ++i)
    {
        if (byte == 3)
            dest[offset[((source[i] >> (byte * 8)) + 128) & 0xff]++] = source[i];
        else
            dest[offset[((source[i]) >> (byte * 8)) & 0xff]++] = source[i];
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
    int mid_workload = size / num_process;
    int remainder = size % num_process;

    for (int i = 0; i < remainder; ++i)
    {
        displs[i] = i * (mid_workload + 1);
        send_num_work[i] = mid_workload + 1;
    }

    for (int i = remainder; i < num_process; ++i)
    {
        displs[i] = mid_workload * i + remainder;
        send_num_work[i] = mid_workload;
    }
}

int main(int argc, char *argv[])
{
    int *Array_Radix_Seq = NULL;
    int *Array_Radix_Pp = NULL;

    int *displs;        // ������ �������� ������������ ������ ������ Array
    int *send_num_work; // ������ ���-�� ������ ��� ������� ��������
    int *rbuf;

    int size = 0;

    double sequentTimeWorkBubble = 0;
    double parallelTimeWorkQsort = 0;

    double Total_sequentTimeWorkBubble = 0;
    double Total_parallelTimeWorkQsort = 0;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &num_process);
    MPI_Comm_rank(MPI_COMM_WORLD, &curr_rank);

    if (curr_rank == ROOT)
    {
        /*cout << "Enter size of array: " << endl;
        cin >> size;*/
        //size = atoi(argv[1]);
        size = 100;

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
        sequentTimeWorkBubble = MPI_Wtime();
        radixsort(Array_Radix_Seq, size);
        Total_sequentTimeWorkBubble = MPI_Wtime() - sequentTimeWorkBubble;

        if (size < 500)
        {
            cout << "\nSorted array - sequential version - LSD Radix Sort: " << endl;
            PrintArray(Array_Radix_Seq, size);
        }
    }

    // Parallel
    if (curr_rank == ROOT)
        parallelTimeWorkQsort = MPI_Wtime();

    send_num_work = new int[num_process];
    displs = new int[num_process];

    // �������� ������ �� ��������� �������� ���� ���������
    MPI_Bcast(&size, 1, MPI_INT, ROOT, MPI_COMM_WORLD);

    // ������������ ������ �������� � ���-�� ������
    Calc_work_and_displs(displs, send_num_work, size);

    rbuf = new int[send_num_work[curr_rank]];

    int N = send_num_work[curr_rank];

    int count[256];
    int offset[256];
    int GCount[256]; //Global count

    MPI_Status status;

    for (int byte = 0; byte < 4; ++byte)
    {
        //	����� ������� �� �����
        MPI_Scatterv(Array_Radix_Pp, send_num_work, displs, MPI_INT, rbuf, N, MPI_INT, ROOT, MPI_COMM_WORLD);

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

        int *Sizes_of_Arrs = new int[num_process];

        // ���� offset, ����� ������� ������� ��� ���� ��������� ������������� �������
        int **NewArr;
        int CurrSizeArr; // ������������ ����� ��������� � ������� NewArr[i]
        NewArr = new int *[num_process];
        for (int i = 0; i < num_process; i++)
        {
            CurrSizeArr = 0;

            for (int j = i * 256 / num_process; j < (i + 1) * 256 / num_process; j++)
                CurrSizeArr += GCount[j];

            if (CurrSizeArr > 0)
            {
                NewArr[i] = new int[CurrSizeArr];
                Sizes_of_Arrs[i] = CurrSizeArr;
            }
            else
            {
                NewArr[i] = NULL;
                Sizes_of_Arrs[i] = 0;
            }
        }

        int *index = new int[num_process]; // �������� index[i] ���������� ��������� �������� rbuf[i] � ������� NewArr[j]
                                           // /���������� ���������, ������� ����� � NewArr[i](������)
        for (int i = 0; i < num_process; i++)
            index[i] = 0;

        for (int i = 0; i < N; ++i)
        {
            int ind; // ���� � ������� byte � �������� rbuf[i]
            if (byte == 3)
                ind = ((rbuf[i] >> (byte * 8)) + 128) & 0xff;
            else
                ind = ((rbuf[i]) >> (byte * 8)) & 0xff;

            //� ������� �� ������� ������� ��������
            int j = 0;
            for (; j < num_process; ++j)
                if (j * 256 / num_process <= ind && ind < (j + 1) * 256 / num_process)
                    break;

            //���� �� � j ��������� ������� �������� �� ����������� �������� � j ����� ������
            // ����� ������� ��������� ������� � ��� ����� ������������ �� �������� ������� �� �������
            if (Sizes_of_Arrs[j] > 0)
            {
                NewArr[j][index[j]] = rbuf[i];
                index[j]++;
            }
        }

        // � ���� ������ ������� ��� ������� ������� ������ ��������� � ��������������� ����������
        int *Gindex = new int[num_process];
        MPI_Alltoall(index, 1, MPI_INT, Gindex, 1, MPI_INT, MPI_COMM_WORLD);
        // Sends data from all to all processes

        // ���������� ������. ��������, RecvBuffers[i] - �����, ���������� �� i-��� ��������
        int **RecvBuffers = new int *[num_process];
        for (int i = 0; i < num_process; i++)
        {
            if (Gindex[i] > 0)
                RecvBuffers[i] = new int[Gindex[i]];
            else
                RecvBuffers[i] = NULL;
        }

        // ��������� ������� ������� ������ ���������, ������ ��� ��, ��� ��� �������� ������� ������ � ������������� ��������
        for (int i = 0; i < num_process; i++)
        {
            if (i != curr_rank)
            {
                MPI_Sendrecv(NewArr[i], index[i], MPI_INT, i, 1, RecvBuffers[i],
                    Gindex[i], MPI_INT, i, 1, MPI_COMM_WORLD, &status);
            }
        }

        // i-�� ������� ���������� ��� ���� ���������� ����� ������� � ���� �����,
        // ������� �� ������(NewArr[i]) � 1 ������ tmpbuf � ��� ������������������, � ������� ��� ��� � Array_Radix_PP
        int *tmpbuf;
        if (Sizes_of_Arrs[curr_rank] > 0)
        {
            tmpbuf = new int[Sizes_of_Arrs[curr_rank]];
            for (int i = 0, k = 0; i < num_process; i++)
            {
                if (i != curr_rank)
                {
                    for (int j = 0; j < Gindex[i]; j++)
                        tmpbuf[k++] = RecvBuffers[i][j];
                }
                else
                {
                    for (int j = 0; j < index[i]; j++)
                        tmpbuf[k++] = NewArr[i][j];
                }
            }
        }
        else
            tmpbuf = NULL;

        /* ������������ �� ������ ������(���������)*/
        int lim = 0; // ��������, ����� �������� �� ������ ������� ������ �������
        for (int i = 0; i < curr_rank; i++)
            lim += Sizes_of_Arrs[i];

        int *dest = new int[Sizes_of_Arrs[curr_rank]];
        for (int i = 0; i < Sizes_of_Arrs[curr_rank]; i++)
        {
            if (byte == 3)
                dest[offset[((tmpbuf[i] >> (byte * 8)) + 128) & 0xff]++ - lim] = tmpbuf[i];
            else
                dest[offset[((tmpbuf[i]) >> (byte * 8)) & 0xff]++ - lim] = tmpbuf[i];
        }

        // �������� ��� Gatherv
        int *displs_1 = new int[num_process];

        memset(displs_1, 0, sizeof(int) * num_process);
        displs_1[0] = 0;
        for (int i = 1; i < num_process; i++)
            displs_1[i] = displs_1[i - 1] + Sizes_of_Arrs[i - 1];

        // �������� ��� � ���� ������. Array_Radix_Pp ����� ������������ �� ����� byte
        MPI_Gatherv(dest, Sizes_of_Arrs[curr_rank], MPI_INT, Array_Radix_Pp, Sizes_of_Arrs,
            displs_1, MPI_INT, ROOT, MPI_COMM_WORLD);

        delete[] displs_1;
        delete[] dest;
        delete[] tmpbuf;

        for (int i = 0; i < num_process; i++)
            delete[] RecvBuffers[i];

        delete[] RecvBuffers;
        delete[] Gindex;
        delete[] index;

        for (int i = 0; i < num_process; i++)
            delete[] NewArr[i];

        delete[] NewArr;
        delete[] Sizes_of_Arrs;
    }

    if (curr_rank == ROOT)
    {
        Total_parallelTimeWorkQsort = MPI_Wtime() - parallelTimeWorkQsort;

        if (size < 500)
        {
            cout << "Sorted array - parallel version - LSD Radix Sort:" << endl;
            PrintArray(Array_Radix_Pp, size);
            cout << endl;
        }

        cout << "Time sequence version LSD Radix Sort: " << Total_sequentTimeWorkBubble << " sec." << endl;
        cout << "Time parallel version LSD Radix Sort: " << Total_parallelTimeWorkQsort << " sec." << endl;

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
    delete[] send_num_work;
    delete[] displs;
    delete[] rbuf;

    MPI_Finalize();

    return 0;
}