#include <winsock2.h>
#include "port_checker.h"
#include <iphlpapi.h>
#include <ws2tcpip.h>
#include <sstream>
#include <cstring>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

bool PortChecker::init() {
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
}

void PortChecker::cleanup() {
    WSACleanup();
}

std::string PortChecker::ipToString(DWORD ip) {
    struct in_addr addr;
    addr.s_addr = ip;
    return std::string(inet_ntoa(addr));
}

std::string PortChecker::stateToString(DWORD state) {
    switch (state) {
        case MIB_TCP_STATE_CLOSED:     return "CLOSED";
        case MIB_TCP_STATE_LISTEN:     return "LISTENING";
        case MIB_TCP_STATE_SYN_SENT:   return "SYN_SENT";
        case MIB_TCP_STATE_SYN_RCVD:   return "SYN_RCVD";
        case MIB_TCP_STATE_ESTAB:      return "ESTABLISHED";
        case MIB_TCP_STATE_FIN_WAIT1:  return "FIN_WAIT1";
        case MIB_TCP_STATE_FIN_WAIT2:  return "FIN_WAIT2";
        case MIB_TCP_STATE_CLOSE_WAIT: return "CLOSE_WAIT";
        case MIB_TCP_STATE_CLOSING:    return "CLOSING";
        case MIB_TCP_STATE_LAST_ACK:   return "LAST_ACK";
        case MIB_TCP_STATE_TIME_WAIT:  return "TIME_WAIT";
        case MIB_TCP_STATE_DELETE_TCB: return "DELETE_TCB";
        default: return "UNKNOWN";
    }
}

std::vector<PortInfo> PortChecker::getAllListeningPorts(const std::string& protocol) {
    std::vector<PortInfo> result;

    // 获取 TCP 端口（带 PID 的扩展表）
    if (protocol == "ALL" || protocol == "TCP") {
        DWORD size = 0;
        GetExtendedTcpTable(nullptr, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_LISTENER, 0);
        
        if (size > 0) {
            std::vector<BYTE> buffer(size);
            MIB_TCPTABLE_OWNER_PID* tcpTable = reinterpret_cast<MIB_TCPTABLE_OWNER_PID*>(buffer.data());
            
            if (GetExtendedTcpTable(tcpTable, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_LISTENER, 0) == NO_ERROR) {
                for (DWORD i = 0; i < tcpTable->dwNumEntries; i++) {
                    PortInfo info;
                    info.port = ntohs(static_cast<uint16_t>(tcpTable->table[i].dwLocalPort));
                    info.pid = tcpTable->table[i].dwOwningPid;
                    info.protocol = "TCP";
                    info.state = "LISTENING";
                    info.localAddr = ipToString(tcpTable->table[i].dwLocalAddr);
                    info.remoteAddr = "0.0.0.0";
                    info.remotePort = 0;
                    result.push_back(info);
                }
            }
        }
    }

    // 获取 UDP 端口
    if (protocol == "ALL" || protocol == "UDP") {
        DWORD size = 0;
        GetExtendedUdpTable(nullptr, &size, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0);
        
        if (size > 0) {
            std::vector<BYTE> buffer(size);
            MIB_UDPTABLE_OWNER_PID* udpTable = reinterpret_cast<MIB_UDPTABLE_OWNER_PID*>(buffer.data());
            
            if (GetExtendedUdpTable(udpTable, &size, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0) == NO_ERROR) {
                for (DWORD i = 0; i < udpTable->dwNumEntries; i++) {
                    PortInfo info;
                    info.port = ntohs(static_cast<uint16_t>(udpTable->table[i].dwLocalPort));
                    info.pid = udpTable->table[i].dwOwningPid;
                    info.protocol = "UDP";
                    info.state = "LISTENING";
                    info.localAddr = ipToString(udpTable->table[i].dwLocalAddr);
                    info.remoteAddr = "*";
                    info.remotePort = 0;
                    result.push_back(info);
                }
            }
        }
    }

    return result;
}

PortInfo PortChecker::checkPort(uint16_t port, const std::string& protocol) {
    PortInfo result;
    result.port = port;
    result.pid = 0;
    result.protocol = protocol;
    result.state = "NOT_LISTENING";

    auto allPorts = getAllListeningPorts(protocol);
    for (const auto& p : allPorts) {
        if (p.port == port) {
            result = p;
            break;
        }
    }
    return result;
}

std::vector<PortInfo> PortChecker::getPortConnections(uint16_t port) {
    std::vector<PortInfo> result;

    // 获取所有 TCP 连接（包括所有状态）
    DWORD size = 0;
    GetExtendedTcpTable(nullptr, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
    
    if (size > 0) {
        std::vector<BYTE> buffer(size);
        MIB_TCPTABLE_OWNER_PID* tcpTable = reinterpret_cast<MIB_TCPTABLE_OWNER_PID*>(buffer.data());
        
        if (GetExtendedTcpTable(tcpTable, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR) {
            for (DWORD i = 0; i < tcpTable->dwNumEntries; i++) {
                uint16_t localPort = ntohs(static_cast<uint16_t>(tcpTable->table[i].dwLocalPort));
                uint16_t remotePort = ntohs(static_cast<uint16_t>(tcpTable->table[i].dwRemotePort));
                
                if (localPort == port || remotePort == port) {
                    PortInfo info;
                    info.port = localPort;
                    info.pid = tcpTable->table[i].dwOwningPid;
                    info.protocol = "TCP";
                    info.state = stateToString(tcpTable->table[i].dwState);
                    info.localAddr = ipToString(tcpTable->table[i].dwLocalAddr);
                    info.remoteAddr = ipToString(tcpTable->table[i].dwRemoteAddr);
                    info.remotePort = remotePort;
                    result.push_back(info);
                }
            }
        }
    }

    return result;
}

std::vector<PortInfo> PortChecker::scanPorts(uint16_t startPort, uint16_t endPort) {
    std::vector<PortInfo> result;
    auto allPorts = getAllListeningPorts("ALL");
    
    for (const auto& p : allPorts) {
        if (p.port >= startPort && p.port <= endPort) {
            result.push_back(p);
        }
    }
    return result;
}
