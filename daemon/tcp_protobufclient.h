
#pragma once
#define WIN32_LEAN_AND_MEAN

#include "win/process.h"
#include "net/tcp_client.h"
#include "tcp_protobufnode.h"

#pragma warning(disable:4251)
#include "master.pb.h"
#pragma warning(default:4251)

class Projects;

class TcpProtobufClient : public su::Net::TcpClient
{
    struct Task
    {
        su::Process::AppObject* m_process;
        //Master::Packet m_packet;
        uint32_t m_id = 0;
        std::string m_sourceFile = "";
        std::string m_outputFile = "";
        bool m_abortOnError = false;
    };

public:
    TcpProtobufClient() = delete;
    TcpProtobufClient(TcpProtobufNode& client, Projects& projects, su::Log* plog = nullptr);
    virtual ~TcpProtobufClient() = default;

protected:
    // su::TcpClient
    virtual void doWork() override;
    //virtual void doClose() override;
    virtual bool onConnect() override;
    virtual bool onRecvFromNode() override;

private:
    bool runTaskProcess(const Master::Task& packet);

private:
    std::mutex m_mutex;
    Projects& m_projects;
    std::vector<Task*> m_tasks;
};

