---
title: "我的第一个 C++ 编程练习：从真实疑惑理解对象生命周期与 RAII"
date: "2026-10-28"
draft: false
categories: ["学习手记"]
tags: ["C++", "RAII", "内存管理", "编程练习", "学习复盘"]
summary: "从看不懂构造函数，到分清析构与垃圾回收、对象与资源：保留真实提问、代码和输出，复盘第一次 C++ 练习中的理解变化。"
learning_note: true
---

> 一次从“补全求和”开始、最终弄清“谁在什么时候释放资源”的练习。

我之前主要使用 Java、Go 和 Python。为了逐步学习 C++/CUDA，为后续并行计算相关的学习和研究打基础，我开始做一些小型编程练习。

第一题没有涉及 GPU，也没有复杂算法：写一个管理整数缓冲区的 `Buffer`，填入 `0～999`，计算总和，再观察函数正常结束、提前返回和抛出异常时会发生什么。

求和很快就补出来了，真正让我停下来反复追问的，是下面几个问题：

- `explicit Buffer(std::size_t n) : data_(n)` 这一行到底在做什么？
- 两行 `= delete` 为什么要写？它们是不是在释放资源？
- 怎么观察析构函数发挥作用的时机和现象？
- 析构是不是一种垃圾自动回收机制？
- 如果资源已经和对象生命周期绑定，为什么又说是否释放资源取决于析构逻辑？

这篇记录沿着这些疑惑展开。**这次最核心的收获，是把“语言触发对象析构”和“类型负责资源清理”这两件事分清楚，再理解 RAII 怎样把它们连接起来。**

## 1. 先明确：这个练习要验证什么

练习中的 `Buffer` 使用 `std::vector<int>` 管理整数存储空间，提供填充和求和功能，并在构造、析构时打印日志。

验证分成两部分：

| 要验证的事情 | 观察方式 |
|---|---|
| 计算结果正确 | `sum == 499500`，用断言检查 |
| 对象何时析构 | 查看构造、析构日志与 `return`、`throw`、`catch` 的先后关系 |

这里的 `499500` 来自：

```text
0 + 1 + 2 + ... + 999 = 999 × 1000 ÷ 2 = 499500
```

这两个验证目标不能混为一谈：求和正确，只能证明求和逻辑通过了这一组数据；打印了 `PASS`，也需要进一步看测试究竟覆盖了什么。

## 2. 从看不懂的一行构造函数开始

我最先问的是：

> “explicit Buffer(std::size_t n) : data_(n) 给我讲一下其中的语法知识，c++中的这个::或者:之类的和特殊符号相关的语法我不太熟悉。”

把相关代码放在一起，就容易理解一些：

```cpp
class Buffer {
public:
    explicit Buffer(std::size_t n) : data_(n) {
        ++alive;
        std::cout << "construct, alive=" << alive << '\n';
    }

    inline static int alive = 0;

private:
    std::vector<int> data_;
};
```

创建对象：

```cpp
Buffer buffer(1000);
```

可以按下面的顺序阅读：

1. 调用 `Buffer` 的构造函数，参数 `n` 为 `1000`。
2. 在进入构造函数体之前，用 `n` 直接构造成员 `data_`。
3. `data_` 成为包含 `1000` 个 `int` 元素的 `vector`。
4. 执行函数体，增加计数并打印日志。

### 2.1 `::`、`:` 和 `.` 各有自己的职责

| 写法 | 在本练习中的含义 |
|---|---|
| `std::size_t` | 使用 `std` 命名空间中的 `size_t` 类型 |
| `std::vector<int>` | 使用标准库模板 `vector`，元素类型为 `int` |
| `Buffer::alive` | 访问 `Buffer` 类的静态成员 |
| `: data_(n)` | 构造函数的成员初始化列表 |
| `buffer.fill()` | 通过对象调用成员函数 |
| `data_.size()` | 获取这个 `vector` 的元素数量 |

`std::size_t` 是适合表示大小、元素数量等信息的无符号整数类型。`data_` 尾部的下划线只是成员变量的命名习惯，没有额外的语言功能。

同一个符号要结合上下文理解。比如下面两个冒号，含义就不同：

```cpp
explicit Buffer(std::size_t n) : data_(n) {}  // 成员初始化列表

for (int value : data_) {}                  // 范围 for 循环
```

### 2.2 成员初始化发生在构造函数体之前

`data_(n)` 里的 `n` 表示**元素个数**，不是字节数。对本例的整数 `vector` 来说，这些元素最初的值为零，随后 `fill()` 会覆盖它们。

这与在函数体中写赋值不同：

```cpp
Buffer(std::size_t n) {
    data_ = std::vector<int>(n);
}
```

后一种写法先默认构造 `data_`，进入函数体后再赋值；初始化列表则直接把成员构造成需要的状态。

如果有多个成员，它们实际按**类中的声明顺序**初始化，而不是按初始化列表的书写顺序。

### 2.3 `explicit` 让构造意图更明确

加上 `explicit` 后：

```cpp
Buffer a(1000);     // 可以：直接初始化
Buffer b{1000};     // 可以：直接列表初始化
// Buffer c = 1000; // 不可以：不能这样隐式转换
```

我现在会把这一行完整读成：

> 定义一个接收元素数量的构造函数，限制通过它进行隐式转换，并在执行构造函数体前，用这个数量直接初始化成员容器。

## 3. `= delete`：禁止复制，不是执行释放

接下来让我困惑的是：

```cpp
Buffer(const Buffer&) = delete;
Buffer& operator=(const Buffer&) = delete;
```

它们分别禁止两种操作：

```cpp
Buffer a(10);

// Buffer b(a);  // 拷贝构造：用 a 创建新对象 b

Buffer c(20);
// c = a;        // 拷贝赋值：将 a 赋给已有对象 c
```

其中：

- `const Buffer&` 表示通过常量引用接收另一个 `Buffer`。
- `operator=` 是赋值运算符函数。
- 第二行开头的 `Buffer&` 是返回类型，通常用于返回当前对象的引用。
- `= delete` 表示这个函数被显式禁用，尝试使用它会产生编译错误。

**这里的 `= delete` 与运行时的 `delete`、`delete[]` 不是一回事。**

本练习禁止复制，主要是为了让计数实验保持简单：计数只在当前这个普通构造函数中增加。如果直接允许编译器生成的拷贝构造，就可能增加了对象，却没有同步增加 `alive`，使观察结果失真。

另外，`std::vector` 本身支持正确的复制。不能因为这个练习禁止复制，就推导出“含有 vector 的类复制一定会重复释放内存”。真正需要警惕的是手工拥有资源的裸指针类：若多个对象复制了同一个地址，却都把自己当作所有者，才可能出现重复释放等问题。

## 4. 补全 `sum()`：让计算结果先正确

我补上的代码是：

```cpp
long long sum() const {
    long long result = 0;
    for (int n : data_) {
        result += n;
    }
    return result;
}
```

范围 `for` 依次取出 `data_` 的元素，并累加到 `result`。

这里还有三个值得记住的小点：

| 写法 | 作用 |
|---|---|
| `long long` | 为累加结果提供比 `int` 至少同等、通常更大的表示范围 |
| 函数末尾的 `const` | 承诺通过这个成员函数不修改对象的普通非静态数据成员 |
| `const auto result = buffer.sum();` | 推导返回值类型，同时让局部结果不可修改 |

填充函数则把索引写入对应位置：

```cpp
void fill() {
    for (std::size_t i = 0; i < data_.size(); ++i) {
        data_[i] = static_cast<int>(i);
    }
}
```

本次只有 `1000` 个元素，索引可以表示为 `int`；`static_cast<int>` 是显式类型转换，并不意味着任何大小的索引都可以安全转成 `int`。

验证代码：

```cpp
const auto result = buffer.sum();
std::cout << "sum=" << result << '\n';
assert(result == 499500);
```

`assert` 在这里检查程序应当满足的条件。条件为真时继续执行，不会自动打印“通过”；为假时会报告失败并终止程序。定义 `NDEBUG` 可以禁用这些断言，所以不要把必要业务操作放在断言表达式里。

## 5. 我的真实输出：加了一层大括号，测试含义也变了

为了观察析构，我在 `run()` 中加入了内层作用域，并把原来的 `buffer` 放了进去。关键结构是：

```cpp
void run(int mode) {
    std::cout << "准备进入内部域\n";
    {
        std::cout << "进入内部域\n";
        Buffer buffer(1000);
        buffer.fill();

        const auto result = buffer.sum();
        std::cout << "sum=" << result << '\n';
        assert(result == 499500);

        std::cout << "准备退出内部域\n";
    }

    std::cout << "退出内部域\n";

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
```

下面是**我当时贴出的完整运行输出**，只整理了聊天中的换行格式：

```text
mode=0
准备进入内部域
进入内部域
construct, alive=1
sum=499500
准备退出内部域
destruct, alive=0
退出内部域
normal exit
mode=1
准备进入内部域
进入内部域
construct, alive=1
sum=499500
准备退出内部域
destruct, alive=0
退出内部域
early return
mode=2
准备进入内部域
进入内部域
construct, alive=1
sum=499500
准备退出内部域
destruct, alive=0
退出内部域
throw
caught: demo
PASS
```

最值得看的，是异常路径：

```text
准备退出内部域
destruct, alive=0
退出内部域
throw
caught: demo
```

对象确实析构了，但 `destruct` 出现在 `throw` **之前**。原因是执行到内层 `}` 时，`buffer` 已经离开自己的作用域。

因此，这份输出证明了：

> 内层作用域结束时，其中的局部对象会析构。

它还没有证明：

> 仍然存活的局部对象会因为提前返回或异常传播而析构。

同样，`mode=1` 中的对象也在执行 `return` 前就已析构。这次修改不是无效实验，它验证了作用域边界；只是我需要准确说明自己验证了什么。

## 6. 最核心的疑惑：析构是不是垃圾自动回收？

我后来把自己的理解描述为：

> “我的理解是这就是一个垃圾自动回收机制……也就是分析对象是否还需要接着使用，不使用的话就释放相关资源……”

这里混在一起的，其实是“何时销毁对象”和“怎样清理资源”两个问题。

### 6.1 析构不会分析“以后还会不会用到”

对本例这种已经完成构造、具有自动存储期的局部对象，正常离开所属作用域时会调用析构函数；提前返回和异常传播离开作用域时，也会发生相应清理。

它不是在运行时扫描对象，判断对象是否还有引用、未来是否还会使用。

例如，下面即使 `buffer` 不再参与计算，也不会因为“用完了”就马上析构：

```cpp
{
    Buffer buffer(1000);
    buffer.fill();

    std::cout << "后面没有再使用 buffer\n";
    // buffer 仍然存在
}   // 到这里才离开其作用域
```

反过来，如果把局部对象的地址保存到外面，离开作用域后，它也不会因为外面还保存着地址就继续存活。此时继续使用这个地址会有悬空访问风险。

这里讨论的是自动存储期局部对象。通过 `new` 创建的动态对象，不会因为某个普通指针变量离开作用域就自动销毁。

### 6.2 对象会析构，不代表它指向的所有资源都会自动释放

我的另一个真实疑问是：

> “好像资源就是和对象的生命周期绑定……还是需要由析构函数中的内容决定释放资源？”

可以把答案拆成两句：

**语言规则负责在相应时机触发析构；类型的实现负责正确清理它拥有的资源。**

看本练习的析构函数：

```cpp
~Buffer() {
    --alive;
    std::cout << "destruct, alive=" << alive << '\n';
}
```

里面没有 `delete`，内存为什么仍能释放？

因为这里有三层关系：

| 层次 | 本例中的实体 | 清理责任 |
|---|---|---|
| 外层对象 | `Buffer buffer` | 离开作用域时触发析构 |
| 成员对象 | `std::vector<int> data_` | 在 `Buffer` 析构过程中自动析构 |
| 动态存储空间 | `vector` 管理的整数缓冲区 | 由 `vector` 的资源管理实现释放 |

析构顺序可以写成：

```text
离开 buffer 所属作用域
    ↓
执行 Buffer 的析构函数体
    ↓
析构成员 data_
    ↓
vector 释放自己管理的整数存储空间
```

所以，**我的 `~Buffer()` 负责打印和更新计数；整数缓冲区的释放由成员 `vector` 负责。** 即使不手写这个析构函数，编译器隐式生成的析构函数也会析构 `data_`，只是不会再执行这些日志和计数操作。成员清理顺序可参见 [C++ 工作草案：析构函数](https://eel.is/c++draft/class.dtor)。

这也修正了我原先“在析构函数内部再绑定一些容器”的说法：容器早已作为成员在对象构造期间建立，析构时负责按规则清理，不需要临时再绑定。

### 6.3 用裸指针对照，才能看出 RAII 做了什么

下面是一个**故意遗漏清理的反例，仅用于解释**：

```cpp
class BadBuffer {
public:
    explicit BadBuffer(std::size_t n)
        : data_(new int[n]) {}

    ~BadBuffer() {
        // 漏掉了 delete[] data_;
    }

private:
    int* data_;
};
```

离开作用域时，`BadBuffer` 仍然会析构，但指针成员生命周期结束，不会自动释放 `new[]` 申请的数组。

这说明：“对象会析构”是语言机制，“资源会得到正确清理”还需要所有者实现相应职责。

RAII（Resource Acquisition Is Initialization，资源获取即初始化）就是把资源交给拥有它的对象：在初始化时建立管理关系，在析构时释放资源，让资源清理沿着对象的生命周期发生。日常代码中，优先使用 `vector`、智能指针等现成的资源管理类型，可以减少手工维护清理路径的负担。[C++ Core Guidelines R.1](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rr-raii) 推荐的正是这种做法。

### 6.4 打印 `alive=0` 时，内存已经全部释放了吗？

还没有这样的保证。

`--alive` 和日志位于 `Buffer` 的析构函数体中，执行它们时，成员 `data_` 尚未完成后续析构。这个计数只是教学观察工具，不能当作内存释放进度。

我原先的回答是：

> “因为先执行析构函数体内的内容再发生的销毁对象。”

现在更准确的说法是：

> 先执行外层对象的析构函数体，再自动析构成员；执行析构函数体本身就是整个析构过程的一部分。

另外，`buffer` 是局部管理对象，不等于其整数数据也具有自动存储期。`vector` 管理的是动态分配的元素存储空间。管理对象和被管理的内存，需要分开看。

## 7. 修正实验：让三个析构时机都能被观察到

为了保留内部作用域实验，同时验证函数退出路径，可以保留外层 `buffer`，再单独创建内层对象 `inner`：

```cpp
void run(int mode) {
    Buffer buffer(1000);
    buffer.fill();

    const auto result = buffer.sum();
    std::cout << "sum=" << result << '\n';
    assert(result == 499500);

    std::cout << "准备进入内部域\n";
    {
        Buffer inner(10);
        std::cout << "准备退出内部域\n";
    }  // inner 析构，外层 buffer 仍然存活
    std::cout << "已经退出内部域\n";

    if (mode == 1) {
        std::cout << "early return\n";
        return;  // 退出 run，buffer 析构
    }

    if (mode == 2) {
        std::cout << "throw\n";
        throw std::runtime_error("demo");
        // 异常向调用方传播时，buffer 析构
    }

    std::cout << "normal exit\n";
}  // 正常路径下，buffer 在这里析构
```

调用方在外面捕获异常：

```cpp
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
```

`catch (const std::exception& e)` 通过常量引用接收异常，`e.what()` 用于取得异常说明。

下面是**整理本文时补做的修正版验证输出**，与上一节中我当时提交的真实输出区分开来。三个模式都会先出现这一段内部作用域日志：

```text
construct, alive=1
sum=499500
准备进入内部域
construct, alive=2
准备退出内部域
destruct, alive=1
已经退出内部域
```

随后，各自的尾部为：

```text
# mode=0：正常退出
normal exit
destruct, alive=0

# mode=1：提前返回
early return
destruct, alive=0

# mode=2：异常传播
throw
destruct, alive=0
caught: demo
```

这里的 `# mode=...` 是本文为阅读添加的标注，不是程序打印的内容。完整原始输出随示例文件一起提供。

关键变化是：异常路径中变成了 `throw → destruct → caught`。异常从 `run()` 向外层处理器传播，离开中间作用域并销毁相应局部对象，这个过程叫**栈展开**。相关对象按构造完成的相反顺序销毁。[C++ 工作草案：栈展开](https://eel.is/c++draft/except.ctor) 描述了这一规则。

本例的异常会被外层 `catch` 捕获。不要把这种清理保证扩大到强制终止进程、断言失败等终止路径。

## 8. 编译命令也要读懂

当时我还问了：

> “-Wall -Wextra -Wpedantic -g 这几个参数我不太了解是什么意思。”

原先命令中的源文件叫 `rail.cpp`，本文配套文件使用更明确的名字。以修正版为例：

```bash
g++ -std=c++17 -Wall -Wextra -Wpedantic -g raii_revised.cpp -o raii_lab
./raii_lab
```

在示例文件所在目录执行即可；上面的运行方式适用于 Linux/WSL。使用 Windows 原生 GCC 时，可输出为 `raii_lab.exe`，在 PowerShell 中执行 `.\raii_lab.exe`。

| 参数 | 我现在的理解 |
|---|---|
| `g++` | 调用 GCC 的 C++ 编译驱动 |
| `-std=c++17` | 按 C++17 标准编译；本例使用了 C++17 的内联静态数据成员 |
| `-Wall` | 开启一组常用警告，并不代表所有警告 |
| `-Wextra` | 增加一些 `-Wall` 未开启的警告 |
| `-Wpedantic` | 对部分不符合所选 ISO C++ 标准的用法、扩展给出诊断 |
| `-g` | 生成调试信息，帮助调试器对应源码、行号等 |
| `raii_revised.cpp` | 输入的源文件 |
| `-o raii_lab` | 指定输出程序名称 |

前三个 `-W...` 主要帮助在编译阶段发现可疑写法；警告通常不会阻止生成程序，另加 `-Werror` 等设置时情况会改变。`-g` 服务于后续调试，不会自动启动调试器，也不等于自动检查内存泄漏。参数含义可查阅 [GCC 警告选项](https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html)和 [GCC 调试选项](https://gcc.gnu.org/onlinedocs/gcc/Debugging-Options.html)。

## 9. 这次练习留下的核心知识

回头看，我不只是补完了一个循环，也修正了几处理解：

| 最初的疑惑或理解 | 现在形成的知识点 |
|---|---|
| `::`、`:` 看起来像相近的符号 | 前者指定作用域，后者要按初始化列表、范围循环等语境理解 |
| `= delete` 是不是在释放对象 | 它禁用特定函数；本例用它禁止复制 |
| 析构会分析对象是否还需要使用 | 自动存储期局部对象按作用域和退出规则析构 |
| 对象析构了，里面的资源就一定释放 | 资源所有者必须正确实现清理职责，裸指针不会自动释放所指内存 |
| 所有释放代码都必须写在 `~Buffer()` 里 | 成员容器会自动析构，可以把清理职责交给它们 |
| 输出 `PASS` 就证明所有路径都测过 | 需要检查对象在哪个作用域，以及日志是否支持测试结论 |

最初的三个自检问题，也可以重新回答：

**为什么提前 `return` 不需要手动释放本例的整数缓冲区？**  
退出作用域时，已完成构造且仍存活的局部 `Buffer` 会析构，其成员 `vector` 随之析构并释放管理的存储空间。我的原版里，`buffer` 则更早就因内层作用域结束而析构了。

**对象与资源生命周期是什么关系？**  
它们是不同层次的实体。RAII 类型通过所有权和清理实现，把资源的持有与对象生命周期关联起来；这不是任意裸指针天然具备的能力。

**为什么析构日志先于成员资源的释放？**  
因为先执行外层析构函数体，再析构成员。本例的日志记录了函数体执行到的位置，不能单独证明所有资源已经释放。

对后面的 C++/CUDA 学习，我会继续沿用这次形成的三个问题：**谁拥有资源？它活到什么时候？离开每一条路径时，谁负责清理？** 以后管理显存、文件和锁，也可以从这三个问题入手。智能指针与移动语义，则留给后续练习进一步学习。

---

## 配套代码与资料

- [原版完整代码](/downloads/cpp-raii-first-practice/raii_original.cpp)：保留我提交时的程序逻辑，仅还原聊天转义和换行。
- [原版真实输出](/downloads/cpp-raii-first-practice/original-output.txt)。
- [修正版完整代码](/downloads/cpp-raii-first-practice/raii_revised.cpp)：保留外层对象，增加独立的内层对象。
- [修正版完整验证输出](/downloads/cpp-raii-first-practice/revised-output.txt)。

本文依据《C++特殊符号语法解析》《解释析构与RAII》中的提问，以及《安排编程练习》中提交的代码、输出和自检回答整理。原始分享链接为[对话一](https://chatgpt.com/share/6ae0cf55-2128-83e9-9731-78217e8e9d0f)与[对话二](https://chatgpt.com/share/6ae1a430-874c-83e9-8d83-dacf30524eec)。技术解释已重新组织并核对参考资料。

整理时，两份配套程序均使用 MSVC 按 C++17 编译运行，原版输出与聊天记录一致，修正版三条路径通过断言并输出 `PASS`。本文列出的 GCC 命令为复现用法，本次未完成 GCC 实测，也未运行内存检查工具；日志与计数不构成无内存泄漏的证明。

