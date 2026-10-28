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

## 学习手记专题

入口为 `/categories/学习手记/`，首页和顶部导航均可进入。记录编程练习、学习中的真实疑惑、踩坑过程与阶段感悟。

后续文章仍放在 `content/posts/`，将 `categories` 设为 `["学习手记"]` 即可自动汇总；增加 `learning_note: true` 可显示专题导读与折叠目录。技术标签按内容填写，不局限于 C++。

建议从“为什么做 → 最初疑惑 → 代码与真实输出 → 理解如何修正 → 本次收获”展开。补做的实验与预期输出要标明来源，避免混写成当时已经完成的经历。配套代码、日志放在 `static/downloads/<文章英文名>/`，正文使用对应的 `/downloads/...` 链接。

## 本地预览与发布

安装 Hugo extended 0.165.0 或更新版本，运行 `hugo server -D` 预览；运行 `hugo --gc --minify` 构建。提交到 `main` 后，现有 GitHub Actions 工作流会部署到 Pages。

博客的公开名称是 **Gfit**，口号是 **Go For it**。账号登录名和 Pages 仓库名保留，以维持现有网址。
