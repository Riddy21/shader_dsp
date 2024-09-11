#include "catch2/catch_all.hpp"
#include <vector>

#include "audio_parameter.h"
#include "audio_texture2d_parameter.h"

TEST_CASE("MakeUniqueTest") {
    std::vector<std::unique_ptr<AudioParameter>> audio_parameters;

    AudioTexture2DParameter audio_parameter = AudioTexture2DParameter("audio_parameter",
                                                                      AudioParameter::ConnectionType::INPUT,
                                                                      512,
                                                                      512);

    audio_parameters.push_back(std::make_unique<AudioTexture2DParameter>(audio_parameter));

    REQUIRE(audio_parameters.size() == 1);
    float * value = new float[512*512]();
    audio_parameters[0]->set_value(value);
    REQUIRE(dynamic_cast<AudioTexture2DParameter*>(audio_parameters[0].get())->get_texture() == audio_parameter.get_texture());
}