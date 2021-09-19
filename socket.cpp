#include "socket.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <memory>
#include <algorithm>
#include <tinythread.h>
#include <tls.h>

#include <macros/leave_loop_if.hpp>
#include <macros/scope_guard.hpp>
#include <macros/repeat_until.hpp>

using namespace nt::http;

namespace {
#ifdef __MINGW32__
int
inet_pton(int af, const char* src, void* dst)
{
    sockaddr_storage ss;
    int  size = sizeof(ss);
    char src_copy[INET6_ADDRSTRLEN + 1];

    ::ZeroMemory(&ss, sizeof(ss));
    /* stupid non-const API */
    strncpy(src_copy, src, INET6_ADDRSTRLEN + 1);
    src_copy[INET6_ADDRSTRLEN] = 0;

    if (::WSAStringToAddress(src_copy, af, NULL, (sockaddr*)&ss, &size) == 0) {
        switch (af) {
        case AF_INET:
            *(in_addr*)dst = ((sockaddr_in*)&ss)->sin_addr;
            return 1;
        case AF_INET6:
            *(in6_addr*)dst = ((sockaddr_in6*)&ss)->sin6_addr;
            return 1;
        }
    }

    return 0;
}

const char*
inet_ntop(int af, const void* src, char* dst, socklen_t size)
{
    sockaddr_storage ss;
    unsigned long s = size;

    ::ZeroMemory(&ss, sizeof(ss));
    ss.ss_family = af;

    switch (af) {
    case AF_INET:
        ((sockaddr_in*)&ss)->sin_addr = *(in_addr*)src;
        break;
    case AF_INET6:
        ((sockaddr_in6*)&ss)->sin6_addr = *(in6_addr*)src;
        break;
    default:
        return NULL;
    }

    /* cannot direclty use &size because of strict aliasing rules */
    return
        (::WSAAddressToString((sockaddr*)&ss, sizeof(ss), NULL, dst, &s) == 0) ?
        dst :
        NULL;
}
#endif

void*
get_in_addr(sockaddr* sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((sockaddr_in*)sa)->sin_addr);
    }

    return &(((sockaddr_in6*)sa)->sin6_addr);
}

std::string
get_in_ip(sockaddr* sa)
{
    char client_ip[INET6_ADDRSTRLEN];

    inet_ntop(sa->sa_family, get_in_addr(sa), client_ip, sizeof(client_ip));

    return client_ip;
}

unsigned int
get_in_port(sockaddr* sa)
{
    if (sa->sa_family == AF_INET) {
        return ::ntohs(((sockaddr_in*)sa)->sin_port);
    }

    return ::ntohs(((sockaddr_in6*)sa)->sin6_port);
}

const std::string
_get_last_error()
{
#ifdef LOSE
    int error = ::WSAGetLastError();
#elif defined(LINUX)
    int error = errno;
#endif

    switch (error) {
#ifdef LOSE
    case WSA_INVALID_HANDLE: return "Specified event object handle is invalid.";
    case WSA_NOT_ENOUGH_MEMORY: return "Insufficient memory available.";
    case WSA_INVALID_PARAMETER: return "One or more parameters are invalid.";
    case WSA_OPERATION_ABORTED: return "Overlapped operation aborted.";
    case WSA_IO_INCOMPLETE: return "Overlapped I/O event object not in signaled state.";
    case WSA_IO_PENDING: return "Overlapped operations will complete later.";
    case WSAEINTR: return "Interrupted function call.";
    case WSAEBADF: return "File handle is not valid.";
    case WSAEACCES: return "Permission denied.";
    case WSAEFAULT: return "Bad address.";
    case WSAEINVAL: return "Invalid argument.";
    case WSAEMFILE: return "Too many open files.";
    case WSAEWOULDBLOCK: return "Resource temporarily unavailable.";
    case WSAEINPROGRESS: return "Operation now in progress.";
    case WSAEALREADY: return "Operation already in progress.";
    case WSAENOTSOCK: return "Socket operation on nonsocket.";
    case WSAEDESTADDRREQ: return "Destination address required.";
    case WSAEMSGSIZE: return "Message too long.";
    case WSAEPROTOTYPE: return "Protocol wrong type for socket.";
    case WSAENOPROTOOPT: return "Bad protocol option.";
    case WSAEPROTONOSUPPORT: return "Protocol not supported.";
    case WSAESOCKTNOSUPPORT: return "Socket type not supported.";
    case WSAEOPNOTSUPP: return "Operation not supported.";
    case WSAEPFNOSUPPORT: return "Protocol family not supported.";
    case WSAEAFNOSUPPORT: return "Address family not supported by protocol family.";
    case WSAEADDRINUSE: return "Address already in use.";
    case WSAEADDRNOTAVAIL: return "Cannot assign requested address.";
    case WSAENETDOWN: return "Network is down.";
    case WSAENETUNREACH: return "Network is unreachable.";
    case WSAENETRESET: return "Network dropped connection on reset.";
    case WSAECONNABORTED: return "Software caused connection abort.";
    case WSAECONNRESET: return "Connection reset by peer.";
    case WSAENOBUFS: return "No buffer space available.";
    case WSAEISCONN: return "Socket is already connected.";
    case WSAENOTCONN: return "Socket is not connected.";
    case WSAESHUTDOWN: return "Cannot send after socket shutdown.";
    case WSAETOOMANYREFS: return "Too many references.";
    case WSAETIMEDOUT: return "Connection timed out.";
    case WSAECONNREFUSED: return "Connection refused.";
    case WSAELOOP: return "Cannot translate name.";
    case WSAENAMETOOLONG: return "Name too long.";
    case WSAEHOSTDOWN: return "Host is down.";
    case WSAEHOSTUNREACH: return "No route to host.";
    case WSAENOTEMPTY: return "Directory not empty.";
    case WSAEPROCLIM: return "Too many processes.";
    case WSAEUSERS: return "User quota exceeded.";
    case WSAEDQUOT: return "Disk quota exceeded.";
    case WSAESTALE: return "Stale file handle reference.";
    case WSAEREMOTE: return "Item is remote.";
    case WSASYSNOTREADY: return "Network subsystem is unavailable.";
    case WSAVERNOTSUPPORTED: return "Winsock.dll version out of range.";
    case WSANOTINITIALISED: return "Successful WSAStartup not yet performed.";
    case WSAEDISCON: return "Graceful shutdown in progress.";
    case WSAENOMORE: return "No more results.";
    case WSAECANCELLED: return "Call has been canceled.";
    case WSAEINVALIDPROCTABLE: return "Procedure call table is invalid.";
    case WSAEINVALIDPROVIDER: return "Service provider is invalid.";
    case WSAEPROVIDERFAILEDINIT: return "Service provider failed to initialize.";
    case WSASYSCALLFAILURE: return "System call failure.";
    case WSASERVICE_NOT_FOUND: return "Service not found.";
    case WSATYPE_NOT_FOUND: return "Class type not found.";
    case WSA_E_NO_MORE: return "No more results.";
    case WSA_E_CANCELLED: return "Call was canceled.";
    case WSAEREFUSED: return "Database query was refused.";
    case WSAHOST_NOT_FOUND: return "Host not found.";
    case WSATRY_AGAIN: return "Nonauthoritative host not found.";
    case WSANO_RECOVERY: return "This is a nonrecoverable error.";
    case WSANO_DATA: return "Valid name, no data record of requested type.";
    case WSA_QOS_RECEIVERS: return "QoS receivers.";
    case WSA_QOS_SENDERS: return "QoS senders.";
    case WSA_QOS_NO_SENDERS: return "No QoS senders.";
    case WSA_QOS_NO_RECEIVERS: return "QoS no receivers.";
    case WSA_QOS_REQUEST_CONFIRMED: return "QoS request confirmed.";
    case WSA_QOS_ADMISSION_FAILURE: return "QoS admission error.";
    case WSA_QOS_POLICY_FAILURE: return "QoS policy failure.";
    case WSA_QOS_BAD_STYLE: return "QoS bad style.";
    case WSA_QOS_BAD_OBJECT: return "QoS bad object.";
    case WSA_QOS_TRAFFIC_CTRL_ERROR: return "QoS traffic control error.";
    case WSA_QOS_GENERIC_ERROR: return "QoS generic error.";
    case WSA_QOS_ESERVICETYPE: return "QoS service type error.";
    case WSA_QOS_EFLOWSPEC: return "QoS flowspec error.";
    case WSA_QOS_EPROVSPECBUF: return "Invalid QoS provider buffer.";
    case WSA_QOS_EFILTERSTYLE: return "Invalid QoS filter style.";
    case WSA_QOS_EFILTERTYPE: return "Invalid QoS filter type.";
    case WSA_QOS_EFILTERCOUNT: return "Incorrect QoS filter count.";
    case WSA_QOS_EOBJLENGTH: return "Invalid QoS object length.";
    case WSA_QOS_EFLOWCOUNT: return "Incorrect QoS flow count.";
    case WSA_QOS_EUNKOWNPSOBJ: return "Unrecognized QoS object.";
    case WSA_QOS_EPOLICYOBJ: return "Invalid QoS policy object.";
    case WSA_QOS_EFLOWDESC: return "Invalid QoS flow descriptor.";
    case WSA_QOS_EPSFLOWSPEC: return "Invalid QoS provider-specific flowspec.";
    case WSA_QOS_EPSFILTERSPEC: return "Invalid QoS provider-specific filterspec.";
    case WSA_QOS_ESDMODEOBJ: return "Invalid QoS shape discard mode object.";
    case WSA_QOS_ESHAPERATEOBJ: return "Invalid QoS shaping rate object.";
    case WSA_QOS_RESERVED_PETYPE: return "Reserved policy QoS element type.";
#elif defined(LINUX)
    case EPERM: return "Operation not permitted";
    case ENOENT: return "No such file or directory";
    case ESRCH: return "No such process";
    case EINTR: return "Interrupted system call";
    case EIO: return "I/O error";
    case ENXIO: return "No such device or address";
    case E2BIG: return "Argument list too long";
    case ENOEXEC: return "Exec format error";
    case EBADF: return "Bad file number";
    case ECHILD: return "No child processes";
    // case EAGAIN: return "Try again";
    case ENOMEM: return "Out of memory";
    case EACCES: return "Permission denied";
    case EFAULT: return "Bad address";
    case ENOTBLK: return "Block device required";
    case EBUSY: return "Device or resource busy";
    case EEXIST: return "File exists";
    case EXDEV: return "Cross-device link";
    case ENODEV: return "No such device";
    case ENOTDIR: return "Not a directory";
    case EISDIR: return "Is a directory";
    case EINVAL: return "Invalid argument";
    case ENFILE: return "File table overflow";
    case EMFILE: return "Too many open files";
    case ENOTTY: return "Not a typewriter";
    case ETXTBSY: return "Text file busy";
    case EFBIG: return "File too large";
    case ENOSPC: return "No space left on device";
    case ESPIPE: return "Illegal seek";
    case EROFS: return "Read-only file system";
    case EMLINK: return "Too many links";
    case EPIPE: return "Broken pipe";
    case EDOM: return "Math argument out of domain of func";
    case ERANGE: return "Math result not representable";
    case EDEADLK: return "Resource deadlock would occur";
    case ENAMETOOLONG: return "File name too long";
    case ENOLCK: return "No record locks available";
    case ENOSYS: return "Invalid system call number";
    case ENOTEMPTY: return "Directory not empty";
    case ELOOP: return "Too many symbolic links encountered";
    case EWOULDBLOCK: return "Operation would block";
    case ENOMSG: return "No message of desired type";
    case EIDRM: return "Identifier removed";
    case ECHRNG: return "Channel number out of range";
    case EL2NSYNC: return "Level 2 not synchronized";
    case EL3HLT: return "Level 3 halted";
    case EL3RST: return "Level 3 reset";
    case ELNRNG: return "Link number out of range";
    case EUNATCH: return "Protocol driver not attached";
    case ENOCSI: return "No CSI structure available";
    case EL2HLT: return "Level 2 halted";
    case EBADE: return "Invalid exchange";
    case EBADR: return "Invalid request descriptor";
    case EXFULL: return "Exchange full";
    case ENOANO: return "No anode";
    case EBADRQC: return "Invalid request code";
    case EBADSLT: return "Invalid slot";
    case EBFONT: return "Bad font file format";
    case ENOSTR: return "Device not a stream";
    case ENODATA: return "No data available";
    case ETIME: return "Timer expired";
    case ENOSR: return "Out of streams resources";
    case ENONET: return "Machine is not on the network";
    case ENOPKG: return "Package not installed";
    case EREMOTE: return "Object is remote";
    case ENOLINK: return "Link has been severed";
    case EADV: return "Advertise error";
    case ESRMNT: return "Srmount error";
    case ECOMM: return "Communication error on send";
    case EPROTO: return "Protocol error";
    case EMULTIHOP: return "Multihop attempted";
    case EDOTDOT: return "RFS specific error";
    case EBADMSG: return "Not a data message";
    case EOVERFLOW: return "Value too large for defined data type";
    case ENOTUNIQ: return "Name not unique on network";
    case EBADFD: return "File descriptor in bad state";
    case EREMCHG: return "Remote address changed";
    case ELIBACC: return "Can not access a needed shared library";
    case ELIBBAD: return "Accessing a corrupted shared library";
    case ELIBSCN: return ".lib section in a.out corrupted";
    case ELIBMAX: return "Attempting to link in too many shared libraries";
    case ELIBEXEC: return "Cannot exec a shared library directly";
    case EILSEQ: return "Illegal byte sequence";
    case ERESTART: return "Interrupted system call should be restarted";
    case ESTRPIPE: return "Streams pipe error";
    case EUSERS: return "Too many users";
    case ENOTSOCK: return "Socket operation on non-socket";
    case EDESTADDRREQ: return "Destination address required";
    case EMSGSIZE: return "Message too long";
    case EPROTOTYPE: return "Protocol wrong type for socket";
    case ENOPROTOOPT: return "Protocol not available";
    case EPROTONOSUPPORT: return "Protocol not supported";
    case ESOCKTNOSUPPORT: return "Socket type not supported";
    case EOPNOTSUPP: return "Operation not supported on transport endpoint";
    case EPFNOSUPPORT: return "Protocol family not supported";
    case EAFNOSUPPORT: return "Address family not supported by protocol";
    case EADDRINUSE: return "Address already in use";
    case EADDRNOTAVAIL: return "Cannot assign requested address";
    case ENETDOWN: return "Network is down";
    case ENETUNREACH: return "Network is unreachable";
    case ENETRESET: return "Network dropped connection because of reset";
    case ECONNABORTED: return "Software caused connection abort";
    case ECONNRESET: return "Connection reset by peer";
    case ENOBUFS: return "No buffer space available";
    case EISCONN: return "Transport endpoint is already connected";
    case ENOTCONN: return "Transport endpoint is not connected";
    case ESHUTDOWN: return "Cannot send after transport endpoint shutdown";
    case ETOOMANYREFS: return "Too many references: cannot splice";
    case ETIMEDOUT: return "Connection timed out";
    case ECONNREFUSED: return "Connection refused";
    case EHOSTDOWN: return "Host is down";
    case EHOSTUNREACH: return "No route to host";
    case EALREADY: return "Operation already in progress";
    case EINPROGRESS: return "Operation now in progress";
    case ESTALE: return "Stale file handle";
    case EUCLEAN: return "Structure needs cleaning";
    case ENOTNAM: return "Not a XENIX named type file";
    case ENAVAIL: return "No XENIX semaphores available";
    case EISNAM: return "Is a named type file";
    case EREMOTEIO: return "Remote I/O error";
    case EDQUOT: return "Quota exceeded";
    case ENOMEDIUM: return "No medium found";
    case EMEDIUMTYPE: return "Wrong medium type";
    case ECANCELED: return "Operation Canceled";
    case ENOKEY: return "Required key not available";
    case EKEYEXPIRED: return "Key has expired";
    case EKEYREVOKED: return "Key has been revoked";
    case EKEYREJECTED: return "Key was rejected by service";
    case EOWNERDEAD: return "Owner died";
    case ENOTRECOVERABLE: return "State not recoverable";
    case ERFKILL: return "Operation not possible due to RF-kill";
    case EHWPOISON: return "Memory page has hardware error";
#endif
    default: return "Error code: " + std::to_string(error);
    }
}

const std::string
_get_last_error(const std::string& prefix)
{
    return prefix + " " + _get_last_error();
}

void
_tls()
{
    tls       * tls;
    tls_config* tls_config;

    if (tls_init() != 0) {
        printf("tls_init() failed\n");
        return;
    }

    if ((tls = tls_server()) == nullptr) {
        printf("tls_server() failed\n");
        return;
    }

    if ((tls_config = tls_config_new()) == nullptr) {
        printf("tls_config_new() failed\n");
        return;
    }

    tls_close(tls);
    tls_free(tls);
    tls_config_free(tls_config);
}
}

Socket::Socket() :
      port("0"),
      queue_count(0),
      max_connections(FD_SETSIZE - 1),
      server_socket(0),
      is_open(false),
      protocol(0)
{
    _tls();

    connections.reserve(max_connections);
    // connections = std::vector<Connection*>(max_connections, new Connection());

#ifdef LOSE
    int     ret;
    WSADATA wsaData;

    if ((ret = ::WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0) {
        std::string error = _get_last_error("Failed to start up.");

        throw std::runtime_error(error.c_str());
    }
#endif
}

Socket::~Socket() noexcept
{
    if (is_open) {
        close();
    }

#ifdef LOSE
    if (::WSACleanup() == SOCKET_ERROR) {
        std::string error = _get_last_error("Failed to clean up.");

        std::cerr << error << std::endl;
        // throw std::runtime_error(error.c_str());
    }
#endif
}

addrinfo*
Socket::get_addrinfo(const char* server_address)
{
    addrinfo* server_info;
    addrinfo hints = {0};

    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = protocol; // 0 = ANY
    hints.ai_flags    = AI_PASSIVE;

    int result = 0;

    if ((result = ::getaddrinfo(server_address, port.c_str(), &hints, &server_info)) != SOCKET_NOERROR) {
        std::string error = _get_last_error("Failed to get information about the specified network port/service '" + port + "'.");

        throw std::runtime_error(error.c_str());
    }


    return server_info;
}

static int
get_bound_port(SOCKET socket)
{
    sockaddr_in sin;

    socklen_t addrlen = (socklen_t)sizeof(sin);

    if(::getsockname(socket, (sockaddr*)&sin, &addrlen) != SOCKET_NOERROR) {
        std::string error = _get_last_error("Unable to get bound port.");

        throw std::runtime_error(error.c_str());
    }

    return ntohs(sin.sin_port);
}

void
Socket::create_socket(addrinfo* server_info)
{
    int  bind_result = SOCKET_NOERROR;
    server_socket = INVALID_SOCKET;

    for (addrinfo* p = server_info; p != nullptr; p = p->ai_next) {
        if (is_open) {
            close_socket(server_socket);

            is_open = false;
        }

        continue_if ((server_socket = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == INVALID_SOCKET);

#ifdef LOSE
        static const char ONE = '1';
        int REUSE_PORT_ADDR = SO_REUSEADDR;
#elif defined(LINUX)
        static const int ONE = 1;
        int REUSE_PORT_ADDR = SO_REUSEPORT;
#endif

        is_open = true;

        break_if (::setsockopt(server_socket, SOL_SOCKET, REUSE_PORT_ADDR, &ONE, sizeof(ONE)) != SOCKET_ERROR &&
                  (bind_result = ::bind(server_socket, p->ai_addr, p->ai_addrlen)) != SOCKET_ERROR);
    }

    server_info = nullptr;

    if (server_socket == INVALID_SOCKET) {
        std::string error = _get_last_error("Failed to create socket.");

        throw std::runtime_error(error.c_str());
    }

    if(bind_result == SOCKET_ERROR) {
        std::string error = _get_last_error("Failed to bind port/service " + port + ".");

        close_socket(server_socket);

        is_open = false;

        throw std::runtime_error(error.c_str());
    }

    std::cout << "listening to port " << get_bound_port(server_socket) << std::endl;

    is_open = true;
}

void
Socket::bind(const char* server_address, const char* service)
{
    addrinfo* server_info = nullptr;

    ______________________________________________________________
        if(server_info != nullptr) {
            freeaddrinfo(server_info);
        }
    _____________________________________________________________;

    port = service;

    server_info = get_addrinfo(server_address);

    create_socket(server_info);
}

void
Socket::bind(const char* server_address, const unsigned short port_no)
{
    bind(server_address, std::to_string(port_no).c_str());
}

void
Socket::listen(const unsigned int count, event_callback callback)
{
    queue_count = count;

    int listen_result = ::listen(server_socket, queue_count);

    if(listen_result == SOCKET_ERROR) {
        std::string error = _get_last_error("Failed to listen to port/service " + port + ".");

        close_socket(server_socket);

        throw std::runtime_error(error.c_str());
    }

    connections.push_back(server_socket);
}

void
Socket::handle_connection()
{
    sockaddr_storage client_addr;

    socklen_t storage_size = sizeof(client_addr);
    SOCKET client          = ::accept(server_socket, (sockaddr*)&client_addr, &storage_size);

    if (client == INVALID_SOCKET) {
        std::string error = _get_last_error("Failed to accept from client.");

        // todo: do not throw, just close the socket or what not
        throw std::runtime_error(error.c_str());
    }

    FD_SET(client, &read_list);
    connections.push_back(client);

    {
        std::string  client_ip   = get_in_ip((sockaddr*)&client_addr);
        unsigned int client_port = get_in_port((sockaddr*)&client_addr);

        std::cout << "------------------------------\n"
                  << "[" << server_socket << "] has "
                  << "new connection from " << client_ip << ":" << client_port
                  << " [" << client << "]"
                  << std::endl;
    }
}

void
Socket::receive_data(SOCKET connection)
{
    int         bytes_rx;
    std::string request;

    repeat {
        char buffer[MAX_INPUT] = {0};
        const static int flags = 0; //MSG_DONTWAIT;

        if ((bytes_rx = ::recv(connection, buffer, sizeof(buffer) - 1, flags)) == SOCKET_ERROR) {
            std::string error = _get_last_error("Failed to receive data.");

            // todo: do not throw, just close the socket or what not
            // throw std::runtime_error(error.c_str());
            break;
        }

        buffer[bytes_rx] = '\0';

        request += std::string(buffer);

        if (request.size() >= 4) {
            // signifies the end of the headers
            // if there is post data, one needs to check the
            // Content-Length
            std::string end = request.substr(request.size() - 4, 4);
            bool is_end = end == "\r\n\r\n";

            if(is_end) {
                // FD_CLR(connection, &read_list);
                break;
            }
        }
    }
    until(bytes_rx == 0);

    if(request.empty()) {
        request = "(nothing)";
    }

    std::cout << "received from [" << connection << "]\n"
              << request
              << std::endl;
}

void
Socket::write_data(SOCKET connection)
{
    sockaddr_storage client_addr;

    socklen_t storage_size = sizeof(client_addr);

    ::getpeername(connection, (sockaddr*)&client_addr, &storage_size);

    std::string  client_ip   = get_in_ip((sockaddr*)&client_addr);
    unsigned int client_port = get_in_port((sockaddr*)&client_addr);

    auto hostname = (char*)calloc(HOST_NAME_MAX + 1, sizeof(char));
    if (::gethostname(hostname, HOST_NAME_MAX) == SOCKET_ERROR) {
        std::string error = _get_last_error("Failed to get host name.");

        // todo: do not throw, just close the socket or what not
        throw std::runtime_error(error.c_str());
    }

    tthread::this_thread::sleep_for(tthread::chrono::milliseconds(0));

    std::string body = "<p>host ip: " + std::string(client_ip) + ":" + std::to_string(client_port) +
                       "</p>\n"
                       "<p>host name: " + std::string(hostname) +
                       "</p>\n"
                       "<p>request</p>\n"
                       // "<pre>" + request + "</pre>\n"
                       //                     "<a href=''>refresh</a>"
                       "\r\n";

    std::string response = "HTTP/1.1 200 OK\r\n"
                           "Content-Type: text/html; charset=UTF-8\r\n"
                           "Connection: keep-alive\r\n"
                           "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" +
                           body.c_str();

    ::free(hostname);
    ::send(connection, response.c_str(), response.size(), 0);
    // close_socket(connection);
    FD_CLR(connection, &write_list);
    
    std::cout << "writing response"
              << "to [" << connection << "]"
              << std::endl;
}

void
Socket::reset_socket_lists()
{
    FD_ZERO(&read_list);
    FD_ZERO(&write_list);
    // FD_ZERO(&error_list);

    auto closed = [](const Connection& c){
        return c.socket == INVALID_SOCKET;
    };

    connections.erase(std::remove_if(connections.begin(),
                                     connections.end(),
                                     closed),
                      connections.end());

    for (auto &connection: connections) {
        // continue_if(connection.socket == INVALID_SOCKET);

        FD_SET(connection.socket, &read_list);
        FD_SET(connection.socket, &write_list);
        // FD_SET(connection->socket, &error_list);
    }
}

unsigned int
Socket::get_last_socket()
{
    unsigned int last_socket = server_socket;

    for (auto& connection: connections) {
        if (connection.socket > last_socket) {
            last_socket = connection.socket;
        }
    }

    return last_socket;
}

bool
Socket::select()
{
    reset_socket_lists();

    unsigned int last_socket = get_last_socket();
    timeval* timeout = nullptr;

    int select_result;

    std::cout << "selecting from " << connections.size() << " connection(s)\n";

    if ((select_result = ::select(last_socket + 1, &read_list, &write_list, nullptr, timeout)) == SOCKET_ERROR) {
        std::string error = _get_last_error("Failed to poll connections.");

        close_socket(server_socket);

        throw std::runtime_error(error.c_str());
    }

    return select_result == 0;
}

void
Socket::open()
{
    unsigned int ceiling = 0;

    while (true) {
        continue_if (select());

        if(FD_ISSET(server_socket, &read_list)) {
            if (ceiling < max_connections) {
                handle_connection();
                ceiling++;
            }
        }

        for (auto& connection : connections) {
            continue_if (connection.socket == INVALID_SOCKET
                        // || connection.socket == server_socket
                            );

            if(connection.socket != server_socket && FD_ISSET(connection.socket, &read_list)) {
                receive_data(connection.socket);
                // close_socket(connection.socket);
                // connection.socket = INVALID_SOCKET;
                // ceiling--;
            }

            if(FD_ISSET(connection.socket, &write_list)) {
                write_data(connection.socket);
                close_socket(connection.socket);
                connection.socket = INVALID_SOCKET;
                ceiling--;
            }
        }

    }
}

void
Socket::close()
{
    close_socket(server_socket);

    is_open = false;
}

void
Socket::close_socket(SOCKET s)
{
#ifdef LOSE
    int shutdown_result = ::shutdown(s, SD_BOTH);

    if(shutdown_result == SOCKET_ERROR && ::WSAGetLastError() == WSAENOTCONN) {
        shutdown_result = SOCKET_NOERROR;
    }
#elif defined(LINUX)
    int shutdown_result = ::shutdown(s, SHUT_RDWR);

    if(shutdown_result == SOCKET_ERROR && errno == ENOTCONN) {
        shutdown_result = SOCKET_NOERROR;
    }
#endif

    if(shutdown_result == SOCKET_ERROR) {
        std::string error = _get_last_error("Failed to shutdown connection.");

        std::cerr << error << std::endl;
        // throw std::runtime_error(error.c_str());
    }

#ifdef LOSE
    ::closesocket(s);
#elif defined(LINUX)
    ::close(s);
#endif
}
