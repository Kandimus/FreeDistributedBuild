
#include "tcp_protobufclient.h"

#include <filesystem>

#include "fileex.h"
#include "stringex.h"

#include "project.h"
#pragma warning(disable:4251)
#include "master.pb.h"
#include "slave.pb.h"
#pragma warning(default:4251)

namespace fs = std::filesystem;

TcpProtobufClient::TcpProtobufClient(TcpProtobufNode& node, Projects& projects, su::Log* plog) :
    su::Net::TcpClient(node, plog),
    m_projects(projects)
{
}

void TcpProtobufClient::doWork()
{
    su::Net::TcpClient::doWork();

    for (size_t ii = 0; ii < m_tasks.size(); ++ii)
    {
        auto task = m_tasks[ii];

        if (task->m_process->isRunning())
        {
            TcpClient::restartKeepAliveTimer();
            continue;
        }

        Slave::Packet packet;

        auto info = packet.mutable_info();
        auto result = packet.mutable_result();
        int exitCode = 0;
        auto resultCode = task->m_process->getProcessExitCode(&exitCode);

        info->set_task_count(m_projects.getFreeCore() - (uint32_t)m_tasks.size());

        result->set_id(task->m_id);
        result->set_exit_code(exitCode);
        result->set_process_code(static_cast<int32_t>(resultCode));
        result->set_outputfile(task->m_outputFile);

        if (su::fs::load(task->m_outputFile, *result->mutable_outputdata()) != su::fs::OK)
        {
            LOGSPI(getLog(), "Can not  transfer output file.", task->m_id);
        }

        LOGSPI(getLog(), "The task %u is finished!", task->m_id);

        if (!packet.IsInitialized())
        {
            LOGSPE(getLog(), "The output protobuf message is not initialized!");

            disconnect();
            break;
        }

        if (!((TcpProtobufNode*)getNode())->send(packet))
        {
            LOGSPE(getLog(), "Can not send the protobuf message to the server.");
            disconnect();
            break;
        }

        if (resultCode != su::Process::ExitCodeResult::Exited || exitCode)
        {
            LOGSPE(getLog(), "Task %u: Fault to run process '%s %s' on '%s' directory. Status %i. Exit code %i",
                   task->m_id,
                   static_cast<int>(resultCode),
                   exitCode);

            if (task->m_abortOnError)
            {
                LOGSPW(getLog(), "Disconnecting because the 'AbortOnError' flag is true");
                disconnect();
                break;
            }
        }

        // Clear tmp files
        su::fs::deleteFile(task->m_sourceFile);
        su::fs::deleteFile(task->m_outputFile);

        delete task;
        m_tasks.erase(m_tasks.begin() + ii);
        --ii;
    }
}

bool TcpProtobufClient::onConnect()
{
    Slave::Packet packet;

    packet.mutable_info()->set_task_count(m_projects.getFreeCore());

    TcpProtobufNode* node = static_cast<TcpProtobufNode*>(getNode());

    node->send(packet);

    return true;
}

bool TcpProtobufClient::onRecvFromNode()
{
    auto protoNode = static_cast<TcpProtobufNode*>(getNode());

    while (protoNode->countRecvPackets())
    {
        auto data = protoNode->extractRecvPacket();

        Master::Packet packet;

        packet.ParseFromArray(data.raw.data(), (int)data.raw.size());

        if (!packet.IsInitialized())
        {
            LOGSPE(getLog(), "Received task is not initialization. Ignore");
            return false;
        }

        if (packet.has_task())
        {
            if (!runTaskProcess(packet.task()))
            {
                protoNode->clearRecvPackets();
                return false;
            }
        }

        if (packet.has_system())
        {
            if (packet.system().has_close() && packet.system().close())
            {
                LOGSPI(getLog(), "Server sent the disconnect command");
                return false;
            }
        }
    }

    return true;
}

bool TcpProtobufClient::runTaskProcess(const Master::Task& packet)
{
    std::string prjname = packet.project();
    std::string workingdir = packet.workingdir();
    std::string application = packet.application();
    std::string commandline = packet.commandline();
    std::string outputfile = packet.outputfile();
    std::string sourcefile = packet.sourcefile();

    LOGSPI(getLog(), "Received task %u: prj='%s' wdir='%s' app='%s' params='%s' out='%s'",
           packet.id(),
           prjname.c_str(),
           workingdir.c_str(),
           application.c_str(),
           commandline.c_str(),
           outputfile.c_str());

    auto prj = m_projects.getProject(prjname);

    if (!prj)
    {
        LOGSPE(getLog(), "Project '%s' not found. The task was ignored", prjname.c_str());
        return false;
    }

    // Converting project related variables
    workingdir = su::String_replace(workingdir, "$(pdir)", prj->m_path, true);
    application = su::String_replace(application, "$(pdir)", prj->m_path, true);

    commandline = su::String_replace(commandline, "$(pdir)", prj->m_workPath, true);
    sourcefile = su::String_replace(sourcefile, "$(pdir)", prj->m_workPath, true);
    outputfile = su::String_replace(outputfile, "$(pdir)", prj->m_workPath, true);

    LOGSPI(getLog(), "Run task %u: prj='%s' wdir='%s' app='%s' params='%s' src='%s' out='%s'",
           packet.id(),
           prjname.c_str(),
           workingdir.c_str(),
           application.c_str(),
           commandline.c_str(),
           sourcefile.c_str(),
           outputfile.c_str());

    Task* task = new Task;

    task->m_id = packet.id();
    task->m_sourceFile = sourcefile;
    task->m_outputFile = outputfile;
    task->m_abortOnError = packet.abortonerror();

    size_t result = su::fs::save(task->m_sourceFile, packet.inputdata());
    if (result != su::fs::OK)
    {
        LOGSPE(getLog(), "Cant save '%s' source file. Error %u", sourcefile.c_str(), result);
        return false;
    }

    task->m_process = new su::Process::AppObject(application.c_str(),
                                                 commandline.c_str(),
                                                 workingdir.c_str(),
                                                 su::Process::LaunchMode::NoConsole);
    task->m_process->execute();

    m_tasks.push_back(task);

    return true;
}
