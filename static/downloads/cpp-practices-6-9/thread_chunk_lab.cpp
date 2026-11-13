#include <cassert>
#include <cstddef>
#include <iostream>
#include <memory>
#include <thread>
//处理传递片段
void add_inplace(int* data,std::size_t n,int delta){
    for(std::size_t i = 0;i<n;i++){
        data[i] += delta;
    }
}

//按线程拆分数组分别处理
void add_parallel(int* data,std::size_t n,int delta){
    //将数组拆分
    if (n == 0) return;
    std::size_t mid = n/2;

    //使用工作线程处理前半段数组
    std::thread worker(add_inplace,data,mid,delta);

    //主线程也调用add,处理后半段
    add_inplace(data+mid,n-mid,delta);

    //等待工作线程正常结束
    worker.join();
}

//初始化数组并传导加和
void check_chunk(std::size_t n){
    //创建数组
    auto owner = std::make_unique<int[]>(n);

    //初始化
    for(std::size_t i = 0;i<n;i++){
        owner[i] = static_cast<int>(i);
    }

    //将指针复制传递,保留原指针后续比较
    
    add_parallel(owner.get(),n,10);

    //核对验证处理后的数组
    long long total = 0;
    for(std::size_t i=0;i<n;i++){
        total += owner[i];
        assert(owner[i] == static_cast<int>(i) + 10);
    }
    const long long count = static_cast<long long>(n);
    assert(total == count*(count-1)/2+10*count);

    std::cout<<"n="<<n<<",sum="<<total<<"\n";
}

int main(){
    //测试数组
    check_chunk(10);
    check_chunk(9);
    check_chunk(1);
    check_chunk(0);
    check_chunk(100000);

    std::cout<<"PASS\n";
}
