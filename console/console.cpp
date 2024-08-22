#define WIN32_LEAN_AND_MEAN

#include "console.h"

#include <chrono>
#include <filesystem>
#include <vector>
#include <mutex>

#pragma warning(disable:4267)
#include "tinyxml2.h"
#include "tinyxml2/helper.h"
#pragma warning(default:4267)

#include "commandline.h"
#include "crc32.h"
#include "log.h"
#include "net/net.h"
#include "stringex.h"
#include "tickcount.h"
#include "uniint.h"

#include "common.h"
#include "whoishere.h"

#include "tcp_protobufserver.h"

namespace fs = std::filesystem;

namespace Arg
{
    const su::CommandLineOption CONFIG = { "config", 'c' };
    const su::CommandLineOption SDIR =   { "sdir",   'S' };
    const su::CommandLineOption ODIR =   { "odir",   'O' };
    const su::CommandLineOption WDIR =   { "wdir",   'W' };
    const su::CommandLineOption OFILE =  { "ofile",  'o' };
    const su::CommandLineOption SFILE =  { "sfile",  's' };
    const su::CommandLineOption LOG =    { "log"  ,  'l' };
    const su::CommandLineOption WAIT =   { "wait" ,  'w' };
};

namespace Global
{
    std::atomic_bool abort;
    su::Crc32 crc32;
}

bool taskGenerator(const tinyxml2::XMLElement* element, const TaskVariables& vars, std::vector<TaskInfo>& tasks, uint32_t& id)
{
    std::vector<TaskInfo> out;

    auto sourceFile = su::tinyxml2::getAttributeString(element, "SourceFile", "", false);
    auto outputFile = su::tinyxml2::getAttributeString(element, "OutputFile", "", false);
    auto application = su::tinyxml2::getAttributeString(element, "Application", "", false);
    auto params = su::tinyxml2::getAttributeString(element, "Params", "", false);
    auto workingDir = su::tinyxml2::getAttributeString(element, "WorkingDir", "", false);
    bool isAbortOnError = su::tinyxml2::getAttributeBool(element, "AbortOnError", true);

    sourceFile = su::String_replace(sourceFile, "/", "\\", true);

    if (sourceFile.find('*') != std::string::npos)
    {
        auto posLastSlash = sourceFile.rfind('\\');
        std::string sourceDir = posLastSlash != std::string::npos ? sourceFile.substr(0, posLastSlash + 1) : ".\\";

        sourceFile = vars.replace(sourceFile);
        posLastSlash = sourceFile.rfind('\\');

        std::string dir = posLastSlash != std::string::npos ? sourceFile.substr(0, posLastSlash) : ".\\";
        std::string mask = su::String_tolower(posLastSlash != std::string::npos ? sourceFile.substr(posLastSlash + 2) : sourceFile);

        for (const auto& entry : fs::directory_iterator(dir))
        {
            std::string filename = su::String_tolower(entry.path().string());
            auto pos = filename.rfind('\\');

            if (filename.substr(filename.size() - mask.size()) == mask)
            {
                TaskInfo mti(++id);

                mti.m_vars = vars;
                mti.m_vars.SourceFile = sourceFile;
                mti.m_vars.OutputFile = outputFile;

                out.push_back(mti);
            }
        }
    }
    else
    {
        TaskInfo mti(++id);

        mti.m_vars = vars;
        mti.m_vars.SourceFile = sourceFile;
        mti.m_vars.OutputFile = outputFile;

        out.push_back(mti);
    }

    for (auto& task : out)
    {
        // step 1. processing the source file and raw name
        task.m_vars.SourceFile = task.m_vars.replace(task.m_vars.SourceFile);
        task.m_vars.SourceFileName = su::String_rawFilename(fs::path(task.m_vars.SourceFile).filename().string());

        // step 2. processing the output file name
        task.m_vars.OutputFile = task.m_vars.replace(task.m_vars.OutputFile);

        std::string commandline = task.m_vars.replace(params);
        // step 3. other parameters
        task.m_message.set_project(task.m_vars.PrjName);
        task.m_message.set_outputfile(task.m_vars.OutputFile);
        task.m_message.set_application(task.m_vars.replace(application));
        task.m_message.set_commandline(task.m_vars.replace(params));
        task.m_message.set_workingdir(task.m_vars.replace(workingDir));
        task.m_message.set_abortonerror(isAbortOnError);

        if (!task.m_message.IsInitialized())
        {
            LOGE("Can not create new task");
            return false;
        }
    }

    if (out.size())
    {
        tasks.insert(tasks.end(), out.begin(), out.end());
    }

    return true;
}

bool loadConfig(const std::string& filename, const TaskVariables& vars, std::vector<TaskInfo>& tasks)
{
    tinyxml2::XMLDocument doc;
    uint32_t id = 0;

    if (tinyxml2::XML_SUCCESS != doc.LoadFile(filename.c_str()))
    {
        LOGE("Loading configuration '%s' failed: %s", filename.c_str(), doc.ErrorStr());
        return false;
    }

    auto root = doc.FirstChildElement("BuildSet");
    if (!root)
    {
        LOGE("Loading configuration '%s' failed: can not found the `BuildSet` root element", filename.c_str());
        return false;
    }

    for (auto project = root->FirstChildElement("Project"); project; project = project->NextSiblingElement("Project"))
    {
        TaskVariables prjVars = vars;

        prjVars.PrjName = su::tinyxml2::getAttributeString(project, "Name", "", true);

        for (auto element = project->FirstChildElement("Task"); element != nullptr; element = element->NextSiblingElement("Task"))
        {
            if (!taskGenerator(element, prjVars, tasks, id))
            {
                return false;
            }
        }
    }

    LOGN("Loaded configuration '%s' was successful", filename.c_str());
    LOGI("Created %i tasks", tasks.size());

    return true;
}

// send upd datagramm
bool sendBroadcast(const std::string& prjName)
{
    std::vector<uint32_t> addresses = su::Net::getLocalIps();
    size_t fault = 0;
    //const uint32_t localhost = 0x0100007f; // 127.0.0.1

    for (auto addr : addresses)
    {
        su::UniInt32 ip = addr;

        NetPacket::WhoIsHere packet;
        packet.magic = Global::udpMagicNumber;
        packet.version = 0x0100;
        packet.masterIP = addr;
        packet.masterPort = Global::tcpDefaultPort;
        memset(packet.project, 0, sizeof(packet.project));
        strncpy_s(packet.project, sizeof(packet.project), prjName.c_str(), prjName.size() + 1);
        packet.crc32 = Global::crc32.get(&packet, sizeof(NetPacket::WhoIsHere) - sizeof(packet.crc32));

        std::string strIp = su::String_format2("%i.%i.%i.%i", ip.u8[0], ip.u8[1], ip.u8[2], ip.u8[3]);

        int bytesSent = su::Net::updSend(Global::udpMulticastIp, Global::udpDefaultPort, &packet, sizeof(packet), su::Net::Multicast);
        if (sizeof(packet) != bytesSent)
        {
            ++fault;
            LOGE("Interface %s: Error sending multicast packet to %s:%i",
                 strIp.c_str(), Global::udpMulticastIp.c_str(), Global::udpDefaultPort);
        }
        else
        {
            LOGN("Interface %s: Multcast packet sent to %s:%i",
                 strIp.c_str(), Global::udpMulticastIp.c_str(), Global::udpDefaultPort);
        }


    }
    return !fault;
}

int main(int argc, const char** argv)
{
    su::CommandLine cl;

    cl.addOption(Arg::CONFIG, "", "config file")
        .addOption(Arg::SDIR, ".\\", "Source directory. Value $(SDIR)")
        .addOption(Arg::ODIR, ".\\", "Output directory. Value $(ODIR)")
        .addOption(Arg::WDIR, ".\\", "Working directory. Value $(WDIR)")
        .addOption(Arg::OFILE, "", "Source file. Value $(SFILE)")
        .addOption(Arg::SFILE, "", "Output file. Value $(OFILE)")
        .addOption(Arg::LOG, "1", "Log level (0 only error ... 4 debug)")
        .addOption(Arg::WAIT, "3000", "Timer of waiting of daemons respond")
        .parse(argc, argv);

    su::Log::instance().setDir("./logs/");
    su::Log::instance().setFilename("fdbconsole.log");
    su::Log::instance().setTerminal(false);
    su::Log::instance().setLevel(static_cast<su::Log::Level>(atoi(cl.getOption(Arg::LOG).c_str())));

#ifdef DEBUG
    su::Log::instance().setTerminal(true);
    su::Log::instance().setLevel(su::Log::Level::Debug);
#endif

    printf("\nfdbconsole v1.1\n");

    std::string configFile = cl.getOption(Arg::CONFIG);
    if (configFile.empty())
    {
        printf("\nUsage: fdbconsole.exe --config=<xml file> [arguments]\n\n");
        cl.printArguments();
        return 0;
    }

    TaskVariables taskVarible;
    std::vector<TaskInfo> tasks;
    
    taskVarible.SDir = cl.getOption(Arg::SDIR);
    taskVarible.ODir = cl.getOption(Arg::ODIR);
    taskVarible.WDir = cl.getOption(Arg::WDIR);
    taskVarible.SFile = cl.getOption(Arg::SFILE);
    taskVarible.OFile = cl.getOption(Arg::OFILE);
    
    loadConfig(configFile, taskVarible, tasks);

    if (tasks.empty())
    {
        LOGW("No tasks created!");
        return 1;
    }

    WSADATA wsaData;
    auto iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != NO_ERROR)
    {
        printf("Error at WSAStartup()\n");
        return 1;
    }

    Global::abort = false;

    TcpProtobufServer server(tasks, su::Net::TcpBroadcastAddress, Global::tcpDefaultPort, 0, &su::Log::instance());
    server.run(16);

    server.start();
    if (server.isStarted())
    {
        LOGN("Server has been started");
    }
    else
    {
        LOGE("Can not starting the Server. Error: %i", server.node()->getLastError());
        return 1;
    }

    if (!sendBroadcast(tasks[0].m_vars.PrjName))
    {
        server.finish();
        while (server.status() != su::ThreadClass::Status::Finished)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        server.close();

        return 1;
    }

    size_t timeWaiting = atoi(cl.getOption(Arg::WAIT).c_str());
    su::TickCount workTimer;

    int total = 0;
    int isError = 0;
    int isSuccess = 0;

    if (timeWaiting)
    {
        workTimer.start(5000);
    }

    while (total != tasks.size())
    {
        isError = 0;
        isSuccess = 0;

        if (workTimer.isFinished())
        {
            LOGW("No one daemon responded! Exit.");
            break;
        }

        for (TaskInfo& task : tasks)
        {
            std::lock_guard<std::mutex> guard(task.m_mutex);

            if (task.m_result == su::Process::ExitCodeResult::NoInit)
            {
                continue;
            }

            if (task.m_result == su::Process::ExitCodeResult::NotStarted)
            {
                ++isError;
            }

            if (task.m_result == su::Process::ExitCodeResult::Exited)
            {
                if (task.m_exitCode)
                {
                    ++isError;
                }
                else
                {
                    ++isSuccess;
                }
            }
        }
        total = isError + isSuccess;

        printf("localhost        %i/%i/%i %3.1f%%\r", isError, isSuccess, (int)tasks.size(), double(total) / tasks.size() * 100.0);

        std::this_thread::sleep_for(std::chrono::milliseconds(96));
    }

    server.finish();
    server.thread()->join();

    if (workTimer.isFinished())
    {
        LOGW("No one daemon responded! Tasks: %i success, %i fault", isSuccess, isError);
        return 1;
    }

    LOGI("All processes have been completed. Success %i. Errors %i", isSuccess, isError);
    return 0;

}

/*
в теории можно после добавления PDIR работать так - установить WDIR, в сырцах использовать только *.ucX", оутпут как "../$(SourceFileName).cnkX"
а приложение запускать, указывая пут от PDIR.

*/
