---
title: "KMP 算法与字符串匹配：strStr / 重复子串 / 滑动窗口"
date: "2025-08-06"
draft: false
categories: ["算法"]
tags: ["LeetCode", "KMP", "字符串", "滑动窗口", "C"]
summary: "暴力匹配 O(nm) 到 KMP O(n+m) 的核心是 next 数组——失配时不回退主串，只回退模式串。再用 KMP 思想解决「重复子串」，用滑动窗口解决「最长双子串」。"
---

字符串匹配的暴力解法是 O(nm)：主串每走一步，模式串都要从头比。KMP 把这个过程优化到 O(n+m)，核心思想一句话：**失配时主串指针不回退，只回退模式串指针**。

## 1. 实现 strStr（LeetCode 28）

### 暴力枚举（Python，起步）

```python
class Solution:
    def strStr(self, haystack: str, needle: str) -> int:
        if not needle:
            return 0
        for i in range(len(haystack) - len(needle) + 1):
            if haystack[i:i + len(needle)] == needle:
                return i
        return -1
```

时间 O(nm)、空间 O(1)，简单但慢。

### KMP（C）

KMP 的关键是 `next` 数组：`next[i]` 表示模式串 `needle[0..i]` 这个前缀里，最长的「既是真前缀又是真后缀」的长度。失配时根据它跳过不可能匹配的位置。

```c
void getNext(char* needle, int* next) {
    int len = strlen(needle);
    next[0] = 0;
    int j = 0;
    for (int i = 1; i < len; i++) {
        while (j > 0 && needle[i] != needle[j]) j = next[j - 1];  // 回退
        if (needle[i] == needle[j]) j++;
        next[i] = j;
    }
}

int strStr(char* haystack, char* needle) {
    int n = strlen(haystack), m = strlen(needle);
    if (m == 0) return 0;
    int* next = (int*)malloc(sizeof(int) * m);
    getNext(needle, next);
    int j = 0;
    for (int i = 0; i < n; i++) {
        while (j > 0 && haystack[i] != needle[j]) j = next[j - 1];
        if (haystack[i] == needle[j]) j++;
        if (j == m) return i - m + 1;   // 完全匹配
    }
    return -1;
}
```

时间 O(n+m)、空间 O(m)。**重点在 `next` 数组的构建**——`j = next[j-1]` 那一步是「利用已知匹配信息回退」，而不是简单把 `j` 清零。

## 2. 重复的子字符串（LeetCode 459）

判断一个字符串能否由它的某个子串重复多次构成。

### KMP 思想

如果 `s` 由重复子串组成，那么 `next[len-1]`（最长相等前后缀长度）会有特定性质：

```c
bool repeatedSubstringPattern(char* s) {
    int a = strlen(s);
    int* next = (int*)malloc(sizeof(int) * a);
    next[0] = 0;
    int j = 0;
    for (int i = 1; i < a; i++) {
        while (j > 0 && s[i] != s[j]) j = next[j - 1];
        if (s[i] == s[j]) j++;
        next[i] = j;
    }
    int m = next[a - 1];
    free(next);
    // 关键：重复子串长度 = a - m；若 a 能被它整除，则成立
    return m > 0 && a % (a - m) == 0;
}
```

**核心结论**：重复子串的长度 = `len - next[len-1]`。如果 `len` 能被这个长度整除，就说明字符串由重复子串组成。

### 暴力枚举（对照）

```c
bool repeatedSubstringPattern(char* s) {
    int len = strlen(s);
    for (int i = 1; i * 2 <= len; i++) {   // 子串长度 i
        if (len % i == 0) {
            int match = 0;
            for (int j = i; j < len; j++)
                if (s[j] != s[j - i]) { match = 1; break; }
            if (!match) return true;
        }
    }
    return false;
}
```

时间 O(n²)，用标志位 `match` 提前跳出内层循环。

## 3. 最长双子串（LeetCode 159，滑动窗口）

找出**至多包含两个不同字符**的最长子串。这是滑动窗口的经典题：

```python
class Solution:
    def lengthOfLongestSubstringTwoDistinct(self, s: str) -> int:
        if len(s) <= 2:
            return len(s)
        left = 0
        max_len = 0
        pos = {}                      # 字符 -> 最后出现的位置
        for right in range(len(s)):
            pos[s[right]] = right
            if len(pos) > 2:          # 窗口内超过 2 个不同字符
                min_idx = min(pos.values())
                del pos[s[min_idx]]   # 丢掉最靠左的字符
                left = min_idx + 1
            max_len = max(max_len, right - left + 1)
        return max_len
```

### C 版（双指针 + 计数）

```c
int lengthOfLongestSubstringTwoDistinct(char* s) {
    int left = 0, right = 0, max_len = 0;
    int cnt[256] = {0};
    int distinct = 0;
    while (s[right]) {
        if (cnt[s[right]] == 0) distinct++;
        cnt[s[right]]++;
        while (distinct > 2) {       // 收缩左边界
            cnt[s[left]]--;
            if (cnt[s[left]] == 0) distinct--;
            left++;
        }
        max_len = max(max_len, right - left + 1);
        right++;
    }
    return max_len;
}
```

滑动窗口的套路：**右指针不断扩展，条件不满足时移动左指针收缩**，过程中记录最优解。

## 小结

| 题目 | 核心思想 | 复杂度 |
|---|---|---|
| strStr | KMP + next 数组 | O(n+m) |
| 重复子串 | `len - next[len-1]` | O(n) |
| 最长双子串 | 滑动窗口 | O(n) |

KMP 的 `next` 数组是字符串题里最值得反复啃的一块——理解了「失配回退」，很多字符串问题都能找到比暴力更优的解法。
