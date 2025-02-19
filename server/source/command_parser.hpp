#ifndef HEADER_COMMAND_PARSER
#define HEADER_COMMAND_PARSER

#include <optional>
#include <string>
#include <cstdint>

class CommandParser
{
private:
    std::string scene_directory = "./scene";
    std::string study_directory = "./study";

    std::optional<std::string> scene_file_name;
    float scene_scale = 1.0f;
    float scene_exposure = 1.0f;
    float scene_indirect_intensity = 1.0f;

    std::optional<std::string> sky_file_name;
    float sky_intensity = 1.0f;
    float sky_rotation = 0.0f;

public:
    CommandParser() = default;

    bool parse(uint32_t argument_count, const char** argument_list);

    const std::string& get_scene_directory() const;
    const std::string& get_study_directory() const;

    std::optional<std::string> get_scene_file_name() const;
    float get_scene_scale() const;
    float get_scene_exposure() const;
    float get_scene_indirect_intensity() const;

    std::optional<std::string> get_sky_file_name() const;
    float get_sky_intensity() const;
    float get_sky_rotation() const;
};

#endif