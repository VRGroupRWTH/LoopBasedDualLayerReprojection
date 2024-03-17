#include "command_parser.hpp"

#include <spdlog/spdlog.h>
#include <vector>

struct Parameter
{
    std::string name;
    std::string value;
};

bool CommandParser::parse(uint32_t argument_count, const char** argument_list)
{
    std::vector<Parameter> parameters;
    std::vector<std::string> flags;

    for (uint32_t index = 1; index < argument_count - 1; index++)
    {
        std::string argument = argument_list[index];

        if (argument.substr(0, 2) != "--")
        {
            spdlog::error("Invalid argument: {}", argument);

            return false;
        }

        std::size_t offset = argument.find('=');

        if (offset == std::string::npos)
        {
            std::string name = argument.substr(2);

            flags.push_back(name);
        }

        else
        {
            std::string name = argument.substr(2, offset - 2);
            std::string value = argument.substr(offset + 1);

            Parameter parameter;
            parameter.name = name;
            parameter.value = value;

            parameters.push_back(parameter);
        }
    }

    for (const std::string& flag : flags)
    {
        spdlog::error("Invalid flag: {}", flag);

        return false;
    }

    for (const Parameter& parameter : parameters)
    {
        if (parameter.name == "scene_scale")
        {
            this->scene_scale = std::atof(parameter.value.c_str());
        }

        else if (parameter.name == "scene_exposure")
        {
            this->scene_exposure = std::atof(parameter.value.c_str());
        }

        else if (parameter.name == "scene_indirect_intensity")
        {
            this->scene_indirect_intensity = std::atof(parameter.value.c_str());
        }

        else if (parameter.name == "sky_file_name")
        {
            this->sky_file_name = parameter.value;
        }

        else if (parameter.name == "sky_intensity")
        {
            this->sky_intensity = std::atof(parameter.value.c_str());
        }

        else if (parameter.name == "scene_directory")
        {
            this->scene_directory = parameter.value;
        }

        else if (parameter.name == "study_directory")
        {
            this->study_directory = parameter.value;
        }

        else
        {
            spdlog::error("Invalid parameter: {}", parameter.name);

            return false;
        }
    }

    if (argument_count > 1)
    {
        this->scene_file_name = argument_list[argument_count - 1];
    }

    return true;
}

const std::string& CommandParser::get_scene_directory() const
{
    return this->scene_directory;
}

const std::string& CommandParser::get_study_directory() const
{
    return this->study_directory;
}

std::optional<std::string> CommandParser::get_scene_file_name() const
{
    return this->scene_file_name;
}

float CommandParser::get_scene_scale() const
{
    return this->scene_scale;
}

float CommandParser::get_scene_exposure() const
{
    return this->scene_exposure;
}

float CommandParser::get_scene_indirect_intensity() const
{
    return this->scene_indirect_intensity;
}

std::optional<std::string> CommandParser::get_sky_file_name() const
{
    return this->sky_file_name;
}

float CommandParser::get_sky_intensity() const
{
    return this->sky_intensity;
}