/**
 * @file srftp.cpp
 * @author alonperl vladi
 * @date 14 June 2015
 * @information This file is the client code for OS-EX5-2015
 */
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <bits/stat.h>
#include <netinet/in.h>
#include <limits>

using namespace std;
#define MSG_ERR_USAGE "Usage: clftp server-port server-hostname file-to-transfer filname-in-server"
#define EXPECTED_ARGC 5
#define USAGE_PORT_POS 1
#define USAGE_SERVER_HOSTNAME_POS 2
#define USAGE_FILE_TO_TRANSFER_POS 3
#define USAGE_FILE_NAME_IN_SERVER_POS 4
#define SERVER_IS_DOWN 0
#define PORT_MIN 1
#define PORT_MAX 65535
#define SYS_ERROR(system_call) cerr << "Error: function:" << system_call << " errno: " << errno << "\n"; exit(1);
#define NET_ERROR(system_call) cerr << "Error: function:" << system_call << " errno: " << h_errno << "\n"; exit(1);
#define MAX_FILE_SIZE_MSG
#define BLOCK_SIZE 1024
#define DATA_LEFT_TO_SEND 0
#define CHECK_READ(readval); if((readval)){throw;}
#define FAIL -1

class netReadException {};
class netWriteException {};
/**
 * this function check if the arguments are legal and sets them.
 * if not legal returns false, else returns true.
 */
bool check_arguments(int argc, char** argv, struct stat *fileStat)
{
    // check for valid number of parameters
    if(argc != EXPECTED_ARGC)
    {
        return false;
    }
    // check for valid server-port (1-65535)
    int port = atoi(argv[USAGE_PORT_POS]);
    if(port < PORT_MIN || port > PORT_MAX)
    {
        return false;
    }
    // check file-to-transfer is a directory or it doesnt exist
    char* path = realpath(argv[USAGE_FILE_TO_TRANSFER_POS], NULL);
    if(path == NULL)
    {
        free(path);
        return false;
    }
    if(stat(argv[USAGE_FILE_TO_TRANSFER_POS], fileStat) == FAIL)
    {
        free(path);
        return false;
    }
    if(S_ISDIR(fileStat->st_mode))
    {
        free(path);
        return false;
    }
    free(path);

    //TODO : check if file is accessible ??

    return true;
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
        if(read_bytes  < 1)
        {
            netReadException e;
            throw e;
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
        if(read_bytes  < 1)
        {
            netWriteException e;
            throw e;
        }
    }
}

int main(int argc, char** argv) {

    struct stat fileStat = {0};
    //check args
    if (!check_arguments(argc, argv, &fileStat)) {
        cout << MSG_ERR_USAGE << endl;
        exit(1);
    }
    // get params from usage args
    // server-port[1] server-hostname[2] file-to-transfer[3] filname-in-server[4]
    int port = atoi(argv[USAGE_PORT_POS]);
    struct hostent *server = gethostbyname(argv[USAGE_SERVER_HOSTNAME_POS]);
    if(server == NULL)
    {
        NET_ERROR("gethostbyname");
    }


    // creating socket
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        SYS_ERROR("socket");
    }
    struct sockaddr_in server_address;
    bzero((char *) &server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    bcopy((char *) server->h_addr, (char *) &server_address.sin_addr.s_addr, server->h_length);
    server_address.sin_port = htons(port);

    //connect to server
    if (connect(server_socket, ((struct sockaddr *) &server_address),
                sizeof(server_address)) < 0) {
        close(server_socket);
        SYS_ERROR("connect");
    }

    // get the max_file_size from the server
    off_t max_file_size;
    try {
        readToBuf(server_socket, &max_file_size, sizeof(off_t));
    } catch (const netReadException& e) {
        close(server_socket);
        SYS_ERROR("read");
    }
    bool prepareToSend = true;
    if (max_file_size < fileStat.st_size) {
        prepareToSend = false;
        writeFromBuf(server_socket, &prepareToSend, sizeof(bool));
        close(server_socket);
        cout << "Transmission failed: too big file" << endl;
        return 0;
    }
    //transmission starting, tell the server to prepare
    writeFromBuf(server_socket, &prepareToSend, sizeof(bool));

    // file_to_transfer size is ok, starting transmission
    size_t file_name_size = strlen(argv[USAGE_FILE_NAME_IN_SERVER_POS]) + 1;
    char *file_name = argv[USAGE_FILE_NAME_IN_SERVER_POS];
    try {
        //first send the size of the file-name-in-server and after send the name.
        writeFromBuf(server_socket, &file_name_size, sizeof(size_t));
        writeFromBuf(server_socket, file_name, file_name_size);
        // send the file size
        writeFromBuf(server_socket, &fileStat.st_size, sizeof(off_t));
    } catch(...) {
        close(server_socket);
        NET_ERROR("write");
    }

    // open file in server directory
    ifstream ifs;
    ifs.open(argv[USAGE_FILE_TO_TRANSFER_POS]);
    if (!ifs.good()) {
        close(server_socket);
        SYS_ERROR("open");
    }
    try {
        // read file to blocks and send each
        size_t off;
        char data_to_send[BLOCK_SIZE];
        for (off = fileStat.st_size; off > BLOCK_SIZE; off -= BLOCK_SIZE) {
            ifs.read(data_to_send, BLOCK_SIZE)
            CHECK_READ(!ifs.good());
            writeFromBuf(server_socket, &data_to_send, BLOCK_SIZE);
        }
        if (off > DATA_LEFT_TO_SEND) {
            ifs.read(data_to_send, off)
            CHECK_READ(!ifs.good());
            writeFromBuf(server_socket, &data_to_send, off);
        }
        ifs.close();
        close(server_socket);
    } catch(const netWriteException& e) {
        ifs.close();
        close(server_socket);
        NET_ERROR("write");
    } catch(...) {
        ifs.close();
        close(server_socket);
        SYS_ERROR("read");
    }
    return 0;
}