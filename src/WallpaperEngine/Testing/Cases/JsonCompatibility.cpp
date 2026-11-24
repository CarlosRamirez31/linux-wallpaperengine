#include <catch2/catch_test_macros.hpp>
#include "WallpaperEngine/Data/Utils/JsonCompatibility.h"

TEST_CASE ("Effect JSON trailing commas preserve quoted text") {
    using WallpaperEngine::Data::Utils::withoutTrailingJsonCommas;
    CHECK (withoutTrailingJsonCommas (R"([1,2, ])") == R"([1,2  ])");
    CHECK (withoutTrailingJsonCommas (R"({"a":[1,],})") == R"({"a":[1 ] })");
    CHECK (withoutTrailingJsonCommas (R"([",]",])") == R"([",]" ])");
    CHECK (withoutTrailingJsonCommas (R"([1,2])") == R"([1,2])");
}
