#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <netdb.h>
#include <stdio.h>
#include <iostream>
#include <arpa/inet.h>
#include <string>
#include <map>
#include <sstream>
#include <fstream>
#include <memory>
#include <vector>
#include <cstring>
#include <sys/wait.h>
#include <queue>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <chrono>
#include <thread>

#define mill 4
#define MAXLINE 10240

using namespace std;

const int size1MB = 100;  // 10KB, 1MB = 10240
int chunkNum = 1;
struct packet *a;
vector<uint32_t> acks;
int port;
int maxWindow;
int seed;
double prop;

/* Data-only packets */
struct packet {
    /* Header */
//    uint16_t cksum; /* Optional bonus part */
    uint16_t len;
    uint32_t seqno;
    /* Data */
    char data[10000]; /* Not always 500 bytes, can be less */
};

/* Ack-only packets are only 8 bytes */
struct ack_packet {
//    uint16_t cksum; /* Optional bonus part */// Driver code
    uint16_t len;
    uint32_t ackno;
};

struct Data
{
    int forked;
    bool arr[1000];
};


key_t key = 1234;
int shm_id = shmget(key, sizeof(struct Data), IPC_CREAT | 0666);
struct Data *data = (struct Data *) shmat(shm_id, NULL, 0);

// set a timer
void timer(unsigned int sec)
{
//    usleep(sec);
    this_thread::sleep_for(chrono::milliseconds(sec));
}


// Create Chunk File
unique_ptr<ofstream> createChunkFile(vector<string> &vecFilenames) {
    stringstream filename;
    filename << "chunk" << (vecFilenames.size() + 1) << ".txt";
    vecFilenames.push_back(filename.str());
    return make_unique<ofstream>(filename.str(), ios::trunc);
}

// split
void split(istream &inStream, int nMegaBytesPerChunk, vector<string> &vecFilenames) {
    unique_ptr<char[]> buffer(new char[size1MB]);
    int nCurrentMegaBytes = 0;
    unique_ptr<ostream> pOutStream = createChunkFile(vecFilenames);
    while (!inStream.eof()) {
        inStream.read(buffer.get(), size1MB);
        pOutStream->write(buffer.get(), inStream.gcount());
        ++nCurrentMegaBytes;
        if (nCurrentMegaBytes >= nMegaBytesPerChunk) {
            pOutStream = createChunkFile(vecFilenames);
            nCurrentMegaBytes = 0;
        }
    }
}

// copy a file into a buffer
int copyToBuffer(char *fileName, char data[]) {
    ifstream in(fileName);
    string contents((istreambuf_iterator<char>(in)), istreambuf_iterator<char>());

    ifstream fin;
    fin.open(fileName, ios::in);

    char my_character ;
    int i = 0;

    while (i < contents.length()) {

        fin.get(my_character);
        data[i] = my_character;
        i++;
    }
    return static_cast<int>(contents.length());
}

// check if file is exist
inline bool exists_test2(const std::string &name) {
    return (access(name.c_str(), F_OK) != -1);
}

// next sequence number
int getNextSeqNumber(int oldNum) {
    if (oldNum == 1) {
        return 0;
    }
    return 1;
}

// send a chunk
void sendChunk(int currentChunk, int sockfd, struct sockaddr_in cliaddr, socklen_t fromlen) {

    string nextChunk = "chunk" + to_string(currentChunk) + ".txt";

    // check existence of file to indicate that all chunks have been sent
    if (exists_test2(nextChunk)) {
        // send next chunk
        // make data packet struct
        a->seqno = static_cast<uint32_t>(currentChunk);
        char *charNextChunk = const_cast<char *>(nextChunk.c_str());

        // copy file to packet data buffer
        int size = copyToBuffer(charNextChunk, a->data);
        a->len = static_cast<uint16_t>(size);

        // send a chunk to client
        double ran = (double) rand() / (double)(RAND_MAX);
        cout << seed << "\t" << ran << "\t" << prop << endl;

        while(ran <= prop) {
            ran = (double) rand() / (double)(RAND_MAX);
            cout << "*************packet no " << charNextChunk << " is lost" << endl;
            timer(100);
        }
        sendto(sockfd, a, MAXLINE, MSG_CONFIRM, (const struct sockaddr *) &cliaddr, fromlen);
//        cout << "Server sent " << nextChunk << endl;
        remove(nextChunk.c_str());
    }
}


void sendAndReceive(int currentChunk, int sockfd, struct sockaddr_in cliaddr, socklen_t fromlen) {

    struct ack_packet *ack;
    ack = static_cast<ack_packet *>(malloc(sizeof(struct ack_packet)));

    // send
    sendChunk(currentChunk, sockfd, cliaddr, fromlen);

    // receive
    int n = static_cast<int>(recvfrom(sockfd, ack, MAXLINE, 0, (struct sockaddr *) &cliaddr, &fromlen));
    if (n < 0) {
        perror("Received Failed!!");
        cout << endl;
    }

    uint32_t ackno = ack -> ackno;
    acks.push_back(ackno);
    data->arr[int(ackno) - 1] = 1;
//    cout << "int of ack: " << int(ackno) << endl;

    chunkNum ++;

}


int main() {

    int sockfd;
    char buffer[MAXLINE];
    struct sockaddr_in servaddr, cliaddr;
    socklen_t fromlen;
    a = static_cast<packet *>(malloc(sizeof(struct packet)));
    int firstChunkInWindow = 1;
    int numberOfChunks = 1;
    int copyOfNumberOfChunks;
    if(getpid() != 0) {
        data->forked = 0;
    }
    // create a shared memory
    memset(data->arr, 0, sizeof(data->arr));

    string line;
    vector<string> res;
    ifstream myfile("input.txt");
    if (myfile.is_open()) {
        while (getline(myfile, line)) {
            res.push_back(line);
        }
        myfile.close();
    }
    port = atoi(res[0].c_str());
    maxWindow = atoi(res[1].c_str());
    seed = atoi(res[2].c_str());
    prop = stod(res[3].c_str());
    srand(seed);


    // Creating socket file descriptor
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));

    // Filling server information
    servaddr.sin_family = AF_INET; // IPv4
    inet_pton(AF_INET, "0.0.0.0", &servaddr.sin_addr);  // ip address
    servaddr.sin_port = htons(port);

    // Bind the socket with the server address
    if (bind(sockfd, (const struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
        perror("bind failed");
        cout << endl;
        exit(EXIT_FAILURE);
    }

    int n;
    fromlen = sizeof(struct sockaddr_in);

    // if it is the first turn then receive a datagram with file name
    // else receive acknowledgement from user
    // receive requested file name from client
    n = static_cast<int>(recvfrom(sockfd, buffer, MAXLINE, 0, (struct sockaddr *) &cliaddr, &fromlen));

    if (n < 0) {
        perror("Received Failed!!");
        cout << endl;
    }
    buffer[n] = '\0';

    string fileName = buffer;
    cout << "File Name: " << fileName << endl;

    // check existence of requested file
    if (exists_test2(fileName)) {
        // split file
        std::ifstream ifs(fileName);
        std::vector<std::string> vecFilenames;
        split(ifs, 100, vecFilenames);

        // get number of chunks
        string temp = "chunk1.txt";
        while(exists_test2(temp)) {
            numberOfChunks ++;
            temp = "chunk" + to_string(numberOfChunks) + ".txt";
        }
        numberOfChunks --;
        copyOfNumberOfChunks = numberOfChunks;
        sendto(sockfd, to_string(numberOfChunks).c_str(), MAXLINE, MSG_CONFIRM, (const struct sockaddr *) &cliaddr, fromlen);
    } else {
        cout << "File not found" << endl;
        exit(1);
    }

    // fork
    int windowSize = 52;
    deque<pid_t> pids;
    int i;
    int nW = windowSize;

//    cout << "before forked: " << data->forked << endl;
/* Start children */
    for (i = 0; i < nW; i++) {
        timer(mill);
        pids.push_back(fork());
        numberOfChunks--;
        if (pids.back() < 0) {
            perror("fork");
            abort();
        } else if (pids.back() == 0) {
            data->forked = data->forked + 1;
            chunkNum++;
            int currentChunk = firstChunkInWindow + i;
            sendAndReceive(currentChunk, sockfd, cliaddr, fromlen);
            exit(0);
        }
    }
/* Wait for children to exit */
//    cout << "Forked: " << data->forked << endl;
    int status;
    pid_t pid;
    while(!pids.empty()) {
//        cout << "while" << endl;
        pid = wait(&status);
        if(pids.front() == pid) {
//            cout << "if" << endl;
            c:
            if(!pids.empty()) {
                pids.pop_front();
            }
            if(numberOfChunks > 0) {
                timer(mill);
                pids.push_back(fork());
                numberOfChunks--;
//                cout << "Pushed Back" << endl;
                if (pids[nW - 1] < 0) {
                    perror("fork");
                    abort();
                } else if (pids[nW - 1] == 0) {
                    int currentChunk = firstChunkInWindow + nW;
                    sendAndReceive(currentChunk, sockfd, cliaddr, fromlen);
                    exit(0);
                }
                firstChunkInWindow++;
                if(data->arr[firstChunkInWindow - 1]) {
//                    cout << "goto" << endl;
                    goto c;
                }
            }
//            cout << "size of queue: " << pids.size() << endl;
        }
    }
}