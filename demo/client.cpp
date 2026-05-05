#include <iostream>
#include <cstring>
#include <unistd.h>


#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#include "./helper.cpp"

int main() {

  int sockfd = socket(AF_INET, SOCK_STREAM, 0);

  if (sockfd == -1) {
    std::cerr << "socket failed\n";
    return 1;
  }

  sockaddr_in server_addr{};

  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(8080);

  if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
    std::cerr << "invalid address\n";
    close(sockfd);
    return 1;
  }

  if (connect(sockfd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
    std::cerr << "connect failed\n";
    close(sockfd);
    return 1;
  }

  std::cout << "Connected to server\n";

  std::string line;
  
  while (std::getline(std::cin, line)) {
    if (line == "quit" || line.empty()) break;

    Message message;

    message.type = MessageType::Echo;
    message.body = line;

    if (!send_message(sockfd, message)) {
      std::cerr << "send failed\n";
      break;
    }

    Message reply;
    
    if (!recv_message(sockfd, reply)) {
      std::cerr << "recv failed\n";
      break;
    }


    std::cout << "Socket received: " << reply.body << "\n";

    
  }

   close(sockfd);

  return 0;
   
}
