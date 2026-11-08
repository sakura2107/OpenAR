#include <winsock2.h>
#include "net/Http.h"
#include <thread>
#include <stdexcept>
#pragma comment(lib, "ws2_32.lib")
void http_contract_tests() {
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data))
        throw std::runtime_error("Winsock startup failed");
    struct Wsa {
        ~Wsa() { WSACleanup(); }
    } wsa;
    auto run = [](const std::string &response, size_t cap, bool expected_success) {
        SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (server == INVALID_SOCKET)
            throw std::runtime_error("Socket failed");
        struct Socket {
            SOCKET s;
            ~Socket() { closesocket(s); }
        } socket_guard{server};
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = 0;
        if (bind(server, reinterpret_cast<sockaddr *>(&address), sizeof(address)) || listen(server, 1))
            throw std::runtime_error("Bind/listen failed");
        int length = sizeof(address);
        getsockname(server, reinterpret_cast<sockaddr *>(&address), &length);
        std::jthread worker([&] {
            SOCKET client = accept(server, nullptr, nullptr);
            if (client == INVALID_SOCKET)
                return;
            char request[4096];
            recv(client, request, sizeof(request), 0);
            send(client, response.data(), static_cast<int>(response.size()), 0);
            shutdown(client, SD_BOTH);
            closesocket(client);
        });
        ar::net::HttpOptions options;
        options.max_response_bytes = cap;
        options.timeout_ms = 1000;
        auto result = ar::net::request_http(
            "GET", "http://127.0.0.1:" + std::to_string(ntohs(address.sin_port)) + "/", {}, options);
        if (bool(result) != expected_success)
            throw std::runtime_error("HTTP completeness/limit contract failed");
        if (result && (result.value().status != 200 || result.value().body != "abc"))
            throw std::runtime_error("HTTP response mismatch");
    };
    run("HTTP/1.1 200 OK\r\nContent-Length: 3\r\nConnection: close\r\n\r\nabc", 32, true);
    run("HTTP/1.1 200 OK\r\nContent-Length: 10\r\nConnection: close\r\n\r\nabc", 32, false);
    run("HTTP/1.1 200 OK\r\nContent-Length: 3\r\nConnection: close\r\n\r\nabc", 2, false);
}
