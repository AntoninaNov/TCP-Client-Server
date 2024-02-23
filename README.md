# TCP Protocol Implementation - Byte Usage

## Server Side:

### Socket Creation (`socket` call)
- Socket descriptor is an `int` (typically 4 bytes).

### Binding Socket (`bind` call)
- `serverAddr` (`sockaddr_in`): 16 bytes.

### Listening for Connections (`listen` call)
- No significant data storage.

### Accepting Client (`accept` call)
- No specific data structure size for return value (client socket descriptor).

### Sending Response (`sendResponse` method)
- `response` (`string`): Variable size.
- `dataToSend` (`string`): Variable size, includes `<END>` marker.
- `bufferSize` (`size_t`): 8 bytes.
- `dataLength` (`size_t`): 8 bytes.
- `toSend` (`size_t`): 8 bytes.
- Network message: Variable size, depends on `dataToSend`.

### Receiving Message (`receiveMessage` method)
- `buffer` (char array): 1024 bytes.
- `totalData` (`string`): Variable size.
- `endMarker` (`string`): Fixed size, length of `<END>`.
- Handling Commands (`handleCommands` method)
- Command-specific operations, variable byte usage depending on command.

## Client Side:

### Socket Creation (`socket` call)
- Socket descriptor is an `int` (typically 4 bytes).

### Sending Message (`sendMessage` method)
- Similar to Server's `sendResponse`.

### Receiving Message (`receiveMessage` method)
- Similar to Server's `receiveMessage`.

### Sending File (`sendFile` method)
- `buffer` (char array): 1024 bytes.
- `bufferSize` (`size_t`): 8 bytes.
- `eofMarker` (`const char*`): Size of `<EOF>` string.
- File data: Variable size, depends on file content.

### Receiving File (`receiveFile` method)
- Similar to `sendFile`, with additional handling for writing to file.

### Command Handling (`inputCommand` method)
- `inputParams` (`vector<string>`): Variable size.
- Command-specific data handling.

<img width="627" alt="image" src="https://github.com/AntoninaNov/TCP-Client-Server/assets/56410053/ea308836-2e9f-4726-8ac8-dffd99c1d061">
