/*
 * Author: Jeremy Wildsmith
 * Description: Contains all server related functionality of filetransfer application
 */

#define _LARGE_FILES
#define _GNU_SOURCE         /* See feature_test_macros(7) */

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <limits.h>
#include <math.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/wait.h>

#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })


#include "common.h"

/// @brief Validates a requested filename
/// @param filename Name to be validated.
/// @return Zero if file name is invalid, 1 otherwise.
int validate_filename(char* filename) {
    static const char* invalid = "\\/";

    int i;

    for(i = 0; i < strlen(filename); i++) {
        for(int ii = 0; ii < strlen(invalid); ii++) {
            if(filename[i] == invalid[ii])
                return 0;
        }
    }

    return i != 0;
}

/// @brief Attempts to allocate a file descriptor to an appropriate location to store the specified file
/// @param dirName The directory portion of where the file is to be saved. If it does not exist, it is created with ALLPERMS permissions.
/// @param filename The requsted filename
/// @return -1 if no fd could be allocated, or a valid fd (which must be closed by the caller)
int allocate_free_file_version(const char* dirName, const char* filename) {
    char pathBuffer[PATH_MAX];

    if(mkdir(dirName, ALLPERMS) < 0 && errno != EEXIST) {
        fprintf(stderr, "Error, unable to initialize proper file directory structure for upload.\n");
        return -1;
    }

    if (snprintf(pathBuffer, PATH_MAX, "%s/%s", dirName, filename) < 0) {
        fprintf(stderr, "Error, file was resolved to an invalid name. Aborting upload.\n");
        return -1;
    }

    int fd = open(pathBuffer, O_CREAT | O_RDWR | O_EXCL, DEFFILEMODE);

    if(fd >= 0)
        return fd;

    if(errno != EEXIST)
        return -1;

    struct dirent *dirEnt;
    DIR *dir;

    if ((dir = opendir(dirName)) == 0)
        return -1;

    char* extEnd = strchr(filename, '.');
    int baseNameLen = strlen(filename) ;

    if(extEnd != 0)
        baseNameLen = extEnd - filename;
    
    int maxVerNum = 0;
    while ((dirEnt = readdir(dir)) != NULL)
    {
        if(strncmp(dirEnt->d_name, filename, baseNameLen) == 0 && strncmp(dirEnt->d_name + baseNameLen, "-v", 2) == 0) {
            int verNum;

            if(sscanf(dirEnt->d_name + baseNameLen, "-v%d", &verNum) == EOF)
                continue;
            
            maxVerNum = maxVerNum > verNum ? maxVerNum : verNum;
        }
    }

    if(snprintf(pathBuffer, PATH_MAX, "%s/%.*s-v%d%s", dirName, baseNameLen, filename, maxVerNum + 1, filename + baseNameLen) < 0) {
        fprintf(stderr, "Error, file was resolved to an invalid name. Aborting upload.\n");
    }

    printf("Using version file: %s\n", pathBuffer);

    return open(pathBuffer, O_CREAT | O_RDWR | O_EXCL, DEFFILEMODE);
}

/// @brief Handles a client upload request
/// @param remoteName Name of the remote
/// @param clientSocket Socket corresponding to the remote client
/// @param baseDir Base directory where uploaded contents are stored
/// @return -1 if the recieved message is invalid, 1 if a file upload request was recieved and successfully processed, 0 if an end of transmission
///         message is sent.
int handle_client_upload(const char* remoteName, const int clientSocket, const char* baseDir)  {
    int headerSize = NAME_MAX + log10(sizeof(off64_t)) + 2;
    char headerBuffer[headerSize];

    char* fileName = &headerBuffer[0];
    char* strFileSize = 0;

    for(int i = 0; ; i++) {
        if(i >= headerSize || read(clientSocket, &headerBuffer[i], 1) != 1) {
            fprintf(stderr, "Error reading header data. Aborting connection with client.\n");
            return -1;
        }

        if(headerBuffer[i] == '\0') {
            if(strFileSize != 0)
                break;
            else
                strFileSize = &headerBuffer[i + 1];
        }
    }

    long fileSize;
    char* pEnd;

    if (((fileSize = strtol(strFileSize, &pEnd, 10)) == 0 && pEnd == strFileSize) || fileSize < 0) {
        fprintf(stderr, "Error, reading header data. Invalid file size specified.\n");
        return -1;
    }

    //Empty filename indicates end of upload transmission.
    if (strlen(fileName) == 0)
        return 0;

    if(!validate_filename(fileName)) {
        fprintf(stderr, "Error, reading header data. Invalid file name specified \"%s\".\n", fileName);
        return -1;
    }

    printf("Processing file with size \"%ld\" and name \"%s\"...\n", fileSize, fileName);

    char destBase[PATH_MAX];

    if(snprintf(destBase, PATH_MAX, "%s/%s/", baseDir, remoteName) < 0) {
        fprintf(stderr, "Error computing destination directory.\n");
        return -1;
    }
    
    int fd = allocate_free_file_version(destBase, fileName);

    if(fd < 0) {
        fprintf(stderr, "Error allocating destination file.\n");
        return -1;
    }

    if(fileSize > 0)
    {
        size_t expected = fileSize;
        size_t read = 0;
        char recvBuffer[READ_BUFFER_SIZE];
        while(expected > 0 && (read = recv(clientSocket, &recvBuffer[0], min(READ_BUFFER_SIZE, expected), 0)) != -1)
        {
            expected -= read;

            printf("\rDownloading file: %.2Lf%% Complete.", 100 * (long double)(fileSize - expected) / fileSize);

            size_t written = 0;
            while(written < read) {
                ssize_t numWrite = write(fd, &recvBuffer[written], read - written);

                if(numWrite == -1) {
                    fprintf(stderr, "Error writing to destination file: %s\n", strerror(errno));
                    close(fd);
                    return -1;
                }

                written += numWrite;
            }
        }

        printf("\n");

        if(expected != 0) {
            fprintf(stderr, "Error reading all file contents from stream. File missing data.");
            return -1;
        }
    }

    printf("Done processing file.\n");

    close(fd);

    return 1;
}

/// @brief Handles forks a subprocess to handle the client connection, reading requests for uploading files.
/// @param remoteName Name of the remote connection, used for organizing file uploads by remote
/// @param clientSocket Socket for communicating with remote client
/// @param baseDir Base directory where file uploads will be nested into
/// @return Upon success, will return 0 in the subprocess managing the client, a non-zero process is returned by the parent process.
///         If an error occures (due to forking errors), then -1 is returned.
int handle_client(const char* remoteName, const int clientSocket, const char* baseDir) {
    pid_t child = fork();

    if(child < 0) {
        fprintf(stderr, "Error, aborting handling of client, failed to fork.\n");
        return -1;
    } else if(child > 0)
        return child; //Parent process can return, child process will handle client.
    
    printf("Handling remote: %s\n", remoteName);

    int status;

    while((status = handle_client_upload(remoteName, clientSocket, baseDir)) == 1);

    if(status < 0)
        fprintf(stderr, "Error occured processing entire upload request. Connection terminated prematurely.\n");
    else
        printf("Upload transmission completed.\n");

    return 0;
}

/// @brief Runs the server using the defined base directory and listening on the specified port.
/// @param baseDirectory Base directory where all uploaded files will be placed
/// @param port Port where server will listen for new incoming connections
void run_server(const char* baseDirectory, const int port) {
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));  
    
    serv_addr.sin_family = AF_INET;  

    //Bind to all available interfaces
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port); 
    
    if(bind(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        fprintf(stderr, "Error occured while attempting to bind to interfaces: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if(listen(sock, 20) < 0) {
        fprintf(stderr, "Error listening: %s\n", strerror(errno));
        close(sock);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in clnt_addr;
    socklen_t clnt_addr_size = sizeof(clnt_addr);

    int lastClientHandler = -1;

    while(lastClientHandler != 0) {
        printf("Waiting to accept...\n");
        int clientSocket = accept(sock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);
        if(clientSocket < 0) {
            
            if(errno == EINTR)
                break;
            
            fprintf(stderr, "Error accepting client: %s\n", strerror(errno));
            continue;
        }


        char ipbuffer[INET_ADDRSTRLEN];
        if (inet_ntop(clnt_addr.sin_family, &clnt_addr.sin_addr, ipbuffer, INET_ADDRSTRLEN) == 0) {
            fprintf(stderr, "Error identifying remote. Terminating connection with remote.\n");
            close(clientSocket);
            continue;
        }

        printf("Established connection with remote: %s\n", ipbuffer);

        lastClientHandler = handle_client(ipbuffer, clientSocket, baseDirectory);

        if (lastClientHandler < 0)
            fprintf(stderr, "Error handling new client, fork failed %s", strerror(errno));
        else if (lastClientHandler == 0) {
            if(shutdown(clientSocket, SHUT_WR) < 0) {
                fprintf(stderr, "Error gracefully closing client socket.");
            } else {
                char discardBuffer[READ_BUFFER_SIZE];
                while(read(sock, discardBuffer, READ_BUFFER_SIZE) > 0);
            }

            close(clientSocket); //If lastClientHandler == 0, then we returned via the fork, which owns the clientSocket.
            exit(EXIT_SUCCESS);
        }
    }
    
    if(lastClientHandler != 0)
        printf("Server shutting down...\n");

    close(sock);

    printf("Waiting for pendings transfers to complete...\n");

    //Wait for all child processes to finish.
    int status = 0;
    while (wait(&status) > 0 || errno == EINTR);

    printf("Done.\n");
}


void termination_handler(int signum)
{
    //Do nothing. accept
}

/// @brief Main entrypoint of the server
/// @param argc Number of CLI arguments
/// @param argv List of CLI Arguments
/// @return Exit code
int main_server(int argc, char* argv[]) {
    int port = PORT_DEFAULT;
    char baseDir[PATH_MAX] = ".";
    char* endptr = 0;
    char opt;

    while ((opt = getopt(argc, argv, "p:d:")) != -1) {
        switch (opt) {
            case 'd':
                if(strlen(optarg) == 0) {
                    fprintf(stderr, "Error, server directory argument cannot be empty.\n");
                    exit(EXIT_INVALID_ARGUMENT);
                }

                if(!resolve_dirpath(optarg, baseDir)) {
                    fprintf(stderr, "Error, unable to resolve specified base directory to valid path: \"%s\"\n", baseDir);
                    exit(EXIT_INVALID_ARGUMENT);
                }
                
                break;
            case 'p':
                port = strtol(optarg, &endptr, 10);

                if(port < 0 || port > USHRT_MAX || (port == 0 && endptr == optarg)) {
                    fprintf(stderr, "Error, invalid port provided: \"%s\"; must be number within range [0, 65535]\n", optarg);
                    exit(EXIT_INVALID_ARGUMENT);
                }
                break;
            default:
                fprintf(stderr, "Error, invalid switch provided\n");
                exit(EXIT_INVALID_ARGUMENT);
        }
    }


    struct sigaction new_action;
    new_action.sa_handler = termination_handler;
    
    sigemptyset(&new_action.sa_mask);
    sigaction(SIGINT, &new_action, NULL);

    printf("Using base directory: %s\n", baseDir);
    printf("Hostig on port: %d\n", port);

    run_server(baseDir, port);

    return 0;
}