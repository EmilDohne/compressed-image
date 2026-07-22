// Include doctest and configure our own main function
#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"


#include <filesystem>
#include <vector>
#include <memory>
#include <format>
#include <mutex>

#define _COMPRESSED_PROFILE 1
#include "compressed/detail/scoped_timer.h"

struct test_log_reporter : public doctest::ConsoleReporter
{
    const doctest::TestCaseData* tc = nullptr;
    std::mutex mutex;

    // Track both test cases and subcases for the final report summary
    std::vector<std::pair<std::string, double>> test_durations;

    // Flat raw structure captured during test runs
    struct subcase_report
    {
        std::string name;
        double seconds = 0.0;
        size_t depth = 0;
    };

    std::vector<subcase_report> buffered_subcases;

    // Reconstructed tree structure used for grouped reporting
    struct subcase_node
    {
        std::string name;
        double total_seconds = 0.0;
        int call_count = 0;
        std::vector<subcase_node> children;
    };

    test_log_reporter(const doctest::ContextOptions& opt) : doctest::ConsoleReporter(opt)
    {
    }

    void test_case_start(const doctest::TestCaseData& in) override
    {
        std::lock_guard<std::mutex> lock(mutex);
        tc = &in;
        buffered_subcases.clear();
        active_subcase_stack.clear();
    }

    void test_case_reenter(const doctest::TestCaseData& in) override
    {
        std::lock_guard<std::mutex> lock(mutex);
        tc = &in;
        active_subcase_stack.clear();
    }

    void test_case_end(const doctest::CurrentTestCaseStats& in) override
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!tc) return;

        // Track the root parent test case
        test_durations.push_back({std::string(tc->m_name), in.seconds});

        constexpr int column_width = 120;
        constexpr const char* blue = "\033[36m";
        constexpr const char* red = "\033[31m";
        constexpr const char* gold = "\033[33m";
        constexpr const char* reset = "\033[0m";
        constexpr const char* dim_dot = "\033[90m";

        if (!in.testCaseSuccess)
        {
            std::string reason = "Assertion failure";
            if (in.failure_flags & doctest::TestCaseFailureReason::Exception) reason = "Unhandled Exception";
            else if (in.failure_flags & doctest::TestCaseFailureReason::Crash) reason = "Crash";
            else if (in.failure_flags & doctest::TestCaseFailureReason::ShouldHaveFailedButDidnt)
                reason = "Expected failure missing";

            std::cout
                << gold << "\n===============================================================================\n"
                << blue << "[doctest]" << reset << " Failure in test case: " << red << tc->m_name << reset
                << " | Reason: " << gold << reason << "\n"
                << "===============================================================================\n" << reset
                << std::endl;
        }
        else
        {
            std::string time_str = format_duration_clean(in.seconds);
            std::string time_color = get_duration_color(in.seconds);
            std::string right_side = std::format("{}{} ok{}", time_color, time_str, reset);
            std::string right_side_plain = std::format("{} ok", time_str);

            size_t max_name_len = column_width - std::string("[doctest] ").length() - right_side_plain.length() - 3;
            std::string display_name = tc->m_name;
            if (display_name.length() > max_name_len)
            {
                display_name = truncate_string(display_name, max_name_len);
            }

            std::string left_side = std::format("[doctest] {}", display_name);
            int fill_dots = column_width - static_cast<int>(left_side.length() + right_side_plain.length());
            if (fill_dots < 3) fill_dots = 3;

            std::cout << left_side << dim_dot << std::string(fill_dots, '.') << reset << right_side << std::endl;
        }

        // 1. Reconstruct hierarchical tree from raw flat loop history
        std::vector<subcase_node> root_nodes;
        std::vector<std::string> current_path_names;

        for (const auto& sub : buffered_subcases)
        {
            if (current_path_names.size() >= sub.depth)
            {
                current_path_names.resize(sub.depth - 1);
            }
            current_path_names.push_back(sub.name);

            std::vector<subcase_node>* current_level = &root_nodes;
            for (size_t d = 0; d < current_path_names.size(); ++d)
            {
                const std::string& name = current_path_names[d];
                auto it = std::find_if(
                    current_level->begin(),
                    current_level->end(),
                    [&name](const subcase_node& node) { return node.name == name; }
                );

                if (it == current_level->end())
                {
                    current_level->push_back(subcase_node{name, 0.0, 0, {}});
                    it = current_level->end() - 1;
                }

                if (d == current_path_names.size() - 1)
                {
                    it->total_seconds += sub.seconds;
                    it->call_count++;
                }
                current_level = &it->children;
            }
        }

        // 2. Extract and recursively add subcase profiles into final summary metrics
        collect_subcase_durations(root_nodes, std::string(tc->m_name));

        // 3. Print the collapsed tree down the console pipe using clean ASCII configurations
        print_subcase_tree(root_nodes, "", column_width);

        buffered_subcases.clear();
        tc = nullptr;
    }

    void subcase_start(const doctest::SubcaseSignature& in) override
    {
        std::lock_guard<std::mutex> lock(mutex);

        size_t idx = buffered_subcases.size();
        buffered_subcases.push_back({in.m_name.c_str(), 0.0, active_subcase_stack.size() + 1});
        active_subcase_stack.push_back({in.m_name.c_str(), std::chrono::high_resolution_clock::now(), idx});
    }

    void subcase_end() override
    {
        auto end_time = std::chrono::high_resolution_clock::now();
        if (active_subcase_stack.empty()) return;

        auto top = active_subcase_stack.back();
        active_subcase_stack.pop_back();

        std::chrono::duration<double> elapsed = end_time - top.start_time;

        std::lock_guard<std::mutex> lock(mutex);
        if (top.report_index < buffered_subcases.size())
        {
            buffered_subcases[top.report_index].seconds = elapsed.count();
        }
    }

    void log_assert(const doctest::AssertData& in) override
    {
        if (!in.m_failed) return;

        std::lock_guard<std::mutex> lock(mutex);
        constexpr const char* red = "\033[31m";
        constexpr const char* gold = "\033[33m";
        constexpr const char* reset = "\033[0m";

        std::cout << "\n"
            << red << "  `-- ASSERTION FAILURE:\n" << reset
            << "      " << gold << "File:   " << reset << in.m_file << ":" << in.m_line << "\n"
            << "      " << gold << "Expr:   " << reset << in.m_expr << "\n"
            << "      " << gold << "Decomp: " << red << in.m_decomp << reset << "\n"
            << std::endl;
    }

    void test_run_end(const doctest::TestRunStats& /*in*/) override
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (test_durations.empty()) return;

        constexpr const char* gold = "\033[33m";
        constexpr const char* reset = "\033[0m";

        std::sort(
            test_durations.begin(),
            test_durations.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; }
        );

        std::cout << "\n" << gold << "Slowest test paths & subcases (Top 10):" << reset << "\n";

        size_t display_count = std::min(size_t(10), test_durations.size());
        for (size_t i = 0; i < display_count; ++i)
        {
            const auto& [name, seconds] = test_durations[i];
            std::string time_str = format_duration_clean(seconds, true);
            std::string time_color = get_duration_color(seconds);

            std::cout << std::format("  [{}{}{}] {}\n", time_color, time_str, reset, name);
        }
        std::cout << std::endl;
    }

    void report_query(const doctest::QueryData&) override
    {
    }

    void test_run_start() override
    {
    }

    void test_case_exception(const doctest::TestCaseException&) override
    {
    }

    void log_message(const doctest::MessageData&) override
    {
    }

    void test_case_skipped(const doctest::TestCaseData&) override
    {
    }

private:
    struct subcase_timing
    {
        std::string name;
        std::chrono::high_resolution_clock::time_point start_time;
        size_t report_index;
    };

    inline static thread_local std::vector<subcase_timing> active_subcase_stack;

    void collect_subcase_durations(const std::vector<subcase_node>& nodes, const std::string& parent_path)
    {
        for (const auto& node : nodes)
        {
            std::string current_path = parent_path + " > " + node.name;
            std::string report_name = current_path;

            test_durations.push_back({report_name, node.total_seconds});

            if (!node.children.empty())
            {
                collect_subcase_durations(node.children, current_path);
            }
        }
    }

    void print_subcase_tree(const std::vector<subcase_node>& nodes, const std::string& prefix, int column_width)
    {
        constexpr const char* reset = "\033[0m";
        constexpr const char* dim_dot = "\033[90m";

        for (size_t i = 0; i < nodes.size(); ++i)
        {
            const auto& node = nodes[i];
            bool is_last = (i == nodes.size() - 1);
            std::string branch = is_last ? "`-- " : "|-- ";

            std::string display_name = node.name;

            std::string sub_time_str = format_duration_clean(node.total_seconds);
            std::string sub_time_color = get_duration_color(node.total_seconds);
            std::string right_side = std::format("{}{} ok{}", sub_time_color, sub_time_str, reset);
            std::string right_side_plain = std::format("{} ok", sub_time_str);

            size_t left_base_len = 10 + prefix.length() + branch.length();
            size_t space_budget = column_width - left_base_len - right_side_plain.length() - 3;

            if (display_name.length() > space_budget)
            {
                display_name = truncate_string(display_name, space_budget);
            }

            std::string left_side = std::format("          {}{}{}", prefix, branch, display_name);
            int fill_dots = column_width - static_cast<int>(left_side.length() + right_side_plain.length());
            if (fill_dots < 3) fill_dots = 3;

            std::cout << left_side << dim_dot << std::string(fill_dots, '.') << reset << right_side << std::endl;

            if (!node.children.empty())
            {
                std::string next_prefix = prefix + (is_last ? "    " : "|   ");
                print_subcase_tree(node.children, next_prefix, column_width);
            }
        }
    }

    static std::string format_duration_clean(double seconds, bool fixed_width = false)
    {
        if (fixed_width)
        {
            if (seconds < 0.001) return std::format("{:>8}", "<1ms");
            if (seconds < 1.0) return std::format("{:>6.1f}ms", seconds * 1000.0);
            return std::format("{:>6.2f}s ", seconds);
        }
        if (seconds < 0.001) return "(<1ms)";
        if (seconds < 1.0) return std::format("({:.1f}ms)", seconds * 1000.0);
        return std::format("({:.2f}s)", seconds);
    }

    static std::string get_duration_color(const double seconds)
    {
        if (seconds < 0.010) return "\033[90m"; // Dim Grey (<10ms)
        if (seconds < 0.250) return "\033[32m"; // Clean Green (<250ms)
        if (seconds < 1.000) return "\033[0m"; // Standard Text (<1s)
        if (seconds < 3.000) return "\033[33m"; // Warning Yellow (<3s)
        return "\033[1;31m"; // Bold Panic Red (>=3s)
    }

    static std::string truncate_string(const std::string& str, size_t max_len)
    {
        if (str.length() <= max_len) return str;
        if (max_len <= 3) return "...";
        return str.substr(0, max_len - 3) + "...";
    }
};

REGISTER_LISTENER("test_log", /*priority=*/1, test_log_reporter);


int main()
{
    compressed::detail::Instrumentor::Get().BeginSession("Tests");

    doctest::Context context;
    int res = context.run();

    if (context.shouldExit())
    {
        compressed::detail::Instrumentor::Get().EndSession();
        return res;
    }
    compressed::detail::Instrumentor::Get().EndSession();
    return res;
}
