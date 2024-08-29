
#include "fileex.h"

#include <chrono>

#include "tcp_protobufserver.h"
#include "tcp_protobufnode.h"
#include "global_constants.h"


TcpProtobufServer::TcpProtobufServer(std::vector<TaskInfo>& taskInfo, const std::string& ip, uint16_t port,
                                     uint32_t maxClients, su::Log* plog) :
    su::Net::TcpServer(ip, port, maxClients, plog),
    m_tasks(taskInfo)
{
    m_immediatelyCloseClients = true;
}

void TcpProtobufServer::closeAllClients()
{
    Master::Packet packet;

    packet.mutable_system()->set_close(true);

    std::lock_guard<std::mutex> guard(getMutex());

    for (auto node: m_clients)
    {
        static_cast<TcpProtobufNode*>(node)->send(packet);
    }
}

void TcpProtobufServer::doWork()
{
    su::Net::TcpServer::doWork();

    for (auto node: m_clients)
    {
        sendTasksToSlave(static_cast<TcpProtobufNode*>(node));
    }

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

    while (protoNode->countRecvPackets())
    {
        auto data = protoNode->extractRecvPacket();

        Slave::Packet packet;

        packet.ParseFromArray(data.raw.data(), (int)data.raw.size());

        if (!packet.IsInitialized())
        {
            LOGSPE(getLog(), "Can not parse the slave protobuf packet");
            return false;
        }

        if (packet.has_info())
        {
            protoNode->m_freeCores = packet.info().task_count();
            LOGSPN(getLog(), "The client %s sent %i free cores",
                   protoNode->fullId().c_str(), protoNode->m_freeCores);

            sendTasksToSlave(protoNode);
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

bool TcpProtobufServer::sendTasksToSlave(TcpProtobufNode* node)
{
    if (node->m_freeCores <= 0)
    {
        return true;
    }

    for (auto& task: m_tasks)
    {
        std::lock_guard<std::mutex> guard(task.m_mutex);

        if (task.m_node || task.m_result != su::Process::ExitCodeResult::NoInit)
        {
            continue;
        }

        Master::Packet packet;

        packet.mutable_task()->CopyFrom(task.m_message);

        if (su::fs::load(task.m_vars.SourceFile, *packet.mutable_task()->mutable_inputdata()) != su::fs::OK)
        {
            LOGSPE(getLog(), "Can not load '%s' source file", task.m_vars.SourceFile.c_str());
        }

        LOGSPI(getLog(), "Loaded '%s' source file, size %u",
               task.m_vars.SourceFile.c_str(), packet.mutable_task()->inputdata().size());

        if (!packet.mutable_task()->IsInitialized())
        {
            LOGSPE(getLog(), "Initialization of task %u failed", packet.mutable_task()->id());
            return false;
        }

        task.m_node = node;

        LOGSPN(getLog(), "Send to the client %s task %i", node->fullId().c_str(), packet.mutable_task()->id());
        node->send(packet);

        --node->m_freeCores;
        return true;
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
        task.m_doneIp = task.m_node->fullId();
        task.m_node = nullptr;

        if (!task.m_exitCode && task.m_result == su::Process::ExitCodeResult::Exited && packet.has_outputdata())
        {
            task.m_exitCode = su::fs::save(task.m_vars.OutputFile, packet.outputdata());
            if (task.m_exitCode != su::fs::OK)
            {
                LOGSPE(getLog(), "Can not save output file to '%s', size %u",
                       task.m_vars.OutputFile.c_str(),
                        packet.outputfile().size());
            }
        }

        LOGSPI(getLog(), "Received result packet with id %i from client %s", packet.id(), node->fullId().c_str());

        return true;
   }

    LOGSPE(getLog(), "Received wrong packet with id %i from client %s", packet.id(), node->fullId().c_str());

   return false;
}
