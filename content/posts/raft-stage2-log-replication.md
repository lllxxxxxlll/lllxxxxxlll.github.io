---
title: "从零手写一个最小 Raft（二）：日志都有了，为什么命令还没生效？"
date: "2026-10-14"
draft: false
categories: ["工程与系统"]
tags: ["Raft", "Go", "分布式系统", "并发", "实践"]
summary: "从五节点实验中“日志都有了，命令却还没生效”的现象出发，梳理日志复制、多数派提交与状态机执行，并记录 Go 并发、索引边界和异步测试中的踩坑与修正。"
author: "Gfit"
---

> 系列上一篇：[Raft 实践（一）：领导选举]({{< relref "/posts/Raft实践1-领导选举.md" >}})。

在日志复制的测试里，我看到过一帧很奇怪的输出：五个节点都有了同一条命令，但部分节点的 `commitIndex` 还是 0，所有节点的 `lastApplied` 也都没有变化。

```text
节点       日志末条            commitIndex  lastApplied
0         {1 提交命令}         0            0
1 Leader  {1 提交命令}         1            0
2         {1 提交命令}         1            0
3         {1 提交命令}         0            0
4         {1 提交命令}         0            0
```

每份日志前面还有一条 `{Term:0}` 的哨兵，真正的命令位于 index 1。

当时我以为复制出了问题：大家都记下来了，为什么还没生效？后来才发现，我把三个观察点混在了一起——**日志存在、日志已提交、命令已执行。**

![五节点实验快照：相同日志，不同提交进度](https://cdn.jsdelivr.net/gh/lllxxxxxlll/images@85585310a2ce1b5fc6925d540b073f0866c0ff2c/blog/raft-stage2/fig07-snapshot-recap.png)

*图中的数据来自这次实验输出。节点依次采样，并非全局原子快照。*

这是我手写 Raft 的第二阶段记录。上一阶段搭起了选主与心跳的骨架，这一阶段开始让命令在节点之间复制。我用的是 Go 和同进程内存模拟 RPC，当前实现主要覆盖正常路径，还没有持久化和完整的故障测试。

下面从这帧输出出发，讲讲日志复制的流程，以及我被锁、channel、切片和测试断言绊住的过程。文中的修正代码是实现示例，需结合完整工程验证。

## 01 一条命令，要经过几个观察点？

客户端调用 `Start()`，Leader 先把命令追加到自己的日志，然后返回索引和任期。这一步只能说明：**当前 Leader 接受了提案。**

接下来，Leader 通过 `AppendEntries` 把日志发给 Follower。Follower 检查前缀是否匹配，处理冲突，再追加缺少的条目。Leader 收到成功回执后，更新对该节点的复制进度。

当某个当前任期的日志条目被多数节点复制，Leader 才能推进 `commitIndex`。后续 `AppendEntries` 携带 `LeaderCommit`，让 Follower 更新自己的提交认知。各节点的 applier 再把已提交日志交给状态机，状态机按顺序执行。

![一条命令的生命周期](https://cdn.jsdelivr.net/gh/lllxxxxxlll/images@85585310a2ce1b5fc6925d540b073f0866c0ff2c/blog/raft-stage2/fig01-pipeline.png)

*接受提案、复制、提交、交付和执行，是不同事件。*

我的实现按心跳周期发送日志，因此复制、提交通知和应用之间容易出现可观察的时间差。Raft 本身并不要求新命令必须等下一轮心跳才能复制；这是这一版实现的选择。空 `Entries` 的 `AppendEntries` 用作心跳，有条目时则同时承担日志复制。

回头看开篇的输出，就容易理解了：所有节点的日志都有 index 1，说明这些副本已记录命令；节点 1 和 2 的 `commitIndex=1`，说明它们已经知道该条目被提交；其余节点尚未反映这一提交进度。

一种可能的原因，是提交通知还没有传到它们。但仅凭这帧输出，不能确定每个 goroutine 当时运行到了哪里，也不能直接判断状态机是否已执行。判断执行结果，要看消费端实际更新后的状态。

## 02 分清自己的状态，和 Leader 的账本

这一阶段最容易搞混的是索引。我最初甚至以为 Follower 也需要维护 `matchIndex`。后来发现，只要分清观察视角，事情就清楚了。

| 状态 | 谁维护 | 含义 |
| --- | --- | --- |
| `log` | 所有节点 | 本地日志条目 |
| `commitIndex` | 所有节点 | 本节点已知的最高提交索引 |
| `lastApplied` | 所有节点，协议语义 | 已应用到状态机的最高索引 |
| `nextIndex[i]` | Leader | 下一次准备发给节点 i 的起始索引 |
| `matchIndex[i]` | Leader | 已确认节点 i 复制到的最高索引 |

![节点状态与 Leader 的复制账本](https://cdn.jsdelivr.net/gh/lllxxxxxlll/images@85585310a2ce1b5fc6925d540b073f0866c0ff2c/blog/raft-stage2/fig02-index-ledger.png)

*Follower 不需要用 nextIndex、matchIndex 计算提交点。*

`nextIndex` 表示下一次尝试从哪里发。Leader 刚上任时可以乐观地认为对方与自己一样长；前缀检查失败，再向前回退。

`matchIndex` 则来自成功回执，是已确认的进度。**计算多数派复制到了哪里，必须使用 `matchIndex`。** 在同一个 Leader 任期内，它应当单调前进；换任期后会重新初始化，不能跨任期理解为永远递增。

这里还有一个很容易漏的细节：如果数组包含 Leader 自己，就必须维护它自己的那一格。Leader 本地追加日志后，自己的 `matchIndex` 也应同步更新。

我曾经漏掉这一步。五节点里，Leader 和两个 Follower 都有 index 5，本来已经满足多数派。但记账结果却是：

```text
漏掉自己：[0, 5, 5, 0, 0] → 排序：[0, 0, 0, 5, 5]
正确记账：[5, 5, 5, 0, 0] → 排序：[0, 0, 5, 5, 5]
```

漏计自己会把提交点压低。三节点也一样：Leader 加一个 Follower 本来已经是多数，漏掉 Leader 就可能无法推进。

## 03 Start 返回，不代表写入已经成功

我原来复用了 `ApplyMsg` 作为 `Start` 的返回结果，用 `CommandValid` 表示是否接受命令。这样能跑，但容易混淆“接受提案”和“交付已提交命令”。博客里把两者拆开会更清楚：

```go
type StartResult struct {
    Index    int
    Term     int
    IsLeader bool
}

func (rf *Raft) Start(command interface{}) StartResult {
    rf.mu.Lock()
    defer rf.mu.Unlock()

    if rf.role != Leader {
        return StartResult{-1, rf.currentTerm, false}
    }

    rf.log = append(rf.log, LogEntry{
        Term: rf.currentTerm, Command: command,
    })
    index := len(rf.log) - 1
    rf.matchIndex[rf.id] = index
    rf.maybeAdvanceCommit() // 单节点也需要尝试提交
    return StartResult{index, rf.currentTerm, true}
}
```

这段示例假设 `maybeAdvanceCommit()` 在调用者已持锁时执行，并且没有日志压缩、index 0 是哨兵。

`IsLeader=true` 只表示节点在接受提案时是 Leader。之后它仍可能退位，条目也可能尚未复制到多数。客户端如果需要“写入成功”的语义，还要等相应条目提交，并按业务接口的约定等待执行结果。

命令追加后也应保持不可变。若 `command` 内部包含 map、slice 或指针，外部继续修改它，可能破坏日志内容的稳定性。

## 04 AppendEntries：先处理任期，再检查日志

Follower 收到复制请求后，处理顺序很重要：

1. 拒绝过期任期的请求。
2. 遇到更高任期，更新任期并退为 Follower。
3. 同任期收到 Leader 请求，也退为 Follower，但保留本任期的投票记录。
4. 重置选举计时器。
5. 验证 `(PrevLogIndex, PrevLogTerm)`。
6. 处理冲突与追加，最后更新提交进度。

我最初把计时器重置放在“复制成功”之后。但日志正在修复时，请求可能连续因前缀不匹配而失败。Follower 明明一直收到当前 Leader 的请求，却仍然超时发起选举，就会干扰修复。

因此，非过期任期的 Leader 请求应先刷新计时器，再检查日志是否匹配。返回的 `reply.Term` 也要反映更新后的任期。

下面是关键处理逻辑的示例；更高任期时 `becomeFollower` 应重置投票状态：

```go
func (rf *Raft) AppendEntries(
    args AppendEntriesArgs, reply *AppendEntriesReply,
) {
    rf.mu.Lock()
    defer rf.mu.Unlock()
    reply.Term, reply.Success = rf.currentTerm, false

    if args.Term < rf.currentTerm {
        return
    }
    if args.Term > rf.currentTerm {
        rf.becomeFollower(args.Term)
    }
    rf.role = Follower
    rf.resetElectionTimeout()
    reply.Term = rf.currentTerm

    if args.PrevLogIndex < 0 ||
        args.PrevLogIndex >= len(rf.log) {
        return
    }
    if rf.log[args.PrevLogIndex].Term != args.PrevLogTerm {
        return
    }

    for i, e := range args.Entries {
        idx := args.PrevLogIndex + 1 + i
        if idx < len(rf.log) {
            if rf.log[idx].Term == e.Term {
                continue
            }
            rf.log = rf.log[:idx]
            rf.log = append(rf.log, args.Entries[i:]...)
            break
        }
        rf.log = append(rf.log, args.Entries[i:]...)
        break
    }

    lastVerified := args.PrevLogIndex + len(args.Entries)
    newCommit := min(args.LeaderCommit, lastVerified)
    if newCommit > rf.commitIndex {
        rf.commitIndex = newCommit
    }
    reply.Success = true
}
```

正常 Raft 协议中，已提交日志不应被新 Leader 的日志冲突覆盖。如果这一不变量被破坏，需要排查选举限制、提交规则或其他实现错误。

### 为什么只比较任期，不比较 command？

Raft 的 Log Matching Property 保证：同一 index、同一 term 的条目相同，而且之前的前缀也相同。因此，遵守协议的节点可以用 `(index, term)` 判断匹配。

这不是任意两份数组都具备的性质。它依赖日志追加、选举和提交逻辑共同遵守 Raft 的约束。

### Follower 的提交上限，不能只看本地长度

我原来的写法是：

```go
min(args.LeaderCommit, len(rf.log)-1)
```

问题是，本地可能还有本次 RPC 没有验证过的旧后缀。比如本地日志到 index 8，当前 RPC 只验证到 index 5，LeaderCommit 是 7，直接按本地长度取上限就可能把旧后缀提交掉。

应当把提交推进限制在**本次 RPC 已验证的末尾**：

```go
lastVerified := args.PrevLogIndex + len(args.Entries)
newCommit := min(args.LeaderCommit, lastVerified)
```

空心跳的 `Entries` 长度为 0，此时已验证上限就是 `PrevLogIndex`。新值只有大于当前 `commitIndex` 时才更新，提交索引不能回退。

![找到共同前缀，修复冲突后缀](https://cdn.jsdelivr.net/gh/lllxxxxxlll/images@85585310a2ce1b5fc6925d540b073f0866c0ff2c/blog/raft-stage2/fig04-consistency-fallback.png)

*前缀检查失败时回退 nextIndex；前缀匹配后，只在遇到冲突条目时截断后缀。*

## 05 多数派提交，不能无条件取中位数

Leader 根据成功回执维护 `matchIndex`。对固定 N 个节点，多数派大小是：

```go
quorum := N/2 + 1
```

将所有节点的 `matchIndex` 升序排序后，多数派已达到的最高候选位置为 `N-quorum`：

```go
// 调用者持锁；matchIndex 包含 Leader 自己。
func (rf *Raft) maybeAdvanceCommit() {
    if rf.role != Leader {
        return
    }
    tmp := append([]int(nil), rf.matchIndex...)
    sort.Ints(tmp)

    quorum := len(tmp)/2 + 1
    candidate := tmp[len(tmp)-quorum]
    if candidate > rf.commitIndex &&
        rf.log[candidate].Term == rf.currentTerm {
        rf.commitIndex = candidate
    }
}
```

三节点和五节点时，这恰好是中位数。但四节点需要三个节点同意，不能照搬 `tmp[len(tmp)/2]`：

```text
排序后：[0, 0, 5, 5]
多数派：3 个节点
候选位置：4 - 3 = 1
候选索引：0
```

只有两个节点达到 index 5，还没有满足多数。

![多数派提交点与任期检查](https://cdn.jsdelivr.net/gh/lllxxxxxlll/images@85585310a2ce1b5fc6925d540b073f0866c0ff2c/blog/raft-stage2/fig03-commit-median.png)

*先求多数派复制进度，再检查候选条目的任期。*

另一个不能省的条件是 `rf.log[candidate].Term == rf.currentTerm`。

**Leader 不能仅凭副本计数，直接提交旧任期条目。** 它可以通过提交当前任期的一条日志，间接提交其之前的旧日志。这是论文 Figure 8 讨论的安全边界。

日志安全还依赖选举限制：RequestVote 必须比较候选人的末条日志任期和索引，先比 `LastLogTerm`，相同时再比 `LastLogIndex`。如果阶段一只实现了任期和投票数，加入日志后就需要补上这项检查。

### 异步回执，还要防乱序

发出 RPC 时，要记下当时的任期、目标节点和发送范围。请求返回后，重新加锁检查：自己是否仍是该任期的 Leader？如果响应带有更高任期，应先按协议退位；已经过时的复制结果则不能更新当前任期的账本。

成功回执确认的末尾为：

```go
matched := args.PrevLogIndex + len(args.Entries)
rf.matchIndex[peer] = max(rf.matchIndex[peer], matched)
rf.nextIndex[peer] = max(rf.nextIndex[peer], matched+1)
```

这段只是进度更新示意。完整实现还要防止旧失败回执回退已经成功推进的 `nextIndex`。教学版本可先让每个 Follower 的复制请求串行执行；若允许并发，就需要跟踪请求版本或明确的回退条件。

## 06 我真正卡住的地方：锁、channel 和切片

读论文时，日志复制看起来很顺。写成 goroutine 后，我才发现，一个看似无关的并发细节，就能让整条链路停住。

### 持锁发送 channel，让节点失去响应

如果 applier 持有 `rf.mu`，又向无人接收的 `applyCh` 发送消息，它会一直等接收者。与此同时，处理选举和复制的 goroutine 也在等这把锁，整个节点就可能卡住。

我后来把职责拆开：**锁内读取已提交日志，锁外交付消息。** 发送可能阻塞，但不能一直占住协议状态的锁。

这里还藏着一个命名问题。原代码在锁内先增加 `lastApplied`、构造一批消息，解锁后才发送。于是这个变量实际记录的是“已经取出待交付的位置”，无法证明消息已经送达，更无法证明外部状态机已经执行。

更清楚的设计，是把三个进度分别记录：

| 进度 | 推进时机 |
| --- | --- |
| 待交付游标 | applier 取出日志时 |
| 已交付索引 | 消息成功发送后 |
| 实际执行索引 | 状态机执行完成后 |

由一个 applier 顺序交付，再由状态机顺序执行。如果 Raft 层需要准确知道执行完成的位置，就必须有相应的确认机制。接收一条 `ApplyMsg` 和执行完这条命令，仍然是两件事。

### 切片拷贝，解决不了所有共享问题

```go
entries := rf.log[ni:]
```

这只创建了切片视图，仍然共享底层数组。解锁后发送 RPC，其他 goroutine 可能同时修改日志。

可以在锁内复制条目：

```go
entries := append([]LogEntry(nil), rf.log[ni:]...)
```

但这是元素级浅拷贝。如果 `Command` 包含指针、map 或 slice，它们仍可能指向同一份可变数据。我的实现需要明确“提案后命令不可变”的约定，或者建立深拷贝、序列化边界。

### 其余几个小错误，也足够让测试卡住

| 错误 | 结果 | 修改方式 |
| --- | --- | --- |
| 同一 goroutine 重复 Lock | Mutex 不可重入，死锁 | 一次加锁，对应一次解锁 |
| 给 reply 指针变量重新赋值 | 调用方结果没变化 | 修改 `reply.X` 或 `*reply` |
| 判断索引用 `> len(log)` | 等于长度时仍越界 | 先判 `>=`，再访问 |
| 忘记日志末尾追加分支 | Follower 日志无法增长 | 同时覆盖已有位置和新位置 |
| 相同条目再次 append | 日志重复 | 匹配时跳过 |
| 哨兵实现 nextIndex 退到 0 | prevIndex 变成 -1 | 以 1 为回退下限 |
| 循环内 defer Unlock | 锁延迟到函数退出才释放 | 缩小作用域或显式解锁 |

![并发与边界检查提示](https://cdn.jsdelivr.net/gh/lllxxxxxlll/images@85585310a2ce1b5fc6925d540b073f0866c0ff2c/blog/raft-stage2/fig05-concurrency-cheatsheet.png)

*这些实现细节决定协议 goroutine 能否继续运行。*

还有一个容易被旧教程误导的点：Go 1.22 起，在对应语言版本下，`for ... := range ...` 的循环变量逐次创建，不能再无条件地说“所有闭包都捕获同一个循环变量”。外部复用变量和共享可变对象仍然需要检查。显式传入节点参数后，也应在函数体里使用该参数。

## 07 测试名字，要和真正的断言对得上

这个阶段，我从测试里学到的东西不比 Raft 少。

最开始，我把等待函数叫作 `waitCommit`，里面却只检查各节点是否存在同一条日志。后来才意识到：名字写的是提交，验证的却是复制。这会让测试和理解一起偏掉。

应该按观察层次区分：

| 等待函数 | 检查条件 | 能证明什么 |
| --- | --- | --- |
| `waitReplicated` | 指定节点存在对应 term、command 的日志 | 副本已有条目 |
| `waitCommitted` | 指定节点 commitIndex 到达目标，且日志正确 | 节点已知提交 |
| `waitDelivered` | 消费端收到对应 ApplyMsg | 消息已交付 |
| `waitApplied` | 状态机已执行，结果符合预期 | 命令实际生效 |

![异步流程中的不同观察时刻](https://cdn.jsdelivr.net/gh/lllxxxxxlll/images@85585310a2ce1b5fc6925d540b073f0866c0ff2c/blog/raft-stage2/fig06-timeline-snapshot.png)

*示意一种可能的调度顺序，不是实测延迟图，也不要求所有节点同时推进。*

异步测试需要等待目标状态出现，不能调用 `Start()` 后立刻断言最终结果。下面给出等待复制的示例；`nReplicated` 应在锁内检查日志内容，并验证目标 term 和 command：

```go
func waitReplicated(
    t *testing.T, peers []*Raft,
    index, expected int,
) {
    t.Helper()
    deadline := time.Now().Add(2 * time.Second)
    count := 0
    for time.Now().Before(deadline) {
        count = nReplicated(t, peers, index)
        if count >= expected {
            return
        }
        time.Sleep(10 * time.Millisecond)
    }
    t.Fatalf("replication timeout: want %d, got %d",
        expected, count)
}
```

轮询是这个教学测试的简单实现，实际工程也可以用事件通知等待。关键是有明确条件、有截止时间，失败时能打印诊断信息。

测试启动时，要同时有选举与复制循环、applier，以及 `applyCh` 消费者。缺少 applier，日志不会交付；缺少消费者，发送就可能阻塞。后台 goroutine 还需要停止机制，避免测试结束后继续运行。

我还踩过两个很基础的坑：`go test -run Basic` 匹配不到 `TestStart`，看见 PASS 却其实没有运行目标测试；哨兵 index 0 不会被应用，提交一条命令却等待两条输出，必然超时。

我常用的检查命令是：

```bash
go vet ./my_raft
go test -race -v ./my_raft
go test -race -v -run TestStart ./my_raft
go test -race -count=5 -run TestStart ./my_raft
```

`go vet` 检查特定静态问题，并不保证发现切片越界。`-race` 检查实际执行路径上的数据竞争，多次运行有助于暴露偶发错误。这些检查通过，也不能证明 Raft 的容错与安全性已经完整实现。

接下来我需要补的测试包括：单节点提交、三节点只复制到 Leader 加一个 Follower、四节点多数门槛、重复请求、冲突后缀、空心跳不误提交旧后缀，以及旧回执乱序和消费者暂停后的恢复。

## 08 从内存 RPC 到真实网络，还有多远？

现在的调用是：

```go
peer.RequestVote(args, &reply)
```

双方在同一进程中，被调方修改 `reply` 指向的内存，调用方就能看到结果。

真实网络没有共享地址空间，需要序列化请求、传输、反序列化、执行，再把响应传回来。Go `net/rpc` 的调用形式仍可以接收本地的 `reply` 指针，但填进去的是反序列化后的响应；它与远端的内存没有关联。

协议 handler 的逻辑可以复用，但迁移到真实网络不只是替换一次函数调用。超时、失败重试、并发回执、停机机制，以及崩溃恢复所需的持久化，都需要明确处理。

## 09 这一阶段，我改变了什么理解？

以前我觉得日志复制就是“Leader 把数据发给其他节点”。真正写下来后，才发现更关键的是：谁知道这条日志可以提交？提交信息怎样传播？状态机什么时候执行？测试到底验证了哪个事件？

开篇那帧输出因此成了我最有用的观察材料。它让我开始把每个字段、每个 goroutine 和每个断言，放回它所属的那一步。

这一版还只是正常路径下的最小教学实现。`currentTerm`、`votedFor` 和日志还没有持久化；节点宕机、网络分区和选举交替下的安全性，也需要继续验证。

下一阶段，我准备把这些“正常情况下看起来合理”的代码放进故障场景：模拟 Leader 崩溃和网络分区，检查日志修复，优化按任期回退，并复现 Figure 8 中旧任期日志不能直接计数提交的反例。

到那时，再看今天这帧日志，应该会有新的理解。

---

参考资料：

- [Raft 扩展论文](https://raft.github.io/raft.pdf)：Figure 2、§5.3、§5.4、Figure 8。
- [Go 循环变量语义说明](https://go.dev/wiki/LoopvarExperiment)：Go 1.22 的循环变量变化与版本条件。
- [Go net/rpc 文档](https://pkg.go.dev/net/rpc)：RPC 方法约定与调用接口。