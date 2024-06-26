# Loop-Based Dual-Layer Image Warping
The rendering framework provided in this repository was developed for testing and evaluating the `Loop-Based Dual-Layer Image Warping` technique. The goal of this technique was to stream complex scenes to mobile devices and give a user the opportunity to naturally and freely explore the scene surrounding him. The server application that is part of this framework is responsible for gathering scene information, which is then displayed by the client application of the framework. In addition to images that together define a cube map, the server also transmits a mesh for each image, which allows the client to apply image warping techniques. These image warping techniques are used to hide the transmission latency. For that, the client distorts the images using the additionally provided mesh so that they respond to the movement of the user. In addition to this image adaptation, the server can also provide a second cube map along with a second set of meshes. This addition cube map captures surfaces occluded in the first cube map, which allows for a more stable image warping.

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

```
mkdir server/wrapper/build/
cd server/wrapper/build
emcmake cmake ..
make
```

### Execution

```
const server_ip = "localhost";
const server_port = 9000;
const client_port = 443;
```

The standard port is 443 which is the standard port for https

```
npm run dev
```

### Certificates

Using a self signed certificat
```
plugins:
[
    basicSsl()
]
```

Using a correct certificat
```
https:
{
    key: fs.readFileSync("<path>/private_key.pem"),
    cert: fs.readFileSync("<path>/certificat.pem")
}
```
