#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <cstring>
#include <charconv>
#include <cstdlib>
#include <cassert>

#include "vm/vm.hpp"
#include "vm/instructions_list.hpp"


constexpr std::size_t BUFFER_MAX = 256;

enum class TokenType {
    TopLevel,           // Waiting for meta construct
    Nothing,            // Waiting for instruction
    Instruction,        // instruction identifier
    ImmediateFloat,     // floating point number
    ImmediateInteger,   // integer number
    StackSelector,      // N:N:N:N (where N = 0..3)
    FunctionName,       // [a-zA-Z][a-zA-Z0-9_]*
    AwaitingOpening,    // Waiting for '{'
};

enum class Directive {
    None,
    Function,   // Parsing a function
    Data,       // Parsing floating point data
    Bytes,      // Parsing binary data
    Text,       // Parsing textual data
    Run,        // Parsing run command
};

bool eof (std::ifstream::int_type ch) {
    return ch == std::char_traits<char>::eof();
}

extern std::unordered_map<Instructions::InstructionType, OperandInfo> instruction_operands;
extern std::array<InstructionInfo, Instructions::COUNT> instruction_names;

const std::unordered_map<OperandType, const char*> operand_types {
    {OperandType::Value, "Float"},
    {OperandType::ByteOffset, "Byte Offset"},
    {OperandType::SlotId, "Slot"},
    {OperandType::Function, "Function"},
    {OperandType::NativeFunction, "Native Function"},
    {OperandType::StackSelector, "Stack Selector Mask"},
    {OperandType::ShadowBlendMask, "Shadow Blend Mask"},
    {OperandType::IValue8, "8Bit Integer"},
    {OperandType::IValue16, "16Bit Integer"},
};


std::size_t char_to_number_instructions(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';  // '0' maps to 0, '9' maps to 9
    } else if (c >= 'a' && c <= 'z') {
        return c - 'a' + 10;  // 'a' maps to 10, 'z' maps to 35
    } else if (c >= 'A' && c <= 'Z') {
        return c - 'A' + 10;  // Convert 'A'-'Z' to 'a'-'z', map to 10-35
    } else if (c == '_') {
        return 36;  // '_' maps to 36
    } else {
        return -1;  // Invalid character
    }
}

std::size_t char_to_number_directives(char c) {
    if (c >= 'a' && c <= 'z') {
        return c - 'a';  // 'a' maps to 10, 'z' maps to 35
    } else if (c >= 'A' && c <= 'Z') {
        return c - 'A';  // Convert 'A'-'Z' to 'a'-'z', map to 10-35
    } else {
        return -1;  // Invalid character
    }
}

enum class PatchType {
    ByteOffset,
    LoopEnd,
};

struct Patches {
    PatchType type;
    std::uint32_t offset;
};

using CharToIndexMapper = std::size_t(char);
template <typename Payload, CharToIndexMapper CharToIndex, std::size_t N>
struct Trie {
    using T = Trie<Payload, CharToIndex, N>;

    Payload* data;
    std::array<T*, N> children;

    static T* insert (T* root, const char* name, Payload* data) {
        if (root == nullptr) {
            root = new T{nullptr, {0}};
        }
        auto chars = name;
        T* node = root;
        char ch;
        while ((ch = *chars++) != 0) {
            auto index = CharToIndex(ch);
            auto temp = node->children[index];
            if (temp == nullptr) {
                temp = new T{nullptr, {0,}};
                node->children[index] = temp;
            }
            node = temp;
        }
        if (node->data != nullptr) {
            output("[ERROR] {} already exists in trie.\n", name);
            return root;
        }
        node->data = data;
        return root;
    }

    static Payload* find (T* root, const char* name) {
        auto n = name;
        char ch;
        while ((ch = *name++) != 0) {
            auto index = CharToIndex(ch);
            root = root->children[index];
            if (root == nullptr) {
                return nullptr;
            }
        }
        return root->data;
    }

    static T* next (T* node, char ch) {
        auto index = CharToIndex(ch);
        if (node == nullptr || index > N) {
            return nullptr;
        }
        return node->children[index];
    }
};

struct Instruction {
    OpCode opcode;
    OperandInfo* info;
};

struct DirectiveInfo {
    Directive directive;
};

struct FunctionsInfo {
    std::uint8_t index;
};

struct FunctionCallPatch {
    char name[255];
    std::size_t offset;
    int line;
    int col;
};


bool parse (std::ifstream& file, std::vector<OpCode>& bytecode,  std::array<const OpCode*, 256>& functions_table, std::uint8_t& num_functions) {
    TokenType token = TokenType::TopLevel;
    std::ifstream::int_type ch;

    // Track current source location
    int line = 1;
    int col = 0;

    // Track the current token
    char buffer[BUFFER_MAX] = {0};
    std::size_t buffer_idx = 0;
    buffer[0] = 0;

    // Track whether a float value already has a dot or not
    bool float_has_dot = false;

    // Track whether token has started yet
    bool in_token = false;

    // Track current directive
    Directive current_directive = Directive::None;

    // Track if a loop was started and a '{' is required after any operands
    bool loop_started = false;

    // Accumulate current mask
    std::uint32_t partial_bitmask = 0;
    int current_bitmask_index = 0;
    bool separator_required = false;
    bool shadow_stack_required = false;
    bool shadow_selector = false;

    // Track how many bytes we expect for curerntly parsed integer
    int integer_size = 0;
    // Track the amount of numeric values we are expecting
    int num_numbers = 0;

    // Build Trie of instructions
    using InstructionsTrie = Trie<Instruction, char_to_number_instructions, 37>;
    InstructionsTrie* instr_root = nullptr;
    for (const auto& info : instruction_names) {
        OperandInfo* operand = nullptr;
        auto it = instruction_operands.find(Instructions::InstructionType(info.opcode));
        if (it != instruction_operands.end()) {
            operand = &it->second;
        }
        instr_root = InstructionsTrie::insert(instr_root, info.name, new Instruction{info.opcode, operand});
    }
    // Insert aliases
    if (const auto istr = InstructionsTrie::find(instr_root, "skipz")) {
        instr_root = InstructionsTrie::insert(instr_root, "when", new Instruction{istr->opcode, istr->info});
    }
    if (const auto istr = InstructionsTrie::find(instr_root, "skipnz")) {
        instr_root = InstructionsTrie::insert(instr_root, "when_not", new Instruction{istr->opcode, istr->info});
    }

    // Build trie of directives
    using DirectivesTrie = Trie<DirectiveInfo, char_to_number_directives, 26>;
    DirectivesTrie* directives_root = nullptr;
    directives_root = DirectivesTrie::insert(directives_root, "func", new DirectiveInfo{Directive::Function});
    directives_root = DirectivesTrie::insert(directives_root, "data", new DirectiveInfo{Directive::Data});
    directives_root = DirectivesTrie::insert(directives_root, "bytes", new DirectiveInfo{Directive::Bytes});
    directives_root = DirectivesTrie::insert(directives_root, "text", new DirectiveInfo{Directive::Text});
    directives_root = DirectivesTrie::insert(directives_root, "run", new DirectiveInfo{Directive::Run});

    // Build trie of function names
    using FunctionsTrie = Trie<FunctionsInfo, char_to_number_instructions, 37>;
    FunctionsTrie* functions_root = nullptr;
    std::vector<std::size_t> functions_offsets;
    functions_offsets.reserve(32);

    // Track current match within Trie
    InstructionsTrie* current_match = instr_root;
    DirectivesTrie* current_directive_match = directives_root;
    FunctionsTrie* current_function_match = functions_root;
    std::vector<FunctionCallPatch> function_calls_to_patch;

    // Track which bytecode needs to be patched with byte offsets
    std::array<Patches, 32> pending_size_patches;
    std::size_t num_pending_size_patches = 0;

    // Reserve some starting amount of space for bytecode
    bytecode.reserve(128);
    // Generate preamble
    bytecode.push_back(Instructions::jump); /* [0] Jump to the start function. */
    bytecode.push_back(Instructions::nop);  /* [1] Shis instruction will be patched*/

    char start_function[255];
    start_function[0] = 0;

    // Parse assembly code and generate bytecode
    do {
        ch = file.get();
        if (ch == '\n') {
            line++;
            col = 0;
        } else {
           col++;
        }
        bool whitespace = (ch == ' ' || ch == '\n' || ch == ';' || eof(ch));
        switch (token) {
            case TokenType::TopLevel: {
                if (current_directive == Directive::None) {
                    // Parsing directive type
                    if (!in_token && !whitespace) {
                        in_token = true;
                        buffer_idx = 0;
                        current_directive_match = directives_root;
                    }
                    if (in_token) {
                        bool error = false;
                        if (!whitespace) {
                            if (ch == ':') {
                                if (current_directive_match == nullptr || current_directive_match->data == nullptr) {
                                    buffer[buffer_idx++] = ch;
                                    error = true;
                                } else {
                                    auto data = current_directive_match->data;
                                    current_directive = data->directive;
                                    buffer_idx = 0;
                                    in_token = false;

                                }
                            } else {
                                buffer[buffer_idx++] = ch;
                                current_directive_match = DirectivesTrie::next(current_directive_match, ch);
                                if (current_directive_match == nullptr) {
                                    error = true;
                                }
                            }
                        } else {
                            buffer[buffer_idx++] = ch;
                            error = true;
                        }
                        if (error) {
                            buffer[buffer_idx] = 0;
                            output("[ERROR {}:{}] Expected directive (func, data, bytes, text, run), got: {}\n", line, col, (const char*)buffer);
                            return false;
                        }
                    }
                } else {
                    // Parsing directive name
                    if (!in_token &&
                        ((ch >= 'a' && ch <= 'z') ||
                         (ch >= 'A' && ch <= 'Z'))) {
                        in_token = true;
                        buffer_idx = 0;
                    }
                    if (in_token) {
                        if (whitespace) {
                            in_token = false;
                            buffer[buffer_idx] = 0;
                            buffer_idx = 0;
                            token = TokenType::AwaitingOpening;
                            switch (current_directive) {
                                case Directive::Function: {
                                    functions_root = FunctionsTrie::insert(functions_root, buffer, new FunctionsInfo{ std::uint8_t(functions_offsets.size()) });
                                    functions_offsets.push_back(bytecode.size());
                                    current_function_match = functions_root;
                                    break;
                                }
                                case Directive::Run: {
                                    std::strcpy(start_function, buffer);
                                    token = TokenType::TopLevel;
                                    current_directive = Directive::None;
                                    break;
                                }
                                case Directive::Bytes:
                                case Directive::Data: {
                                    break;
                                }
                                default: {
                                    output("Unimplemented directive.\n");
                                    break;
                                }
                            };
                        } else if ((ch >= 'a' && ch <= 'z') ||
                                   (ch >= 'A' && ch <= 'Z') ||
                                   (ch >= '0' && ch <= '9') ||
                                   ch == '_') {
                            buffer[buffer_idx++] = ch;
                        } else {
                            buffer[buffer_idx] = 0;
                            output("[ERROR {}:{}] Expected directive name, got: {}\n", line, col, (const char*)buffer);
                            return false;
                        }
                    }
                }
                break;
            }
            case TokenType::Nothing: {
                if (ch == '}') {
                    if (num_pending_size_patches > 0) {
                        if (bytecode.size() > 1) {
                            auto patch = pending_size_patches[--num_pending_size_patches];
                            if (patch.type == PatchType::ByteOffset) {
                                auto offset = patch.offset;
                                std::uint16_t size = bytecode.size() - (offset + 2);
                                // Patch size
                                bytecode[offset] = size & 0xff;
                                bytecode[offset + 1] = (size >> 8) & 0xff;
                            } else if (patch.type == PatchType::LoopEnd) {
                                bytecode.push_back(Instructions::end_loop);
                            }
                        }
                    } else {
                        current_directive = Directive::None;
                        token = TokenType::TopLevel;
                        in_token = false;
                    }
                } else if (!whitespace) {
                    bool error = false;
                    if (current_directive == Directive::Data) {
                        if ((ch >= '0' && ch <= '9') || (buffer_idx == 0 && ch == '-') || (!float_has_dot && ch == '.')) {
                            token = TokenType::ImmediateFloat;
                            num_numbers = 1;
                        } else {
                            error = true;
                        }
                    } else if (current_directive == Directive::Bytes) {
                        if ((ch >= '0' && ch <= '9') || (buffer_idx == 0 && ch == '-')) {
                            token = TokenType::ImmediateInteger;
                            integer_size = 1;
                            num_numbers = 1;
                        } else {
                            error = true;
                        }
                    } else if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')) {
                        current_match = InstructionsTrie::next(instr_root, ch);
                        token = TokenType::Instruction;
                    }
                    if (error) {
                        output("[ERROR {}:{}] Unexpected symbol: {}\n", line, col, char(ch));
                        return false;
                    } else {
                        buffer[0] = ch;
                        buffer_idx = 1;
                    }
                }
                break;
            }
            case TokenType::Instruction: {
                if (!whitespace) {
                    current_match = InstructionsTrie::next(current_match, ch);
                    buffer[buffer_idx++] = ch;
                } else {
                    buffer[buffer_idx] = 0;
                    buffer_idx = 0;
                    if (current_match == nullptr || current_match->data == nullptr) {
                        output("[ERROR {}:{}] Invalid identifier: {}\n", line, col, (const char*)buffer);
                        return false;
                    }
                    auto leaf = current_match->data;
                    auto opcode = leaf->opcode;
                    bytecode.push_back(opcode);
                    token = TokenType::Nothing;
                    auto operands = leaf->info;
                    num_numbers = 0;
                    integer_size = 0;
                    in_token = false;
                    if (operands != nullptr) {
                        num_numbers = operands->count;
                        token = TokenType::ImmediateInteger;
                        switch (operands->type) {
                            case OperandType::Value: {
                                token = TokenType::ImmediateFloat;
                                float_has_dot = false;
                                break;
                            }
                            case OperandType::ByteOffset: {
                                token = TokenType::AwaitingOpening;
                                pending_size_patches[num_pending_size_patches++] = {
                                    PatchType::ByteOffset,
                                    std::uint32_t(bytecode.size()),
                                };
                                bytecode.push_back(0);
                                bytecode.push_back(0);
                                break;
                            }
                            case OperandType::SlotId: {
                                integer_size = 1;
                                break;
                            }
                            case OperandType::Function: {
                                token = TokenType::FunctionName;
                                break;
                            }
                            case OperandType::NativeFunction: {
                                integer_size = 2;
                                break;
                            }
                            case OperandType::StackSelector: {
                                token = TokenType::StackSelector;
                                shadow_selector = false;
                                break;
                            }
                            case OperandType::ShadowBlendMask: {
                                token = TokenType::StackSelector;
                                shadow_selector = true;
                                break;
                            }
                            case OperandType::IValue8: {
                                integer_size = 1;
                                break;
                            }
                            case OperandType::IValue16: {
                                integer_size = 2;
                                break;
                            }
                            // default: {
                            //     __builtin_unreachable();
                            // }
                        }
                    }
                    // Loop starts need to track loop end
                    if (opcode == Instructions::set_loop ||
                        opcode == Instructions::set_loop_a ||
                        opcode == Instructions::set_loop_slot) {
                        pending_size_patches[num_pending_size_patches++] = {
                            PatchType::LoopEnd,
                            std::uint32_t(bytecode.size() + (integer_size * num_numbers)),
                        };
                        loop_started = true;
                        if (token == TokenType::Nothing) {
                            token = TokenType::AwaitingOpening;
                        }
                    }
                }
                break;
            }
            case TokenType::ImmediateInteger: {
                if ((ch >= '0' && ch <= '9') || (buffer_idx == 0 && ch == '-')) {
                    buffer[buffer_idx++] = ch;
                } else if (whitespace) {
                    buffer[buffer_idx] = 0;
                    I32 i32;
                    auto [p, error] = std::from_chars(buffer, buffer + buffer_idx, i32);
                    U32 i = std::bit_cast<U32>(i32);
                    assert(error == std::errc());
                    do {
                        bytecode.push_back(i & 0xff);
                        i >>= 8;
                    } while (--integer_size > 0);
                    buffer_idx = 0;
                    if (--num_numbers == 0) {
                        token = TokenType::Nothing;
                        if (loop_started) {
                            token = TokenType::AwaitingOpening;
                        }
                    }
                } else {
                    output("[ERROR {}:{}] Invalid symbol in number: {}\n", line, col, char(ch));
                    return false;
                }
                break;
            }
            case TokenType::ImmediateFloat: {
                if ((ch >= '0' && ch <= '9') || (buffer_idx == 0 && ch == '-') || (!float_has_dot && ch == '.')) {
                    buffer[buffer_idx++] = ch;
                    if (ch == '.') {
                        float_has_dot = true;
                    }
                } else if (whitespace) {
                    buffer[buffer_idx] = 0;
                    float f = std::strtof(buffer, nullptr);
                    auto bytes = std::bit_cast<std::array<uint8_t, 4>>(f);
                    bytecode.push_back(bytes[0]);
                    bytecode.push_back(bytes[1]);
                    bytecode.push_back(bytes[2]);
                    bytecode.push_back(bytes[3]);
                    buffer_idx = 0;
                    if (--num_numbers == 0) {
                        token = TokenType::Nothing;
                        if (loop_started) {
                            token = TokenType::AwaitingOpening;
                        }
                    }
                } else if (ch == '.') {
                    output("[ERROR {}:{}] Number cannot have two decimal points!\n", line, col);
                    return false;
                } else {
                    output("[ERROR {}:{}] Invalid symbol in number: {}\n", line, col, char(ch));
                    return false;
                }
                break;
            }
            case TokenType::AwaitingOpening: {
                if (ch == '{') {
                    token = TokenType::Nothing;
                    loop_started = false;
                } else if (!whitespace) {
                    output("[ERROR {}:{}] Invalid symbol: '{}' expecing '{{'\n", line, col, char(ch));
                    return false;
                }
                break;
            }
            case TokenType::StackSelector: {
                buffer[buffer_idx++] = ch;
                bool invalid_symbol = false;
                if (!in_token) {
                    separator_required = false;
                    shadow_stack_required = shadow_selector;
                    current_bitmask_index = 0;
                    partial_bitmask = 0;
                }
                if (current_bitmask_index == (shadow_selector ? 8 : 4)) {
                    if (whitespace) {
                        if (shadow_selector) {
                            bytecode.push_back(partial_bitmask & 0xff);
                            partial_bitmask >>= 8;
                        }
                        bytecode.push_back(partial_bitmask & 0xff);
                        token = TokenType::Nothing;
                        buffer_idx = 0;
                    } else {
                        invalid_symbol = true;
                    }
                } else if (separator_required) {
                    if (ch == ':') {
                        separator_required = false;
                        shadow_stack_required = shadow_selector;
                    } else {
                        invalid_symbol = true;
                    }
                } else  if (ch >= '0' && ((shadow_stack_required && ch <= '1') || (!shadow_stack_required && ch <= '3'))) {
                    in_token = true;
                    partial_bitmask <<= 2;
                    partial_bitmask |= (ch - '0');
                    current_bitmask_index++;
                    if (!shadow_stack_required) {
                        separator_required = true;
                    }
                    shadow_stack_required = false;
                } else if (in_token || !whitespace) {
                    invalid_symbol = true;
                }
                if (invalid_symbol) {
                    if (buffer[buffer_idx-1] == '\n') {
                        buffer[buffer_idx-1] = '\\';
                        buffer[buffer_idx++] = 'n';
                    }
                    buffer[buffer_idx] = 0;

                   if (shadow_selector) {
                       output("[ERROR {}:{}] Invalid symbol: '{}' expecting stack selector 'SN:SN:SN:SN' (S ∈ {{0,1}}, N ∈ {{0,1,2,3}})\n", line, col, (const char*)buffer);
                   } else {
                       output("[ERROR {}:{}] Invalid symbol: '{}' expecting stack selector 'N:N:N:N' (N ∈ {{0,1,2,3}})\n", line, col, (const char*)buffer);
                   }
                    return false;
                }
                break;
            }
            case TokenType::FunctionName: {
                bool error = false;
                if (!in_token &&
                    ((ch >= 'a' && ch <= 'z') ||
                     (ch >= 'A' && ch <= 'Z'))) {
                    in_token = true;
                    buffer_idx = 0;
                }
                if (in_token) {
                    if ((ch >= 'a' && ch <= 'z') ||
                        (ch >= 'A' && ch <= 'Z') ||
                        (ch >= '0' && ch <= '9') ||
                        ch == '_') {
                        buffer[buffer_idx++] = ch;
                    } else if (whitespace) {
                        buffer[buffer_idx] = 0;
                        buffer_idx = 0;
                        auto func = FunctionsTrie::find(functions_root, buffer);
                        if (func != nullptr) {
                            // Add function index to bytecode
                            bytecode.push_back(func->index);
                        } else {
                            // Function wasn't found, but it might be out of order
                            // so add a dummy value and make note to patch it later
                            function_calls_to_patch.push_back({
                                .line = line,
                                .col = col,
                            });
                            auto& patch = function_calls_to_patch.back();
                            std::strcpy(patch.name, buffer);
                            patch.offset = bytecode.size();
                            // Add dummy value, to be overwritten
                            bytecode.push_back(0xff);
                        }
                        token = TokenType::Nothing;
                    } else {
                        error = true;
                    }
                } else if (!whitespace) {
                    error = true;
                }
                if (error) {
                    buffer[buffer_idx] = 0;
                    output("[ERROR {}:{}] Expecting function name, got '{}'\n", line, col, (const char*)buffer);
                    return false;
                }
                break;
            }
        };
        if (ch == ';') {
            do {
                ch = file.get();
                if (eof(ch) || ch == '\n') {
                    break;
                }
            } while (true);
            line++;
            col = 0;
        }
    } while (!eof(ch));
    if (num_pending_size_patches > 0) {
        output("[ERROR {}:{}] Invalid end of input, expecting '}}'.\n", line, col);
        return false;
    }
    if (start_function[0] == 0) {
        output("[ERROR] No `RUN: <function>` directive found.\n");
        return false;
    }
    auto run = FunctionsTrie::find(functions_root, start_function);
    if (run == nullptr) {
        output("[ERROR] RUN functions '{}' not found.\n", (const char*)start_function);
        return false;
    }
    // Patch the run function.
    bytecode[1] = run->index;

   // Patch any missing functions
   for (const auto& patch : function_calls_to_patch) {
       auto func = FunctionsTrie::find(functions_root, patch.name);
       if (func == nullptr) {
           output("[ERROR {}:{}] Function '{}' does not exist.\n", patch.line, patch.col, (const char*)patch.name);
           return false;
       }
       bytecode[patch.offset] = func->index;
   }
   // Create functions table
   num_functions = functions_offsets.size();
   for (auto idx = 0; idx < num_functions; ++idx) {
       functions_table[idx] =  bytecode.data() + functions_offsets[idx];
   }

    // Make sure bytecode always ends in an end isntruction
    return true;
}

int execute (const char* filename, bool output_bytecode) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        output("Could not open file: {}\n", filename);
        return -1;
    }

    std::vector<OpCode> bytecode;
    std::array<const OpCode*, 256> functions;
    std::uint8_t num_functions = 0;
    if (!parse(file, bytecode, functions, num_functions)) {
        return -1;
    }
    output("Size of bytecode: {} bytes.\n", bytecode.size());
    if (output_bytecode) {
        for (auto byte : bytecode) {
            output("{:02x} ", byte);
        }
        output("\n");
        return 0;
    }

    output("Executing:\n");
    auto env = env::create();
    corelib_install(env);
    Context* ctx = ctx::create(env, bytecode.data(), 32, {functions.data(), num_functions});
    auto result = ctx::execute(ctx);
    ctx::debug(ctx, result.SP);
    ctx::output_slots(ctx);
    ctx::destroy(ctx);
    if (result.return_code == ReturnValue::Ok) {
        output("Ok.\n");
    } else {
        output("Error {:x}.\n", (int)result.return_code);
    }
    env::destroy(env);
    return 0;
}

int main (int argc, char* argv[]) {
    if (argc == 2) {
        if (std::strcmp(argv[1], "--list") == 0) {
            output("Instructions:\n");
            unsigned n = 0;
            for (const auto& info : instruction_names) {
                output("{:02x} {}\n", info.opcode, info.name);
            }
            if (n != 0) {
                output("\n");
            }
            output("\n");
            return 0;
        }

        return execute(argv[1], false);

    } else if (argc == 3 && std::strcmp(argv[1], "--bytecode") == 0) {
        return execute(argv[2], true);
    }
    output("Usage:\n");
    output(" casm <file>\n");
    output(" casm --bytecode <file>\n");
    output(" casm --list\n");
    return -1;
}
