
#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_

//#include "daemon.h"

#include <chrono>
#include <filesystem>
#include <vector>
#include <mutex>

#pragma warning(disable:4267)
#include "tinyxml2.h"
#include "tinyxml2/helper.h"
#pragma warning(default:4267)
#include "log.h"
#include "commandline.h"
#include "stringex.h"
#include "net/udp_node.h"

#include "common.h"

#include "udp_daemonserver.h"

namespace Arg
{
    const su::CommandLineOption CONFIG = { "config", 'c' };
    const su::CommandLineOption LOG = { "log", 'l' };
    const su::CommandLineOption TERMINAL = { "terminal", 't' };
};

bool loadConfig(const std::string& filename, std::vector<Project>& projects)
{
    tinyxml2::XMLDocument doc;

    if (tinyxml2::XML_SUCCESS != doc.LoadFile(filename.c_str()))
    {
        LOGE("Loading configuration '%s' failed: %s", filename.c_str(), doc.ErrorStr());
        return false;
    }

    auto root = doc.FirstChildElement("Config");
    if (!root)
    {
        LOGE("Loading configuration '%s' failed: can not found the `Config` root element", filename.c_str());
        return false;
    }

    // Check version

    for (auto project = root->FirstChildElement("Project"); project; project = project->NextSiblingElement("Project"))
    {
        Project prj;

        prj.m_name = su::tinyxml2::getAttributeString(project, "Name", "", true);
        prj.m_path = su::tinyxml2::getAttributeString(project, "Path", "", true);

        projects.push_back(prj);
    }

    LOGN("Loaded configuration '%s' was successful", filename.c_str());

    return true;
}

int main(int argc, const char** argv)
{
    su::CommandLine cl;

    cl.addOption(Arg::CONFIG, "./FreeDistributedBuild.xml", "config file")
        .addOption(Arg::LOG, "1", "Log level (0 only error ... 4 debug)")
        .addSwitch(Arg::TERMINAL, "Print logs to terminal")
        .parse(argc, argv);

    su::Log::instance().setDir("./logs/");
    su::Log::instance().setFilename("fdbdaemon.log");
    su::Log::instance().setTerminal(cl.isSet(Arg::TERMINAL));
    su::Log::instance().setLevel(static_cast<su::Log::Level>(atoi(cl.getOption(Arg::LOG).c_str())));

#ifdef DEBUG
    su::Log::instance().setTerminal(true);
    su::Log::instance().setLevel(su::Log::Level::Debug);
#endif

    printf("\nfdbdeamon v1.0\n");

    std::string configFile = cl.getOption(Arg::CONFIG);
    if (configFile.empty())
    {
        printf("\nUsage: fdbdeamon.exe --config=<xml file> [arguments]\n\n");
        cl.printArguments();

        LOGE("The configuration file is empty");
        return 0;
    }

    std::vector<Project> projects;

    if (!loadConfig(configFile, projects))
    {
        LOGE("Can not load configuration");
        return 1;
    }

    if (!su::Net::initWinSock2())
    {
        LOGE("Can not initialize network driver");
        return 1;
    }

    su::Net::UdpNode udpNode;
    UdpDaemonServer udpServer(udpNode, Global::udpMulticastIp, Global::udpDefaultPort, projects, &su::Log::instance());

    udpServer.run(16);
    udpServer.start(true);
    udpServer.thread()->join();

    return 0;
}

/*
в теории можно после добавления PDIR работать так - установить WDIR, в сырцах использовать только *.ucX", оутпут как "../$(SourceFileName).cnkX"
а приложение запускать, указывая пут от PDIR.
*/
