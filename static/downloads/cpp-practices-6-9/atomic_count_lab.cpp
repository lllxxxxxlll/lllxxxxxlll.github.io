#include <atomic>
#include <cassert>
#include <iostream>
#include <thread>

// 引用同一个原子计数器，不复制它。
void count_jobs(std::atomic<long long>& counter, int jobs) {
    for (int i = 0; i < jobs; ++i) {
        counter.fetch_add(1);
    }
}

void check_case(int jobs) {
    std::atomic<long long> counter{0};

    // 引用捕获：工作线程使用当前作用域中的变量。
    std::thread worker([&] {
        count_jobs(counter, jobs);
    });

    // 主线程也完成 jobs 次计数。
    count_jobs(counter, jobs);

    worker.join();

    const long long actual = counter.load();
    const long long expected = 2LL * jobs;

    assert(actual == expected);
    std::cout << "jobs/thread=" << jobs
              << ", total=" << actual << '\n';
}

int main() {
    check_case(0);
    check_case(1);
    check_case(1000);
    check_case(100000);
    std::cout << "PASS\n";
}
