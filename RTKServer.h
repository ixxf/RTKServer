#pragma once    
#include "MySocket.h"
#include "MyEpoll.h"
#include <errno.h>
#include <cstring>

namespace RTK{

// 设置套接字可重用
static void setReusable(int fd,bool reusable)
{
    int optval = reusable ? 1 : 0;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1)
    {
        throw std::runtime_error("Failed to set socket options");
    }
}

// 设置套接字为非阻塞模式
static void setNonBlocking(int fd,bool nonBlocking)
{
    // 获取当前的文件描述符标志
    int flags = fcntl(fd, F_GETFL, 0);
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
    if (fcntl(fd, F_SETFL, flags) == -1)
    {
        throw std::runtime_error("Failed to set socket to non-blocking mode");
    }
}

class RTKServer{
private:
    //服务器状态
    bool _server_State;

    //客户端是否连接
    bool _recvIsConnect;
    bool _sendIsConnect;
private:
    //客户端套接字
    int _recvClient;
    int _sendClient;

    //服务器套接字
    int _recvServerFd;
    int _sendServerFd;
    //接收数据服务套接字
    MySocket _recvDateSocket;
    //发送数据服务套接字
    MySocket _sendDateSocket;

    //epoll监听端
    RTKEpoll _rtkEpoll;
private:
    //RTK数据缓冲区
    char buffer[1024];

public:
    //参数1:发送方端口  参数2:接收方端口
    RTKServer( int sendPort=9091,int recvPort=9092)
    :_server_State(false),_recvDateSocket(),_sendDateSocket(),_rtkEpoll()
    {
        //绑定IP和端口
        _recvDateSocket.bindSocket("0.0.0.0",recvPort);
        _sendDateSocket.bindSocket("0.0.0.0",sendPort);

        //设置非阻塞,可重用
        _recvDateSocket.setNonBlocking(true);
        _recvDateSocket.setReusable(true);
        _sendDateSocket.setNonBlocking(true);
        _sendDateSocket.setReusable(true);

        //设置fd
        _recvServerFd=_recvDateSocket.getSocket();
        _sendServerFd=_sendDateSocket.getSocket();

        std::cout<<"_recvServerFd:"<<_recvServerFd<<std::endl;
        std::cout<<"_sendServerFd:"<<_sendServerFd<<std::endl;

        //添加服务器fd到epoll
        _rtkEpoll.add(_recvServerFd,EPOLLIN);
        _rtkEpoll.add(_sendServerFd,EPOLLIN);

        _recvClient=-1;
        _sendClient=-1;

        _recvIsConnect=0;
        _sendIsConnect=0;

        //忽略SIGPIPE后，写操作会返回-1，并设置errno为EPIPE
        std::signal(SIGPIPE, SIG_IGN); // 忽略SIGPIPE
    }

    ~RTKServer(){
        if (_recvClient != -1)
        {
            close(_recvClient);
        }

        if (_sendClient != -1)
        {
            close(_sendClient);
        }
    }


public:
    //开始处理
    void start(){
        _server_State=true;
        //套接字开始监听
        _sendDateSocket.listenSocket(5);
        _recvDateSocket.listenSocket(5); 
        std::cout<<"RTK Server Start."<<std::endl;   

        while(1){
            int result=_rtkEpoll.wait(1000);
            if(result>0){
                handleEpoll(result);
            }
        }
        
    }

public:

private:
    //epoll监听到新处理
    void handleEpoll(int num){
        const auto &events=_rtkEpoll.getEvents();

        for(int i=0;i<num;++i){
            int fd=events[i].data.fd;
            //新的接收方到了
            if(fd==_recvServerFd){
                handleRecvConnect(fd);
            }
            //新的发送方到了
            else if(fd==_sendServerFd){ 
                handleSendConnect(fd); 
            }
            //新数据
            else{
                handleRTKDate(_sendClient);
            }
        }
    }

    //接收服务器fd新连接
    void handleRecvConnect(int fd){
        //旧的还在先关闭
        if(_recvClient!=-1){
            close(_recvClient);
        }
        
        //接收
        sockaddr_in clientAddress{};
        socklen_t clientAddressLength = sizeof(clientAddress);
        _recvClient = accept(fd, (struct sockaddr*)&clientAddress, &clientAddressLength);
        if (_recvClient == -1) {
            std::cerr << "接受连接失败\n";
            _recvIsConnect=false;
            return;
        }
        else{
            std::cout << "接收方新连接\n";
        }

        _recvIsConnect=true;

        //设置非阻塞 可重用
        setReusable(_recvClient,true);
        setNonBlocking(_recvClient,true);
    }

    //发送服务器fd新连接
    void handleSendConnect(int fd){
        //旧的还在先删除
        if(_sendClient!=-1){
            _rtkEpoll.remove(_sendClient);
            close(_sendClient);
        }

        sockaddr_in clientAddress{};
        socklen_t clientAddressLength = sizeof(clientAddress);
        _sendClient = accept(fd, (struct sockaddr*)&clientAddress, &clientAddressLength);
        if (_sendClient == -1) {
            std::cerr << "接受连接失败\n";
            _sendIsConnect=false;
            return;
        }
        else{
            std::cout << "发送方新连接"<<std::endl;
        }


        _sendIsConnect=true;

        //设置非阻塞 可重用
        setReusable(_sendClient,true);
        setNonBlocking(_sendClient,true);

        //添加到检测列表
        _rtkEpoll.add(_sendClient,EPOLLIN|EPOLLET);
    }

    //新数据到达,处理
    void handleRTKDate(int fd){
            //循环接收数据
            // while (true) {
                ssize_t bytes_received = recv(fd, buffer, sizeof(buffer), 0);

                if (bytes_received > 0) {
                    // 处理接收到的数据
                    buffer[bytes_received]=0;
                    //如果接收客户端连接中,发送数据给它
                    if(_recvIsConnect==true){
                        std::cout << "新发送" << std::endl;
                        ssize_t sent = send(_recvClient, buffer, strlen(buffer), 0);
                        if (sent <= 0) {
                            if (sent == 0) {
                                std::cout << "对端关闭连接" << std::endl;
                            } else {
                                if (errno == EWOULDBLOCK || errno == EAGAIN) {
                                    std::cout << "发送缓冲区已满" << std::endl;
                                } else if (errno == ECONNRESET || errno == EPIPE) {
                                    std::cout << "对端异常关闭连接" << std::endl;
                                } else {
                                    std::cout << "发送失败，错误码: " << errno << std::endl;
                                }
                            }
                            std::cout << "关闭recv连接" << std::endl;
                            _recvIsConnect = false;
                            close(_recvClient);
                            _recvClient=-1;
                        }
                    }
                } else if (bytes_received == 0) {
                    // 对端关闭连接,关闭连接
                    std::cout << "Connection closed by peer." << std::endl;
                    _rtkEpoll.remove(fd);
                    close(fd);
                    _sendClient=-1;
                    _sendIsConnect=false;
                    return;
                } else if (bytes_received == -1) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        // 没有数据可读，退出循环
                        return;
                    } else {
                        // 发生错误
                        perror("recv failed");
                        _rtkEpoll.remove(fd);
                        close(fd);
                        _sendClient=-1;
                        _sendIsConnect=false;
                        return;
                    }
                }
        }
    // }

};


};