# Gfit · Go For it

个人博客，使用 Hugo 构建并发布在 GitHub Pages：<https://lllxxxxxlll.github.io/>。

这里写 AI Infra、分布式系统、并行计算的学习与实践，也写工具、项目和生活。网站以文章为中心；[关于页](content/about/index.md)介绍当前阶段，[项目页](content/projects/index.md)记录实践。

## 写一篇文章

在 `content/posts/` 新建 Markdown 文件：

```yaml
---
title: "文章标题"
date: "2026-10-13"
draft: false
categories: ["工程与系统"]
tags: ["Go", "实践"]
summary: "用一句话说明文章要解决的问题。"
---
```

正文用 Markdown。保留已有文章与分类；新文章可根据主题逐步建立专题，无需一次迁移旧内容。

## 本地预览与发布

安装 Hugo extended 0.165.0 或更新版本，运行 `hugo server -D` 预览；运行 `hugo --gc --minify` 构建。提交到 `main` 后，现有 GitHub Actions 工作流会部署到 Pages。

博客的公开名称是 **Gfit**，口号是 **Go For it**。账号登录名和 Pages 仓库名保留，以维持现有网址。
