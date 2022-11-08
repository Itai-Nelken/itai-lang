#include <iostream>
#include <string>
#include <filesystem>
#include <exception>
#include <vector>
#include <fstream>
#include <unistd.h>

namespace fs = std::filesystem;

struct ParseError final : public std::exception {
    ParseError(const char *what) {
        message = std::string(what);
    }

    const char *what() {
        return message.c_str();
    }

    std::string message;
};

struct Test {
    Test(const char *name, const char *file) : name(name), tester_output(""), output(""), path(file) {}

    std::string name, tester_output, output;
    fs::path path;
    bool compiler_failed = false, tester_failed = false;
    struct {
        bool should_fail = false;
        bool should_succeed = false;
    } options;
    int ilc_exit_status = 0;
};

std::string get_ilc_path() {
    std::string path;
    const char *env_path = getenv("TESTER_ILC_PATH");
    if(env_path) {
        path = env_path;
    } else {
        char *cwd = getcwd(NULL, 0);
        fs::path p(cwd);
        free(cwd);
        auto d = p.filename();
        if(d == "compiler") {
            p.append("build/ilc");
            path = std::string(p.c_str());
        } else if(d == "build") {
            p.append("ilc");
            path = std::string(p.c_str());
        } else if(d == "tester") {
            p.append("../build/ilc");
            path = std::string(p.c_str());
        } else {
            throw std::runtime_error("Unknown directory in get_ilc_path()");
        }
    }

    if(!fs::exists(path)) {
        throw std::runtime_error("Failed to find ilc");
    }
    return path;
}

class Tester {
public:
    Tester() {}
    ~Tester() {}
    void add_test(Test &t) {
        tests.push_back({t, tests.size()});
    }

    void run() {
        for(auto &[test, idx] : tests) {
            std::string expected;
            try {
                expected = parse_expected(test);
            } catch(ParseError &err) {
                test.tester_failed = true;
                test.tester_output = err.what();
                continue;
            }
            execute(test);
            check(test, expected);
            test_summary(test, idx);
        }
    }

    void summary() {
        std::cout << "\x1b[1mSummary:\x1b[0m\n";
        std::cout << total_passed_tests << '/' << tests.size() << " tests \x1b[32mpassed\x1b[0m.\n";
        std::cout << total_failed_tests << '/' << tests.size() << " tests \x1b[31mfailed\x1b[0m.\n";
    }

private:
    void test_summary(Test &test, int idx) {
        std::cout << "(" << idx + 1 << "/" << tests.size() << ") " << test.name << ": ";
            if(test.compiler_failed) {
                total_failed_tests++;
                std::cout << "\x1b[1;31mFailed\x1b[0m\n"
                          << "ilc exit status: " << test.ilc_exit_status << '\n'
                          << (test.tester_output.length() > 0 ? std::string("reason:\n" + test.tester_output + '\n') : std::string(""))
                          << (test.output.length() > 0 ? test.output : "")
                          << '\n';
            } else if(test.tester_failed) {
                total_failed_tests++;
                std::cout << "\x1b[1;31mTest parsing failed:\x1b[0m\n"
                          << "reason:\n"
                          << test.output
                          << '\n';
            } else {
                total_passed_tests++;
                std::cout << "\x1b[1;32mPassed\x1b[0m";
            }
            std::cout << '\n';
    }

    void execute(Test &test) {
        FILE *p = popen(std::string_view(get_ilc_path() + " " + test.path.c_str() + " 2>&1").data(), "r");
        char buffer[4096] = {0};
        while(fgets(buffer, sizeof(buffer), p) != nullptr) {
            test.output.append(buffer);
        }
        int status = pclose(p);
        test.ilc_exit_status = WEXITSTATUS(status);
    }

    void check(Test &test, std::string &expected) {
        // trim any escape sequences.
        std::string output;
        for(auto it = test.output.begin(); it != test.output.end(); ++it) {
            if(*it == '\x1b') {
                while(*it != 'm') { ++it; }
                // the 'm' is consumed by the loop.
            } else {
                output += *it;
            }
        }
        // trim a newline from the end of the output (if exists).
        if(output.length() > 0 && output.at(output.length() - 1) == '\n') {
            output.erase(output.length() - 1);
        }
        test.output = std::move(output);
        // no need to check if the compiler failed
        // if it shouldn't, as the output won't be the same
        // so it will fail in the else if below.
        if(test.options.should_fail) {
            if(test.ilc_exit_status == 0) {
                test.compiler_failed = true;
                test.tester_output = std::string("Test should have failed!");
            } else {
                // When checking that the expected error exists,
                // only the first line (the description of the error) is checked.
                test.tester_failed = test.output.substr(0, test.output.find_first_of('\n')) != expected;
            }
            return;
        } else if(test.options.should_succeed) {
            if(test.ilc_exit_status != 0) {
                test.compiler_failed = true;
                test.tester_output = std::string("Test should have succeded!");
            }
            return;
        } else if(test.output != expected) {
            test.compiler_failed = true;
        }
    }

    std::string parse_expected(Test &test) {
        std::ifstream file(test.path.c_str());
        std::string contents;
        std::getline(file, contents);
        file.close();

        size_t pos = 0;
        auto parse = [&contents, &pos](std::string expected) {
            if(contents.length() < pos || contents.substr(pos, expected.length()) != expected) {
                throw ParseError(std::string("Expected '" + expected + "'").c_str());
            }
            pos += expected.length();
        };
        auto skip_whitespace = [&contents, &pos]() {
            try {
                while(contents.at(pos) == ' ') {
                    pos++;
                }
            } catch(std::out_of_range&) {
                throw ParseError("Unexpected end of file!");
            }
        };

        // expect -> '///' 'expect' ((('error')? ':' <output>) | 'success')
        parse("///");
        skip_whitespace();
        parse("expect");
        skip_whitespace();
        if(contents.at(pos) == 'e') {
            parse("error:");
            test.options.should_fail = true;
        } else if(contents.at(pos) == 's') {
            parse("success");
            test.options.should_succeed = true;
            return "";
        } else {
            parse(":");
        }
        skip_whitespace();
        return contents.substr(pos);
    }

    int total_failed_tests = 0;
    int total_passed_tests = 0;
    std::vector<std::pair<Test, int>> tests;
};

int main(void) {
    Tester t;
    for(const auto &dir : fs::directory_iterator(".")) {
        if(dir.path().extension() == ".ilc") {
            Test test(dir.path().filename().stem().c_str(), dir.path().c_str());
            t.add_test(test);
        }
    }
    t.run();
    t.summary();
    return 0;
}
