#pragma once

#include <vector>

#include "net/tcp_server.h"

#include "console.h"

class TcpProtobufServer : public su::Net::TcpServer
{
public:
    TcpProtobufServer(std::vector<TaskInfo>& taskInfo, const std::string& ip, uint16_t port,
                      uint32_t maxClients, su::Log* plog);
    virtual ~TcpProtobufServer() = default;

    void closeAllClients();

protected:
    // ThreadClass
    virtual void doWork() override;
    //virtual void doFinished() override;

    // TcpServer
    //virtual bool send(TcpNode* target, const void* packet, size_t size);
    virtual void onClientDisconnected(su::Net::Node* node) override;
    virtual bool onRecvFromNode(su::Net::Node* node) override;
    virtual su::Net::Node* newClient(SOCKET socket, const sockaddr_in& addr) override;

private:
    bool sendTasksToSlave(TcpProtobufNode* node);
    bool applyResultFromSlave(TcpProtobufNode* node, const Slave::Result& packet);

private:
    std::vector<TaskInfo>& m_tasks;
};
