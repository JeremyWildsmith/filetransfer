# TCP File Transfer Application

A TCP Client and Server implementation where the client is able to upload files to the remote server.

## Compiling

With build-essentials installed, the makefile can be invoked to produce a build using the following commands:

```
make all
```

This will produce the server and client in the `bin/server/server` and `bin/client/client` directories respectively.

## Usage

1. Host a server, this is the endpoint which will recieve files in an upload operation.

`server -p <port> -d <base_directory>`

2. Initial a file transfer from a client instance

`client -p <port> -s <server> <file 1> <file 2> ... <file n>`

## Running Test Cases
Application is tested with various black-box tests imlemetned via BATS. You can run the tests with the following command:

`make test`

**NOTE: You must have BATS installed on your machine. Refer to BATS directory for installation instructions**

https://github.com/bats-core/bats-core

**Must have BATS 1.2. or later available on machine via command `bats`**

## Demo

Below screen-shot shows the client and server instances interacting.

![Demo Image Here](example/e1.png?raw=true "Title")

## License

All code is MIT Licensed.