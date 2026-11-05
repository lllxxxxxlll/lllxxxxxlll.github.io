#include <cassert>
#include <cstddef>
#include <iostream>
#include <type_traits>
#include <utility>

class Buffer {
    int* data_;
    std::size_t size_;
public:
    explicit Buffer(std::size_t n) : data_(new int[n]{}), size_(n) {}
    ~Buffer() { delete[] data_; }
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    Buffer(Buffer&& other) noexcept
        : data_(other.data_), size_(other.size_) {
        other.data_ = nullptr;
        other.size_ = 0;
    }
    Buffer& operator=(Buffer&& other) noexcept {
        if (this != &other) {
            delete[] data_;
            data_ = other.data_;
            size_ = other.size_;
            other.data_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }
    int* data() { return data_; }
    const int* data() const { return data_; }
    std::size_t size() const { return size_; }
};

int main() {
    static_assert(!std::is_copy_constructible_v<Buffer>);
    static_assert(!std::is_copy_assignable_v<Buffer>);
    static_assert(std::is_nothrow_move_constructible_v<Buffer>);
    static_assert(std::is_nothrow_move_assignable_v<Buffer>);
    Buffer a(4);
    a.data()[0] = 42;
    int* original = a.data();
    Buffer b(std::move(a));
    assert(a.data() == nullptr && a.size() == 0);
    assert(b.data() == original && b.size() == 4);
    assert(b.data()[0] == 42);
    std::cout << "move construction PASS\n";
    Buffer c(8);
    c.data()[0] = 99;
    c = std::move(b);
    assert(b.data() == nullptr && b.size() == 0);
    assert(c.data() == original && c.size() == 4 && c.data()[0] == 42);
    std::cout << "move assignment PASS\n";
    c = std::move(c);
    assert(c.data() == original && c.size() == 4 && c.data()[0] == 42);
    std::cout << "self move PASS\n";
    Buffer d(std::move(a));
    assert(d.data() == nullptr && d.size() == 0);
    c = std::move(d);
    assert(c.data() == nullptr && c.size() == 0);
    const Buffer view(2);
    assert(view.data()[0] == 0);
    std::cout << "empty source and const view PASS\nPASS\n";
}
