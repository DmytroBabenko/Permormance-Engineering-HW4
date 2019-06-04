//
// Created by Dmytro Babenko on 2019-06-04.
//

//#include <sys/types.h>
#include <time.h>
#include <stdio.h>
#include <mpi.h>

#include <stdlib.h>

#include <vector>
#include <fstream>
#include <string>

static const char* const imageFile = "lenna.bmp";


template <typename T = uint8_t>
class BMPImage
{
    public:
        explicit BMPImage(const std::string& file)
        : loaded(false)
        , width(0)
        , height(0)
        {
        _loadImage(file);
        }


        bool imageLoaded() const {return loaded; }

        T* R() {return r.data(); }
        T* G() {return r.data(); }
        T* B() {return r.data(); }


        std::vector<T> RCopy() {return r; }
        std::vector<T> GCopy() {return g; }
        std::vector<T> BCopy() {return b; }

        uint32_t channelSize() const {return width * height; }

        uint32_t Width() const {return width;}
        uint32_t Height() const {return height;}


    private:
        void _loadImage(const std::string& file)
        {
            loaded = false;
            std::ifstream input;
            input.open(file.c_str(), std::ios::binary);

            std::istreambuf_iterator<char > begin = std::istreambuf_iterator<char >(input);
            std::istreambuf_iterator<char > end = std::istreambuf_iterator<char>();

            std::vector<uint8_t> buffer(begin, end);

            if (buffer.size() <= HEADER_SIZE)
                return;


            width = *(int*)&buffer[18];
            height = *(int*)&buffer[22];

            const int channelSize  = width * height;

            const int size = CHANNELS * channelSize;
            if (buffer.size() != size + HEADER_SIZE)
            return;

            r.resize(channelSize);
            g.resize(channelSize);
            b.resize(channelSize);

            for (int i = HEADER_SIZE, j = 0; i < size && j < channelSize; i += 3, ++j)
            {
                r[j] = buffer[i + 2];
                g[j] = buffer[i+1];
                b[j] = buffer[i];
            }

            loaded = true;
        }

    private:

        bool loaded;
        int width;
        int height;

        std::vector<T> r;
        std::vector<T> g;
        std::vector<T> b;

    private:
        static const int HEADER_SIZE = 54;
        static const int CHANNELS = 3;
};

struct Image
{
    unsigned long* r;
    int channelSize;
};




unsigned int calcOptimalChunkSize(unsigned int numTasks, unsigned int size)
{
    if (size < 2)
        return size;

    if (numTasks < 2)
        return size;

    if (numTasks >= size)
        return 2;

    if (size % numTasks == 0)
        return size / numTasks;

    return size / numTasks + 1;

}

unsigned int calcOptimalNumProcesses(unsigned size, unsigned optimalChunkSize)
{
    if (size % optimalChunkSize == 0)
        return size / optimalChunkSize;

    return size / optimalChunkSize + 1;
}


int main(int argc, char* argv[])
{

    MPI_Init(&argc, &argv);

    int numProcesses, id;

    clock_t clockStart = clock();
    MPI_Comm_size(MPI_COMM_WORLD, &numProcesses);
    MPI_Comm_rank(MPI_COMM_WORLD, &id);

    unsigned char* data;

    //TODO:
    int size = 512 * 512;
    int chunkSize = calcOptimalChunkSize(numProcesses, size);
    int numOptimalProcesses = calcOptimalNumProcesses(size, chunkSize);


    BMPImage<unsigned char>* image = NULL;
    unsigned char chunkMin = 255;
    if (id == 0)
    {

        image = new BMPImage<unsigned char>(imageFile);

        size = image->channelSize();

        data = image->R();


        for (int s = 1; s < numOptimalProcesses; ++s)
        {
            int idx = s*chunkSize;
            int correctChunkSize =  s < numOptimalProcesses - 1 ? chunkSize : size - idx;
            MPI_Send(data + idx, correctChunkSize, MPI_UNSIGNED_CHAR, s, 1, MPI_COMM_WORLD );
        }

        for (int i = 0; i < chunkSize; ++i)
        {
            if (chunkMin > data[i])
                chunkMin = data[i];
        }
    }
    else if (id < numOptimalProcesses)
    {
        int tempSize = id < numOptimalProcesses - 1  ? chunkSize : size - (numOptimalProcesses - 1) *chunkSize;


        unsigned char* tempArr = (unsigned char*)malloc(tempSize * sizeof(unsigned char));

        MPI_Status status;
        int result = MPI_Recv(tempArr, tempSize, MPI_UNSIGNED_CHAR, 0, 1, MPI_COMM_WORLD, &status );
        if (result != MPI_SUCCESS)
        {
            return -1;
        }

        for (int i = 0; i < tempSize; ++i)
        {
            if (chunkMin > tempArr[i])
                chunkMin = tempArr[i];
        }
    }

    unsigned char min = 255;
    MPI_Reduce(&chunkMin, &min, 1, MPI_UNSIGNED_CHAR, MPI_MIN, 0, MPI_COMM_WORLD);
    clock_t clockEnd = clock();

    if (id == 0)
    {
        printf("MPI cal time: %f ms\n", (clockEnd - clockStart) / (float) (CLOCKS_PER_SEC / 1000));
        printf("MPI calc min: %d\n", (int)min);


        clockStart = clock();
        unsigned char commonMin = 255;
        for (int i = 0; i < size; ++i)
        {
            if (commonMin > data[i])
                commonMin = data[i];
        }
        clockEnd = clock();

        printf("\n");
        printf("Common calc time: %f ms\n", (clockEnd - clockStart) / (float) (CLOCKS_PER_SEC / 1000));
        printf("Common calc min: %d\n", (int)commonMin);

    }

    delete image;
    MPI_Finalize();





}

