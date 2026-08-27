---
title: "Eino 学习路线与核心问题清单"
date: "2025-06-24"
draft: false
categories: ["Agent"]
tags: ["Eino", "学习路线", "Agent", "Go"]
summary: "通过 OncallAgent 实战掌握 Eino 的五阶段路线 + 8 个核心问题清单（P0 卡点 / P1 理解层 / P2 架构层），从「能看懂代码」进阶到「能独立设计、能面试」。"
---

## 目标

通过 **OncallAgent** 项目实战，掌握字节跳动开源的 Go 语言 Agent 框架 **Eino**，达到大厂 Agent 面试水平。这条线的起点是：能看懂代码，但没独立写过；目标是从「能看懂」进阶到「能独立设计、能白板讲清楚」。

## 五阶段路线

| 阶段 | 目标 | 状态 |
|---|---|---|
| 一 | 跑通 `文件 → 加载 → 切分 → 向量化 → 存储` 完整链路 | ✅ 完成 |
| 二 | 改造 test 代码：Lambda 节点 / 具体类型 / Stream vs Invoke / 错误处理 | 🔄 进行中 |
| 三 | 读 Eino 核心源码：`Compile()` / `Invoke()` / `Stream()` | ⬜ 待开始 |
| 四 | 独立实现一个「天气查询 + 内部文档 RAG」Agent | ⬜ 待开始 |
| 五 | 面试准备：白板画架构 + 口头讲概念 + 对比问题 | ⬜ 待开始 |

当前整体掌握度约 **65%**：使用层、原理层已推进，架构层刚开始。

## 8 个核心问题清单

这是我在学习过程中梳理出来的「概念地图」，每个都是面试高频考点，攻克的顺序也按依赖关系排。

### P0 —— 当前卡点（卡住开发）

1. **InputKey / OutputKey 的匹配规则**：为什么生产代码有时设 Key、有时不设？不设 Key 时 Eino 怎么自动推断？泛型参数怎么映射为 START 节点的输入？→ 已攻克，见 [InputKey/OutputKey 机制深度分析](/posts/eino-inputkey-outputkey-mechanism/)。
2. **`Compile()` 内部做了什么**：DAG 无环校验、拓扑排序、`AnyPredecessor`/`AllPredecessor` 的语义、类型检查在编译期还是运行时。

### P1 —— 理解层（跑通 test 后深挖）

3. **Lambda 节点的本质**：函数签名怎么和上下游对齐？`InvokableLambdaWithOption` vs `AnyLambda`。
4. **Tool Calling 的完整数据流**：LLM 输出 tool_call → 解析 → 匹配工具名 → 调用 → 结果拼回消息 → 下一轮推理，以及 `MaxStep` 到达后如何终止。
5. **Stream 和 Invoke 的本质区别**：`StreamReader` 底层是什么？流式下状态字典还在吗？一次性输出节点（如 Indexer）怎么适配流式。

### P2 —— 架构层（面试加分）

6. **Eino Graph 的并发模型**：节点间是并行 goroutine 还是串行？`AllPredecessor` 的等待机制？状态字典有没有并发安全问题。
7. **Eino vs LangChain/LangGraph 选型**：类型安全（Go 泛型编译期 vs Python 运行时）、性能（无 GIL）、生态、团队适配。
8. **错误处理与容错**：一个节点出错整图终止还是可 fallback？Tool 失败后 ReAct 怎么感知重试？`OnErrorFn` 能否拦截传播。

## 已经沉淀出来的内容

- [Eino InputKey/OutputKey 机制深度分析](/posts/eino-inputkey-outputkey-mechanism/) —— 攻克问题 1 的完整记录
- [Redis Stack 向量库改造实录](/posts/redis-stack-vs-milvus-vector-store/) —— 阶段二的里程碑，端到端跑通 RAG 检索

> 这个路线会持续更新。每攻下一个问题，我会把结论写成独立文章，并在本篇的清单里打上链接。
