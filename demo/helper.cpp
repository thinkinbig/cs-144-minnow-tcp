#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <sys/socket.h>
#include <string>
#include <sys/types.h>
#include <arpa/inet.h>
#include <cstring>




const uint32_t MAX_MESSAGE_SIZE = 1024 * 1024; // 1MB


enum class MessageType: uint32_t {
  Echo = 1,
  Time = 2,
  Quit = 3,
  Error = 999
};


struct Message {
  MessageType type;
  std::string body;
};


struct Connection {
  std::string read_buffer;
  std::string write_buffer;

  bool close_after_write = false;
};


bool send_all(int fd, const char* data, std::size_t len) {
  size_t total_sent = 0;

  while (total_sent < len) {
    ssize_t sent = send(fd, data + total_sent, len - total_sent, 0);

    if (sent <= 0) {
      return false;
    }

    total_sent += sent;
  }

  return true;
}


bool recv_all(int fd, char* buffer, size_t len) {

  size_t total_read = 0;

  while (total_read < len) {
    ssize_t n = recv(fd, buffer + total_read, len - total_read, 0);

    if (n <= 0) {
      return false;
    }

    total_read += n;
  }

  return true;  
}


bool recv_message(int fd, Message& msg) {
  uint32_t type_net = 0;
  uint32_t len_net = 0;

  if (!recv_all(fd, reinterpret_cast<char*>(&type_net), sizeof(type_net))) {
    return false;
  }

  if (!recv_all(fd, reinterpret_cast<char*>(&len_net), sizeof(len_net))) {
    return false;
  }

  uint32_t len = ntohl(len_net);


  if (len > MAX_MESSAGE_SIZE) {
    return false;
  }

  uint32_t type = ntohl(type_net);
  msg.type = static_cast<MessageType>(type);
  
  msg.body.resize(len);

  if (!recv_all(fd, msg.body.data(), len)) {
    return false;
  }

  return true;
}

bool send_message(int fd, const Message& msg) {

  if (msg.body.size() > MAX_MESSAGE_SIZE) {
    return false;
  }

  uint32_t type = static_cast<uint32_t>(msg.type);
  uint32_t len = static_cast<uint32_t>(msg.body.size());

  uint32_t type_net = htonl(type);
  uint32_t len_net = htonl(len);

  if (!send_all(fd, reinterpret_cast<const char*>(&type_net), sizeof(type_net))) {
    return false;
  }

  if (!send_all(fd, reinterpret_cast<const char*>(&len_net), sizeof(len_net))) {
    return false;
  }

  if (!send_all(fd, msg.body.data(), msg.body.size())) {
    return false;
  }

  return true;
}


bool read_line(int fd, std::string& line) {
  line.clear();

  char ch;

  while (true) {
    ssize_t n = recv(fd, &ch, 1, 0);

    if (n == 0) {
      return false;
    }

    if (n < 0) {
      return false;
    }

    if (ch == '\n') {
      return true;
    }

    line.push_back(ch);
  }
}


std::string encode_message(const Message& msg) {
  std::string encoded;

  if (msg.body.size() > MAX_MESSAGE_SIZE) {
    return encoded;
  }

  uint32_t type = static_cast<uint32_t>(msg.type);
  uint32_t len = static_cast<uint32_t>(msg.body.size());

  uint32_t type_net = htonl(type);
  uint32_t len_net = htonl(len);

  
  encoded.append(reinterpret_cast<const char*>(&type_net), sizeof(type_net));
  encoded.append(reinterpret_cast<const char*>(&len_net), sizeof(len_net));
  encoded.append(msg.body.data(), msg.body.size());

  return encoded;
}


bool try_parse_message(std::string& buffer, Message& msg) {
  const std::size_t header_size = sizeof(uint32_t) + sizeof(uint32_t);

  if (buffer.size() < header_size) {
    return false;
  }

  uint32_t type_net = 0;
  uint32_t len_net = 0;

  std::memcpy(&type_net, buffer.data(), sizeof(type_net));
  std::memcpy(
    &len_net,
    buffer.data() + sizeof(type_net),
    sizeof(len_net)
  );

  uint32_t type = ntohl(type_net);
  uint32_t len = ntohl(len_net);

  if (len > MAX_MESSAGE_SIZE) {
    return false;
  }

  if (buffer.size() < header_size + len) {
    return false;
  }

  msg.type = static_cast<MessageType>(type);
  msg.body.assign(
    buffer.data() + header_size,
    len
  );

  buffer.erase(0, header_size + len);

  return true;
}

