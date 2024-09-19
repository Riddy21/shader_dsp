#include "catch2/catch_all.hpp"
#include <vector>

#include "audio_parameter.h"
#include "audio_uniform_parameters.h"
#include "audio_texture2d_parameter.h"

TEST_CASE("MakeUniqueTest") {
    std::vector<std::unique_ptr<AudioParameter>> audio_parameters;

    auto audio_parameter = std::make_unique<AudioTexture2DParameter>("audio_parameter",
                                                                      AudioParameter::ConnectionType::INPUT,
                                                                      512,
                                                                      512);

    audio_parameters.push_back(std::move(audio_parameter));

    REQUIRE(audio_parameters.size() == 1);
    float * value = new float[512*512]();
    REQUIRE(audio_parameters[0]->set_value(value));
    // Cast to a 2D parameter
    auto audio_texture = dynamic_cast<AudioTexture2DParameter *>(audio_parameters[0].get())->get_texture();
    REQUIRE(audio_texture == 0);

    auto time_parameter = std::make_unique<AudioIntParameter>("time",
                                                              AudioParameter::ConnectionType::INPUT);
    time_parameter->set_value(new int(19));

    audio_parameters.push_back(std::move(time_parameter));
}