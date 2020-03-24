## Webserver

This is a (very) limited functionality webserver intended as a learning exercise in concurrent
programming. The server's main feature is the ability to run in different modes:
* **Single process**: process each incoming requests sequentially in the main server loop.
* **Fork**: fork a new process for each incoming request.
* **Thread**: create a new thread for each incoming request.
* **Thread pool**: creates a pool of threads, each listening for a new requests.
* **Thread queue**: queue incoming requests, which are processed by a thread pool.

The server will only respond to `GET` requests using HTTP version 1.0. It does not currently parse or
respect any http headers.

### Run
To run the server:
```bash
./server -t thread-queue -p 3711
```
Try curling a file:
```bash
curl -o test.jpg localhost:3711/kang-beach.jpg
```
Try listing a directory:
```bash
curl localhost:3711/tmp
```

### Development
The server has been developed exclusively on linux using gcc. No effort has been made to make it
portable to other operating systems or compilers.

To build:
```bash
make
```
Running with valgrind
```bash
valgrind ./server -t thread-queue -p 3711 -p 3712
```
