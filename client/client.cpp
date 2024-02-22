#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <sstream>

using namespace std;

const int BUF_SIZE = 64 * 1024;
const string SERVER_IP = "127.0.0.1";
const int SERVER_PORT = 12346;
const string TRANSFER_DIR = "/Users/antoninanovak/CLionProjects/ClientAssignment2/transfer_files/";


void sendCommand(int sock, const string& command) {
    if (send(sock, command.c_str(), command.size(), 0) < 0) {
        cout << "Failed to send command." << endl;
        return;
    }
    cout << "Command sent: " << command << endl;
}


string receiveResponse(int sock) {
    char buffer[BUF_SIZE];
    string response;
    memset(buffer, 0, BUF_SIZE);
    if (recv(sock, buffer, BUF_SIZE, 0) < 0) {
        cout << "Failed to receive response." << endl;
        return "";
    }
    response = buffer;
    return response;
}

void handleFileDownload(int sock, const string& filename) {
    string filePath = TRANSFER_DIR + filename; // Use the transfer directory
    ofstream outFile(filePath, ios::binary);
    if (!outFile.is_open()) {
        cout << "Failed to open file for writing: " << filePath << endl;
        return;
    }
    int64_t fileSize;
    recv(sock, reinterpret_cast<char*>(&fileSize), sizeof(fileSize), 0); // Receive the file size first
    int64_t totalBytesReceived = 0;
    char buffer[BUF_SIZE];
    const int progressBarWidth = 50; // Width of the progress bar

    while (totalBytesReceived < fileSize) {
        int bytesReceived = recv(sock, buffer, BUF_SIZE, 0);
        if (bytesReceived <= 0) break; // Connection closed or error
        outFile.write(buffer, bytesReceived);
        totalBytesReceived += bytesReceived;

        // Update progress bar
        double progress = (double)totalBytesReceived / fileSize;
        int pos = progressBarWidth * progress;
        cout << "\rDownloading [" << string(pos, '#') << string(progressBarWidth - pos, ' ') << "] "
             << int(progress * 100.0) << " %" << flush;
    }
    cout << "\nFile downloaded: " << filename << endl;
    outFile.close();
}
void handleFileUpload(int sock, const string& filename) {
    string filePath = TRANSFER_DIR + filename; // Adjusted to use the transfer directory
    ifstream inFile(filePath, ios::binary | ios::ate);
    if (!inFile.is_open()) {
        cout << "Failed to open file for reading: " << filePath << endl;
        return;
    }
    int64_t fileSize = inFile.tellg();
    inFile.seekg(0, ios::beg);

    // Inform the server of the file size first
    send(sock, reinterpret_cast<const char*>(&fileSize), sizeof(fileSize), 0);

    const size_t bufferSize = 64 * 1024; // Increased buffer size to 64KB
    char buffer[bufferSize];
    int64_t totalBytesSent = 0;
    const int progressBarWidth = 50;

    while (inFile.read(buffer, sizeof(buffer)) || inFile.gcount()) {
        ssize_t sent = send(sock, buffer, inFile.gcount(), 0);
        if (sent < 0) {
            cout << "Failed to send file data." << endl;
            break; // Handle partial send or failure
        }
        totalBytesSent += sent;

        // Update progress bar
        double progress = (double)totalBytesSent / fileSize;
        int pos = progressBarWidth * progress;
        cout << "\rUploading [" << string(pos, '#') << string(progressBarWidth - pos, ' ') << "] "
             << int(progress * 100.0) << " %" << flush;
    }
    cout << "\nFile uploaded: " << filename << endl;
    inFile.close();
}

void printCurrentWorkingDirectory() {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        std::cout << "Current working dir: " << cwd << std::endl;
    } else {
        perror("getcwd() error");
    }
}


int main() {
    printCurrentWorkingDirectory();
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        cout << "Socket creation failed." << endl;
        return -1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    serverAddr.sin_addr.s_addr = inet_addr(SERVER_IP.c_str());

    if (connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        cout << "Connection failed." << endl;
        return -1;
    }
    cout << "Connected to the server." << endl;

    string input;
    cout << "Welcome! Please enter your name: ";
    getline(cin, input);
    sendCommand(sock, input + "<END>");

    cout << "Available Commands:\n- LIST\n- GET <filename>\n- PUT <filename>\n- DELETE <filename>\n- INFO <filename>\n- QUIT to exit" << endl;

    while (true) {
        cout << "Enter command: ";
        getline(cin, input);
        if (input == "QUIT") {
            sendCommand(sock, "QUIT<END>"); // Inform the server of the client's intention to quit
            cout << "Exiting, goodbye!" << endl;
            break;
        }

        stringstream ss(input);
        string command, filename;
        ss >> command;

        if (command == "LIST") {
            sendCommand(sock, "LIST<END>");
        } else if (command == "GET") {
            ss >> filename;
            sendCommand(sock, "GET " + filename + "<END>");
            handleFileDownload(sock, filename);
        } else if (command == "PUT") {
            ss >> filename;
            sendCommand(sock, "PUT " + filename + "<END>");
            handleFileUpload(sock, filename);
        } else if (command == "DELETE") {
            ss >> filename;
            sendCommand(sock, "DELETE " + filename + "<END>");
        } else if (command == "INFO") {
            ss >> filename;
            sendCommand(sock, "INFO " + filename + "<END>");
        } else {
            cout << "Invalid command. Please try again." << endl;
            continue; // Skip the response reading if command is invalid
        }

        string response = receiveResponse(sock); // Read server's response
        if (!response.empty()) {
            cout << "Server: " << response << endl;
        }
    }
    close(sock);
    return 0;
}
