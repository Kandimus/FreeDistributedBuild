
#pragma once

#include "crc32.h"
#include "net/udp_server.h"
#include "net/udp_node.h"
#include "tcp_protobufnode.h"

#include "daemon.h"

class TcpProtobufClient;

class UdpDaemonServer : public su::Net::UdpServer
{
public:
    UdpDaemonServer() = delete;
    UdpDaemonServer(su::Net::UdpNode& node, const std::string& ip, uint16_t port,
                    std::vector<Project>& projects, su::Log* plog = nullptr);
    virtual ~UdpDaemonServer() = default;

protected:
    // su::TcpClient
    virtual void doWork() override;

    // su::Net::UdpServer
    virtual bool onRecvFromNode() override;

private:
    bool checkProject(const std::string& name);

private:
    su::Crc32 m_crc32;
    std::vector<Project>& m_projects;
    std::atomic_bool m_working = false;
    TcpProtobufNode m_clientNode;
    TcpProtobufClient* m_client = nullptr;
};

