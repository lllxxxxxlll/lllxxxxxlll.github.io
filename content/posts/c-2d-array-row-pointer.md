---
title: "C 二维动态数组与行指针：两种 malloc 方式与 removeAnagrams 参数分析"
date: "2025-10-13"
draft: false
categories: ["算法"]
tags: ["C", "指针", "内存管理", "二维数组", "malloc"]
summary: "二维动态数组的两种分配方式（指针数组逐行分配 vs 一次性连续分配）、行指针的作用，以及 char** 函数参数的三个常见坑。"
---

## 数组相关内容

开辟二维动态数组，和**内存分配、指针使用**息息相关，其中涉及一个很重要的概念——**行指针**。行指针的作用是标识每一行，用以索引到特定的一行；这一行内容既可以指向二维数组行中的其他内容，也可以指向一个字符串。

下面从两种分配方式来看行指针的运用。

## 方式一：指针数组 + 每行单独分配

先分配行指针数组：

```c
int** arr = (int**)malloc(n * sizeof(int*));
```

再为每一行单独分配：

```c
for (int i = 0; i < n; i++) {
    arr[i] = (int*)malloc(m * sizeof(int));
}
```

**缺点**：空间不连续，释放时需要依次 free：

```c
for (int i = 0; i < n; i++) {
    free(arr[i]);
}
free(arr);
```

## 方式二：一次性分配所有空间

先分配所有所需空间：

```c
int* data = (int*)malloc(rows * cols * sizeof(int));
```

再分配行指针，让每一行「指到」连续空间里的对应位置：

```c
int** arr = (int**)malloc(rows * sizeof(int*));
for (int i = 0; i < rows; i++) {
    arr[i] = data + i * cols;
}
```

**优点**：内存连续，可以一次性释放：

```c
free(data);
free(arr);
```

两种方式都用到了「行指针数组」（`int**` 外层），区别只在行数据本身是**分散 malloc** 还是**一整块连续内存**。理解这一点，就理解了 C 里二维动态数组和 `a[i][j]` 下标访问的底层关系。

## 对常见参数的分析

```c
char** removeAnagrams(char** words, int wordsSize, int* returnSize)
```

### `words`

是一个指向 `char*` 的指针，具体表现为字符串数组的形式：`words[i]` 表示第 i 个字符串，这个字符串通常以 `'\0'` 结尾。

### `wordsSize`

表示 `words` 中字符串的数量（元素个数）。**必须和 `words` 一起使用**——函数无法通过 `words` 本身推断长度，因为 C 不携带数组长度信息：

```c
int wordsSize = sizeof(words) / sizeof(words[0]);  // 仅对静态数组有效
```

### `*returnSize`

在程序中，如果不同部分之间要传递一个值，而这个值又不处于 `return` 的返回位置，就一定要**用指针指向存储这个值的内存地址**，通过直接修改地址上的值来完成传递。否则这个值会随着函数的结束而被销毁、被反复拷贝，无法真正传出去。

```c
// 函数内部
*returnSize = result_count;   // 把结果个数写回给调用者
return result_array;          // 返回 char**

// 调用者
int outSize;
char** result = removeAnagrams(words, wordsSize, &outSize);
// 现在 outSize 就是 result 数组的长度
for (int i = 0; i < outSize; ++i)
    printf("%s\n", result[i]);
```

**重点**：调用者一定要把 `outSize` 的**地址**传过去，才能完成值的修改。
