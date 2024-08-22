
#include "udp_daemonserver.h"

#include "common.h"
#include "whoishere.h"

#include "tcp_protobufclient.h"

UdpDaemonServer::UdpDaemonServer(su::Net::UdpNode& node, const std::string& ip, uint16_t port,
                                 std::vector<Project>& projects, su::Log* plog) :
    su::Net::UdpServer(node, ip, port, plog),
    m_projects(projects),
    m_clientNode(Global::tcpMagicNumber, -1, plog)
{
}

void UdpDaemonServer::doWork()
{
    su::Net::UdpServer::doWork();

    if (m_working)
    {
        if (!m_client->isConnected())
        {
            LOGSPW(getLog(), "Finished all task");
            m_client->finish();
            m_client->thread()->join();

            delete m_client;
            m_client = nullptr;
            m_working = false;
        }
    }

}

bool UdpDaemonServer::onRecvFromNode()
{
    auto udpNode = static_cast<su::Net::UdpNode*>(getNode());

    while (udpNode->countOfPackets())
    {
        if (m_working)
        {
            LOGSPI(getLog(), "Can not received new tasks because already working");
            udpNode->clearPackets();
            return true;
        }

        auto data = udpNode->extractPacket();

        NetPacket::WhoIsHere* packet = (NetPacket::WhoIsHere*)data.data();

        if (packet->crc32 != m_crc32.get(packet, sizeof(NetPacket::WhoIsHere) - sizeof(packet->crc32)))
        {
            continue;
        }

        if (!checkProject(packet->project))
        {
            LOGSPI(getLog(), "Received packet witch unknow project '%s'", packet->project);
            continue;
        }

        std::string hostIp = su::Net::ipToString(packet->masterIP);
        uint16_t hostPort = packet->masterPort;
        m_client = new TcpProtobufClient(m_clientNode, m_projects, getLog());
        m_client->run(16);

        m_client->connect(hostIp, hostPort);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (!m_client->isConnected())
        {
            LOGSPW(getLog(), "Can not connect to server %s:%i", hostIp.c_str(), hostPort);
            m_client->finish();
            while (m_client->status() != su::ThreadClass::Status::Finished)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }

        m_working = true;
    }

    return true;
}

bool UdpDaemonServer::checkProject(const std::string& name)
{
    for (const auto& prj : m_projects)
    {
        if (prj.m_name == name)
        {
            return true;
        }
    }

    return false;
}
