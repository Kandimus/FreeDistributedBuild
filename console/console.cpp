#define WIN32_LEAN_AND_MEAN

#include "console.h"

#include <chrono>
#include <filesystem>
#include <vector>
#include <mutex>
#include <unordered_map>

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

#include "project.h"
#include "global_constants.h"
#include "whoishere.h"

#include "tcp_protobufnode.h"
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

namespace su
{
namespace Terminal
{

void setCursor(SHORT x, SHORT y)
{
    COORD pos = {x, y};
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);
}

}
}

namespace Global
{
    std::atomic_bool abort;
    su::Crc32 crc32;
}

bool taskGenerator(const tinyxml2::XMLElement* element, const TaskVariables& vars, std::vector<TaskInfo>& tasks, uint32_t& id)
{
    std::vector<TaskInfo> out;

    auto sourceFile = su::tinyxml2::getAttributeString(element, "SourceFile", "", true);
    auto outputFile = su::tinyxml2::getAttributeString(element, "OutputFile", "", true);
    auto application = su::tinyxml2::getAttributeString(element, "Application", "", true);
    auto params = su::tinyxml2::getAttributeString(element, "Params", "", true);
    auto workingDir = su::tinyxml2::getAttributeString(element, "WorkingDir", "", true);
    bool isAbortOnError = su::tinyxml2::getAttributeBool(element, "AbortOnError", true);

    sourceFile = su::String_replace(sourceFile, "/", "\\", true);
    outputFile = su::String_replace(outputFile, "/", "\\", true);
    application = su::String_replace(application, "/", "\\", true);
    params = su::String_replace(params, "/", "\\", true);
    workingDir = su::String_replace(workingDir, "/", "\\", true);

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

            if (filename.substr(filename.size() - mask.size()) == mask)
            {
                TaskInfo mti(++id);

                mti.m_vars = vars;
                mti.m_vars.SourceFile = filename;
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

        // step 3. convert absolute paths to project related paths
        auto curCommandline = task.m_vars.replace(params);
        auto curWorkingDir = task.m_vars.replace(workingDir);

        sourceFile = su::String_replace(task.m_vars.SourceFile, task.m_vars.PDir, "$(pdir)", true);
        outputFile = su::String_replace(task.m_vars.OutputFile, task.m_vars.PDir, "$(pdir)", true);
        curCommandline = su::String_replace(curCommandline, task.m_vars.PDir, "$(pdir)", true);
        curWorkingDir = su::String_replace(curWorkingDir, task.m_vars.PDir, "$(pdir)", true);

        // step 4. create the protobuf message
        task.m_message.set_project(task.m_vars.PName);
        task.m_message.set_sourcefile(sourceFile);
        task.m_message.set_outputfile(outputFile);
        task.m_message.set_application(task.m_vars.replace(application));
        task.m_message.set_commandline(curCommandline);
        task.m_message.set_workingdir(curWorkingDir);
        task.m_message.set_abortonerror(isAbortOnError);

        LOGD("Task %u: '%s\\%s %s', '%s' ==> '%s'",
             task.m_message.id(),
             task.m_message.workingdir().c_str(),
             task.m_message.application().c_str(),
             task.m_message.commandline().c_str(),
             task.m_message.sourcefile().c_str(),
             task.m_message.outputfile().c_str());
    }

    if (out.size())
    {
        tasks.insert(tasks.end(), out.begin(), out.end());
    }

    return true;
}

bool loadConfig(const std::string& filename, const Projects& projects, const TaskVariables& vars, std::vector<TaskInfo>& tasks)
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
        std::string prjName = su::tinyxml2::getAttributeString(project, "Name", "", true);
        auto prj = projects.getProject(prjName);

        if (!prj)
        {
            LOGE("Found the unknown project '%s'. All its tasks were skipped", prjName.c_str());
            continue;
        }

        TaskVariables prjVars = vars;
        prjVars.PName = prjName;
        prjVars.PDir = prj->m_path;

        LOGN("Found the project '%s'. Loading tasks", prjVars.PName.c_str());

        for (auto element = project->FirstChildElement("Task"); element != nullptr; element = element->NextSiblingElement("Task"))
        {
            if (!taskGenerator(element, prjVars, tasks, id))
            {
                return false;
            }
        }
    }

    if (tasks.size())
    {
        LOGN("Loaded configuration '%s' was successful", filename.c_str());
        LOGI("Created %i tasks", tasks.size());
    }
    else
    {
        LOGW("No tasks created!");
    }

    return tasks.size();
}

// send upd datagramm
bool sendBroadcast(const std::string& prjName)
{
    std::vector<uint32_t> addresses = su::Net::getLocalIps();
    size_t fault = 0;

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
    su::Log::instance().setFilename("fdbconsole");
    su::Log::instance().setTerminal(false);
    su::Log::instance().setLevel(static_cast<su::Log::Level>(atoi(cl.getOption(Arg::LOG).c_str())));

#ifdef DEBUG
    //su::Log::instance().setTerminal(true);
    su::Log::instance().setLevel(su::Log::Level::Debug);
#endif

    printf("\nfdbconsole v%u.%u\n", Global::versionMajor, Global::versionMinor);

    std::string configFile = cl.getOption(Arg::CONFIG);
    if (configFile.empty())
    {
        printf("\nUsage: fdbconsole.exe --config=<xml file> [arguments]\n\n");
        cl.printArguments();
        return 0;
    }

    Projects projects(&su::Log::instance());
    TaskVariables taskVarible;
    std::vector<TaskInfo> tasks;

    if (!projects.loadFromFile(su::String_filenamePath(cl.getApplication()) + "\\" + Global::fileProjects))
    {
        return 1;
    }

    taskVarible.SDir = su::String_tolower(cl.getOption(Arg::SDIR));
    taskVarible.ODir = su::String_tolower(cl.getOption(Arg::ODIR));
    taskVarible.WDir = su::String_tolower(cl.getOption(Arg::WDIR));
    taskVarible.SFile = su::String_tolower(cl.getOption(Arg::SFILE));
    taskVarible.OFile = su::String_tolower(cl.getOption(Arg::OFILE));
    
    if (!loadConfig(configFile, projects, taskVarible, tasks))
    {
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

    if (!sendBroadcast(tasks[0].m_vars.PName))
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

    if (timeWaiting)
    {
#ifdef DEBUG
        workTimer.start(15000);
#else
        workTimer.start(5000);
#endif
    }

    struct ItemSlaveInfo
    {
        size_t m_success = 0;
        size_t m_errors = 0;
    };

    su::Terminal::setCursor(0, 0);
    printf("--------------------+----------------------\n");

    std::unordered_map<std::string, ItemSlaveInfo> info;
    size_t countError = 0;
    size_t countSuccess = 0;
    bool isFinished = false;
    while (!isFinished)
    {
        if (server.clientsCount())
        {
            workTimer.restart();
        }

        if (workTimer.isFinished())
        {
            break;
        }

        isFinished = true;

        //TODO Если демон вернул ошибку, то эту таску нужно отдать другому??? или пометить как аварийную и продолжить?
        //     или же смотреть переменную AbortOnError, и если она =1, то вообще выходить? Тогда может быть ее вынести
        //     в настройки проекта?
        for (TaskInfo& task : tasks)
        {
            std::lock_guard<std::mutex> guard(task.m_mutex);

            if (task.m_result == su::Process::ExitCodeResult::NoInit)
            {
                isFinished = false;
                continue;
            }

            if (task.m_doneIp.empty())
            {
                continue;
            }

            if (task.m_result == su::Process::ExitCodeResult::NotStarted)
            {
                ++info[task.m_doneIp].m_errors;
            }

            if (task.m_result == su::Process::ExitCodeResult::Exited)
            {
                if (task.m_exitCode)
                {
                    ++info[task.m_doneIp].m_errors;
                }
                else
                {
                    ++info[task.m_doneIp].m_success;
                }
            }
            task.m_doneIp = "";
        }

        countError = 0;
        countSuccess = 0;
        su::Terminal::setCursor(0, 1);

        for (const auto& [ip, value]: info)
        {
            countError += value.m_errors;
            countSuccess += value.m_success;

            printf("%19s | %llu/%llu/%llu          \n",
                   ip.c_str(), value.m_errors, value.m_success, value.m_errors + value.m_success);
        }
        printf("total               | %llu/%llu/%llu %3.1f%%\n",
               countError, countSuccess, tasks.size(),
               double(countError + countSuccess) / tasks.size() * 100.0);

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    server.closeAllClients();
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    server.close();

    if (workTimer.isFinished())
    {
        LOGW("No one daemon else responding! Tasks: %llu success, %llu fault", countSuccess, countError);
        return 1;
    }

    LOGI("All processes have been completed. Success %llu. Errors %llu", countSuccess, countError);
    return 0;

}

/*
в теории можно после добавления PDIR работать так - установить WDIR, в сырцах использовать только *.ucX", оутпут как "../$(SourceFileName).cnkX"
а приложение запускать, указывая пут от PDIR.

*/
