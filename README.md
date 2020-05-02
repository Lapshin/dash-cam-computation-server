# Computation server

This is a computation server for dash cam footage. It calculates indicators
metrics from received file and send results back.

## Building the project

In the project root:

```
mkdir build
cd build
cmake ../
make
```

## Running the tests

After project built, in the build directory:

```
make test
```

## Running the server

From the build directory:

```
./computation-server
```

If you want to use your own library:

```
LD_PRELOAD=<PATH_TO_YOUR_LIB> ./computation-server
```

or

```
LD_LIBRARY_PATH=<PATH_TO_DIR_WITH_LIB> ./build/server
```

## Usage

```
$ ./computation-server --help   
Usage: computation-server [OPTION...]
This is a computation server which receives video files fromdash cams,
calculating a number of computation-intensive driver behaviour indicators, and
sending these back to the vehicle in the same connection

  -d, --daemonize            Run as a daemon
  -i, --ip=address           Server ip address (default: localhost)
  -p, --port=port            Server TCP port (default: 5000)
  -q, --quiet                Print only error messages
  -V, --verbose              Print debug messages
  -?, --help                 Give this help list
      --usage                Give a short usage message
      --version              Print program version

Mandatory or optional arguments to long options are also mandatory or optional
for any corresponding short options.

Report bugs to <alexeyfonlapshin@gmail.com>.

```

## Dash cam simulation

If you want to simulate dash cam client use command from build dir:

```
./tests/dash_cam -s 80000000 -f 1000 | pv -L 2621440 | nc -q 2 localhost 5000 | ./tests/dash_cam -r
```

- It will print 4 indicator results into stdout.
- Video file size is 80000000 bytes and frame size is 1000. Frame size - it is minimum size of data indicator could process.
- Video file transmitting speed limit is 20 Mbit/s, you can tune it in `pv` argument.
- Indicators in the project's library processing data with speed limit 5 MB/s.

## Server design

The design is very simple. It is main thread which operates with server socket able to 
establish one IPv4 TCP connection per session. And X thread workers for indicators calculation, where X number of indicators from the library.
After all initializations, in the loop we have simple steps:

- Allocate all data which could be alocated before running main loop 
- Waiting for new client until it appears
- Arm timer for 35 seconds
- Accept connection
- Receive and process data in parallel
- After calculation finished, send response to the dash cam
- Cleanup all used resources (free buffers, close socket ...)


If Timer expires while processing video file, or any error appears - all progress will dismiss and program will go to the second step 

![](doc/diagram.png)


### Communication between computation server and dash cam

There are two types of messages: message from dash cam with 
video file inside AND message for dash cam with calculated metrics inside

Both messages used the same data structure which declared in include/dash_cam.h

## Author

Alexey Lapshin

## TODO list

- [ ] cmake: implement target Debug and Release
- [ ] cmake: implement make install
- [ ] tests: detect if port already taken, get random unused port
- [ ] implement crc32 hash sum, for checking video files
- [ ] check if indicators count more than X
