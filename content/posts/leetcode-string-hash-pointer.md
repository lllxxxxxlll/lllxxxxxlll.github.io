---
title: "LeetCode 字符串题：Python 与 C 双解（双指针 / 哈希 / 异或）"
date: "2025-08-06"
draft: false
categories: ["算法"]
tags: ["LeetCode", "字符串", "双指针", "哈希", "C", "Python"]
summary: "交替合并字符串、找不同、字母异位词——三道字符串题用 Python 和 C 各写一遍。Python 胜在精简，C 才能逼你真正理解指针、内存与位运算。"
---

刷字符串题的一个体会：**Python 胜在精简，不用关心太多底层细节；但要想真正理解指针、数据类型的变化，还是得用 C 重写一遍。** 下面是三道题的双解。

## 1. 交替合并字符串（LeetCode 1768）

给定两个字符串 `word1` 和 `word2`，交替合并，多余的字符直接追加到末尾。

### Python

```python
class Solution:
    def mergeAlternately(self, word1: str, word2: str) -> str:
        res = []
        i = 0
        while i < len(word1) and i < len(word2):
            res.append(word1[i])
            res.append(word2[i])
            i += 1
        res.append(word1[i:])
        res.append(word2[i:])
        return ''.join(res)
```

### C（双指针）

第一次用单指针做，核心思路是「挨个安放字符」，但 C 里字符串以 `'\0'` 结尾，需要在最后手动补 `'\0'`，循环写得很冗余。改用双指针后干净很多：

```c
char* mergeAlternately(char* word1, char* word2) {
    int a = strlen(word1), b = strlen(word2);
    char* word3 = (char*)malloc(a + b + 1);
    int i = 0, j = 0, k = 0;
    while (i < a || j < b) {
        if (i < a) word3[k++] = word1[i++];
        if (j < b) word3[k++] = word2[j++];
    }
    word3[k] = '\0';
    return word3;
}
```

要点：**指针空间的开辟（`malloc`）**，以及 C 字符串必须以 `'\0'` 结尾——少了它程序会读到越界内存。

## 2. 找不同（LeetCode 389）

`t` 由 `s` 随机重排后再加一个字母，找出多出来的那个字母。

### 计数法（Python）

```python
from collections import Counter

class Solution:
    def findTheDifference(self, s: str, t: str) -> str:
        return list(Counter(t) - Counter(s))[0]
```

### 求和法（C）

两个字符串的 ASCII 码和相减，差值就是多出来的字符：

```c
char findTheDifference(char* s, char* t) {
    int a = strlen(s), b = strlen(t);
    int res = 0;
    for (int i = 0; i < a; i++) res -= s[i];
    for (int j = 0; j < b; j++) res += t[j];
    return (char)res;
}
```

### 异或法（C，最优）

一个数和自己异或得 0，把所有字符异或一遍，剩下的一定是那个只出现一次的字符：

```c
char findTheDifference(char* s, char* t) {
    char res = 0;
    while (*s) res ^= *s++;
    while (*t) res ^= *t++;
    return res;
}
```

时间 O(n)、空间 O(1)。异或这个技巧在「找单个落单元素」类题目里反复出现。

### 哈希计数法（C）

```c
char findTheDifference(char* s, char* t) {
    int cnt[26] = {0};
    for (int i = 0; s[i]; i++) cnt[s[i] - 'a']++;
    for (int j = 0; t[j]; j++) {
        cnt[t[j] - 'a']--;
        if (cnt[t[j] - 'a'] < 0) return t[j];
    }
    return 0;
}
```

把字符通过 `c - 'a'` 映射成数组下标，就是「哈希索引」在字符串题里的标准用法。

## 3. 字母异位词（LeetCode 242）

判断 `t` 是否是 `s` 的字母异位词（字符种类和数量完全相同，只是顺序不同）。

### Python

```python
class Solution:
    def isAnagram(self, s: str, t: str) -> bool:
        if len(s) != len(t):
            return False
        s_dict, t_dict = {}, {}
        for i in range(len(s)):
            s_dict[s[i]] = s_dict.get(s[i], 0) + 1
            t_dict[t[i]] = t_dict.get(t[i], 0) + 1
        return s_dict == t_dict
```

### C（哈希计数）

```c
bool isAnagram(char* s, char* t) {
    int cnt[26] = {0};
    if (strlen(s) != strlen(t)) return false;
    while (*s) cnt[*s++ - 'a']++;
    while (*t) cnt[*t++ - 'a']--;
    for (int i = 0; i < 26; i++)
        if (cnt[i] != 0) return false;
    return true;
}
```

## 小结

这三道题都绕不开「**字符 → 下标 → 计数**」的哈希思想，以及「**双指针 / 异或**」这两个空间 O(1) 的优化技巧。用 C 重写能更深入体会指针、内存管理和位运算，这是 Python 藏起来的底层细节。
