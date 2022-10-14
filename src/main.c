#include <stdio.h>

#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#include "server.h"
#include "client.h"

int main(int argc, char* argv[]) {

#ifdef MODE_SERVER
    return main_server(argc, argv);
#else
    return main_client(argc, argv);
#endif

}