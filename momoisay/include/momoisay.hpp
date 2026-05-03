#pragma once

#include <string_view>

#include "arhodigp.hpp"

namespace momoisay {

void printBuildType();

auto setVersion( [[maybe_unused]] int _key,
                 [[maybe_unused]] std::string_view _value,
                 [[maybe_unused]] arhodigp::state_t _state ) -> bool;

auto run( int _argumentCount, char** _argumentVector ) -> int;

} // namespace momoisay
