#include <cassert>
#include <cstddef>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <utility>

// 只打印生命周期日志，不负责管理数组。
struct Trace {
    ~Trace() {
        std::cout << "Trace destruct\n";
    }
};

void run() {
    Trace trace;

    constexpr std::size_t n = 1000;
    auto p = std::make_unique<int[]>(n);

    for (std::size_t i = 0; i < n; ++i) {
        p[i] = static_cast<int>(i);
    }

    // 借用地址：观察和访问可以，不能自行 delete[]。
    int* borrowed = p.get();

    // TODO 1：替换 nullptr，将 p 的所有权转交给 q。
    std::unique_ptr<int[]> q = std::move(p);

    assert(p == nullptr);
    assert(q != nullptr);
    assert(q.get() == borrowed);

    long long sum = 0;
    // TODO 2：遍历 q 管理的 n 个整数并求和。
    for(std::size_t i =0;i<n;i++){
        sum += q[i];
    }

    std::cout << "sum=" << sum << '\n';
    assert(sum == 499500);

    std::cout << "throw\n";
    throw std::runtime_error("demo");
}

int main() {
    try {
        run();
    } catch (const std::exception& e) {
        std::cout << "caught: " << e.what() << '\n';
    }

    std::cout << "PASS\n";
}
