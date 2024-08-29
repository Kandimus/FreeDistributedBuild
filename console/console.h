
#pragma once

#define WIN32_LEAN_AND_MEAN

#include <string>
#include <mutex>

#include "stringex.h"
#include "win/process.h"

#pragma warning(disable:4251)
#include "master.pb.h"
#include "slave.pb.h"
#pragma warning(default:4251)

class TcpProtobufNode;

struct TaskVariables
{
    std::string PName;
    std::string PDir;
    std::string SDir;
    std::string ODir;
    std::string WDir;
    std::string SFile;
    std::string OFile;

    // calclulated vars
    std::string SourceFile = "";
    std::string SourceFileName = "";
    std::string OutputFile = "";

    std::string replace(const std::string& str) const
    {
        std::string out = str;

        out = su::String_replace(out, "$(pname)", PName, true);
        out = su::String_replace(out, "$(pdir)", PDir, true);

        out = su::String_replace(out, "$(sdir)", SDir, true);
        out = su::String_replace(out, "$(wdir)", WDir, true);
        out = su::String_replace(out, "$(sfile)", SFile, true);
        out = su::String_replace(out, "$(ofile)", OFile, true);

        out = su::String_replace(out, "$(sourcefile)", SourceFile, true);
        out = su::String_replace(out, "$(sourcefilename)", SourceFileName, true);
        out = su::String_replace(out, "$(outputfile)", OutputFile, true);

        return out;
    }

};

struct TaskInfo
{
    TaskInfo(uint32_t id)
    {
        m_message.set_id(id);
    }
    TaskInfo(const TaskInfo& mti)
    {
        copyFrom(mti);
    }
    TaskInfo(const TaskInfo&& mti)
    {
        copyFrom(mti);
    }

    void copyFrom(const TaskInfo& mti)
    {
        m_vars = mti.m_vars;
        m_message = mti.m_message;
        //?????m_node = mti.m_node;
    }

    TaskInfo& operator = (const TaskInfo& ti)
    {
        copyFrom(ti);
        return *this;
    }

    TaskVariables m_vars;

    Master::Task m_message;
    std::mutex m_mutex;
    TcpProtobufNode* m_node = nullptr;
    int32_t m_exitCode = 0;
    su::Process::ExitCodeResult m_result = su::Process::ExitCodeResult::NoInit;
    std::string m_doneIp = "";
};
