---
title: "Raft实践1--领导选举"
date: "2026-08-19T00:48:18+08:00"
summary: "这是手写 Raft 的第一阶段记录： 实现领导选举 + 心跳压制，还没有日志复制。 麻雀虽小，却把 Raft 里最容易踩坑的并发、任期、超时都涉及到了。 文章按我“真正踩过的坑”来组织，希望能帮到同样在啃 Raft 的你。 Raft 心智模型：常驻循环与回调 0. 心智模型：Raft 是一堆协作的 goroutine Raft 不是“一个函数跑到底”，而是“…"
source: "https://mp.weixin.qq.com/s/qG8Duz7y7P-hy7uV4ZDF3g"
categories: ["微信公众号"]
tags: ["公众号迁移"]
author: "gFIT.1"
---

> 本文首发于微信公众号 **gFIT.1**，[原文链接](https://mp.weixin.qq.com/s/qG8Duz7y7P-hy7uV4ZDF3g)。

> 这是手写 Raft 的第一阶段记录：  
> 实现领导选举 + 心跳压制，还没有日志复制。  
> 麻雀虽小，却把 Raft 里最容易踩坑的并发、任期、超时都涉及到了。  
> 文章按我“真正踩过的坑”来组织，希望能帮到同样在啃 Raft 的你。

![Raft 心智模型：常驻循环与回调](/images/posts/Raft实践1-领导选举/3ebfebf5bcd6.png)

Raft 心智模型：常驻循环与回调

## 0. 心智模型：Raft 是一堆协作的 goroutine

Raft 不是“一个函数跑到底”，而是“常驻后台循环 + 被动 RPC 回调”在同一份状态上并发协作：

- 常驻循环：`ticker()`（每个节点都有）、`heartbeatLoop()`（仅 Leader）
- 被动回调：`RequestVote()`（来拉票）、`AppendEntries()`（Leader 发心跳）

节点在三种角色间转换：

![角色状态机：Follower → Candidate → Leader](/images/posts/Raft实践1-领导选举/21c6622546a4.png)

角色状态机：Follower → Candidate → Leader

- 超时未收到心跳 → Follower 变 Candidate
- 获得多数票 → Candidate 变 Leader
- 发现更大 term 或收到合法 Leader 心跳 → 退回 Follower

---

## 1. term：没有全局时钟时的“逻辑时钟”

> 铁律：任何 RPC 的请求或响应中，只要看到对方 `term > currentTerm`，立刻更新 `currentTerm` 并转为 Follower。

![Term 作为逻辑时钟：只随 RPC 传播](/images/posts/Raft实践1-领导选举/680de0f250b4.png)

Term 作为逻辑时钟：只随 RPC 传播

- term 只能“搭 RPC 的车”传播，因此“每一次 RPC 收发都要对表”。
- 四个信息入口都要做 term 检查：`RequestVote`/`AppendEntries`（收请求）、`startElection`/`heartbeatLoop`（收回复）。

代码要点（Go 伪码）：

```
if incoming.Term > rf.currentTerm {  
    rf.becomeFollower(incoming.Term)  
}
```

---

## 2. 选举计时器：随机化 + 三个重置时机

![随机化选举超时](/images/posts/Raft实践1-领导选举/88f3b6546455.png)

随机化选举超时

- 为什么随机：避免所有节点同时超时 → 同时发起选举 → 互相瓜分选票 → 无人过半，陷入振荡。
- 三次重置：发起选举、投票给别人、收到合法心跳（含义都是“系统还活着”）。

```
func (rf *Raft) resetElectionTimeout() {  
    rf.electionResetEvent = time.Now()  
    rf.electionTimeout = time.Duration(300+rand.Intn(300)) * time.Millisecond  
}
```

`ticker` 只负责“到点就发起选举”：

```
func (rf *Raft) ticker() {  
    for {  
        rf.mu.Lock()  
        if rf.role != Leader && time.Since(rf.electionResetEvent) >= rf.electionTimeout {  
            rf.startElection()  
        }  
        rf.mu.Unlock()  
        time.Sleep(10 * time.Millisecond)  
    }  
}
```

---

## 3. 异步计票：过半即当选（不要等所有票）

![异步计票：过半即当选](/images/posts/Raft实践1-领导选举/737290e2b75e.png)

异步计票：过半即当选

- 不要用 `WaitGroup` 等所有票：会攥着锁等网络，被挂掉的节点拖住。
- 正确姿势：每张票在自己的回调里持锁累加；一旦过半立即当选，剩下的不必等待。

关键检查：

```
if rf.role != Candidate || rf.currentTerm != args.Term { return } // 这张票过期了  
if reply.Term > rf.currentTerm { rf.becomeFollower(reply.Term); return } // 我过期，退位  
if reply.VoteGranted { votes++; if votes > len(rf.peers)/2 { rf.becomeLeader() } }
```

---

## 4. 并发与锁：两条纪律避免死锁

![避免死锁的两条纪律](/images/posts/Raft实践1-领导选举/c2be221e8d60.png)

避免死锁的两条纪律

- 纪律 A：handler 自己管自己的锁，调用方不代劳（避免同一把锁重入）。
- 纪律 B：发 RPC 之前先释放自己的锁（避免长占锁与 AB-BA 循环）。

---

## 5. 选举安全：至多一个 Leader

![选举安全：至多一个 Leader](/images/posts/Raft实践1-领导选举/620825458730.png)

选举安全：至多一个 Leader

- 一任期一票：任意两个多数派必有交集，交集节点同任期只投一票 → 不会双主。
- 日志“至少与我一样新”才投票：为后续日志安全埋下伏笔。

```
uptoDate := args.LastLogTerm > lastTerm ||  
    (args.LastLogTerm == lastTerm && args.LastLogIndex >= lastIndex)  
if (rf.votedFor == -1 || rf.votedFor == args.CandidateId) && uptoDate {  
    rf.votedFor = args.CandidateId  
    reply.VoteGranted = true  
    rf.resetElectionTimeout()  
}
```

---

## 6. 心跳：Leader 如何稳住局面

![Leader 心跳压制](/images/posts/Raft实践1-领导选举/4a3c6208074e.png)

Leader 心跳压制

- 当选即启动心跳循环，持续广播；Follower 收到合法心跳就重置计时器，不再发起选举。

```
func (rf *Raft) becomeLeader() {  
    rf.role = Leader  
    go rf.heartbeatLoop()  
}  
  
func (rf *Raft) AppendEntries(args AppendEntriesArgs, reply *AppendEntriesReply) {  
    rf.mu.Lock(); defer rf.mu.Unlock()  
    if args.Term > rf.currentTerm { rf.becomeFollower(args.Term) }  
    reply.Term = rf.currentTerm  
    if args.Term < rf.currentTerm { reply.Success = false; return }  
    rf.resetElectionTimeout()  
    reply.Success = true  
}
```

---

## 7. 我踩过的坑清单（速查）

![常见坑速查海报](/images/posts/Raft实践1-领导选举/31710710411f.png)

常见坑速查海报

- 计票写在发送循环后面 → 永远选不出 Leader（回调未归来时就判断）
- `WaitGroup + 持锁等票` → 卡死/被拖住
- 调用方额外 `peer.mu.Lock()` + handler 内再 lock → 死锁
- `makeRaft` 忘了哨兵日志 → 索引越界
- 选出 Leader 不发心跳 → term 疯涨、反复选举
- 超时值每次循环重算 → 随机化失效（应在重置时 roll 一次）

---

## 8. 结语

这就是 Raft 的“第一性原理 + 最小可跑”的一套。下一篇会在此基础上实现日志复制与提交（AppendEntries 携带条目、nextIndex/matchIndex 推进、提交规则、安全性证明）。

欢迎交流
