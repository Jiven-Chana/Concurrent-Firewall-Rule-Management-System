# Concurrent Firewall Rule Management System with Client-Server Communication

This project is a **Concurrent Firewall Rule Management System** designed to handle multiple client connections, validate network connection requests based on predefined firewall rules, and allow for dynamic rule management. The system is implemented in C and utilizes socket programming, multithreading, and various data structures to manage firewall rules and process client requests efficiently.

## Table of Contents
1. [Project Overview](#project-overview)
2. [Features](#features)
3. [Logic Flow](#logic-flow)
4. [Concurrency](#concurrency)
5. [Results](#results)
6. [Code Structure](#code-structure)
7. [How to Run the Project](#how-to-run-the-project)
8. [Applications](#applications)
9. [Imports and Libraries](#imports-and-libraries)

## Project Overview

This project provides a firewall-like rule management system where clients can communicate with the server to add, verify, or delete network rules. The system supports concurrent client connections and ensures thread-safe access to shared resources, like firewall rules, using locks.

The primary components of this project include:
- **Client connections** for submitting requests.
- **Server-side rule validation** to approve or reject connections based on IP and port range.
- **Multithreaded architecture** for handling multiple client requests simultaneously.
- **Dynamic rule management** that allows adding, listing, and deleting network rules.

## Features

- **Rule Addition**: Allows clients to add new rules specifying IP ranges and port ranges.
- **Rule Deletion**: Enables clients to delete existing firewall rules.
- **Request Verification**: Checks incoming requests against stored rules and either allows or denies connections based on these rules.
- **Request Listing**: Clients can request a list of stored rules.
- **Multithreading**: Handles multiple clients concurrently for efficient processing.

## Logic Flow

1. **Initialization**: The server sets up a listening socket and waits for incoming client connections.
2. **Client Request Processing**:
   - Clients connect and send requests to the server.
   - Each client’s request is processed in a separate thread, ensuring concurrent handling of multiple clients.
3. **Command Handling**:
   - **Add Rule**: Adds a new network rule to the server's rule list.
   - **Delete Rule**: Removes a specified rule from the list.
   - **Verify Connection**: Checks if a given IP and port match any existing rule.
   - **List Requests**: Displays all connection requests made by clients.
   - **List Rules**: Provides a list of current firewall rules.
4. **Rule Validation**: For each request, the server checks if the request matches any rule in the list. It either allows or denies the connection accordingly.
5. **Response to Client**: Each client receives a response based on the outcome of the request (allowed, denied, rule added, etc.).

## Concurrency

- **Multithreading**: Each client connection is handled in a separate thread, allowing the server to handle multiple client requests simultaneously.
- **Mutex Locks**: Shared resources, such as the firewall rule list, are protected with mutex locks to ensure thread safety. This prevents data corruption when multiple threads attempt to read or write to the same resource.
- **Command Queue**: A simple queue mechanism allows for storing and processing client requests without blocking other incoming connections.

## Results

The project achieves efficient handling of firewall rule management and client connections through concurrent processing. The system can handle multiple clients simultaneously, providing rapid responses to requests such as adding, verifying, or deleting rules. The multithreaded structure ensures that the server remains responsive even under heavy load, as requests are processed concurrently.

## Code Structure

The codebase is split into several sections, each with a specific responsibility:

1. **Client Request Handling**: Manages incoming client connections and routes requests to appropriate handlers.
2. **Firewall Rule Management**: Includes functions for adding, deleting, and validating rules against incoming connection requests.
3. **Command Parsing and Execution**: Parses client commands and triggers corresponding functions.
4. **Multithreading and Synchronization**: Manages concurrent connections and ensures thread safety using mutex locks.

### Key Functions

- `init_sckt()`: Initializes a socket for client communication.
- `configAddr()`: Configures server address details for binding.
- `buildCommand()`: Constructs commands from client input.
- `verifyConn()`: Verifies connection requests against firewall rules.
- `sendCommand()`: Sends command responses back to clients.
- `fetch()`: Fetches data from the server.
- `handleAddRule()`, `handleDelete()`, `handleVerify()`, `handleListRequests()`, `handleListRules()`: Command handlers for various client operations.

## How to Run the Project

### Prerequisites

Ensure that you have a **Linux environment** with **GCC** installed to compile and run the C code.

### Compilation

To compile the program, navigate to the project directory and run:

```bash
gcc -o server server.c -lpthread
```

## Starting the Server

Run the server with either interactive mode or network mode:
Interactive Mode: For interactive testing on localhost.
 
```bash
./server -i
```
Network Mode: Specify a port number to listen for incoming client connections.

```bash
./server <port_number>
```
## Connecting a Client

Run a client by providing the server’s IP, port, and the desired command:
```bash
./client <server_ip> <port> <command> [parameters]
```
# Example Commands

1.	Add a Rule: Add a rule for a specific IP range and port range:
```bash
./client localhost 8080 A 192.168.1.0-192.168.1.255 80-100
```
2. Verify a Connection: Check if a specific IP and port are allowed by the rules:
```bash
./client localhost 8080 C 192.168.1.10 80
```
3. Delete a Rule: Delete a specified rule:
```bash
./client localhost 8080 D 192.168.1.0-192.168.1.255 80-100
```
4. List rules: list all existing rules
```bash
./client localhost 8080 L
```
5. List Requests: List all client connection requests.
```bash
./firewall_client localhost 8080 R
```

## Applications

This project can serve as a foundational implementation for:

- **Firewall Systems**: Implementing a dynamic firewall for network security, where rules can be added, deleted, or modified in real-time.
- **Network Access Control**: Managing IP and port access for a range of applications, including VPNs, internal network management, and private data center access control.
- **Load Balancing and Access Control Testing**: Testing load-balancing setups or access rules in network simulation environments.
- **Educational and Research Purposes**: Understanding the fundamentals of socket programming, multithreading, and firewall logic.

## Imports and Libraries

This project uses the following standard libraries in C:

1. **Standard I/O**: 
   - `<stdio.h>` for input-output functions.
2. **Standard Libraries**:
   - `<stdlib.h>` for general utility functions, including dynamic memory allocation.
3. **String Handling**:
   - `<string.h>` for string manipulation functions.
4. **POSIX and UNIX System Calls**:
   - `<unistd.h>` for miscellaneous symbolic constants and types, especially used in threading and file operations.
5. **Socket Programming**:
   - `<sys/socket.h>`, `<netinet/in.h>`, `<arpa/inet.h>` for creating, configuring, and managing sockets.
6. **Multithreading**:
   - `<pthread.h>` for managing concurrent threads.
7. **Data Types and Boolean**:
   - `<stdbool.h>` for using boolean types in C.
8. **Data Structure Functions**:
   - `<sys/types.h>`, `<ctype.h>`, `<strings.h>` for data structure manipulations and additional utility functions.

## How the Program Works Internally

### Initialization and Configuration

- The server sets up a socket with a specified port number (or defaults to interactive mode if no port is provided).
- It binds the socket to an IP address and port and listens for incoming client connections.

### Command Handling and Logic Flow

1. **Adding a Rule**:
   - The `handleAddRule()` function processes `A` commands, which include an IP range and port range, adding these rules to the server’s rule list.
2. **Deleting a Rule**:
   - The `handleDelete()` function handles `D` commands by identifying the rule in the list and removing it.
3. **Verifying a Connection**:
   - The `handleVerify()` function checks an IP and port pair to see if it is allowed under any of the existing rules.
4. **Listing Rules and Requests**:
   - The `handleListRules()` and `handleListRequests()` functions print the current rules or requests respectively to provide a quick overview of server activity.

### Concurrency and Thread Safety

- Each client connection initiates a separate thread. A mutex lock, `ruleLock`, ensures safe access to shared resources like the rule list.
- Each command from a client is parsed and routed to the correct handler function.
- The server’s multithreading capabilities allow simultaneous client connections and requests, improving performance and responsiveness.

### Error Handling and Edge Cases

- **Socket Errors**: Handled via `errHandle()` to print relevant error messages and exit on failure.
- **Invalid Inputs**: Ensures commands are validated before processing, and provides feedback on invalid commands.
- **Request Limits**: Configured limits on requests and rules prevent memory overflow and ensure manageable server load.

## Example Output

Here’s an example output for various commands:
### Adding a Rule
```bash
Client: ./client localhost 1234 A 192.168.1.0-192.168.1.255 80-100
Server: Rule added
```
### Verifying a connection
```bash
Client: ./client localhost 1234 C 192.168.1.10 80
Server: Connection accepted
```
### Deleting a rule 
```bash
Client: ./client localhost 1234 D 192.168.1.0-192.168.1.255 80-100
Server: Rule deleted
```
### Listing rules
```bash
Client: ./client localhost 1234 L
Server:
Rule: 192.168.1.0-192.168.1.255 80-100
```

### Extending the Project

## Potential enhancements include:

1.	Persistent Storage of Rules: Store rules in a database or file for retrieval upon server restart.
2.	Advanced Rule Matching: Add support for protocols, complex IP ranges, and more specific rule conditions.
3.	Improved Client Authentication: Secure the client-server connection with SSL/TLS.
4.	Web Interface: Develop a web-based interface for rule management and monitoring.

### Conclusion

This project offers a robust foundation for understanding network programming concepts, including concurrency, socket communication, and basic firewall logic. Through multithreading and real-time rule management, it showcases how concurrent programming can be applied to manage network connections dynamically and securely.

For questions, issues, or contributions, please feel free to open an issue on the repository or contribute to its development.

Thank you for using the Concurrent Firewall Rule Management System.

