# Loop-Based Dual-Layer Image Warping



### Requirements
Several tools are required for the compliation of the client and server application:
- Git ([Link](https://git-scm.com/downloads))
- CMake ([Link](https://cmake.org/download/))
- C++ Compiler
- Emscripten ([Link](https://emscripten.org/docs/getting_started/downloads.html))
- NodeJS ([Link](https://nodejs.org/en/download/package-manager))

### Dependencies
The dependencies of the server can be easily downloaded using git and the following command
```
git submodule update --init --recursive
```
To setup the dependendies of the client, navigate to the folder `client/` and then run the following command
```
npm install
```

### Server Compliation
The source code for the server is located in the folder `server/`.
Before starting the compilation process, make sure that the folder `server/build/` exists and is empty. If that is not the case, clear or create the folder.
After that, open up a terminal and navigate to this particular folder.
```
mkdir server/build/
cd server/build/
``` 
If you have done this, create the build environment for the server by running CMake using the following command
```
cmake ..
```
Finally, the folder `server/build/` should contain the files for the configured build environment.

### Client Compilation


### Execution

### Certificates


