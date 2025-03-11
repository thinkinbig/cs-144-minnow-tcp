Stanford CS 144 Networking Lab
==============================

These labs are open to the public under the (friendly) request that to
preserve their value as a teaching tool, solutions not be posted
publicly by anybody.

Website: https://cs144.stanford.edu

To set up the build system: `cmake -S . -B build`

To compile: `cmake --build build`

To run tests: `cmake --build build --target test`

To run speed benchmarks: `cmake --build build --target speed`

To run clang-tidy (which suggests improvements): `cmake --build build --target tidy`

To format code: `cmake --build build --target format`

ByteStream Optimization
-----------------------
Our project includes significant optimizations to the ByteStream component, enhancing its throughput and efficiency. The ByteStream is capable of achieving a throughput of up to 13.10 Gbit/s with a pop length of 4096, demonstrating its high performance in handling large data streams efficiently. This optimization ensures that data is processed quickly and effectively, making it suitable for high-speed networking applications.

ARP Implementation
-------------------
The project also features a robust implementation of the Address Resolution Protocol (ARP). Our ARP implementation efficiently resolves network layer addresses into link layer addresses, facilitating seamless communication between devices on the same network. This implementation is designed to handle ARP requests and replies efficiently, ensuring minimal latency and high reliability in network communications. The ARP module is integrated into the network stack, providing essential support for IP-based networking operations.

Performance Data
----------------
- **ByteStream Throughput**:
  - Pop length 4096: 13.10 Gbit/s
  - Pop length 128: 2.61 Gbit/s
  - Pop length 32: 0.63 Gbit/s
- **Reassembler Throughput**:
  - No overlap: 61.68 Gbit/s
  - 10x overlap: 7.18 Gbit/s

Device Information
------------------
- **Operating System**: Linux 5.15.167.4-microsoft-standard-WSL2
- **Processor**: Intel Corporation, GenuineIntel
- **Number of Logical CPUs**: 8
- **Number of Physical CPUs**: 4
- **Total Physical Memory**: 7838 MB
- **Processor Clock Frequency**: 3696.01 MHz
