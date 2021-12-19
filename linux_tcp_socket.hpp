#ifndef HTTPWEBSERVER_LINUX_TCP_SOCKET_HPP__
#define HTTPWEBSERVER_LINUX_TCP_SOCKET_HPP__

#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>

#include "common.hpp"
#include "interfaces/socket.hpp"
#include "connection.hpp"
#include "timeval.hpp"

namespace nt { namespace http {

typedef void (* event_callback)(void*, void*);

class __HttpWebServerSocketPort__ LinuxTcpSocket :
      public nt::http::interfaces::Socket
{
private:
    std::string        port;
    unsigned int       queue_count;
    const unsigned int max_connections;

    SOCKET server_pipe;
    SOCKET server_socket;

    bool is_open;

    std::vector<std::shared_ptr<Connection>> connections;

    fd_set read_list;
    fd_set write_list;
    fd_set error_list;

protected:
    int protocol;

public:
    LinuxTcpSocket();
    ~LinuxTcpSocket() noexcept;

    void bind(const char*, const char*);
    void bind(const char*, const unsigned short);
    void listen(const unsigned int, event_callback);
    void open();
    void close();

private:
    void close_socket(SOCKET);
    void reset_socket_lists();
    unsigned int get_last_socket();
    bool select();
    void handle_connection();
    void receive_data(SOCKET);
    void write_data(SOCKET);
};

}}

#endif /* HTTPWEBSERVER_LINUX_TCP_SOCKET_HPP__ */
