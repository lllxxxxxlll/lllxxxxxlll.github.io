---
title: "CS:APP 学习笔记：show_bytes 与数据表示"
date: "2025-07-31"
draft: false
categories: ["算法"]
tags: ["CSAPP", "C", "指针", "数据表示", "计算机系统"]
summary: "用 show_bytes 直接看 int / float / 指针在内存里的字节表示，理解「同一串字节，解读方式不同，含义就不同」——这是 CS:APP 第二章的核心。"
---

## 为什么看字节

高级语言里，一个 `int x = 12345` 你看到的是「12345」这个数值；但在内存里它就是一串字节。**同一串字节，用不同的类型去解读，会得到完全不同的含义**。看字节是理解「数据表示」最直接的方式。

## show_bytes 的实现

```c
#include <stdio.h>

typedef unsigned char *byte_pointer;

void show_bytes(byte_pointer start, size_t len) {
    size_t i;
    for (i = 0; i < len; i++) {
        printf(" %.2x", start[i]);  // 指针间接引用，用数组下标取出指向的内容
    }
    printf("\n");
}

void show_int(int x)       { show_bytes((byte_pointer)&x, sizeof(int)); }
void show_float(float x)   { show_bytes((byte_pointer)&x, sizeof(float)); }
void show_pointer(void *x) { show_bytes((byte_pointer)&x, sizeof(void *)); }

int main() {
    int x = 12345;
    show_int(x);

    float y = 123.456;
    show_float(y);

    int *p = &x;
    show_pointer(p);
    return 0;
}
```

几个要点：

- `typedef unsigned char *byte_pointer` 相当于 `unsigned char *`——用无符号字节去逐字节「窥视」内存，不会做符号扩展。
- `start[i]` 是「指针 + 数组下标」的间接引用，等价于 `*(start + i)`。
- 想看一个字符串的字节表示，直接传字符串指针即可：

```c
const char *s = "dujiali";
show_bytes((byte_pointer)s, strlen(s) + 1);  // +1 是为了把结尾的 '\0' 也打出来
```

## 这背后是 CS:APP 第二章的核心

1. **信息 = 位 + 上下文**。同样的字节序列，用 `int` 读是整数，用 `float` 读是浮点，用指针读是地址。含义来自「解读方式」，不是字节本身。
2. **大端 vs 小端**：`show_int(12345)` 打出来的字节顺序，能直接判断机器是小端（低位字节在低地址）还是大端。绝大多数 x86/x64 都是小端。
3. **指针也是数据**：`show_pointer` 打印的是「指针变量本身」存储的内容——也就是它指向的地址，占用 `sizeof(void*)` 个字节（64 位下是 8 字节）。

## 学习建议

看教材 P31-P33 时配合 `show_bytes` 手动跑一遍，比只看书直观得多。尤其要自己验证：**同一个数值，int 和 float 的字节表示长得完全不一样**——这就是「数据表示」这门课最有冲击力的一课。

> 生命里真正要做的，不是去经历一件事，而是去经历它的价值。
