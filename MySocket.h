#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>  //sockaddr_in结构体
#include <fcntl.h> // 包含 fcntl 函数的头文件  非阻塞
#include <unistd.h>
#include <stdexcept>


namespace RTK{

class MySocket
{
private:
    // 套接字文件描述符
    int _fd;

public:
    // 构造函数，创建套接字
    MySocket()
    {
        _fd = socket(AF_INET, SOCK_STREAM, 0);
        if (_fd == -1)
        {
            throw std::runtime_error("Failed to create socket");
        }
    }

    // 析构函数，关闭套接字
    ~MySocket()
    {
        if (_fd != -1)
        {
            close(_fd);
        }
    }

   // 绑定套接字到指定的 IP 地址和端口
    void bindSocket(const std::string& ip, int port)
    {
        struct sockaddr_in addr;
        addr.sin_family = AF_INET; // IPv4
        addr.sin_port = htons(port); // 端口号，转换为网络字节序

        // 将 IP 地址从字符串转换为网络字节序
        if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) <= 0)
        {
            throw std::runtime_error("Invalid IP address");
        }

        // 绑定套接字
        if (bind(_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1)
        {
            throw std::runtime_error("Failed to bind socket");
        }
    }

    // 将套接字设置为监听模式，等待客户端连接
    void listenSocket(int backlog)
    {
        if (listen(_fd, backlog) == -1)
        {
            throw std::runtime_error("Failed to listen on socket");
        }
    }   

    // 设置套接字可重用
    void setReusable(bool reusable)
    {
        int optval = reusable ? 1 : 0;
        if (setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1)
        {
            throw std::runtime_error("Failed to set socket options");
        }
    }

    // 设置套接字为非阻塞模式
    void setNonBlocking(bool nonBlocking)
    {
        // 获取当前的文件描述符标志
        int flags = fcntl(_fd, F_GETFL, 0);
        if (flags == -1)
        {
            throw std::runtime_error("Failed to get socket flags");
        }

        // 根据 nonBlocking 参数设置或清除 O_NONBLOCK 标志
        if (nonBlocking)
        {
            flags |= O_NONBLOCK; // 设置为非阻塞
        }
        else
        {
            flags &= ~O_NONBLOCK; // 设置为阻塞
        }

        // 设置新的文件描述符标志
        if (fcntl(_fd, F_SETFL, flags) == -1)
        {
            throw std::runtime_error("Failed to set socket to non-blocking mode");
        }
    }

    // 禁用拷贝构造函数和赋值操作符
    MySocket(const MySocket&) = delete;
    MySocket& operator=(const MySocket&) = delete;

    // 允许移动构造函数和移动赋值操作符
    MySocket(MySocket&& other) noexcept : _fd(other._fd)
    {
        other._fd = -1; // 防止原对象析构时关闭套接字
    }

    MySocket& operator=(MySocket&& other) noexcept
    {
        if (this != &other)
        {
            if (_fd != -1)
            {
                close(_fd);
            }
            _fd = other._fd;
            other._fd = -1; // 防止原对象析构时关闭套接字
        }
        return *this;
    }

public:
    // 返回套接字文件描述符
    int getSocket() const
    {
        return _fd;
    }

    void setSocket(int fd)
    {
        if (_fd != -1)
        {
            close(_fd);
        }

        _fd =  fd;
    }


};



};