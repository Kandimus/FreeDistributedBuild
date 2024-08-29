
#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_

#include <chrono>
#include <filesystem>
#include <vector>
#include <mutex>

#include "log.h"
#include "commandline.h"
#include "stringex.h"
#include "net/udp_node.h"

#include "project.h"
#include "global_constants.h"

#include "udp_daemonserver.h"

// window.cpp
void createWindow(const std::string& winName);

namespace Arg
{
    const su::CommandLineOption CONFIG = { "config", 'c' };
    const su::CommandLineOption LOG = { "log", 'l' };
    const su::CommandLineOption TERMINAL = { "terminal", 't' };
};

int main(int argc, const char** argv)
{
    su::CommandLine cl;

    cl.addOption(Arg::CONFIG, ".\\" + Global::fileProjects, "config file")
        .addOption(Arg::LOG, "1", "Log level (0 only error ... 4 debug)")
        .addSwitch(Arg::TERMINAL, "Print logs to terminal")
        .parse(argc, argv);

    su::Log::instance().setDir("logs\\");
    su::Log::instance().setFilename("fdbdaemon");
    su::Log::instance().setTerminal(cl.isSet(Arg::TERMINAL));
    su::Log::instance().setLevel(static_cast<su::Log::Level>(atoi(cl.getOption(Arg::LOG).c_str())));

#ifdef DEBUG
    su::Log::instance().setTerminal(true);
    su::Log::instance().setLevel(su::Log::Level::Debug);
#endif

    printf("\nfdbdeamon v%u.%u\n", Global::versionMajor, Global::versionMinor);

    std::string configFile = cl.getOption(Arg::CONFIG);
    if (configFile.empty())
    {
        printf("\nUsage: fdbdeamon.exe --config=<xml file> [arguments]\n\n");
        cl.printArguments();

        LOGE("The configuration file is empty");
        return 0;
    }

    FreeConsole();

    Projects projects(&su::Log::instance());

    if (!projects.loadFromFile(configFile))
    {
        return 1;
    }

    if (!su::Net::initWinSock2())
    {
        LOGE("Can not initialize network driver");
        return 1;
    }

    std::string consoleTitleName = su::String_format2("FreeDistributedBuild - deamon v%i.%i",
                                                      Global::versionMajor, Global::versionMinor);

    SetConsoleTitle(consoleTitleName.c_str());

    std::thread windowThread([consoleTitleName]
    {
        createWindow(consoleTitleName.c_str());
    });

    su::Net::UdpNode udpNode;
    UdpDaemonServer udpServer(udpNode, Global::udpMulticastIp, Global::udpDefaultPort, projects, &su::Log::instance());

    udpServer.run(0);
    udpServer.start(true);

    windowThread.join();
    LOGW("Shutdown...");
    udpServer.close();

    return 0;
}
