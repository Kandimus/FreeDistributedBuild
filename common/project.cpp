
#include "project.h"

#include <filesystem>
#include <iomanip>

#pragma warning(disable:4267)
#include "tinyxml2.h"
#include "tinyxml2/helper.h"
#pragma warning(default:4267)
#include "log.h"
#include "fileex.h"
#include "threadpool.h"

Projects::Projects(su::Log* plog)
{
    m_log = plog;
}

bool Projects::loadFromFile(const std::string& filename)
{
    tinyxml2::XMLDocument doc;

    if (tinyxml2::XML_SUCCESS != doc.LoadFile(filename.c_str()))
    {
        LOGSPE(m_log, "Loading configuration '%s' failed: %s", filename.c_str(), doc.ErrorStr());
        return false;
    }

    auto root = doc.FirstChildElement("Config");
    if (!root)
    {
        LOGSPE(m_log, "Loading configuration '%s' failed: can not found the `Config` root element", filename.c_str());
        return false;
    }

    //TODO Check version

    for (auto project = root->FirstChildElement("Project"); project; project = project->NextSiblingElement("Project"))
    {
        Item prj;

        prj.m_name = su::tinyxml2::getAttributeString(project, "Name", "", true);
        prj.m_path = su::tinyxml2::getAttributeString(project, "Path", "", true);
        prj.m_workPath = su::tinyxml2::getAttributeString(project, "WorkDir", "C:\\Windows\\Temp", true);

        prj.m_path = su::String_replace(prj.m_path, "/", "\\", true);
        prj.m_workPath = su::String_replace(prj.m_workPath, "/", "\\", true);

        prj.m_workPath = std::filesystem::absolute(prj.m_workPath).string();
        su::fs::addSeparator(prj.m_workPath);

        m_items[prj.m_name] = prj;
        LOGSPN(m_log, "Project '%s' have been loaded", prj.m_name.c_str());
    }

    if (!loadWorkTime(root->FirstChildElement("WorkTime")))
    {
        return false;
    }

    LOGSPN(m_log, "Loaded configuration '%s' was successful", filename.c_str());

    return true;
}

const Projects::Item* Projects::getProject(const std::string& prjname) const
{
    return m_items.contains(prjname) ? &m_items.at(prjname) : nullptr;
}

uint32_t Projects::getFreeCore() const
{
    int32_t coreReserved = 3;
    int32_t maxThreads = su::ThreadPool::getMaxThreads() - coreReserved;

    maxThreads = maxThreads < 1 ? 1 : maxThreads;

    float maxTasksCount = maxThreads * getWorkPercent() / 100.0f;

    return static_cast<uint32_t>(maxTasksCount + 0.5f);
}

float Projects::getWorkPercent() const
{
    std::time_t t = std::time(0);
    std::tm now = {};
    localtime_s(&now, &t);
    size_t workNow = now.tm_hour * 60 + now.tm_min;

    if (m_workBegin < m_workEnd)
    {
        return (workNow >= m_workBegin && workNow <= m_workEnd) ? m_workBath : m_workDefault;
    }

    return (workNow >= m_workBegin || workNow <= m_workEnd) ? m_workBath : m_workDefault;
}

bool Projects::loadWorkTime(const tinyxml2::XMLElement* root)
{
    if (!root)
    {
        return true;
    }

    auto xmlBegin = root->FirstChildElement("Begin");
    if (xmlBegin)
    {
        std::istringstream ss(xmlBegin->GetText());
        std::tm t;
        ss >> std::get_time(&t, "%H:%M");

        if (ss.fail())
        {
            return false;
        }

        m_workBegin = t.tm_hour * 60 + t.tm_min;
    }

    auto xmlEnd = root->FirstChildElement("End");
    if (xmlEnd)
    {
        std::istringstream ss(xmlEnd->GetText());
        std::tm t;
        ss >> std::get_time(&t, "%H:%M");

        if (ss.fail())
        {
            return false;
        }
        m_workEnd = t.tm_hour * 60 + t.tm_min;
    }

    auto xmlBath = root->FirstChildElement("Bath");
    if (xmlBath)
    {
        m_workBath = std::clamp(static_cast<float>(std::atof(xmlBath->GetText())), 0.0f, 100.0f);
    }

    auto xmlDefault = root->FirstChildElement("Default");
    if (xmlDefault)
    {
        m_workDefault = std::clamp(static_cast<float>(std::atof(xmlDefault->GetText())), 0.0f, 100.0f);
    }

    return true;
}
