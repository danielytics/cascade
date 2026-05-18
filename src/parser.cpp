#include <cstdint>
#include <cstring>
#include <fstream>
#include <cstdio>
#include <format>
#include <vector>
#include <format>
#include <cstdio>
#include <map>
#include <ranges>


template <typename... Args>
void output(std::format_string<Args...> format, Args&&... args) {
    std::fputs(std::format(format, std::forward<Args>(args)...).c_str(), stdout);
}

enum class TokenType {
    Root,
    Pipeline,
    ListLiteral,
    IntegerLiteral,
};

class TokenStack {
public:
    TokenStack () {
        m_stack.reserve(16);
        m_stack.push_back(TokenType::Root);
    }

    void push (TokenType type) {
        m_stack.push_back(type);
    }

    TokenType pop () {
        auto prev = m_stack.back();
        m_stack.pop_back();
        return prev;
    }

    TokenType cur () {
        return m_stack.back();
    }

    void print (TokenType cur, char sep=0) {
        output("{}", s_names[cur]);
        if (sep) {
            std::putc(sep, stdout);
        }
    }

    void print () {
        print(cur());
        output("\n");
    }

    void dump () {
        std::fputs("[ ", stdout);
        for (auto tok : std::views::reverse(m_stack)) {
            print(tok, ' ');
        }
        std::puts("]");
    }

private:
    std::vector<TokenType> m_stack;

    inline static std::map<TokenType, const char*> s_names = {
        // {TokenType::None, "NONE"},
        {TokenType::IntegerLiteral, "INT_LIT"},
        {TokenType::ListLiteral, "LIST_LIT"},
    };
};

class TokenString {
public:

    void push (char ch) {
        m_buf[m_size++] = ch;
    }

    void reset() {
        m_size = 0;
    }

    const char* gets() {
        m_buf[m_size] = 0;
        return m_buf.data();
    }

    std::uint8_t size() const {
        return m_size;
    }

 private:
    std::array<char, 255> m_buf;
    std::uint8_t m_size = 0;
};

class Location {
public:
    Location(const char* filename) : m_filename{filename}  {
        m_buf[0][0] = 0;
        m_buf[1][0] = 0;
    }

    void next(char ch) {
        m_buf[m_idx][m_row++] = ch;
    }

    void endl() {
        ++m_line;
        m_buf[m_idx][m_row] = 0;
        m_row = 0;
        m_idx = 1 - m_idx;
    }

    const char* gets() {
        m_buf[m_idx][m_row] = 0;
        return m_buf[m_idx].data();
    }

    const char* prev() const {
        return m_buf[1 - m_idx].data();
    }

    std::size_t line() const {
        return m_line;
    }

    std::size_t row() const {
        return m_row;
    }

    void mark_token() {
        m_tok_start = m_row;
    }

    void dump() {
        char padding[] = "        ";
        auto prefix1 = std::format("{}", line()-1);
        auto prefix2 = std::format("{}", line());
        padding[prefix2.size() - prefix1.size()] = 0;
        output("{}:{}:{}\n", m_filename, line(), row());
        output("{}{} | {}\n", prefix1, (char*)padding, prev());
        output("{} | {}\n ", prefix2, gets());
        for (auto i=0; i<prefix2.size() + 3; i++) {
            std::putc(' ', stdout);
        }
        for (auto i=0; i < m_tok_start; i++) {
            std::putc(' ', stdout);
        }
        for (auto i=m_tok_start; i < m_row; i++) {
            std::putc('^', stdout);
        }
        std::putc('\n', stdout);
    }

public:
    std::array<char, 1024> m_buf[2];
    const char* m_filename;
    std::size_t m_line = 1;
    std::uint16_t m_row = 0;
    std::uint16_t m_tok_start = 0;
    std::uint8_t m_idx = 0;
};

bool processCharacter(TokenStack& tokens, TokenString& token, std::ifstream::int_type ch) {
    switch (tokens.cur()) {
        case TokenType::Root: {
            switch (ch) {
                case '[': {
                    output("START_LIST\n");
                    tokens.push(TokenType::ListLiteral);
                }
                default: {

                    break;
                };
            }
            break;
        }
        case TokenType::ListLiteral: {
            if (ch == ']') {
                  output("END_LIST\n");
                  tokens.pop();
                  return false;
              } else if (ch >= '0' && ch <= '9') {
                  output("START_INT\n");
                  tokens.push(TokenType::IntegerLiteral);
                  return false;
              }
            break;
        }
        case TokenType::IntegerLiteral: {
            if (ch >= '0' && ch <= '9') {
                token.push(ch);
            } else {
                std::int64_t number = std::atoi(token.gets());
                token.reset();
                output("END_INT {}\n", number);
                tokens.pop();
                return false;
            }
            break;
        }
        case TokenType::Pipeline: {
            break;
        }
    };
    return true;
}

int parser_main (int argc, char* argv[]) {
    if (argc != 2) {
        output("Pass one commandline arg: the file to parse.\n");
        return -1;
    }

    std::ifstream file(argv[1]);
    if (!file.is_open()) {
        output("Could not open file: {}\n", argv[1]);
        return -1;
    }

    std::ifstream::int_type ch;
    TokenStack tokens;
    TokenString token;
    Location loc{argv[1]};
    bool new_token = true;

    bool consumed;
    while ((ch = file.get()) != std::char_traits<char>::eof()) {
        do {
            if ((consumed = processCharacter(tokens, token, ch)) == false) {
                new_token = true;
                loc.mark_token();
            } else if (new_token && token.size() == 1) {
                new_token = false;
                loc.mark_token();
            }
            if (ch == '\n') {
                new_token = true;
                loc.dump();
                loc.endl();
                continue;
            } else if (consumed) {
                loc.next(ch);
            }
        } while (!consumed);
    }

    return 0;
}
