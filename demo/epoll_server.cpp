#include <iostream>
#include <unordered_map>
#include <cerrno>
#include <cstring>
#include <ctime>

#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include "./helper.cpp"


std::unordered_map<int, Connection> connections;


bool set_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);

  if (flags == -1) {
    return false;
  }

  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    return false;
  }

  return true;
}


bool modify_events(int epoll_fd, int fd, uint32_t events) {
  epoll_event ev{};

  ev.events = events;
  ev.data.fd = fd;

  return epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) != -1;
}


void close_connection(
    int epoll_fd,
    int fd,
    std::unordered_map<int, Connection>& connections
) {
  epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
  connections.erase(fd);
  close(fd);
}


Message handle_message(const Message& msg) {
  if (msg.type == MessageType::Echo) {
    return Message{MessageType::Echo, msg.body};
  }

  if (msg.type == MessageType::Time) {
    std::time_t now = std::time(nullptr);
    std::string body = std::ctime(&now);

    return Message{MessageType::Time, body};
  }

  if (msg.type == MessageType::Quit) {
    return Message{MessageType::Quit, "bye"};
  }

  return Message{MessageType::Error, "unknown message type"};
}


void on_writable(
    int epoll_fd,
    int fd,
    std::unordered_map<int, Connection>& connections
) {
  auto it = connections.find(fd);

  if (it == connections.end()) {
    return;
  }

  Connection& conn = it->second;

  while (!conn.write_buffer.empty()) {
    ssize_t n = send(
        fd,
        conn.write_buffer.data(),
        conn.write_buffer.size(),
        0
    );

    if (n > 0) {
      conn.write_buffer.erase(0, n);
      continue;
    }

    if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      return;
    }

    close_connection(epoll_fd, fd, connections);
    return;
  }

  if (conn.close_after_write) {
    close_connection(epoll_fd, fd, connections);
    return;
  }

  modify_events(epoll_fd, fd, EPOLLIN);
}


void on_readable(
    int epoll_fd,
    int fd,
    std::unordered_map<int, Connection>& connections
) {
  auto it = connections.find(fd);

  if (it == connections.end()) {
    return;
  }

  Connection& conn = it->second;

  char buffer[4096];

  while (true) {
    ssize_t n = recv(fd, buffer, sizeof(buffer), 0);

    if (n > 0) {
      conn.read_buffer.append(buffer, n);
      continue;
    }

    if (n == 0) {
      close_connection(epoll_fd, fd, connections);
      return;
    }

    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      break;
    }

    close_connection(epoll_fd, fd, connections);
    return;
  }

  Message msg;

  while (try_parse_message(conn.read_buffer, msg)) {
    Message reply = handle_message(msg);
    std::string encoded = encode_message(reply);

    if (encoded.empty() && !reply.body.empty()) {
      close_connection(epoll_fd, fd, connections);
      return;
    }

    conn.write_buffer.append(encoded);

    if (msg.type == MessageType::Quit) {
      conn.close_after_write = true;
      break;
    }
  }

  if (!conn.write_buffer.empty()) {
    modify_events(epoll_fd, fd, EPOLLIN | EPOLLOUT);
  }
}


void accept_clients(int epoll_fd, int server_fd) {
  while (true) {
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);

    int client_fd = accept(
        server_fd,
        reinterpret_cast<sockaddr*>(&client_addr),
        &client_len
    );

    if (client_fd == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;
      }

      std::cerr << "accept failed: " << std::strerror(errno) << "\n";
      break;
    }

    if (!set_nonblocking(client_fd)) {
      std::cerr << "set client nonblocking failed\n";
      close(client_fd);
      continue;
    }

    epoll_event client_event{};
    client_event.events = EPOLLIN;
    client_event.data.fd = client_fd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_event) == -1) {
      std::cerr << "epoll_ctl add client_fd failed\n";
      close(client_fd);
      continue;
    }

    connections[client_fd] = Connection{};

    std::cout << "Client connected: " << client_fd << "\n";
  }
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

  if (bind(server_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == -1) {
    std::cerr << "bind failed: " << std::strerror(errno) << "\n";
    close(server_fd);
    return 1;
  }

  if (listen(server_fd, 5) == -1) {
    std::cerr << "listen failed: " << std::strerror(errno) << "\n";
    close(server_fd);
    return 1;
  }

  if (!set_nonblocking(server_fd)) {
    std::cerr << "set server nonblocking failed\n";
    close(server_fd);
    return 1;
  }

  int epoll_fd = epoll_create1(0);

  if (epoll_fd == -1) {
    std::cerr << "epoll_create1 failed\n";
    close(server_fd);
    return 1;
  }

  epoll_event server_event{};
  server_event.events = EPOLLIN;
  server_event.data.fd = server_fd;

  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &server_event) == -1) {
    std::cerr << "epoll_ctl add server_fd failed\n";
    close(epoll_fd);
    close(server_fd);
    return 1;
  }

  std::cout << "Server listening on port 8080...\n";

  const int MAX_EVENTS = 64;
  epoll_event events[MAX_EVENTS];

  while (true) {
    int n_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);

    if (n_events == -1) {
      if (errno == EINTR) {
        continue;
      }

      std::cerr << "epoll_wait failed: " << std::strerror(errno) << "\n";
      break;
    }

    for (int i = 0; i < n_events; i++) {
      int fd = events[i].data.fd;
      uint32_t event_flags = events[i].events;

      if (fd == server_fd) {
        accept_clients(epoll_fd, server_fd);
        continue;
      }

      if (event_flags & (EPOLLERR | EPOLLHUP)) {
        close_connection(epoll_fd, fd, connections);
        continue;
      }

      if (event_flags & EPOLLIN) {
        on_readable(epoll_fd, fd, connections);
      }

      if (event_flags & EPOLLOUT) {
        on_writable(epoll_fd, fd, connections);
      }
    }
  }

  close(epoll_fd);
  close(server_fd);

  return 0;
}
