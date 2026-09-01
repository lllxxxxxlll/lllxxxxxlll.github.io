---
title: "github特性1"
date: "2026-08-21T12:16:03+08:00"
summary: "结论先说：GitHub 天生是“版本化资产库”。把图片放入仓库（源站），用 PicGo（Contents API）写入，再用 jsDelivr 作为公共 CDN 加速，就能快速搭好零成本图床。它适合个人与低量访问；当你需要可控缓存、日志、私有访问与 SLA 时，迁移到“对象存储 + 商业 CDN”。本文以真实过程为线索，突出 GitHub 特性、一个可复现案…"
source: "https://mp.weixin.qq.com/s/pFJkGNOIb-qJBoRdmoRDLg"
categories: ["微信公众号"]
tags: ["公众号迁移"]
author: "gFIT.1"
---

> 本文首发于微信公众号 **gFIT.1**，[原文链接](https://mp.weixin.qq.com/s/pFJkGNOIb-qJBoRdmoRDLg)。

> 结论先说：GitHub 天生是“版本化资产库”。把图片放入仓库（源站），用 PicGo（Contents API）写入，再用 jsDelivr 作为公共 CDN 加速，就能快速搭好零成本图床。它适合个人与低量访问；当你需要可控缓存、日志、私有访问与 SLA 时，迁移到“对象存储 + 商业 CDN”。本文以真实过程为线索，突出 GitHub 特性、一个可复现案例，以及企业级的初步认识。

## 0. 成果预览

- 样例 CDN（@latest）：![https://cdn.jsdelivr.net/gh/lllxxxxxlll/images@latest/img/2026/08/19/8ad1398d04711b9f1e2cce388c0ccadd.png](/images/posts/github特性1/bdea2cc2ac6a.png)

  https://cdn.jsdelivr.net/gh/lllxxxxxlll/images@latest/img/2026/08/19/8ad1398d04711b9f1e2cce388c0ccadd.png
- 对应 raw（等价换算）：

- 把 `cdn.jsdelivr.net/gh/<u>/<r>@<ver>/...` 换成 `raw.githubusercontent.com/<u>/<r>/<ver>/...`

## 1. GitHub 认知与特性（能力与边界）

### GitHub 其他能力速览（与本案可结合）

- 工作流自动化（Actions）

- 用处：上传前自动压缩图片、生成 WebP/AVIF、重复文件去重、链接校验、把仓库中的图片同步到对象存储。
- 与本案结合：对 `img/` 目录的提交触发优化；发布时打 tag，生成 Release 资产，对应 jsDelivr 的 `@tag`。
- 示例（最小可用，压缩 PNG/JPEG 并生成 WebP）:

  ```
  name: optimize-images  
  on:  
    push:  
      paths: ['img/**']  
  jobs:  
    optimize:  
      runs-on: ubuntu-latest  
      steps:  
        - uses: actions/checkout@v4  
        - uses: actions/setup-node@v4  
          with: { node-version: '20' }  
        - run: |  
            npm i -g sharp-cli  
            find img -type f \( -iname '*.png' -o -iname '*.jpg' -o -iname '*.jpeg' \) | while read f; do  
              sharp "$f" --quality 80 --withoutMetadata --progressive --output "$f"  
              sharp "$f" --quality 80 --withoutMetadata --progressive --output "${f%.*}.webp"  
            done  
        - name: Commit optimized assets  
          run: |  
            git config user.name "github-actions"  
            git config user.email "actions@github.com"  
            git add -A  
            git diff --cached --quiet || git commit -m "chore(img): optimize & webp"  
            git push
  ```

- 版本发布（Tags / Releases）

- 用处：把某一批图片打版本，对应 jsDelivr 的 `@tag`；Release 附件可作为离线包备份。

- Pages（静态站）

- 用处：为图床写一个只读浏览/搜索页；记录“命名规范/版本位使用”。

- 安全与合规

- 分支保护（避免误改历史）、Secret Scanning（防止泄露 Token）。

- API / Webhook / Apps

- 用处：批量索引图片（GraphQL）、上传完成后通知站点预热、以 GitHub App 获取更细粒度权限。

- 版本化快照：同一路径覆盖后，旧版本仍保存在历史提交；文件页按 `y` 得到含提交 SHA 的永久链接（permalink）。
- 最小权限：细粒度 Token（Fine‑grained），Repository access 选目标仓库；Repository permissions 添加 `Contents: Read & Write`。
- 版本位三种：分支（如 `main`）、提交 `@<sha>`、标签 `@<tag>`；jsDelivr 还提供 `@latest` 指向默认分支最新提交。
- API 写入：PicGo 通过 Contents API PUT 文件；覆盖需要带旧 `sha`；频繁写入会遇到 rate limit（个体使用通常无感）。
- 不是对象存储：缺少自定义响应头、精细缓存、日志与鉴权；不适合高流量商业分发。

![架构总览：源站→CDN](/images/posts/github特性1/d991f2ae32f2.jpeg)

架构总览：源站→CDN

## 2. 案例：GitHub 源站 + PicGo 写入 + jsDelivr 分发

**步骤清单（可复现）**

1. 1. 创建公开仓库 `images`（默认分支 `main`），网页端初始化 `README.md` 与 `img/.gitkeep`。
2. 2. 生成细粒度 Token：Repository access 勾选该仓库；Repository permissions 添加 `Contents: Read & Write`。  
   ![如图所示](/images/posts/github特性1/47160581dbd3.jpeg)

   如图所示

     
   ![GitHub PAT Contents 权限](/images/posts/github特性1/e36ea86f1f49.png)

   GitHub PAT Contents 权限
3. 3. 安装 PicGo-Core：`npm i -g picgo`（或用桌面版）。
4. 4. 配置上传器：`repo`、`branch: main`、`path: img/`、`customUrl: https://cdn.jsdelivr.net/gh/<u>/<r>@latest`。
5. 5. 安装并启用 `picgo-plugin-rename-file`，模板 `"{y}/{m}/{d}/{origin}-{hash}"`。
6. 6. 上传一张图：`picgo -d u /abs/path/foo.png`；返回 CDN 链接并验证 raw/CDN。
7. 7. 日常用 `@latest`；需要稳定引用时改成 `@<sha>` 或 `@tag`。

> 一条主线：把图写入仓库 → 直接得到 CDN 链接 → 以“语义+哈希+日期”的命名获得强缓存与可读性。

![上传流水线：本地→PicGo→Repo→CDN](/images/posts/github特性1/a321ad288513.png)

上传流水线：本地→PicGo→Repo→CDN

### 2.1 基础配置（PicGo → GitHub）

- Uploader 选 GitHub；关键参数：

- `repo: lllxxxxxlll/images`
- `branch: main`
- `path: img/`
- `customUrl: https://cdn.jsdelivr.net/gh/lllxxxxxlll/images@latest`（让 PicGo 直回 CDN 链接）
- ![PicGo 配置成功输出](/images/posts/github特性1/e6d9dacfd9c8.png)

  PicGo 配置成功输出

### 2.2 命名策略：语义 + 哈希 + 日期

> 目标：既可读，又不可变，CDN 可长期强缓存。

![命名策略：img/YYYY/MM/DD/slug-hash.ext（AI 占位）](/images/posts/github特性1/135a168085c5.png)

命名策略：img/YYYY/MM/DD/slug-hash.ext（AI 占位）

- 上传器 `path` 提供前缀：`img/`
- 插件 `picgo-plugin-rename-file` 提供模板（顶层命名空间）：

```
{  
  "picgoPlugins": { "rename-file": true },  
  "picgo-plugin-rename-file": { "format": "{y}/{m}/{d}/{origin}-{hash}" }  
}
```

- 最终 key = `path` + `format` + 原扩展名 → `img/YYYY/MM/DD/slug-hash.ext`

### 2.3 常见坑与修复（真实经历）

- 插件未生效：配置块应叫 `picgo-plugin-rename-file`，日志中的注册名 `rename-file` 不是配置键。
- 分支错配/未初始化：`master`/`main` 不一致或仓库空，上传报 404/422；先在网页端创建 `README.md` 或 `img/.gitkeep`。
- 401 Bad credentials：Token 错或权限不足；细粒度 Token 选 `Contents: Read & Write`。
- 调试：`picgo -d u` 检查 `beforeUploadPlugins: rename-file running` 与返回 key。

### 2.4 链接与版本位策略（实践选择）

- 日常贴图：用 `@latest`，省心；接受 1–2 分钟缓存延迟。
- 长期稳定：用 `@<sha>` 或 `@<tag>` 锁定快照；文件页按 `y` 复制 permalink 再转为 CDN。
- 快捷换算（zsh 函数）：

```
raw2cdn() { echo "$1" | sed -E 's#https://raw.githubusercontent.com/([^/]+)/([^/]+)/([^/]+)/(.*)#https://cdn.jsdelivr.net/gh/\1/\2@\3/\4#'; }  
gh2cdn()  { echo "$1" | sed -E 's#https://github.com/([^/]+)/([^/]+)/blob/([^/]+)/(.*)#https://cdn.jsdelivr.net/gh/\1/\2@\3/\4#'; }
```

## 3. 两种链接与换算（raw ↔ cdn）

- 规则：raw 与 cdn 仅域名与“版本位”不同；可相互换算。
- 建议：开发/自检用 raw；对外发布用 CDN；文档长链用 `@sha`；批处理用上面的 zsh 函数。

![CDN 缓存：MISS→回源→HIT](/images/posts/github特性1/a8e42202c5ce.png)

CDN 缓存：MISS→回源→HIT

## 4. 版本与链接策略（@latest / @sha / @tag）

- `@latest`：指向默认分支最新提交；覆盖同名文件后会指向新内容。
- `@<sha>`：锁定历史快照；后续覆盖不影响已有链接。
- `@<tag>`：发布版的别名；搭配 Releases 管理一批资源。

![@latest vs @sha](/images/posts/github特性1/e395bc4333de.png)

@latest vs @sha

## 5. 常见故障排查（速查表）

- 401 = Token/权限问题；
- 404/422 = 分支/路径/未初始化；
- 插件不生效 = 命名空间或与“时间戳重命名”冲突；
- CDN 未更新 = 缓存行为；改新文件名或用 `@sha`；  
  ![](/images/posts/github特性1/69286a40fb73.png)

## 6. 企业级 CDN 初识与迁移心智

> 不改命名策略，只替换存储与分发的“筋骨”。

![企业级迁移蓝图](/images/posts/github特性1/35a98e54b43d.png)

企业级迁移蓝图

- 源站换成对象存储（S3/OSS/COS/R2），CDN 绑定自定义域 `img.example.com`，配置 TLS。
- 哈希资源配强缓存头：`Cache-Control: public, max-age=31536000, immutable`；“别名/列表”配短 TTL。
- 可控项：缓存键/TTL/刷新、实时日志、WAF/防盗链、签名 URL、边缘计算。
- 链接迁移：`cdn.jsdelivr.net/...` → `https://img.example.com/img/...`，路径与命名不变。
- 决策阈值：流量规模、合规/可控需求、私有访问。
