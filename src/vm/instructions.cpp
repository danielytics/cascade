#include <array>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <map>
#include <string>

#include "types.hpp"
#include "instructions_list.hpp"

namespace InstructionsImpls {
    #define INSTRUCTION_DECLS
    #include "instructions.hpp"
}

std::array<InstructionFunction*, Instructions::COUNT> instructions_table = {
    #define INSTRUCTION_FUNCPTRS
    #include "instructions.hpp"
};

std::unordered_map<Instructions::InstructionType, OperandInfo> instruction_operands = {
    #define INSTRUCTION_OPERAND_INFO
    #include "instructions.hpp"
};

;
std::array<InstructionInfo, Instructions::COUNT> instruction_names = {
    #define INSTRUCTION_NAMES
    #include "instructions.hpp"
};

////////////////////////////////////////////////////////////////////////////////
#define INSTRUCTION_IMPLS
#include "instructions.hpp"
#undef INSTRUCTION
////////////////////////////////////////////////////////////////////////////////
