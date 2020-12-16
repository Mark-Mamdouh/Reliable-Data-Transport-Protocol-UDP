#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <iostream>
#include <vector>
#include <fstream>

#define PORT 54000
#define MAXLINE 1024

using namespace std;

string ipAddress;
int port;
int clientPort;
string fileName;
int window;

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

// copy data to file
void copyDataToFile(char data[], char *fileName, int size) {
    ofstream out(fileName);
    if(! out)
    {
        cout<<"Cannot open output file\n";
    }
    out.write(data, size);
    out.close();
//    int i = 0;
//    while(i < size) {
//        out << data[i];
//        i ++;
//    }
//    out.close();
//    ofstream myfile;
//    myfile.open (fileName);
//    int i = 0;
//    while(i < size) {
//        myfile << data[i];
//        i ++;
//    }
//    myfile.close();
}

// join
void join(vector<string> &vecFilenames, ostream &outStream) {
    for(int n = 0; n < vecFilenames.size(); ++n) {
        ifstream ifs(vecFilenames[n]);
        outStream << ifs.rdbuf();
    }
}

int main() {
    int sockfd;
    struct sockaddr_in servaddr{};
    socklen_t fromlen;
    bool success = false;
    struct packet *a;
    a = static_cast<packet *>(malloc(sizeof(struct packet)));
    struct ack_packet *b;
    b = static_cast<ack_packet *>(malloc(sizeof(struct ack_packet)));

    string line;
    vector<string> res;
    freopen("input2.txt" , "r" , stdin);
    while (getline(cin , line)) {
        res.push_back(line);
    }
    ipAddress = res[0];
    port = atoi(res[1].c_str());
    clientPort = atoi(res[2].c_str());
    fileName = res[3];
    window = atoi(res[3].c_str());

    // Creating socket file descriptor
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));

    // Filling server information
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = INADDR_ANY;

    int n;
    fromlen = sizeof(struct sockaddr_in);
    int numChunks = 0;

    // request file from server
    sendto(sockfd, fileName.c_str(), sizeof(fileName), MSG_CONFIRM, (const struct sockaddr *) &servaddr, sizeof(servaddr));
//    cout << "Client sent a datagram with a file name: " << fileName << endl;

    while(true) {

        // receive a chunk from server
//        memset(a->data, '0', sizeof(a->data));
        n = static_cast<int>(recvfrom(sockfd, a, MAXLINE, MSG_WAITALL, (struct sockaddr *) &servaddr, &fromlen));

        if(n >= 0) {
            // receiving success
//            cout << "Server sent a chunk with: \n1- len: " << a->len
//                 << "\n2- seqno: " << a->seqno
//                 /*<< "\n3- data: " << a->data */<< endl;
//
//            cout << "data size: " << sizeof(a->data);

            if(a->len != 1) {
                // chunk received
                numChunks ++;
                if(numChunks == 2555) {
                    cout << "";
                }

                // copy received data to chunk file
                string nextChunk = "chunk" + to_string(numChunks) + ".txt";
                char *charNextChunk = const_cast<char *>(nextChunk.c_str());
                copyDataToFile(a->data, charNextChunk, a->len);

                // send ack packet
                b -> len = 8;
                b -> ackno = static_cast<uint32_t>(numChunks);
                sendto(sockfd, b, sizeof(b), MSG_CONFIRM, (const struct sockaddr *) &servaddr, sizeof(servaddr));
            } else {
                // not a chunk only confirmation message from server
                // that all chunks have successfully been sent
                success = true;
                break;
            }
        } else {
            // failed to receive
            perror("Received Failed!!");
            cout << endl;
            break;
        }
    }
    // close the socket
    close(sockfd);

    // join file chunks if successful
    if(success) {
        std::vector<std::string> vecFilenames;
        for(int i = 1; i <= numChunks ; i++) {
            vecFilenames.push_back("chunk" + to_string(i) + ".txt");
        }
        std::string filenameAfter = fileName;
        std::ofstream ofs(filenameAfter, std::ios::trunc);
        join(vecFilenames, ofs);
    }

    // remove chunks
    for(int i = 1; i <= numChunks; i++) {
        string fileN = "chunk" + to_string(i) + ".txt";
        remove(fileN.c_str());
    }
    return 0;
}
