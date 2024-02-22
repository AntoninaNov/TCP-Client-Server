#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <chrono>

using namespace std;
namespace fs = std::filesystem;

class Server {
private:
    int serverSocket;
    sockaddr_in serverAddr;
    int port = 12346;
    string serverDirectory = "/Users/antoninanovak/CLionProjects/Server/storage";
    mutex mtx;

    void logError(const string& msg) {
        cerr << msg << endl;
    }

    void serverConfig() {
        serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket == -1) {
            logError("Error creating socket");
            exit(EXIT_FAILURE);
        }
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(port);

        if (::bind(serverSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == -1) {
            logError("Bind failed");
            exit(EXIT_FAILURE);
        }
    }

    void listenForConnections() {
        if (listen(serverSocket, SOMAXCONN) == -1) {
            logError("Listen failed");
            exit(EXIT_FAILURE);
        }
        cout << "Server listening on port " << port << endl;
    }

    void startAcceptingClients() {
        while (true) {
            int clientSocket = accept(serverSocket, nullptr, nullptr);
            if (clientSocket == -1) {
                cerr << "Accept failed" << endl;
                continue;
            }
            string clientSubfolder = "client_" + to_string(clientSocket);
            thread clientThread(&Server::handleClient, this, clientSocket, clientSubfolder);
            clientThread.detach();
        }
    }

    void handleClient(int clientSocket, string clientSubfolder) {
        string clientName = receiveMessage(clientSocket);
        if (clientName.empty()) {
            cerr << "Failed to receive client's name. Closing connection." << endl;
            close(clientSocket);
            return;
        }

        string clientDirectory = serverDirectory + "/" + clientName;
        if (!fs::exists(clientDirectory)) {
            fs::create_directories(clientDirectory);
        }

        auto start = chrono::high_resolution_clock::now();

        receiveCommands(clientSocket, clientDirectory);

        auto end = chrono::high_resolution_clock::now();
        chrono::duration<double, milli> elapsed = end - start;
        cout << "Handled client in " << elapsed.count() << " ms." << endl;
    }

    void cleanup() {
        close(serverSocket);
        cout << "Server closed" << endl;
    }

    string receiveMessage(int clientSocket) {
        string totalData;
        char buffer[1024];
        const string endMarker = "<END>";
        while (true) {
            memset(buffer, 0, sizeof(buffer));
            ssize_t bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
            if (bytesReceived > 0) {
                totalData.append(buffer, bytesReceived);
                if (totalData.find(endMarker) != string::npos) {
                    break;
                }
            } else if (bytesReceived == 0) {
                logError("Client disconnected");
                break;
            } else {
                logError("recv failed with error");
                break;
            }
        }
        size_t endPos = totalData.find(endMarker);
        return endPos != string::npos ? totalData.substr(0, endPos) : "";
    }


    void sendResponse(int clientSocket, const string& response) {
        lock_guard<mutex> lock(mtx); // Ensure thread safety
        string dataToSend = response + "<END>"; // Append end marker
        ssize_t totalBytesSent = 0;
        size_t dataLength = dataToSend.length();

        while (totalBytesSent < dataLength) {
            ssize_t bytesSent = send(clientSocket, dataToSend.c_str() + totalBytesSent,
                                     dataLength - totalBytesSent, 0);
            if (bytesSent == -1) {
                throw runtime_error("send failed with error: " + to_string(errno));
            }
            totalBytesSent += bytesSent;
        }
    }

    void receiveCommands(int clientSocket, const string& clientDirectory) {
        while (true) { // Loop to continuously receive commands
            string received = receiveMessage(clientSocket);
            // Check for a specific termination command or condition to break the loop
            if (received.empty() || received == "QUIT") {
                sendResponse(clientSocket, "Connection closing...");
                cout << "Connection with client closed." << endl;
                break; // Exit the loop to stop receiving commands
            }
            cout << received << endl;
            if (!received.empty()) {
                vector<string> params;
                istringstream iss(received);
                string word;
                while (iss >> word) {
                    params.push_back(word);
                }

                if (params.empty()) {
                    cout << "No command received." << endl;
                    continue; // Continue listening for commands
                }

                string command = params[0];
                string filename = (params.size() > 1) ? params[1] : "";

                // Process the received command
                handleCommands(clientSocket, clientDirectory, command, filename);
            }
            else {
                cout << "No command received." << endl;
            }
        }
    }

    void listFiles(int clientSocket, const string& clientDirectory) {
        cout << "Listing storage in directory: " << clientDirectory << endl; // Debugging line

        string fileList = "Files in directory: " + clientDirectory + "\n";
        try {
            if (fs::is_empty(clientDirectory)) {
                fileList += "Directory is empty.\n";
            } else {
                for (const auto& entry : fs::directory_iterator(clientDirectory)) {
                    fileList += entry.path().filename().string() + "\n";
                }
            }
        } catch (const filesystem::filesystem_error& e) {
            cerr << "Filesystem error: " << e.what() << endl; // Debugging line for filesystem errors
            fileList += "Failed to list storage due to a filesystem error.\n";
        } catch (const exception& e) {
            cerr << "Error listing storage: " << e.what() << endl; // Debugging line for other exceptions
            fileList += "Failed to list storage due to an unexpected error.\n";
        }

        sendResponse(clientSocket, fileList);
    }

    void fileInfo(int clientSocket, const string& clientDirectory, const string& filename) {
        string filePath = clientDirectory + "/" + filename;
        if (!fs::exists(filePath)) {
            sendResponse(clientSocket, "File does not exist.\n");
            return;
        }
        auto ftime = fs::last_write_time(filePath);

        // Convert file_time_type to time_t
        auto sctp = chrono::time_point_cast<chrono::system_clock::duration>(ftime - fs::file_time_type::clock::now() + chrono::system_clock::now());
        time_t cftime = chrono::system_clock::to_time_t(sctp);

        auto size = fs::file_size(filePath);

        // Use localtime to convert time_t to readable string (optional)
        struct tm *ptm = std::localtime(&cftime);
        char buffer[32];
        // Format: YYYY-MM-DD HH:MM:SS
        strftime(buffer, 32, "%Y-%m-%d %H:%M:%S", ptm);

        sendResponse(clientSocket, "File: " + filename + ", Size: " + to_string(size) + " bytes, Last modified: " + buffer + "\n");
    }


    void deleteFile(int clientSocket, const string& clientDirectory, const string& filename) {
        string filePath = clientDirectory + "/" + filename;
        if (fs::remove(filePath)) {
            sendResponse(clientSocket, "File deleted successfully.\n");
        } else {
            sendResponse(clientSocket, "Failed to delete file.\n");
        }
    }

    void sendFile(int clientSocket, const string& clientDirectory, const string& filename) {
        string filePath = clientDirectory + "/" + filename;
        ifstream file(filePath, ios::binary | ios::ate);
        if (!file.is_open()) {
            sendResponse(clientSocket, "Failed to open file.\n");
            return;
        }
        int64_t fileSize = file.tellg();
        file.seekg(0, ios::beg);
        send(clientSocket, reinterpret_cast<const char*>(&fileSize), sizeof(fileSize), 0);

        const size_t bufferSize = 64 * 1024; // Increase buffer size to 64KB
        char buffer[bufferSize];
        int64_t totalBytesSent = 0;

        const int progressBarWidth = 50; // Width of the progress bar

        while (file.read(buffer, sizeof(buffer)) || file.gcount()) {
            send(clientSocket, buffer, file.gcount(), 0);
            totalBytesSent += file.gcount();

            // Progress bar
            double progress = (double)totalBytesSent / fileSize;
            int pos = progressBarWidth * progress;
            cout << "\r[" << string(pos, '#') << string(progressBarWidth - pos, ' ') << "] " << int(progress * 100.0) << " %";
            cout.flush();
        }
        cout << endl; // Ensure the next console output is on a new line
    }

    void receiveFile(int clientSocket, const string& clientDirectory, const string& filename) {
        string filePath = clientDirectory + "/" + filename;
        ofstream outputFile(filePath, ios::binary);
        if (!outputFile.is_open()) {
            sendResponse(clientSocket, "Failed to create file.\n");
            return;
        }
        int64_t totalBytesReceived = 0;
        int64_t fileSize;
        recv(clientSocket, reinterpret_cast<char*>(&fileSize), sizeof(fileSize), 0);

        const size_t bufferSize = 64 * 1024; // Increase buffer size to 64KB
        char buffer[bufferSize];

        const int progressBarWidth = 50; // Width of the progress bar

        while (totalBytesReceived < fileSize) {
            int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
            if (bytesReceived <= 0) break; // Connection closed or error
            outputFile.write(buffer, bytesReceived);
            totalBytesReceived += bytesReceived;

            // Progress bar
            double progress = (double)totalBytesReceived / fileSize;
            int pos = progressBarWidth * progress;
            cout << "\r[" << string(pos, '#') << string(progressBarWidth - pos, ' ') << "] " << int(progress * 100.0) << " %";
            cout.flush();
        }
        cout << endl; // Ensure the next console output is on a new line
        outputFile.close();
        sendResponse(clientSocket, "File received successfully.\n");
    }

    void handleCommands(int clientSocket, const string& clientDirectory, const string& command, const string& filename) {
        if (command == "LIST") {
            listFiles(clientSocket, clientDirectory);
        } else if (command == "GET") {
            sendFile(clientSocket, clientDirectory, filename);
        } else if (command == "PUT") {
            receiveFile(clientSocket, clientDirectory, filename);
        } else if (command == "INFO") {
            fileInfo(clientSocket, clientDirectory, filename);
        } else if (command == "DELETE") {
            deleteFile(clientSocket, clientDirectory, filename);
        } else {
            sendResponse(clientSocket, "Invalid command.\n");
        }
    }

public:
    Server() {
        serverConfig();
    }

    ~Server() {
        cleanup();
    }

    void setupServer() {
        listenForConnections();
        startAcceptingClients();
    }

    bool isReady() const {
        return serverSocket != -1;
    }

    bool initialize() {
        return isReady();
    }

    void run() {
        setupServer();
    }
};

int main() {
    Server server;

    if(!server.initialize()) {
        cerr << "Server initialization failed\n";
        return 1;
    }

    try {
        server.run();
    } catch(const std::exception& e) {
        cerr << "Error running server: " << e.what() << "\n";
        return 1;
    }

    return 0;
}

