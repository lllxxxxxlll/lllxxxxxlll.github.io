#include <algorithm>
#include <cassert>
#include <cstddef>
#include <iostream>
#include <memory>
#include <stdexcept>

void add_inplace(int* data,std::size_t n,int delta){
    //每个元素加delta
    for(std::size_t i = 0;i<n;i++){
        data[i] += delta;
    }
}

void add_chunked(int* data,std::size_t n,
                std::size_t chunk_size,int delta){
        if (chunk_size == 0) {
            throw std::invalid_argument("chunk_size must be positive");
        }
        std::size_t begin = 0;
        while(begin<n){
            //取足够遍历的长度
            std::size_t len = std::min(chunk_size,n-begin);

            //传递每一个区间完成加和
            add_inplace(data+begin,len,delta);

            // 按实际处理长度推进 begin
            begin += len;
        }
}

//完成数组的装填和划分
void check_case(std::size_t n,std::size_t chunk_size){
    //数组装填
    auto owner = std::make_unique<int[]>(n);

    for(std::size_t i = 0;i<n;i++){
        owner[i] = static_cast<int>(i);
    }

    // 按值传入借用指针，修改数组元素
    int* process = owner.get();
    add_chunked(process,n,chunk_size,10);

    //判断指针状态
    assert(owner.get() == process);

    //计算总和
    long long total = 0;
    for(std::size_t i = 0;i<n;i++){
        assert(owner[i]==static_cast<int>(i+10));
        total += owner[i];
    }

    //总和断言
    const long long count = static_cast<long long>(n);
    assert(total == count*(count-1)/2 + 10*count);

    std::cout << "n=" << n
              << ", chunk=" << chunk_size
              << ", sum=" << total << '\n';
}

int main() {
    check_case(10, 4);  // 尾块不足
    check_case(8, 4);   // 恰好整除
    check_case(3, 8);   // 块比数组大
    check_case(5, 1);   // 每块一个元素
    check_case(0, 4);   // 空区间

    add_chunked(nullptr, 0, 4, 10);
    bool rejected = false;
    try { add_chunked(nullptr, 0, 0, 10); }
    catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
    std::cout << "PASS\n";
}
