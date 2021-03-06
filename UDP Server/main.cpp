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
#include <chrono>
#include <thread>

#define PORT 54000
#define MAXLINE 1024

using namespace std;

const int size1MB = 10;  // 1KB, 1MB = 10240
bool firstTurn = true;
int chunkNum = 1;
struct packet *a;
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
    char data[1000]; /* Not always 500 bytes, can be less */
};

/* Ack-only packets are only 8 bytes */
struct ack_packet {
//    uint16_t cksum; /* Optional bonus part */// Driver code
    uint16_t len;
    uint32_t ackno;
};

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

// set a timer
void timer(unsigned int sec)
{
    this_thread::sleep_for(chrono::milliseconds(sec));
}

// send a chunk
void sendChunk(string nextChunk, int sockfd, struct sockaddr_in cliaddr, socklen_t fromlen) {

    // make data packet struct
    a->seqno = static_cast<uint32_t>(getNextSeqNumber(a->seqno));
    char *charNextChunk = const_cast<char *>(nextChunk.c_str());

    // copy file to packet data buffer
    int size = copyToBuffer(charNextChunk, a->data);
    a->len = static_cast<uint16_t>(size);

    double ran = (double) rand() / (double)(RAND_MAX);
    cout << seed << "\t" << ran << "\t" << prop << endl;

    while(ran <= prop) {
        ran = (double) rand() / (double)(RAND_MAX);
        cout << "*************packet no " << charNextChunk << " is lost" << endl;
        timer(100);
    }
    // send a chunk to client
    sendto(sockfd, a, MAXLINE, MSG_CONFIRM, (const struct sockaddr *) &cliaddr, fromlen);
    cout << "Server sent a chunk" << chunkNum << endl;

    // increase chunk number
    chunkNum ++;
}

int main() {
    int sockfd;
    char buffer[MAXLINE];
    struct sockaddr_in servaddr, cliaddr;
    socklen_t fromlen;
    struct ack_packet *ack;
    ack = static_cast<ack_packet *>(malloc(sizeof(struct ack_packet)));
    a = static_cast<packet *>(malloc(sizeof(struct packet)));

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

    while (true) {

        // if it is the first turn then receive a datagram with file name
        // else receive acknowledgement from user
        if (firstTurn) {

            firstTurn = false;

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

                struct packet *b;
                b = static_cast<packet *>(malloc(sizeof(struct packet)));
                b->seqno = 0;
                int s = copyToBuffer(const_cast<char *>("chunk1.txt"), b->data);
                b->len = static_cast<uint16_t>(s);
                // send first chunk
                sendto(sockfd, b, MAXLINE, MSG_CONFIRM, (const struct sockaddr *) &cliaddr, fromlen);
                cout << "Server sent first chunk" << chunkNum << endl;

                chunkNum++;
            } else {
                cout << "File not found" << endl;
                break;
            }
        } else {
            // receive acknowledgement from client

            n = static_cast<int>(recvfrom(sockfd, ack, MAXLINE, 0, (struct sockaddr *) &cliaddr, &fromlen));
            if (n < 0) {
                perror("Received Failed!!");
                cout << endl;
            }
            // check if client received the chunk
            if(ack -> ackno == chunkNum - 1) {
                // packet reached client successfully
                string nextChunk = "chunk" + to_string(chunkNum) + ".txt";

                // check existence of file to indicate that all chunks have been sent
                if (exists_test2(nextChunk)) {
                    // send next chunk
                    sendChunk(nextChunk, sockfd, cliaddr, fromlen);
                } else {
                    // all chunks have been sent
                    cout << "All chunks have been successfully sent" << endl;

                    // send a packet to inform client that all file chunks have been sent
                    // make data packet struct
                    struct packet *c;
                    c = static_cast<packet *>(malloc(sizeof(struct packet)));
                    c->len = 1;
                    c->seqno = static_cast<uint32_t>(getNextSeqNumber(c->seqno));
                    strcpy(c->data, "All chunks sent");
                    sendto(sockfd, c, sizeof(c->data), MSG_CONFIRM, (const struct sockaddr *) &cliaddr, fromlen);
                    firstTurn = true;
                    break;
                }
            } else {
                // send chunk again
                chunkNum --;
                string nextChunk = "chunk" + to_string(chunkNum) + ".txt";
                sendChunk(nextChunk, sockfd, cliaddr, fromlen);
            }
        }
    }

    // remove chunks
    for(int i = 1; i <= chunkNum; i++) {
        string fileN = "chunk" + to_string(i) + ".txt";
        remove(fileN.c_str());
    }
    return 0;
}
