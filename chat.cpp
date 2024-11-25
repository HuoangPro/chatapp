#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <netdb.h>
#include <ifaddrs.h>

#define MAX_BUFFER_SIZE 1024

class PeerConnection {
public:
    int socket_fd;
    std::string ip;
    int port;

    PeerConnection(int socket, std::string ip_addr, int port_no)
        : socket_fd(socket), ip(ip_addr), port(port_no) {}
};

class ChatApp {
private:
    int listen_socket;
    struct sockaddr_in server_addr;
    std::vector<PeerConnection> connections;

public:
    ChatApp(int port) {
        listen_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_socket == -1) {
            std::cerr << "Socket creation failed." << std::endl;
            exit(1);
        }

        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(port);

        if (bind(listen_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
            std::cerr << "Bind failed." << std::endl;
            exit(1);
        }

        if (listen(listen_socket, 5) == -1) {
            std::cerr << "Listen failed." << std::endl;
            exit(1);
        }
    }

    void run() {
        std::thread listener_thread(&ChatApp::acceptConnections, this);

        while (true) {
            std::string command;
            std::getline(std::cin, command);
            processCommand(command);
        }

        listener_thread.join();
    }

    void listenToPeer(int socket_fd, std::string peer_ip, int peer_port) {
        char buffer[MAX_BUFFER_SIZE];
        while (true) {
            memset(buffer, 0, sizeof(buffer));
            int bytes_received = recv(socket_fd, buffer, sizeof(buffer), 0);

            if (bytes_received <= 0) {
                std::cout << "Connection with " << peer_ip << ":" << peer_port << " closed." << std::endl;
                close(socket_fd);
                return;
            }

            std::cout << "\nMessage received from " << peer_ip << ":" << peer_port << std::endl;
            std::cout << "Message: " << buffer << std::endl;
            std::cout << "> "; // Re-prompt the user
            std::cout.flush(); // Flush the console to ensure prompt is shown
            
        }
    }

    void acceptConnections() {
        while (true) {
            int client_socket;
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            client_socket = accept(listen_socket, (struct sockaddr *)&client_addr, &addr_len);

            if (client_socket < 0) {
                std::cerr << "Failed to accept connection." << std::endl;
                continue;
            }

            std::string peer_ip = inet_ntoa(client_addr.sin_addr);
            int peer_port = ntohs(client_addr.sin_port);
            connections.push_back(PeerConnection(client_socket, peer_ip, peer_port));

            std::cout << "\nNew connection from " << peer_ip << ":" << peer_port << std::endl;
            std::cout << "> "; // Re-prompt the user

            // Start a thread to listen for messages from this peer
            std::thread listener_thread(&ChatApp::listenToPeer, this, client_socket, peer_ip, peer_port);
            listener_thread.detach(); // Detach thread to run independently
        }
    }

    void connectToPeer(const std::string &command) {
        size_t pos = command.find(' ', 8);
        if (pos == std::string::npos) {
            std::cerr << "Invalid command syntax." << std::endl;
            return;
        }

        std::string ip = command.substr(8, pos - 8);
        int port = std::stoi(command.substr(pos + 1));

        int peer_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (peer_socket == -1) {
            std::cerr << "Socket creation failed." << std::endl;
            return;
        }

        struct sockaddr_in peer_addr;
        peer_addr.sin_family = AF_INET;
        peer_addr.sin_port = htons(port);
        peer_addr.sin_addr.s_addr = inet_addr(ip.c_str());

        if (connect(peer_socket, (struct sockaddr *)&peer_addr, sizeof(peer_addr)) == -1) {
            std::cerr << "Connection to " << ip << ":" << port << " failed." << std::endl;
            return;
        }

        std::cout << "Connected to " << ip << ":" << port << std::endl;
        connections.push_back(PeerConnection(peer_socket, ip, port));

        // Start a thread to listen for messages from this peer
        std::thread listener_thread(&ChatApp::listenToPeer, this, peer_socket, ip, port);
        listener_thread.detach(); // Detach thread to run independently
    }


    void processCommand(const std::string &command) {
        if (command == "help") {
            showHelp();
        } else if (command == "myip") {
            showMyIp();
        } else if (command == "myport") {
            showMyPort();
        } else if (command.rfind("connect", 0) == 0) {
            connectToPeer(command);
        } else if (command == "list") {
            listConnections();
        } else if (command.rfind("terminate", 0) == 0) {
            terminateConnection(command);
        } else if (command.rfind("send", 0) == 0) {
            sendMessage(command);
        } else if (command == "exit") {
            exitApp();
        } else {
            std::cout << "Unknown command. Type 'help' for usage." << std::endl;
        }
    }

    void showHelp() {
        std::cout << "Available Commands:" << std::endl;
        std::cout << "help - Show command manual" << std::endl;
        std::cout << "myip - Show the IP address of this process" << std::endl;
        std::cout << "myport - Show the listening port of this process" << std::endl;
        std::cout << "connect <destination> <port> - Connect to a peer" << std::endl;
        std::cout << "list - List all active connections" << std::endl;
        std::cout << "terminate <connection id> - Terminate a specific connection" << std::endl;
        std::cout << "send <connection id> <message> - Send message to a peer" << std::endl;
        std::cout << "exit - Close all connections and terminate this process" << std::endl;
    }

    void showMyIp() {
        struct ifaddrs *ifaddr, *ifa;
        char host[NI_MAXHOST];

        // Get a linked list of network interfaces
        if (getifaddrs(&ifaddr) == -1) {
            std::cerr << "Error getting network interfaces." << std::endl;
            return;
        }

        // Loop through linked list to find the first non-loopback IPv4 address
        for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == nullptr)
                continue;

            // Check for IPv4 addresses
            if (ifa->ifa_addr->sa_family == AF_INET) {
                int s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
                                    host, NI_MAXHOST, nullptr, 0, NI_NUMERICHOST);
                if (s != 0) {
                    std::cerr << "getnameinfo() failed: " << gai_strerror(s) << std::endl;
                    continue;
                }

                // Ignore loopback addresses
                if (strcmp(host, "127.0.0.1") != 0) {
                    std::cout << "IP addresses of this machine:" << ": " << host << std::endl;
                }
            }
        }

        // Free the linked list
        freeifaddrs(ifaddr);
    }

    void showMyPort() {
        std::cout << "My Port: " << ntohs(server_addr.sin_port) << std::endl;
    }

    void listConnections() {
        std::cout << "Active Connections:" << std::endl;
        for (size_t i = 0; i < connections.size(); ++i) {
            std::cout << i + 1 << ": \033[0;35m" << connections[i].ip << " " << connections[i].port << "\033[0m" << std::endl;
        }
    }

    void terminateConnection(const std::string &command) {
        int conn_id = std::stoi(command.substr(10)) - 1;

        if (conn_id >= 0 && conn_id < connections.size()) {
            close(connections[conn_id].socket_fd);
            connections.erase(connections.begin() + conn_id);
            std::cout << "Connection terminated." << std::endl;
        } else {
            std::cerr << "Invalid connection ID." << std::endl;
        }
    }

    void sendMessage(const std::string &command) {
        size_t space_pos = command.find(' ', 5);
        int conn_id = std::stoi(command.substr(5, space_pos - 5)) - 1;
        std::string message = command.substr(space_pos + 1);

        if (conn_id >= 0 && conn_id < connections.size()) {
            send(connections[conn_id].socket_fd, message.c_str(), message.length(), 0);
            std::cout << "Message sent to connection " << conn_id + 1 << std::endl;
        } else {
            std::cerr << "Invalid connection ID." << std::endl;
        }
    }

    void exitApp() {
        for (auto &conn : connections) {
            close(conn.socket_fd);
        }
        std::cout << "Exiting..." << std::endl;
        exit(0);
    }

};

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: ./chat <port>" << std::endl;
        return 1;
    }

    int port = std::stoi(argv[1]);
    ChatApp app(port);
    app.run();

    return 0;
}
