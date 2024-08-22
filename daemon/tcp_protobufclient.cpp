
#include "tcp_protobufclient.h"

#include <filesystem>

#include "threadpool.h"
#include "stringex.h"

#include "master.pb.h"
#include "slave.pb.h"

namespace fs = std::filesystem;

TcpProtobufClient::TcpProtobufClient(TcpProtobufNode& node, std::vector<Project>& projects, su::Log* plog) :
    su::Net::TcpClient(node, plog),
    m_projects(projects)
{
}

void TcpProtobufClient::doWork()
{
    su::Net::TcpClient::doWork();

    std::lock_guard<std::mutex> guard(m_mutex);

    for (size_t ii = 0; ii < m_tasks.size(); ++ii)
    {
        auto task = m_tasks[ii];

        if (task->m_process->isRunning())
        {
            continue;
        }

        Slave::Packet packet;

        auto result = packet.mutable_result();
        int exitCode = 0;
        auto resultCode = task->m_process->getProcessExitCode(&exitCode);

        result->set_id(task->m_packet.id());
        result->set_exit_code(exitCode);
        result->set_process_code(static_cast<int32_t>(resultCode));
        result->set_outputfile(task->m_packet.outputfile());

        if (!packet.IsInitialized())
        {
            LOGSPE(getLog(), "The output protobuf message is not initialized!");

            disconnect();
            ThreadClass::finish();
            break;
        }

        if (!((TcpProtobufNode*)getNode())->send(packet))
        {
            LOGSPE(getLog(), "Can not send the protobuf message to the server.");
            disconnect();
            ThreadClass::finish();
            break;
        }

        if (resultCode != su::Process::ExitCodeResult::Exited || exitCode)
        {
            LOGSPE(getLog(), "Fault to run process '%s %s' on '%s' directory. Status %i. Exit code %i",
                   task->m_packet.application().c_str(),
                   task->m_packet.commandline().c_str(),
                   task->m_packet.workingdir().c_str(),
                   static_cast<int>(resultCode),
                   exitCode);

            if (task->m_packet.abortonerror())
            {
                disconnect();
                ThreadClass::finish();
                break;
            }
        }

        delete task;
        m_tasks.erase(m_tasks.begin() + ii);
        --ii;
    }
}

bool TcpProtobufClient::onConnect()
{
    Slave::Packet packet;

    uint32_t persent = 75;
    uint32_t maxThreads = su::ThreadPool::getMaxThreads();
    float maxTasksCount = persent * maxThreads / 100.0f;

    packet.mutable_info()->set_task_count(uint32_t(maxTasksCount + 0.5f));

    TcpProtobufNode* node = static_cast<TcpProtobufNode*>(getNode());

    node->send(packet);

    return true;
}

bool TcpProtobufClient::onRecvFromNode()
{
    auto protoNode = static_cast<TcpProtobufNode*>(getNode());

    while (protoNode->countOfPackets())
    {
        auto data = protoNode->extractPacket();

        Master::Packet packet;

        packet.ParseFromArray(data.data(), (int)data.size());

        if (!packet.IsInitialized())
        {
            return false;
        }

        if (!runTaskProcess(packet))
        {
            disconnect();
            ThreadClass::close(); // closing the thread
        }

    }

    return true;
}

bool TcpProtobufClient::runTaskProcess(const Master::Packet& packet)
{
//    if (Global::g_abort)
//    {
//        ti->m_result = su::Process::ExitCodeResult::NotStarted;
//        ti->m_exitCode = -1;
//        return;
//    }

    //fs::current_path(workingDir);

    LOGSPI(getLog(), "Run process '%s %s' on '%s' working dir ",
           packet.application().c_str(), packet.commandline().c_str(), packet.workingdir().c_str());

    Task* task = new Task;

    task->m_packet = packet;
    task->m_process = new su::Process::AppObject(packet.application().c_str(),
                                                 packet.commandline().c_str(),
                                                 packet.workingdir().c_str(),
                                                 su::Process::LaunchMode::NoConsole);
    task->m_process->execute();

    std::lock_guard<std::mutex> guard(m_mutex);

    m_tasks.push_back(task);

    return true;
}

