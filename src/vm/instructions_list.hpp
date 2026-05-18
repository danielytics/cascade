#pragma once

namespace Instructions {
    enum InstructionType {
        _DUMMY=-1,
        #define INSTRUCTION_ENUMS
        #include "instructions.hpp"
        COUNT
    };
}
