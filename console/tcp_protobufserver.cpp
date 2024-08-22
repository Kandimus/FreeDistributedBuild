
#include "tcp_protobufserver.h"
#include "tcp_protobufnode.h"
#include "common.h"

TcpProtobufServer::TcpProtobufServer(std::vector<TaskInfo>& taskInfo, const std::string& ip, uint16_t port,
                                     uint32_t maxClients, su::Log* plog) :
    su::Net::TcpServer(ip, port, maxClients, plog),
    m_tasks(taskInfo)
{
}

void TcpProtobufServer::onClientDisconnected(su::Net::Node* node)
{
    auto protoNode = static_cast<TcpProtobufNode*>(node);

    for (auto& task: m_tasks)
    {
        std::lock_guard<std::mutex> guard(task.m_mutex);

        if (task.m_node != protoNode)
        {
            continue;
        }

        task.m_node = nullptr;

        LOGSPW(getLog(), "Do free the %i task because the client %s was disconnect",
               task.m_message.id(), node->fullId().c_str());
    }
}

bool TcpProtobufServer::onRecvFromNode(su::Net::Node* node)
{
    auto protoNode = static_cast<TcpProtobufNode*>(node);

    while (protoNode->countOfPackets())
    {
        auto data = protoNode->extractPacket();

        Slave::Packet packet;

        packet.ParseFromArray(data.data(), (int)data.size());

        if (!packet.IsInitialized())
        {
            LOGSPE(getLog(), "Can not parse the slave protobuf packet");
            return false;
        }

        if (packet.has_info())
        {
            sendTasksToSlave(protoNode, packet.info());
        }

        if (packet.has_result())
        {
            applyResultFromSlave(protoNode, packet.result());
        }
    }

    return true;
}

su::Net::Node* TcpProtobufServer::newClient(SOCKET socket, const sockaddr_in& addr)
{
    return new TcpProtobufNode(Global::tcpMagicNumber, socket, addr, getNextClientId(), getLog());
}

bool TcpProtobufServer::sendTasksToSlave(TcpProtobufNode* node, const Slave::Info& packet)
{
    uint32_t count = packet.task_count();

    LOGSPN(getLog(), "The client %s sent %i free cores", node->fullId().c_str(), count);

    for (auto& task: m_tasks)
    {
        std::lock_guard<std::mutex> guard(task.m_mutex);

        if (task.m_node || task.m_result != su::Process::ExitCodeResult::NoInit)
        {
            continue;
        }

        task.m_node = node;

        LOGSPN(getLog(), "Send to the client %s task %i", node->fullId().c_str(), task.m_message.id());
        node->send(task.m_message);

        --count;
        if (!count)
        {
            break;
        }
    }

    return true;
}

bool TcpProtobufServer::applyResultFromSlave(TcpProtobufNode* node, const Slave::Result& packet)
{
    for (auto& task: m_tasks)
    {
        std::lock_guard<std::mutex> guard(task.m_mutex);

        if (task.m_node != node || packet.id() != task.m_message.id())
        {
            continue;
        }

        task.m_exitCode = packet.exit_code();
        task.m_result = static_cast<su::Process::ExitCodeResult>(packet.process_code());
        task.m_node = nullptr;

        LOGSPI(getLog(), "Received result packet with id %i from client %s", packet.id(), node->fullId().c_str());

        return true;
   }

    LOGSPE(getLog(), "Received wrong packet with id %i from client %s", packet.id(), node->fullId().c_str());

   return false;
}
