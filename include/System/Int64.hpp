//
// Created by robertvokac on 6/7/25.
//

#pragma once
#include "SharpRuntime/SharpRuntimeHelper.hpp"

namespace System {


class Int64 {
public:

    static constexpr SharpRuntime::longcs MaxValue = SharpRuntime::LONGCS_MAX;
    static constexpr SharpRuntime::longcs MinValue = SharpRuntime::LONGCS_MIN;
};
}

