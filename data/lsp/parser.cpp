#include <yaml-cpp/yaml.h>
#include <string>
#include <vector>
#include <stdexcept>

struct LspConfig {
    std::string name;
    std::string command;
    std::vector<std::string> fileTypes;
    std::vector<std::string> args;
};

LspConfig parseLspConfig(const std::string& path) {
    YAML::Node root = YAML::LoadFile(path);

    LspConfig cfg;

    // Required fields
    if (!root["name"])
        throw std::runtime_error("Missing 'name'");
    cfg.name = root["name"].as<std::string>();

    if (!root["command"])
        throw std::runtime_error("Missing 'command'");
    cfg.command = root["command"].as<std::string>();

    // fileTypes (optional but expected)
    if (root["fileTypes"]) {
        for (const auto& ft : root["fileTypes"]) {
            cfg.fileTypes.push_back(ft.as<std::string>());
        }
    }

    // args
    if (root["args"]) {
        for (const auto& arg : root["args"]) {
            cfg.args.push_back(arg.as<std::string>());
        }
    }

    return cfg;
}