
#pragma once

#include "crc32.h"
#include "net/udp_server.h"
#include "net/udp_node.h"
#include "tcp_protobufnode.h"

class TcpProtobufClient;
class Projects;

class UdpDaemonServer : public su::Net::UdpServer
{
public:
    UdpDaemonServer() = delete;
    UdpDaemonServer(su::Net::UdpNode& node, const std::string& ip, uint16_t port,
                    Projects& projects, su::Log* plog = nullptr);
    virtual ~UdpDaemonServer();

protected:
    // su::TcpClient
    virtual void doWork() override;

    // su::Net::UdpServer
    virtual bool onRecvFromNode() override;

private:
    enum Status
    {
        Idle,
        Connectig,
        Working,
    };

    std::mutex m_mutex;
    su::Crc32 m_crc32;
    Projects& m_projects;
    Status m_status = Idle;
    TcpProtobufNode m_clientNode;
    std::unique_ptr<TcpProtobufClient> m_client;
    bool m_isConnectedPulse = false;
};

