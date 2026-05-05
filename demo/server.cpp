#include <asm-generic/socket.h>
#include <iostream>
#include <cstring>
#include <thread>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#include "./helper.cpp"



void handle_client(int client_fd) {
  while (true) {
    Message received;

    received.type = MessageType::Echo;

    if (!recv_message(client_fd, received)) {
      std::cerr << "received from client failed\n";
      break; 
    }

    std::cout << "Server received: " << received.body << "\n";


    if (!send_message(client_fd, received)) {
      std::cerr << "failed to sent a message to client\n";
      break;
    }

    
  }


  close(client_fd);
}


int main() {

  int server_fd = socket(AF_INET, SOCK_STREAM, 0);

  if (server_fd == -1) {
    std::cerr << "socket failed\n";
    return 1;
  }

  int opt = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(8080);


  if (bind(server_fd, (sockaddr*)&address, sizeof(address)) == -1) {
    std::cerr << "bind failed\n";
    close(server_fd);
    return 1;
  }

  if (listen(server_fd, 5) == -1) {
    std::cerr << "listen failed\n";
    close(server_fd);
    return 1;
  }

  std::cout << "Server listening on port 8080...\n";

  while (true) {
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);
  
    int client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);
  
    if (client_fd == -1) {
      std::cerr << "accept failed\n";
      close(server_fd);
      return 1;
    }

    std::cout << "Client connected\n";

    std::thread t(handle_client, client_fd);
    t.detach();
  }
  
  close(server_fd);
  
  return 0;
  
}
