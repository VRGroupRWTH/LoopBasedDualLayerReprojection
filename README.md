
# Loop-Based Dual-Layer Image Warping
The rendering framework provided in this repository was developed for testing and evaluating the `Loop-Based Dual-Layer Image Warping` technique. The goal of this technique was to stream complex scenes to mobile devices and give a user the opportunity to naturally and freely explore the scene surrounding him. The server application that is part of this framework is responsible for gathering scene information, which is then displayed by the client application of the framework. In addition to images that together define a cube map, the server also transmits a mesh for each image, which allows the client to apply image warping techniques. These image-warping techniques are used to hide the transmission latency. For that, the client distorts the images using the additionally provided mesh so that they respond to the movement of the user. In addition to this image adaptation, the server can also provide a second cube map along with a second set of meshes. This addition cube map captures surfaces occluded in the first cube map, which allows for a more stable image warping.

### Requirements
Several tools are required for the compilation of the client and server applications:
- Git ([Link](https://git-scm.com/downloads))
- CMake ([Link](https://cmake.org/download/))
- C++ Compiler
- Emscripten ([Link](https://emscripten.org/docs/getting_started/downloads.html))
- NodeJS ([Link](https://nodejs.org/en/download/package-manager))

### Dependencies
The dependencies of the server can be easily downloaded using git and the following command:
```shell
git submodule update --init --recursive
```
To set up the dependencies of the client, navigate to the folder `client/` and then run the following command:
```shell
npm install
```

### Server Compilation
The source code for the server is located in the folder `server/`.
Before starting the compilation process, make sure that the folder `server/build/` exists and is empty. If that is not the case, clear or create the folder.
After that, open up a terminal and navigate to this particular folder.
```shell
mkdir server/build/
cd server/build/
``` 
If you have done this, create the build environment for the server by running CMake using the following command:
```shell
cmake ..
```
Finally, the folder `server/build/` should contain the files for the configured build environment.

### Client Compilation
Even though the client was implemented as a web application, there is still some form of compilation required.
To make creating and parsing binary packets easier, the client uses a wrapper library written in C++, which needs to be compiled to WebAssembly using Emscripten.
Before compiling the wrapper library, make sure all environment variables of Emscripten are properly setup. For more information on that [here](https://emscripten.org/docs/getting_started/downloads.html#installation-instructions-using-the-emsdk-recommended).

Similar to the compilation of the server, the first step is to create a folder `server/wrapper/build/` in which the files for the build environment should be placed.
```shell
mkdir server/wrapper/build/
cd server/wrapper/build/
```
After navigating to this build folder, the following commands can be used to compile the wrapper library:
```shell
emcmake cmake ..
make
```
The results of the compilation process are then placed in the folder `server/wrapper/binary/`.

### Execution
Server and client need to be started separately. The location of the server executable is dependent on the build environment with which the server was compiled. After opening a terminal and navigating to the executable, the following command can be used to start the server:
```shell
cd <executable path>
server.exe --scene_directory="<path>\scenes" --study_directory="<path>\study"
```
The server will then search for loadable scenes in the folder `scene_directory` while all results and logs are written to the folder `study_directory`.

The client, on the other hand, can be easily started using administrative rights and the following terminal command:
```shell
cd client/
npm run dev
```

After starting both applications, the server will be reachable by the port `9000` while the client is reachable by the port `443`. In some cases, it might be necessary to change the ports used by the server and client. The port used by the server can be changed in the file `server/source/application.cpp`. For that, add the desired port number as an argument to the following `create` function call:
```c++
if (!this->server->create(<port>))
```
The ports used by the client, on the other hand, are set in the file `client/vite.config.ts`.
```javascript
const server_ip = "localhost";
const server_port = 9000;
const client_port = 443;
```
The constants `server_ip` and `server_port` need to match the configuration of the server, while the constant `client_port` determines the port used by the client. It is recommended to use port `443` for the client. Otherwise, it may not be possible to establish a secure HTTPS connection.

### Certificates
Since the WebXR API used by the client requires a secure HTTPS connection to function properly, the client needs to be equipped with certificates. There are two possible ways these certificates can be set up. 

The easiest way is to use self-signed certificates, which can be easily setup by changing the file `client/vite.config.ts` and adding the following plugin to the client:
```javascript
plugins:
[
    basicSsl()
]
```
The disadvantage of this approach is that some browsers will still treat the connection to the client as insecure. On most browsers, the warning that is shown in this case can be easily ignored. However, several software limitations make it nearly impossible to run the client using self-signed certificates on Apple devices. Due to the monopolistic behavior of Apple, iPhones and iPads cannot use other browsers besides the Safari browser, which does not properly support the WebXR API at the time of writing. While the WebXR Viewer ([link](https://apps.apple.com/de/app/webxr-viewer/id1295998056)) provides a working implementation of the WebXR API on Apple devices, it unfortunately does not accept self-signed certificates.

The more complicated way is to use proper certificates, which can be configured and installed using the following lines of the file `client/vite.config`:
```javascript
https:
{
    key: fs.readFileSync("<path>/private_key.pem"),
    cert: fs.readFileSync("<path>/certificat.pem")
}
```
In addition to the certificates, a proper DNS is required to run the client over the WebXR Viewer on Apple devices.