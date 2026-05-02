#include "http.h"
#include <ws2tcpip.h>
#include <iostream>
#include <stdexcept>

#include <openssl/err.h>

#pragma comment(lib, "Ws2_32.lib")

HTTPClient::HTTPClient() : ctx(nullptr) {
    initWinsock();
    initSSL();
}

HTTPClient::~HTTPClient() {
    cleanupSSL();
    cleanupWinsock();
}

// ---------- Init / Cleanup ----------

void HTTPClient::initWinsock() {
    WSADATA wsaData{};
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        throw std::runtime_error("WSAStartup failed");
    }
}

void HTTPClient::cleanupWinsock() {
    WSACleanup();
}

void HTTPClient::initSSL() {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();

    ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) {
        throw std::runtime_error("SSL_CTX_new failed");
    }

    // 🔐 Basic verification (recommended)
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);
    // SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, nullptr);
    // NOTE: On Windows/MSYS2 you may need to set a CA bundle path:
    SSL_CTX_load_verify_locations(ctx, "C:\\msys64\\mingw64\\etc\\ssl\\certs\\ca-bundle.crt", nullptr);
    if (SSL_CTX_set_default_verify_paths(ctx) != 1)
    {
        ERR_print_errors_fp(stderr);
        throw std::runtime_error("Failed to load system certs");
    }
}

void HTTPClient::cleanupSSL() {
    if (ctx) {
        SSL_CTX_free(ctx);
        ctx = nullptr;
    }
    EVP_cleanup();
}

// ---------- URL ----------

URL HTTPClient::parseURL(const std::string& url) {
    URL u{};
    std::string temp = url;

    if (temp.rfind("http://", 0) == 0) {
        temp = temp.substr(7);
        u.port = 80;
        u.is_https = false;
    } else if (temp.rfind("https://", 0) == 0) {
        temp = temp.substr(8);
        u.port = 443;
        u.is_https = true;
    } else {
        throw std::runtime_error("Only http/https supported");
    }

    size_t pos = temp.find('/');
    if (pos == std::string::npos) {
        u.host = temp;
        u.path = "/";
    } else {
        u.host = temp.substr(0, pos);
        u.path = temp.substr(pos);
    }
    return u;
}

// ---------- TCP connect (with retry over res list) ----------

SOCKET HTTPClient::connectToHost(const std::string& host, int port) {
    addrinfo hints{}, *res = nullptr;

    hints.ai_family = AF_UNSPEC;      // allow IPv4/IPv6
    hints.ai_socktype = SOCK_STREAM;  // TCP

    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res) != 0) {
        return INVALID_SOCKET;
    }

    SOCKET sock = INVALID_SOCKET;

    for (addrinfo* p = res; p; p = p->ai_next) {
        sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sock == INVALID_SOCKET) continue;

        if (connect(sock, p->ai_addr, (int)p->ai_addrlen) == 0) {
            break; // success
        }
        closesocket(sock);
        sock = INVALID_SOCKET;
    }

    freeaddrinfo(res);
    return sock;
}

// ---------- Unified I/O ----------

int HTTPClient::writeAll(SOCKET sock, SSL* ssl, const char* data, int len) {
    int sent = 0;
    while (sent < len) {
        int n = ssl ? SSL_write(ssl, data + sent, len - sent)
                    : send(sock, data + sent, len - sent, 0);
        if (n <= 0) return n;
        sent += n;
    }
    return sent;
}

int HTTPClient::readSome(SOCKET sock, SSL* ssl, char* buf, int cap) {
    return ssl ? SSL_read(ssl, buf, cap)
               : recv(sock, buf, cap, 0);
}

// ---------- GET ----------

std::string HTTPClient::GET(const std::string& urlStr) {
    URL url = parseURL(urlStr);

    SOCKET sock = connectToHost(url.host, url.port);
    if (sock == INVALID_SOCKET) {
        throw std::runtime_error("TCP connect failed");
    }

    SSL* ssl = nullptr;

    if (url.is_https) {
        // Create SSL object and bind to socket
        ssl = SSL_new(ctx);
        if (!ssl) {
            closesocket(sock);
            throw std::runtime_error("SSL_new failed");
        }

        // SNI (very important)
        SSL_set_tlsext_host_name(ssl, url.host.c_str());
        SSL_set_fd(ssl, (int)sock);

        // Handshake
        if (SSL_connect(ssl) <= 0) {
            ERR_print_errors_fp(stderr);
            SSL_free(ssl);
            closesocket(sock);
            throw std::runtime_error("TLS handshake failed");
        }
    }

    // Build request
    std::string req =
        "GET " + url.path + " HTTP/1.1\r\n"
        "Host: " + url.host + "\r\n"
        "Connection: close\r\n\r\n";

    if (writeAll(sock, ssl, req.c_str(), (int)req.size()) <= 0) {
        if (ssl) SSL_free(ssl);
        closesocket(sock);
        throw std::runtime_error("send/SSL_write failed");
    }

    // Read response
    std::string response;
    char buf[4096];
    int n;

    while ((n = readSome(sock, ssl, buf, sizeof(buf))) > 0) {
        response.append(buf, n);
    }

    if (ssl) SSL_free(ssl);
    closesocket(sock);

    return response;
}