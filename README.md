# udputil
Simple UDP client / server application that can send simple UDP broadcasts, or regular datagrams.

## Building udputil
Building UDPUtil is rather simple no matter which platform you're on.

### Windows
```
cl udputil.cpp /Feudputil.exe
```
Or you can load the included project file into Visual Studio 2017 and compile it there.

### Linux
```
g++ udputil.cpp -o udputil
```

UDPUtil will print out when it receives datagrams, but if it does not understand the datagram, it outputs it in a Hex Editor style output, that will auto-adjust to your shell's size.
