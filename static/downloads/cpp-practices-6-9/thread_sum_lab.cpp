#include <cassert>
#include <cstddef>
#include <iostream>
#include <memory>
#include <thread>

// 只读借用数组，返回前 n 个元素之和。
long long sum(const int* data, std::size_t n) {
    long long sum  = 0;
    for(std::size_t i = 0;i<n;i++){
        sum += data[i];
    }
    return sum;
}

long long sum_parallel(const int* data, std::size_t n) {
    if (n == 0) return 0;
    std::size_t mid = n / 2;

    long long left = 0;

    // 工作线程计算前半段。
    std::thread worker([&] {
        left = sum(data, mid);
    });

    long long right = sum(data+mid,n-mid);

    worker.join();

    // 等待结束后，再读取 left 并合并。
    return left + right;
}

void check_case(std::size_t n) {
    auto owner = std::make_unique<int[]>(n);

    for (std::size_t i = 0; i < n; ++i) {
        owner[i] = static_cast<int>(i + 1);
    }

    long long result = sum_parallel(owner.get(), n);
    long long expected =
        static_cast<long long>(n) * (n + 1) / 2;

    assert(result == expected);

    // 求和不应修改原数组。
    for (std::size_t i = 0; i < n; ++i) {
        assert(owner[i] == static_cast<int>(i + 1));
    }

    std::cout << "n=" << n << ", sum=" << result << '\n';
}

int main() {
    check_case(10);
    check_case(9);
    check_case(1);
    check_case(0);
    check_case(1000);

    std::cout << "PASS\n";
}
