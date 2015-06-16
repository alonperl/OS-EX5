/**
 * @file srftp.cpp
 * @author alonperl vladi
 * @date 14 June 2015
 * @information This file is the server code for OS-EX5-2015
 *              Can accept multiple connection by creating each client a new thread.
 */
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string.h>
#include <stdlib.h>
#include <limits>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <bits/stat.h>
#include <netinet/in.h>
#include <pthread.h>
#include <vector>
#include <pthread.h>


using namespace std;

#define PORT_ARG 1
#define MAX_SIZE_ARG 2
#define MAX_PORT_NUM (unsigned int)65535
#define MIN_PORT_NUM (unsigned int)1
#define NUM_OF_ARGS 3
#define MAX_FILE_SIZE sizeof(off_t)
#define MIN_FILE_SIZE 0
#define FAILURE -1
#define MAX_HOST_NAME 128
#define MAX_PENDING_CONN 5
#define SUCCESS 0
#define SYS_ERROR(system_call) cerr << "Error: function:" << system_call << " errno: " << errno << endl; exit(1);
#define PACKET_SIZE 1024
#define NUMBER_OF_CONNECITONS 5
#define NO_BYTES_RECIEVED 1
#define BYTES_LEFT_TO_RECIEVE 0

//struct to store the server info form the args.
struct arguments_struct
{
    unsigned int port;
    off_t maxSize;
};

struct arguments_struct server_info;
/**
 * this function check if the arguments are legal and sets them.
 * if not legal returns false, else returns true.
 */
bool checkArgs(int argc, char const* argv[])
{
    if(argc != NUM_OF_ARGS)
    {
        return false;
    }
    server_info.port = (unsigned int)stol(argv[PORT_ARG]);
    if(server_info.port > MAX_PORT_NUM || server_info.port < MIN_PORT_NUM)
    {
        return false;
    }
    if(atoi(argv[MAX_SIZE_ARG]) <= MIN_FILE_SIZE)
    {
        return false;
    }
    server_info.maxSize = (unsigned int)atoi(argv[MAX_SIZE_ARG]);
    return true;
}
/**
 * This function initiate all the server params for starting the listening
 * and gets the clients connections.
 * @param portNum port number for the server from the arguments
 */
int createServer(unsigned int portNum)
{
    struct hostent *host;
    char hostName[MAX_HOST_NAME + 1];
    gethostname(hostName, MAX_HOST_NAME);
    host = gethostbyname(hostName);
    if(host == NULL)
    {
        SYS_ERROR("gethostbyname");
    }

    //set socket address
    struct sockaddr_in socket_addr;
    socket_addr.sin_family = host->h_addrtype;
    socket_addr.sin_port = htons(server_info.port);
    memcpy(&socket_addr.sin_addr, host->h_addr, host->h_length);

    //creating new socket.
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(server_socket < 0)
    {
        SYS_ERROR("socket");
    }
    // bind the server
    if(bind(server_socket, (struct sockaddr*) &socket_addr, sizeof(struct sockaddr_in)) < 0)
    {
        close(server_socket);
        SYS_ERROR("bind");
    }
    //listen to clients
    listen(server_socket, MAX_PENDING_CONN);
    return server_socket;
}
/**
 * This function is receiving the data of size "size" from the socket
 * @param socket the socket file descriptor for the current client.
 * @param buf the buffer to store the data to send
 * @param size size of the buffer (amount of data to send)
 */
void readToBuf(int socket, void* buf, size_t size)
{
    int read_bytes = 0;
    auto msg_buf = (char*)buf;
    for(size_t off = 0; off < size; off += read_bytes)
    {
        read_bytes = read(socket, msg_buf + off, size - off);
        if(read_bytes  < NO_BYTES_RECIEVED)
        {
            SYS_ERROR("read");
        }
    }
}
/**
 * This function is sending the data of size "size" to the socket
 * @param socket the socket file descriptor for the current client.
 * @param buf the buffer that store the data to send
 * @param size size of the buffer (amount of data to send)
 */
void writeFromBuf(int socket, void* buf, size_t size)
{
    int read_bytes;
    auto msg_buf = (char*)buf;
    for(size_t off = 0; off < size; off += read_bytes)
    {
        read_bytes = write(socket, msg_buf + off, size - off);
        if(read_bytes  < NO_BYTES_RECIEVED)
        {
            SYS_ERROR("write");
        }
    }
}
/**
 * This function handle with a new client, deal with all the files recieve
 * The flow of the network packets after connection is :
 * Server -> Max-File-Size
 * Client -> True\False if starting to send file
 * Client -> File-Name-Size
 * Client -> File-Name
 * Client -> File-size
 * Client XXX-> File divided into packets
 * @param param client socket that got while accept
 */
void* client_handler(void * param)
{
    int clientSocket = *((int *) param);
    ofstream file;
    char buf[PACKET_SIZE];
    size_t off;
    off_t fileSize;
    size_t fileNameSize;
    //send to the client the max-file-size
    writeFromBuf(clientSocket, &server_info.maxSize, sizeof(off_t));
    bool prepareToRecieve = false;
    //recieve boolean to know if client starting to send the file
    readToBuf(clientSocket, &prepareToRecieve, sizeof(bool));
    if(prepareToRecieve)
    {
        //get the size of the file-name to save
        readToBuf(clientSocket, &fileNameSize, sizeof(size_t));
        char file_name[fileNameSize];
        //get the file name to save
        readToBuf(clientSocket, file_name, fileNameSize);
        //get the file size
        readToBuf(clientSocket, &fileSize, sizeof(off_t));
        //create the file in the server
        file.open(file_name);
        if(!file.good())
        {
            SYS_ERROR("open");
        }
        //collect all the packets into the file
        for(off = fileSize; off > PACKET_SIZE; off -= PACKET_SIZE)
        {
            readToBuf(clientSocket, buf, PACKET_SIZE);
            file.write(buf, PACKET_SIZE);
        }
        //if left data to store;
        if(off > BYTES_LEFT_TO_RECIEVE)
        {
            readToBuf(clientSocket, buf, off);
            file.write(buf, off);
        }
        file.close();
    }
    close(clientSocket);
    delete((int*)param);
    return nullptr;
}
/**
 * This function is the main function that run in the backround of the server
 * this will be called after creating the socket, after start the listening.
 * It will open a new thread to each client that will ask to connect.
 * @param serverSocket - integer represents socket descriptor
 *                       that was allocated when the socket called. 
 */
void runServer(int serverSocket)
{
    //listens for clients.
    while(true)
    {
        int *clientSocket = new int(accept(serverSocket, NULL, NULL));
        if(*clientSocket == FAILURE)
        {
            SYS_ERROR("accept");
        }
        pthread_t clientThread;
        pthread_create(&clientThread, NULL, &client_handler, clientSocket);
//        connections.push_back(clientThread);
    }
}

int main(int argc, char const* argv[])
{
    //checks and set args.
    if(!checkArgs(argc, argv))
    {
        cout << "Usage: srftp server-port max-file-size" << endl;
        return FAILURE;
    }
    //creating new socket.
    int serverSocket = createServer(server_info.port);
    runServer(serverSocket);
    return 0;
}
