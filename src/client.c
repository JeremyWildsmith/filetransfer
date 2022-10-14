/*
 * Author: Jeremy Wildsmith
 * Description: Contains all client related functionality of filetransfer application
 */


#define _LARGE_FILES
#define _GNU_SOURCE

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <libgen.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <math.h>
#include <sys/sendfile.h>

#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/types.h>

#include "common.h"

/// @brief Message to indicate the end of file transmission.
static const char TERMINATE_MESSAGE[] = {'\0', '0', '\0'};


/// @brief Handles client upload of an individual file
/// @param remote Remote socket where file is pushed to
/// @param fd Source file descriptor
/// @param resourceName Name of the resource available via fd argument.
void client_upload(int remote, int fd, const char* resourceName) {
    printf("Upload file: \"%s\" ...\n", resourceName);
    
    off64_t fileSize;

    if ((fileSize = lseek64(fd, 0L, SEEK_END)) == -1) {
        fprintf(stderr, "Error seeking source file to determine length. Cannot upload. Skipping\n");
        return;
    }
    
    if (lseek64(fd, 0L, SEEK_SET) == -1) {
        fprintf(stderr, "Error seeking in file. Cannot upload. Skipping\n");
        return;
    }

    printf("\t- File size: %lu\n", fileSize);
    printf("\t- Name: %s\n", resourceName);
    printf("\t- Uploading...");

    if (write(remote, resourceName, strlen(resourceName) + 1) < 0 ||
        dprintf(remote, "%ld", fileSize) < 0 ||
        write(remote, "\0", 1) < 0) {

        fprintf(stderr, "Failed. Skipping.\n");
        return;
    }

    size_t written = 0;
    while(written != fileSize) {
        ssize_t r = sendfile64(remote, fd, 0, (size_t)(fileSize - written));

        if(r < 0) {
            fprintf(stderr, "File transmission failed. Sendfile operation interrupted.\n");
            return;
        }

        written += r;
    }

    printf("Done. Sent %ld bytes.\n", written);
}

/// @brief Starts file transmission from the client.
/// @param host Host address where a connection is initiated for file transfer.
/// @param port Port of remote server to connect to
/// @param files Path of files to be uploaded
/// @param file_count Size of files array
void client_upload_files(const in_addr_t host, const int port, const char* files[], int file_count) {
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));  
    
    serv_addr.sin_family = AF_INET;  
    memcpy(&serv_addr.sin_addr.s_addr, &host, sizeof(in_addr_t));  
    serv_addr.sin_port = htons(port); 
    
    if(connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        fprintf(stderr, "Unable to connect to server: %s\n", strerror(errno));
        close(sock);
        exit(EXIT_FAILURE);
    }

    char pathBuffer[PATH_MAX];
    for(int i = 0; i < file_count; i++) {
        char* fullPath = resolve_filepath(files[i], pathBuffer);

        if(fullPath == 0) {
            fprintf(stderr, "Skipping file: \"%s\", file not accessible, doesn't exist or is not a regular file.\n", files[i]);
            continue;
        }

        int fd = open(fullPath, O_RDONLY);
        
        if(!fd) {
            fprintf(stderr, "Skipping file \"%s\", could not open for reading: %s\n", fullPath, strerror(errno));
            continue;
        }

        const char* resourceName = basename(fullPath);

        client_upload(sock, fd, resourceName);

        close(fd);
    }

    if (write(sock, TERMINATE_MESSAGE, sizeof(TERMINATE_MESSAGE)) < 0) {
        fprintf(stderr, "Error transmitting end of transmission message.");
        return;
    }

    if(shutdown(sock, SHUT_WR) < 0) {
        fprintf(stderr, "Error gracefully closing client socket.");
    } else {
        char discardBuffer[READ_BUFFER_SIZE];
        while(read(sock, discardBuffer, READ_BUFFER_SIZE) > 0);
    }

    close(sock);
}


/// @brief Main entrypoint of the client
/// @param argc Number of CLI arguments
/// @param argv List of CLI Arguments
/// @return Exit code
int main_client(int argc, char* argv[]) {
    int opt;

    int port = PORT_DEFAULT;
    char* strAddress = 0;
    char* endptr = 0;

    while ((opt = getopt(argc, argv, "p:s:")) != -1) {
        switch (opt) {
            case 's':
                if(strlen(optarg) == 0) {
                    fprintf(stderr, "Error, server address cannot be empty.\n");
                    exit(EXIT_INVALID_ARGUMENT);
                }

                strAddress = malloc(strlen(optarg) + 1);

                if(!strAddress) {
                    fprintf(stderr, "Error, necessary memory allocation failed.");
                    exit(EXIT_FAILURE);
                }

                memcpy(strAddress, optarg, strlen(optarg) + 1);
                break;
            case 'p':
                port = strtol(optarg, &endptr, 10);

                if(port < 0 || port > USHRT_MAX || (port == 0 && endptr == optarg)) {
                    fprintf(stderr, "Error, invalid port provided: \"%s\"; must be number within range [0, 65535]\n", optarg);
                    free(strAddress);
                    exit(EXIT_INVALID_ARGUMENT);
                }
                break;
            default:
                fprintf(stderr, "Error, invalid switch provided.\n");
                free(strAddress);
                exit(EXIT_INVALID_ARGUMENT);
        }
    }

    in_addr_t hostAddress;
    if(strAddress == 0 || (hostAddress = inet_addr(strAddress)) == (in_addr_t)-1)
    {
        fprintf(stderr, "Error, invalid or unspecified host IPv4 Address defined. Use -s flag to define a host address.\n");
        free(strAddress);
        exit(EXIT_INVALID_ARGUMENT);
    }

    if (optind == argc) {
        fprintf(stderr, "Error, no files specified to be uploaded.\n");
        free(strAddress);
        exit(EXIT_INVALID_ARGUMENT);
    }

    printf("Uploading to %s:%d\n", strAddress, port);
    
    client_upload_files(hostAddress, port, (const char**)&argv[optind], argc - optind);

    free(strAddress);
    return 0;
}