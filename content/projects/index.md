---
title: "项目"
description: "李贤的项目作品集"
---

## OncallAgent —— AI 智能值班助手（核心作品）

基于字节跳动开源 **Eino** 框架的 Go 语言 AI Agent 系统，实现告警接入 → 日志查询 → 知识检索 → 根因分析的全自动链路。

**技术栈**：Go · Eino v0.7 · DeepSeek V3 · Milvus · MCP · GoFrame v2 · Prometheus · MySQL

**多 Agent 架构**

- 3 套 Agent 模式：Chat Pipeline（RAG+ReAct）/ Knowledge Index Pipeline / Plan-Execute-Replan
- 双模型分工：DeepSeek Think 做规划 + DeepSeek Quick 做执行
- 5 个 Agent 工具：日志查询（MCP/CLS）/ 告警查询（Prometheus）/ 知识库 RAG（Milvus）/ 数据库操作（MySQL）/ 时间感知
- MCP 协议对接腾讯云 CLS 日志服务

**技术决策**

- 选 Go + Eino 而非 Python + LangChain
- 双 Agent 模式共存、双模型分工（Think 强推理 vs Quick 低成本）
- MCP 协议集成 —— 面向未来的工具调用标准

**当前进展**：3 套 Agent Graph 均 Compile 通过；RAG 检索链路（Milvus）已验证；MCP 日志工具已集成；阶段二进行中（Lambda 节点 + Tool Calling）

**待攻克**：Prometheus 查询空壳 → 对接真实告警；MySQL stdin 阻塞 → Agent 场景致命；System prompt 硬编码 → 配置化

[GitHub →](https://github.com/lllxxxxxlll/oncallAgent)

---

## 智能客服 Agent 系统

SpringBoot + **LangChain4j** 构建的 AI 客服对话系统。

**技术栈**：Java · SpringBoot · LangChain4j · MySQL · MyBatis · Redis

**技术亮点**

- Restful 架构，Controller-Service-Client 分层
- IO 密集场景 CompletableFuture + CallerRunsPolicy
- AOP SQL 审计 + 唯一键去重

**性能优化**

- Redis 缓存 API 热点数据
- 200+ 行 XML SQL 从 500ms → 50ms（90% 优化）

---

## AI 视觉检测系统

PaddlePaddle / PyTorch + YOLO + TensorRT 工业级缺陷检测，LLM 智能判伤。

**技术栈**：Python · PyTorch · YOLO · TensorRT · LLM

**模型优化**

- MAE Loss 优化至 0.05
- albumentation 增强 → YOLO 检测率 95%

**部署**

- TensorRT Int16 量化推理
- 端到端准确率 95%
