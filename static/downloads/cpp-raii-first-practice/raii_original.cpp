#include <cassert>
#include <iostream>
#include <stdexcept>
#include <vector>

class Buffer {
public:
    // vector 在构造时申请并管理 n 个整数的存储空间。
    explicit Buffer(std::size_t n) : data_(n) {
        ++alive;
        std::cout << "construct, alive=" << alive << '\n';
    }

    ~Buffer() {
        --alive;
        std::cout << "destruct, alive=" << alive << '\n';
        // 析构函数体结束后，data_ 自身析构并释放所管理的内存。
    }

    // 本次禁止复制，保证计数逻辑简单，不涉及复制所有权。
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    void fill() {
        for (std::size_t i = 0; i < data_.size(); ++i) {
            data_[i] = static_cast<int>(i);
        }
    }

    long long sum() const {
        long long result = 0;
        // TODO：遍历 data_，将每个元素加到 result。
        for(int n : data_){
            result += n;
        }
        return result;
    }

    inline static int alive = 0;

private:
    std::vector<int> data_;
};

void run(int mode) {

    std::cout << "准备进入内部域"<<'\n';

    {
    std::cout <<"进入内部域"<<'\n';
    Buffer buffer(1000);
    buffer.fill();

    const auto result = buffer.sum();
    std::cout << "sum=" << result << '\n';
    assert(result == 499500);

    std::cout<<"准备退出内部域"<< '\n';
    }

    std::cout<<"退出内部域"<<'\n';

    if (mode == 1) {
        std::cout << "early return\n";
        return;
    }

    if (mode == 2) {
        std::cout << "throw\n";
        throw std::runtime_error("demo");
    }

    std::cout << "normal exit\n";
}

int main() {
    for (int mode = 0; mode < 3; ++mode) {
        std::cout << "mode=" << mode << '\n';

        try {
            run(mode);
        } catch (const std::exception& e) {
            std::cout << "caught: " << e.what() << '\n';
        }

        assert(Buffer::alive == 0);
    }

    std::cout << "PASS\n";
}

