#pragma once

#include "NetworkProtocol.hpp"
#include <string>
#include <iostream>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

namespace CinemaKnot {

/**
 * @brief Zero-dependency POSIX UDP socket for Raspberry Pi and macOS.
 * Enables air-gapped broadcast and reception across a local private IoT subnet.
 */
class UdpSocket {
private:
    int m_sockfd{-1};
    uint16_t m_port{0};

public:
    UdpSocket() = default;

    ~UdpSocket() {
        closeSocket();
    }

    bool open(uint16_t port, bool nonBlocking = true) {
        closeSocket();
        m_port = port;

        m_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (m_sockfd < 0) {
            std::cerr << "[UdpSocket] Failed to create socket\n";
            return false;
        }

        // Enable broadcast
        int broadcastEnable = 1;
        setsockopt(m_sockfd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));

        // Allow address reuse
        int reuse = 1;
        setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        if (bind(m_sockfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            std::cerr << "[UdpSocket] Bind failed on port " << port << "\n";
            closeSocket();
            return false;
        }

        if (nonBlocking) {
            int flags = fcntl(m_sockfd, F_GETFL, 0);
            fcntl(m_sockfd, F_SETFL, flags | O_NONBLOCK);
        }

        return true;
    }

    void closeSocket() {
        if (m_sockfd >= 0) {
            close(m_sockfd);
            m_sockfd = -1;
        }
    }

    bool sendPacket(const KnotPacket& pkt, const std::string& targetIp, uint16_t targetPort) {
        if (m_sockfd < 0) return false;

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(targetPort);
        inet_pton(AF_INET, targetIp.c_str(), &addr.sin_addr);

        ssize_t sent = sendto(m_sockfd, &pkt, sizeof(KnotPacket), 0,
                              reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        return sent == sizeof(KnotPacket);
    }

    bool broadcastPacket(const KnotPacket& pkt, uint16_t targetPort) {
        return sendPacket(pkt, "255.255.255.255", targetPort);
    }

    bool receivePacket(KnotPacket& outPkt, std::string* outSenderIp = nullptr) {
        if (m_sockfd < 0) return false;

        sockaddr_in senderAddr{};
        socklen_t senderLen = sizeof(senderAddr);

        ssize_t recvd = recvfrom(m_sockfd, &outPkt, sizeof(KnotPacket), 0,
                                 reinterpret_cast<sockaddr*>(&senderAddr), &senderLen);

        if (recvd == sizeof(KnotPacket) && outPkt.isValid()) {
            if (outSenderIp) {
                char ipStr[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &senderAddr.sin_addr, ipStr, sizeof(ipStr));
                *outSenderIp = ipStr;
            }
            return true;
        }
        return false;
    }
};

} // namespace CinemaKnot
