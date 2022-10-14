
startup_server() {
    echo $WORK_DIR

    WORK_SERVER=$WORK_DIR/server_work
    mkdir -p $WORK_SERVER
    
    $SERVER_TEST -p $TEST_PORT -d $WORK_SERVER&
    SERVER_PID=$!
}


validate_server() {
    diff $WORK_SERVER/127.0.0.1/ $WORK_CLIENT
}

shutdown_server() {
    echo $SERVER_PID
    
    if [[ -n "$SERVER_PID" ]]; then
        while true; do
            if kill -2 $SERVER_PID; then 
                sleep 1
            else
                break
            fi
        done

        unset SERVER_PID
    fi
}
