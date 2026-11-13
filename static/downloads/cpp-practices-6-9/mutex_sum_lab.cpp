#include <cassert>
#include <cstddef>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>

// 只读借用。n == 0 时允许 data 为 nullptr。
long long sum(const int* data, std::size_t n) {
    long long result = 0;
    for (std::size_t i = 0; i < n; ++i) {
        result += data[i];
    }
    return result;
}

// total、mutex 都引用调用方的原对象。
void accumulate_chunk(const int* data, std::size_t n,
                      long long& total, std::mutex& mutex) {
    const long long local = sum(data,n);

    {
        std::lock_guard<std::mutex> guard(mutex);

        total += local;
    } // guard 在这里析构并解锁
}

long long parallel_sum(const int* data, std::size_t n) {
    if (n == 0) {
        return 0;
    }

    const std::size_t mid = n / 2;
    long long total = 0;
    std::mutex mutex;

    // [&]：通过引用使用外部变量，不复制数组或 mutex。
    std::thread worker([&] {
        accumulate_chunk(data, mid, total, mutex);
    });

    // 主线程处理后半段。
    accumulate_chunk(data + mid, n - mid, total, mutex);

    worker.join();

    return total;
}

void check_case(std::size_t n) {
    auto owner = std::make_unique<int[]>(n);

    // 有正数、负数和零；测试规模内不会溢出。
    for (std::size_t i = 0; i < n; ++i) {
        owner[i] = static_cast<int>(i % 7) - 3;
    }

    // 串行版本作为正确性基线。
    const long long expected = sum(owner.get(), n);
    const long long actual = parallel_sum(owner.get(), n);
    assert(actual == expected);

    // 独立核对生成的数据对应的数学结果。
    const auto r = static_cast<long long>(n % 7);
    assert(actual == r * (r - 1) / 2 - 3 * r);

    std::cout << "n=" << n << ", sum=" << actual << '\n';
}

int main() {
    check_case(0);
    check_case(1);
    check_case(9);
    check_case(10);
    check_case(1000);

    assert(parallel_sum(nullptr, 0) == 0);
    std::cout << "PASS\n";
}
