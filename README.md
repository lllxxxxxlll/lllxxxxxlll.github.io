# 个人网站

把分散在各处的博客内容统一到一起，用 [Hugo](https://gohugo.io/) 构建、GitHub Pages 托管的个人站点。

线上地址：<https://lllxxxxxlll.github.io/>

## 技术栈

- **Hugo** 0.165.0（extended 版）
- **GitHub Pages** + **GitHub Actions** 自动部署
- 极简 cnblogs 风自研主题（位于 `layouts/`，未使用第三方主题）

## 内容结构

| 目录 | 内容 |
|---|---|
| `content/about/` | 简历 / 作品集（简介、技能、实习经历、项目、联系方式） |
| `content/projects/` | 项目介绍 |
| `content/posts/` | 博客文章，按分类组织 |

文章分四个分类：

| 分类 | 说明 |
|---|---|
| 踩坑 | 开发过程中记录的坑，首页是简略版，点进去看详细版 |
| Agent | 学习 agent 过程中接触到的内容和概念 |
| 算法 | 算法题解、CS:APP 笔记 |
| 随笔 | 个人随笔 |

## 本地开发

```bash
# 安装 Hugo（extended，≥ 0.165.0）
# macOS: brew install hugo
# Linux: 见 https://gohugo.io/installation/linux/

# 启动本地预览（含草稿）
hugo server -D
# 浏览器打开 http://localhost:1313/

# 构建静态文件（输出到 public/）
hugo --gc --minify
```

## 新增文章

在 `content/posts/` 下新建 `.md` 文件，frontmatter 格式如下：

```yaml
---
title: "文章标题"
date: "2025-08-06"
draft: false
categories: ["踩坑"]        # 踩坑 / Agent / 算法 / 随笔
tags: ["Go", "调试"]
summary: "一句话摘要，显示在文章卡片和首页列表上"
protected: false           # 设为 true 则整篇密码加密（仅随笔用，见下）
---
```

正文用标准 Markdown 书写。文章 URL 由文件名决定（`/posts/<文件名>/`），改标题不影响链接。

## 私密文章（密码保护）

`protected: true` 的文章（如随笔）会在构建后用 [StatiCrypt](https://github.com/robinmoisson/staticrypt) 做客户端 AES 加密：页面需输入密码才能解密显示，源文件也是密文。

1. 在文章 frontmatter 里加 `protected: true`。
2. 在仓库 **Settings → Secrets and variables → Actions** 新建 secret，名字 `ESSAY_PASSWORD`，值是你的密码。
3. 推送到 `main` 后，CI 会自动对 protected 文章加密后再发布。

- 密码只在构建时使用，不写入仓库；加密后的 HTML 是公开的，但没有密码无法解密。
- 请用足够强的密码（纯客户端加密可被离线暴力破解）。
- protected 文章不会出现在 RSS 订阅里，避免内容泄露。

## 部署

推送到 `main` 分支即自动部署，流程见 [`.github/workflows/deploy.yml`](.github/workflows/deploy.yml)：

```
push → 装 Hugo → hugo build → 打包 public/ → deploy-pages 发布
```

> 首次使用需在仓库 **Settings → Pages → Source** 选择 **GitHub Actions**（一次性设置）。
>
> 若站内有 `protected: true` 文章，还需先在 Actions secrets 里配置 `ESSAY_PASSWORD`，否则构建会失败（宁可失败，也不把私有内容明文发布）。

## 配置

站点配置在 [`hugo.toml`](hugo.toml)：

- `baseURL`：站点根地址（当前为根域名）
- `params`：作者名、副标题、GitHub、邮箱
- `permalinks`：文章链接规则（`/posts/:filename/`）
- `taxonomies`：分类 / 标签

## 目录结构

```
.
├── .github/workflows/deploy.yml   # CI 自动部署
├── archetypes/                    # 新文章 frontmatter 模板
├── content/                       # 全部内容（页面 + 文章）
├── layouts/                       # 主题模板（自研极简风）
├── static/css/style.css           # 样式
└── hugo.toml                      # 站点配置
```
