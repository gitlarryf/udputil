# udputil
Simple UDP client / server application that can send simple UDP broadcasts, or regular datagrams.

## Building udputil
Building UDPUtil is rather simple no matter which platform you're on.

### Windows
Without CMake installed:
```
cl udputil.cpp /Feudputil.exe
```
With CMake installed:
```
cmake --build .
```
Once cmake as been executed, you can load the generated project file or solution into Visual Studio and compile it there.

### Linux
```
g++ udputil.cpp -o udputil
```
Or use CMake to generate necessary makefile.

### Timesys Linux for the ProSoft MVI56E-LDM and MVI69E-LDM
```
/opt/timesys/datm3/toolchain/bin/armv5l-timesys-linux-gnueabi-g++ udputil.cpp -o udputil
```
Or use CMake to generate necessary makefile.

UDPUtil will print out whatever datagram payload it receives, but if it does not understand the datagram it outputs the actual payload data it in a "Hex Editor" style output that will auto-adjust to your shell size.
Example:
```
Received 15 bytes of unknown datagram.
PAYLOAD:55 44 50 55 74 69 6C 20 50 61 79 6C 6F 61 64       UDPUtil Payload
```
