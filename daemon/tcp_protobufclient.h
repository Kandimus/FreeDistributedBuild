
#pragma once
#define WIN32_LEAN_AND_MEAN

#include "win/process.h"
#include "net/tcp_client.h"
#include "tcp_protobufnode.h"

#include "master.pb.h"

#include "daemon.h"

class TcpProtobufClient : public su::Net::TcpClient
{
    struct Task
    {
        su::Process::AppObject* m_process;
        Master::Packet m_packet;
    };

public:
    TcpProtobufClient() = delete;
    TcpProtobufClient(TcpProtobufNode& client, std::vector<Project>& projects, su::Log* plog = nullptr);
    virtual ~TcpProtobufClient() = default;

protected:
    // su::TcpClient
    virtual void doWork() override;
    //virtual void doClose() override;
    virtual bool onConnect() override;
    virtual bool onRecvFromNode() override;

private:
    bool runTaskProcess(const Master::Packet& packet);

private:
    std::mutex m_mutex;
    std::vector<Project>& m_projects;
    std::vector<Task*> m_tasks;
};

