// FreeIncrediBuildsConsole.cpp : Defines the entry point for the application.
//

#include "FreeDistributedBuild.h"

#include <chrono>
#include <filesystem>
#include <vector>
#include <mutex>

#pragma warning(disable:4267)
#include "tinyxml2.h"
#pragma warning(default:4267)
#include "log.h"
#include "threadpool.h"
#include "commandline.h"
#include "stringex.h"

#include "Process.h"

namespace fs = std::filesystem;

namespace Arg
{
    const su::CommandLineOption CONFIG = { "config", 'c' };
    const su::CommandLineOption SDIR =   { "sdir",   'S' };
    const su::CommandLineOption ODIR =   { "odir",   'O' };
    const su::CommandLineOption WDIR =   { "wdir",   'W' };
    const su::CommandLineOption OFILE =  { "ofile",  'o' };
    const su::CommandLineOption SFILE =  { "sfile",  's' };
    const su::CommandLineOption LOG = { "log", 'l' };
};

namespace Global
{
    std::vector<Process::AppObject> g_processList;
    std::atomic_bool g_abort;
}


struct TaskVariable
{
    std::string PRJNAME = "";
    std::string SDIR = "";
    std::string ODIR = "";
    std::string WDIR = "";
    std::string SFILE = "";
    std::string OFILE = "";

    // calclulated vars
    std::string SourceFile = "";
    std::string SourceFileName = "";
    std::string OutputFile = "";
};


struct TaskInfo
{
    TaskInfo() = default;
    TaskInfo(const TaskInfo& ti)
    {
        copyFrom(ti);
    }
    TaskInfo(const TaskInfo&& ti)
    {
        copyFrom(ti);
    }

    void copyFrom(const TaskInfo& ti)
    {
        m_variable = ti.m_variable;
        m_sourceFile = ti.m_sourceFile;
        m_outputFile = ti.m_outputFile;
        m_application = ti.m_application;
        m_params = ti.m_params;
        m_workingDir = ti.m_workingDir;
        m_isAbortOnError = ti.m_isAbortOnError;
    }

    TaskInfo& operator = (const TaskInfo& ti)
    {
        copyFrom(ti);
        return *this;
    }

    TaskVariable m_variable;

    std::string m_project = "";
    std::string m_sourceFile = "";
    std::string m_outputFile = "";
    std::string m_application = "";
    std::string m_params = "";
    std::string m_workingDir = "";
    bool m_isAbortOnError = true;

    // result
    std::mutex m_mutex;
    int m_exitCode = 0;
    Process::ExitCodeResult m_result = Process::ExitCodeResult::NoInit;
};

std::string getAttributeString(const tinyxml2::XMLElement* element, const std::string& name, const std::string& def, bool isLower)
{
    std::string out = (element->Attribute(name.c_str())) ? element->Attribute(name.c_str()) : def;
    return isLower ? su::String_tolower(out) : out;
}

bool getAttributeBool(const tinyxml2::XMLElement* element, const std::string& name, bool def)
{
    std::string out = su::String_tolower((element->Attribute(name.c_str())) ? element->Attribute(name.c_str()) : (def ? "true" : "false"));

    if (out == "true" || out == "1")
    {
        return true;
    }
    else if (out == "false" || out == "0")
    {
        return false;
    }

    return !!atoi(out.c_str());
}

std::string replaceVars(const std::string& str, const TaskVariable& vars)
{
    std::string out = str;

    out = su::String_replace(out, "$(PRJNAME)", vars.PRJNAME, true);

    out = su::String_replace(out, "$(SDIR)", vars.SDIR, true);
    out = su::String_replace(out, "$(WDIR)", vars.WDIR, true);
    out = su::String_replace(out, "$(SFILE)", vars.SFILE, true);
    out = su::String_replace(out, "$(OFILE)", vars.OFILE, true);

    out = su::String_replace(out, "$(SourceFile)", vars.SourceFile, true);
    out = su::String_replace(out, "$(SourceFileName)", vars.SourceFileName, true);
    out = su::String_replace(out, "$(OutputFile)", vars.OutputFile, true);

    return out;
}


bool taskGenerator(const tinyxml2::XMLElement* element, const TaskVariable& vars, std::vector<TaskInfo>& tasks)
{
    std::vector<TaskInfo> out;

    auto sourceFile = getAttributeString(element, "SourceFile", "", false);
    auto outputFile = getAttributeString(element, "OutputFile", "", false);
    auto application = getAttributeString(element, "Application", "", false);
    auto params = getAttributeString(element, "Params", "", false);
    auto workingDir = getAttributeString(element, "WorkingDir", "", false);
    bool isAbortOnError = getAttributeBool(element, "AbortOnError", true);

    sourceFile = su::String_replace(sourceFile, "/", "\\", true);

    if (sourceFile.find('*') != std::string::npos)
    {
        auto posLastSlash = sourceFile.rfind('\\');
        std::string sourceDir = posLastSlash != std::string::npos ? sourceFile.substr(0, posLastSlash + 1) : ".\\";

        sourceFile = replaceVars(sourceFile, vars);
        posLastSlash = sourceFile.rfind('\\');

        std::string dir = posLastSlash != std::string::npos ? sourceFile.substr(0, posLastSlash) : ".\\";
        std::string mask = su::String_tolower(posLastSlash != std::string::npos ? sourceFile.substr(posLastSlash + 2) : sourceFile);

        for (const auto& entry : fs::directory_iterator(dir))
        {
            std::string filename = su::String_tolower(entry.path().string());
            auto pos = filename.rfind('\\');

            if (filename.substr(filename.size() - mask.size()) == mask)
            {
                TaskInfo ti;

                ti.m_variable = vars;
                ti.m_sourceFile = sourceDir + (pos != std::string::npos ? filename.substr(pos + 1) : filename);
                ti.m_outputFile = outputFile;
                ti.m_application = application;
                ti.m_params = params;
                ti.m_workingDir = workingDir;

                out.push_back(ti);
            }
        }
    }
    else
    {
        TaskInfo ti;

        ti.m_variable = vars;
        ti.m_sourceFile = sourceFile;
        ti.m_outputFile = outputFile;
        ti.m_application = application;
        ti.m_params = params;
        ti.m_workingDir = workingDir;

        out.push_back(ti);
    }

    if (out.size())
    {
        tasks.insert(tasks.end(), out.begin(), out.end());
    }

    return true;
}

bool loadConfig(const std::string& filename, const TaskVariable& vars, std::vector<TaskInfo>& tasks)
{
    tinyxml2::XMLDocument doc;

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
        TaskVariable prjVars = vars;

        prjVars.PRJNAME = getAttributeString(project, "Name", "", true);

        for (auto element = project->FirstChildElement("Task"); element != nullptr; element = element->NextSiblingElement("Task"))
        {
            if (!taskGenerator(element, prjVars, tasks))
            {
                return false;
            }
        }
    }

    LOGN("Loaded configuration '%s' was successful", filename.c_str());

    return true;
}

void runProcess1(TaskInfo* ti)
{
    if (Global::g_abort)
    {
        ti->m_result = Process::ExitCodeResult::NotStarted;
        ti->m_exitCode = -1;
        return;
    }

    TaskVariable vars = ti->m_variable;

    // step 1. processing the source file and raw name
    vars.SourceFile = replaceVars(ti->m_sourceFile, ti->m_variable);
    vars.SourceFileName = su::String_rawFilename(fs::path(vars.SourceFile).filename().string());

    // step 2. processing the output file name
    vars.OutputFile = replaceVars(ti->m_outputFile, vars);

    // step 3. other parameters
    std::string appPath = replaceVars(ti->m_application, vars);
    std::string commandLine = replaceVars(ti->m_params, vars);
    std::string workingDir = replaceVars(ti->m_workingDir, vars);

    fs::current_path(workingDir);

    LOGI("Run process '%s %s' on '%s' working dir ", appPath.c_str(), commandLine.c_str(), workingDir.c_str());

    Process::AppObject process(appPath.c_str(), commandLine.c_str(), workingDir.c_str(), Process::LaunchMode::NoConsole);
    process.execute();

    while (process.isRunning())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(32));
    }
    
    std::lock_guard<std::mutex> lock(ti->m_mutex);

    ti->m_result = process.getProcessExitCode(&ti->m_exitCode);

    if (ti->m_result != Process::ExitCodeResult::Exited || ti->m_exitCode)
    {
        LOGE("Fault to run process '%s %s' on '%s' directory. Status %i. Exit code %i",
            appPath.c_str(), commandLine.c_str(), workingDir.c_str(), static_cast<int>(ti->m_result), ti->m_exitCode);

        if (ti->m_isAbortOnError)
        {
            Global::g_abort = true;
        }
    }
}


void showInfo(std::vector<TaskInfo>& tasks)
{
    int total = 0;

    while (total < tasks.size())
    {
        int isError = 0;
        int isSuccess = 0;

        for (TaskInfo& task : tasks)
        {
            std::lock_guard<std::mutex> guard(task.m_mutex);

            if (task.m_result != Process::ExitCodeResult::NoInit)
            {
                if (task.m_result == Process::ExitCodeResult::NotStarted)
                {
                    ++isError;
                }

                if (task.m_result == Process::ExitCodeResult::Exited)
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
        }
        total = isError + isSuccess;

        printf("localhost        %i/%i/%i %3.1f%%\r", isError, isSuccess, (int)tasks.size(), double(total) / tasks.size() * 100.0);

        std::this_thread::sleep_for(std::chrono::milliseconds(96));
    }
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
        .parse(argc, argv);

    su::Log::instance().setDir("./");
    su::Log::instance().setFilename("FreeDistributedBuild.log");

    su::Log::instance().setLevel(static_cast<su::Log::Level>(atoi(cl.getOption(Arg::LOG).c_str())));

#ifdef DEBUG
    su::Log::instance().setLevel(su::Log::Level::Debug);
#endif

    printf("\nFreeDistributedBuild v1.0\n");

    std::string configFile = cl.getOption(Arg::CONFIG);
    if (configFile.empty())
    {
        printf("\nUsage: FreeDistributedBuild.exe --config=<xml file> [arguments]\n\n");
        cl.printArguments();
        return 0;
    }

    TaskVariable taskVarible;
    std::vector<TaskInfo> tasks;
    
    taskVarible.SDIR = cl.getOption(Arg::SDIR);
    taskVarible.ODIR = cl.getOption(Arg::ODIR);
    taskVarible.WDIR = cl.getOption(Arg::WDIR);
    taskVarible.SFILE = cl.getOption(Arg::SFILE);
    taskVarible.OFILE = cl.getOption(Arg::OFILE);
    
    loadConfig(configFile, taskVarible, tasks);

    uint32_t persent = 75;
    uint32_t maxThreads = su::ThreadPool::getMaxThreads();
    su::ThreadPool pool(uint32_t(float(maxThreads * persent) / 100.0f));

    LOGI("Starting %i processes", tasks.size());

    std::thread gui([&tasks] { showInfo(tasks); });

    Global::g_abort = false;

    for (TaskInfo& task : tasks)
    {
        task.m_result = Process::ExitCodeResult::NoInit;
        task.m_exitCode = 0;

        pool.add_task(runProcess1, &task);
    }
    pool.wait_all();

    gui.join();

    // check for errors
    int errors = 0;
    for (TaskInfo& task : tasks)
    {
        if (task.m_result != Process::ExitCodeResult::Exited || task.m_exitCode)
        {
            ++errors;
        }
    }

    LOGI("All processes have been completed. Success %i. Errors %i", tasks.size() - errors, errors);

    return 0;

}
