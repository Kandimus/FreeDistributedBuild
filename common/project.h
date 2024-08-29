
#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace su
{
class Log;
}

namespace tinyxml2
{
class XMLElement;
}

class Projects
{
    struct Item
    {
        std::string m_name;
        std::string m_path;
        std::string m_workPath;
    };

public:
    Projects(su::Log* plog);
    virtual ~Projects() = default;

    bool loadFromFile(const std::string& filename);

    const Item* getProject(const std::string& prjname) const;
    uint32_t getFreeCore() const;

private:
    float getWorkPercent() const;
    bool loadWorkTime(const tinyxml2::XMLElement* root);

protected:
    std::unordered_map<std::string, Item> m_items;
    su::Log* m_log = nullptr;

    size_t m_workBegin = 0;
    size_t m_workEnd = 0;
    float m_workBath = 100;
    float m_workDefault = 100;
};
