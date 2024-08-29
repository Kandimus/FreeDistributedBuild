
#include "udp_daemonserver.h"

#include "project.h"
#include "global_constants.h"
#include "whoishere.h"

#include "tcp_protobufclient.h"

UdpDaemonServer::UdpDaemonServer(su::Net::UdpNode& node, const std::string& ip, uint16_t port,
                                 Projects& projects, su::Log* plog) :
    su::Net::UdpServer(node, ip, port, plog),
    m_projects(projects),
    m_clientNode(Global::tcpMagicNumber, -1, plog)
{
}

UdpDaemonServer::~UdpDaemonServer() = default;

void UdpDaemonServer::doWork()
{
    su::Net::UdpServer::doWork();

    if (m_status == Idle)
    {
        return;
    }

    if (!m_client)
    {
        m_status = Idle;
        return;
    }

    if (m_client->isConnecting())
    {
        return;
    }

    if (m_client->isConnected())
    {
        if (m_status == Connectig)
        {
            LOGSPN(getLog(), "Connect to %s is successful", m_client->destination().c_str());
            m_status = Working;
        }
        return;
    }

    if (!m_client->isConnected())
    {
        if (m_status == Connectig)
        {
            LOGSPE(getLog(), "Can not connect to server %s", m_client->destination().c_str());
        }
        if (m_status == Working)
        {
            LOGSPW(getLog(), "Finished all tasks");
        }

        m_client->close();
        m_client = nullptr; // delete std::unique_ptr
        LOGSPN(getLog(), "The Tcp Client has been deleted");
        m_status = Idle;
    }
}

bool UdpDaemonServer::onRecvFromNode()
{
    auto udpNode = static_cast<su::Net::UdpNode*>(getNode());

    while (udpNode->countOfPackets())
    {
        if (m_status != Status::Idle)
        {
            LOGSPI(getLog(), "Can not received new tasks because already working");
            udpNode->clearPackets();
            return true;
        }

        auto data = udpNode->extractPacket();

        NetPacket::WhoIsHere* packet = (NetPacket::WhoIsHere*)data.raw.data();

        if (packet->crc32 != m_crc32.get(packet, sizeof(NetPacket::WhoIsHere) - sizeof(packet->crc32)))
        {
            continue;
        }

        if (!m_projects.getProject(packet->project))
        {
            LOGSPI(getLog(), "Received packet witch unknow project '%s'", packet->project);
            continue;
        }

        LOGSPI(getLog(), "Received packet from '%s'", su::Net::addrToString(data.addr).c_str());

        if (data.addr.sin_addr.S_un.S_addr != packet->masterIP)
        {
            LOGSPI(getLog(), "Ignore job because destination ip is not equal to source ip");
            continue;
        }

        std::string hostIp = su::Net::ipToString(packet->masterIP);
        uint16_t hostPort = packet->masterPort;

        LOGSPI(getLog(), "Job request accepted from %s:%i, project '%s'",
               hostIp.c_str(), hostPort, packet->project);

        if (m_projects.getFreeCore() == 0)
        {
            LOGSPW(getLog(), "No free core. Ignore job request from %s:%i, project '%s'",
                   hostIp.c_str(), hostPort, packet->project);
            continue;
        }

        m_client = std::make_unique<TcpProtobufClient>(m_clientNode, m_projects, getLog());
        m_client->run(0);
        m_client->connect(hostIp, hostPort);
        m_status = Status::Connectig;
        udpNode->clearPackets();
    }

    return true;
}
