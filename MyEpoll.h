#pragma once
#include <sys/epoll.h>
#include <unistd.h>
#include <stdexcept>
#include <vector>

namespace RTK {

class RTKEpoll {
private:
    int _epoll_fd; // RTKEpoll 文件描述符
    std::vector<struct epoll_event> _events; // 用于存储事件的数组

public:
    // 构造函数，创建 RTKEpoll 实例
    RTKEpoll(int max_events = 16) {
        _epoll_fd = epoll_create1(0);
        if (_epoll_fd == -1) {
            throw std::runtime_error("Failed to create RTKEpoll instance");
        }
        _events.resize(max_events); // 初始化事件数组
    }

    // 析构函数，关闭 RTKEpoll 文件描述符
    ~RTKEpoll() {
        if (_epoll_fd != -1) {
            close(_epoll_fd);
        }
    }

    // 添加或修改文件描述符到 RTKEpoll 实例
    void add(int fd, uint32_t events, void* ptr = nullptr) {
        struct epoll_event ev;
        ev.events = events;
        ev.data.fd=fd;
        // ev.data.ptr = ptr; // 可以传递自定义数据

        if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, fd, &ev) == -1) {
            throw std::runtime_error("Failed to add file descriptor to RTKEpoll");
        }
    }

    void modify(int fd, uint32_t events, void* ptr = nullptr) {
        struct epoll_event ev;
        ev.events = events;
        ev.data.fd=fd;
        // ev.data.ptr = ptr; // 可以传递自定义数据

        if (epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, fd, &ev) == -1) {
            throw std::runtime_error("Failed to modify file descriptor in RTKEpoll");
        }
    }

    // 从 RTKEpoll 实例中删除文件描述符
    void remove(int fd) {
        if (epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, fd, nullptr) == -1) {
            throw std::runtime_error("Failed to remove file descriptor from RTKEpoll");
        }
    }

    // 等待事件发生，返回事件数量
    int wait(int timeout = -1) {
        int event_count = epoll_wait(_epoll_fd, _events.data(), _events.size(), timeout);
        if (event_count == -1) {
            throw std::runtime_error("Failed to wait for events");
        }
        return event_count;
    }

    // 获取事件数组
    const std::vector<struct epoll_event>& getEvents() const {
        return _events;
    }

    // 禁用拷贝构造函数和赋值操作符
    RTKEpoll(const RTKEpoll&) = delete;
    RTKEpoll& operator=(const RTKEpoll&) = delete;

    // 允许移动构造函数和移动赋值操作符
    RTKEpoll(RTKEpoll&& other) noexcept : _epoll_fd(other._epoll_fd), _events(std::move(other._events)) {
        other._epoll_fd = -1; // 防止原对象析构时关闭 RTKEpoll 文件描述符
    }

    RTKEpoll& operator=(RTKEpoll&& other) noexcept {
        if (this != &other) {
            if (_epoll_fd != -1) {
                close(_epoll_fd);
            }
            _epoll_fd = other._epoll_fd;
            _events = std::move(other._events);
            other._epoll_fd = -1; // 防止原对象析构时关闭 RTKEpoll 文件描述符
        }
        return *this;
    }
};

} // namespace RTK