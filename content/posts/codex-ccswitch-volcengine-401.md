---
title: "Codex + cc-switch + 火山方舟 401 排坑全记录"
date: "2025-06-20"
draft: false
categories: ["踩坑"]
tags: ["Codex", "cc-switch", "火山方舟", "Agent"]
summary: "三层嵌套的 401：OAuth Token 过期 → API Key 变 null → 代理没生效。auth.json 的两条认证链路必须同时存在。"
---

## 环境

- **工具链**: Codex (VSCode 扩展) + cc-switch v3.16.1 + 火山方舟 Ark API
- **模型**: deepseek-v4-pro-260425
- **付费方式**: 火山方舟按量付费（非 Coding Plan）
- **关键配置**: `~/.codex/config.toml`, `~/.codex/auth.json`, `~/.cc-switch/settings.json`

---

## 最终正确配置

### `~/.codex/auth.json`

```json
{
  "auth_mode": "chatgpt",
  "OPENAI_API_KEY": "ark-你的api-key",
  "tokens": {
    "id_token": "...",
    "access_token": "...",
    "refresh_token": "...",
    "account_id": "..."
  },
  "last_refresh": "..."
}
```

很关键的点：**OAuth Token 和 OPENAI_API_KEY 必须同时存在**。前者维持 Codex 应用的登录态验证，后者用于实际的模型 API 请求（被 cc-switch 代理接管后替换为 PROXY_MANAGED，但 auth.json 里需要真实的 key 供代理读取）。

### `~/.cc-switch/settings.json`

```json
{
  "preserveCodexOfficialAuthOnSwitch": true
}
```

### `~/.codex/config.toml`（cc-switch 代理开启后自动生成）

```toml
model_provider = "huoshan_ds"
model = "deepseek-v4-pro-260425"

[model_providers.huoshan_ds]
name = "huoshan_ds"
base_url = "http://127.0.0.1:15721/v1"    # 代理自动改写
wire_api = "responses"                       # 保持不变
requires_openai_auth = true
experimental_bearer_token = "PROXY_MANAGED"  # 代理自动改写
```

---

## 问题链条：3 层嵌套的 401

我们遇到的不是单一原因，而是 **3 层问题依次暴露**，解一层露出下一层。

### 第 1 层：OAuth Token 过期

**错误信息**: `Your access token could not be refreshed because your refresh token was already used`

**现象**: 重新登录后正常，切一下供应商又坏了。

**根因**: `preserveCodexOfficialAuthOnSwitch = false`（默认值）。每次在 cc-switch 中切换供应商时，它会用数据库里存的旧版 `auth.json` 快照覆盖当前文件。那个快照里的 `refresh_token` 已经在某次刷新时被消耗掉了，Codex 再用它去换新 token 就报错。

**解决**: 把 `preserveCodexOfficialAuthOnSwitch` 改为 `true`。此选项的语义是：切换供应商时只改写 `config.toml`，不动 `auth.json`。

### 第 2 层：API Key 在重新登录后变为 null

**现象**: 开启 `preserveCodexOfficialAuthOnSwitch` 后重新登录，`OPENAI_API_KEY` 变成了 `null`。

**根因**: cc-switch 写入 `auth.json` 的策略是"全量覆盖"。"保持登录态"=  完全不写 `auth.json` → API Key 也不会被注入。`config.toml` 里也没有 API Key 字段（被代理改写前），于是 API Key 两边都找不到。

**解决**: **手动把 `OPENAI_API_KEY` 填入 `auth.json`**。因为 `preserveCodexOfficialAuthOnSwitch = true` 保证了这个文件不再被覆盖，填一次就一直有效。

### 第 3 层：代理看似"开了"但实际没生效

**现象**: API Key 齐全、Token 有效，仍然 401。代理日志没有任何请求记录，端口 15721 没有进程在监听。

**根因**: 两个子问题。

**(a) cc-switch 界面开关没真正写入数据库**

cc-switch 的代理界面开关有时候不会实际触发。验证方法：

```bash
cc-switch proxy show --app codex    # 看状态
# 如果显示「运行中: 否」，数据库里 enabled=0，那就是没开

cc-switch proxy enable --app codex  # CLI 强开
```

开启后验证 3 个信号：

```bash
ss -tlnp | grep 15721                # ① 端口监听
# 应该看到 cc-switch 进程在 LISTEN

cat ~/.codex/config.toml | grep base_url
# ② base_url 被改写为 http://127.0.0.1:15721

cat ~/.codex/config.toml | grep experimental_bearer_token
# ③ token 被改写为 PROXY_MANAGED
```

**(b) Codex 进程没有重启**

Codex 只在启动时读取 `config.toml`，运行中不热加载。我们改了配置后 Codex 进程还在用旧的内存缓存（直连 `https://ark.cn-beijing.volces.com/api/v3`）。

**解决**: 重启 Codex。VSCode 中 `Ctrl+Shift+P` → `Developer: Reload Window`，或关掉终端重开 Codex。

---

## 完整请求链路（代理开启后）

```
Codex (config.toml: base_url = 127.0.0.1:15721/v1)
  │
  │  POST /v1/responses
  │  Authorization: Bearer PROXY_MANAGED
  ▼
cc-switch 本地代理 (127.0.0.1:15721)
  │
  │  Responses API → Chat Completions API 协议转换
  │  PROXY_MANAGED → 真实 API Key
  ▼
火山方舟 Ark (https://ark.cn-beijing.volces.com/api/v3/chat/completions)
  │
  │  Authorization: Bearer ark-89f05401-xxx
  ▼
200 OK
```

## 核心认知

1. **cc-switch 的价值就是协议转换**。Codex 只发 OpenAI Responses API 格式，但 DeepSeek 等国产模型只支持 Chat Completions。cc-switch 在本地代理层完成这个转换。如果你不用代理，自己配 `base_url` 到火山方舟，对方根本不认 `/v1/responses` 端点。

2. **auth.json 两条认证链路**：OAuth Token 管 Codex 应用登录态，`OPENAI_API_KEY` 管模型请求。前者过期报 `refresh token` 错误，后者缺失报 `401`。两者独立但共存于同一个文件。

3. **preserveCodexOfficialAuthOnSwitch 的取舍**：
   - `false`: 切换供应商自动注入 API Key，但 OAuth 快照容易过期
   - `true`: OAuth 稳定，但需要手动维护 `OPENAI_API_KEY` → **推荐**

4. **Codex 没有热加载**：改任何配置 → 必须重启。

---

## 快速排查脚本

```bash
# 一键检查配置状态
echo "=== auth.json ==="
cat ~/.codex/auth.json | python3 -c "import sys,json; d=json.load(sys.stdin); print(f'  API_KEY: {\"SET\" if d.get(\"OPENAI_API_KEY\") else \"NULL\"}'); print(f'  last_refresh: {d.get(\"last_refresh\",\"N/A\")}')"

echo "=== config.toml ==="
grep -E "base_url|experimental_bearer_token|wire_api" ~/.codex/config.toml

echo "=== proxy ==="
ss -tlnp 2>/dev/null | grep 15721 && echo "  PORT: LISTENING" || echo "  PORT: NOT LISTENING"

echo "=== cc-switch ==="
cc-switch proxy show --app codex 2>/dev/null | grep -E "运行中|Codex:"
```

---

## 参考

- [火山方舟 Base URL 及鉴权文档](https://www.volcengine.com/docs/82379/1298459)
- [CC Switch v3.16.1 发布说明](https://github.com/farion1231/cc-switch/releases/tag/v3.16.1)
- [cc-switch FAQ](https://github.com/farion1231/cc-switch/blob/main/docs/user-manual/zh/5-faq/5.2-questions.md)
