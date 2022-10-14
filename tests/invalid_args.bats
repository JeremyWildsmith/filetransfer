#!/usr/bin/env bats

# Using basic command & server invoke arguments
load template_basic

# Test Case 1:
# Upload a file with no content, file should appear on server with correct
# name and size.

@test "Server - Invalid Flags" {
  skip
  run $SERVER_TEST -z
  [ "$status" -eq 2 ]
}

@test "Server - Invalid Port" {
  skip
  run $SERVER_TEST -p 65536
  [ "$status" -eq 2 ]

  run $SERVER_TEST -p abc
  [ "$status" -eq 2 ]

  run $SERVER_TEST -p
  [ "$status" -eq 2 ]

  run $SERVER_TEST -d
  [ "$status" -eq 2 ]
}

@test "Server - Invalid Working Directory (Inaccessible Path)" {
  WORK_DIR=`mktemp -d`
  cd $WORK_DIR
  touch reserved_name
  run timeout --preserve-status -s2 10 $SERVER_TEST -d ./reserved_name

  [ "$status" -eq 2 ]

  rm -rf $WORK_DIR
}


@test "Server - Invalid Working Directory (At Max Path)" {
  skip
  WORK_DIR=`mktemp -d`
  cd $WORK_DIR

  path_buffer=
  for i in {1..2047}; do
    path_buffer=./$path_buffer
  done

  path_at_limit=$path_buffer/
  
  run timeout --preserve-status -s2 10 $SERVER_TEST -d $path_at_limit
  code_a="$status"
  
  rm -rf $WORK_DIR

  [ "$code_a" -eq 0 ]
}

@test "Server - Invalid Working Directory (Exceed Max Path)" {
  skip
  WORK_DIR=`mktemp -d`
  cd $WORK_DIR

  # 4000
  path_buffer=
  for i in {1..2047}; do
    path_buffer=./$path_buffer
  done

  path_exceed_limit=$path_buffer//
 
  run timeout --preserve-status -s2 10 $SERVER_TEST -d $path_exceed_limit
  code_b="$status"
  
  rm -rf $WORK_DIR

  [ "$code_b" -eq 2 ]
}

@test "Server - Invalid Working Directory (Expand Exceed Max Path)" {
  WORK_DIR=`mktemp -d`
  cd $WORK_DIR

  # 4000
  path_buffer="~"
  end=$(echo $HOME | wc -c)
  num_gen=$(( 4096 - $end + 3 ))
  for i in $(seq 1 $num_gen); do
    path_buffer=$path_buffer/
  done
 
  run timeout --preserve-status -s2 10 $SERVER_TEST -d $path_buffer
  code_b="$status"
  
  rm -rf $WORK_DIR

  [ "$code_b" -eq 2 ]
}


@test "Server - Default Args (Port Check)" {
  skip
  $SERVER_TEST &
  pid=$!

  run nc -z -v -w5 127.0.0.1 8888
  
  while true; do
    if kill -2 $pid; then 
        sleep 1
    else
        break
    fi
  done

  [ "$status" -eq 0 ]
}

@test "Server - Default Args (Working Directory Check)" {
  skip
  WORK_DIR=`mktemp -d`
  mkdir -p $WORK_DIR/client
  mkdir -p $WORK_DIR/server

  cd $WORK_DIR/server
  $SERVER_TEST &

  pid=$!
  
  sleep 1
  run nc -z -v -w5 127.0.0.1 8888

  touch $WORK_DIR/client/test

  $CLIENT_TEST -s 127.0.0.1 -p 8888 $WORK_DIR/client/test

  while true; do
    if kill -2 $pid; then 
        sleep 1
    else
        break
    fi
  done

  num_gen=`find $WORK_DIR/server -iname test | wc -l`

  rm -rf $WORK_DIR

  [[ $num_gen -eq 1 ]]
}
