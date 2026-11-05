#include <cassert>
#include <cstddef>
#include <iostream>
#include <memory>
#include <utility>

// 借用数组，修改前 n 个元素，不负责释放。
void add_inplace(int* data, std::size_t n, int delta) {
    // TODO 1：每个元素加 delta
    for(std::size_t i = 0;i<n;i++){
        data[i] += delta;
    }
}

// 只读借用数组，不负责释放。
int sum(const int* data, std::size_t n) {
    // TODO 2：返回前 n 个元素之和
    int s = 0;
    for(std::size_t i = 0;i<n;i++){
        s += data[i];
    }
    return s;
}

struct Payload {
    int value = 10;

    Payload() {
        std::cout << "construct\n";
    }

    ~Payload() {
        std::cout << "destruct\n";
    }
};

void add(Payload& obj) {
    // TODO 3：value 加 5
    obj.value += 5;
}

int read(const Payload& obj) {
    // TODO 4：返回 value
    return obj.value;
}

// 按值接收 unique_ptr，由形参 owned 持有资源。
void consume(std::unique_ptr<Payload> owned) {
    assert(owned);
    assert(read(*owned) == 15);
    std::cout << "inside consume\n";
}

int main() {
    // 第一部分：借用数组
    {
        constexpr std::size_t n = 8;
        auto owner = std::make_unique<int[]>(n);

        for (std::size_t i = 0; i < n; ++i) {
            owner[i] = static_cast<int>(i + 1);
        }

        int* borrowed = owner.get();

        add_inplace(borrowed, n, 9);

        assert(owner.get() == borrowed);
        assert(sum(borrowed, n) == 108);
        std::cout << "sum=" << sum(borrowed, n) << '\n';

        // 空区间：不能访问任何元素。
        add_inplace(nullptr, 0, 9);
        assert(sum(nullptr, 0) == 0);
    }

    // 第二部分：借用对象，再转交所有权
    auto p = std::make_unique<Payload>();
    Payload* borrowed = p.get();

    add(*p);
    assert(p.get() == borrowed);
    assert(read(*p) == 15);

    std::cout << "after borrow: " << read(*p) << '\n';

    // TODO 5：调用 consume，把 p 管理的资源交给形参 owned
    consume(std::move(p));

    assert(!p);
    // 此时 borrowed 已悬空，不能再解引用。
    borrowed = nullptr;

    std::cout << "after consume\n";
    std::cout << "PASS\n";
}
