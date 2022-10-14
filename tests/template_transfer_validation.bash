#!/bin/bash


# Add switch for testing with a remote machine...

load server_local

run_client() {
    $CLIENT_TEST -p $TEST_PORT -s 127.0.0.1 $@
}

setup() {
    if [[ ! -f "$CLIENT_TEST" ]]; then
        echo "Client binary not available. Define with CLIENT_TEST"
        return 1
    fi

    if [[ ! -f "$SERVER_TEST" ]]; then
        echo "Server binary not available. Define with SERVER_TEST"
        return 1
    fi

    TEST_PORT=9999

    WORK_DIR=`mktemp -d`
    WORK_CLIENT=$WORK_DIR/client_work
    
    mkdir -p $WORK_CLIENT

    startup_server
}

teardown() {
    [[ -n "$WORK_DIR" ]] && rm -rf $WORK_DIR
    shutdown_server
}
