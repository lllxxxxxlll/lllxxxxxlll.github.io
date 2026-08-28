#!/usr/bin/env bash
set -euo pipefail

# 用 StatiCrypt 对标记了 protected: true 的文章做客户端 AES 加密。
# 密码从环境变量 ESSAY_PASSWORD 读取（CI 里来自 GitHub secret）。

PASSWORD="${ESSAY_PASSWORD:-}"
TEMPLATE="scripts/staticrypt-template.html"

if [ -z "$PASSWORD" ]; then
  echo "::error:: 未设置 ESSAY_PASSWORD（GitHub secret），拒绝部署以免私有内容泄露。" >&2
  exit 1
fi

if ! command -v staticrypt >/dev/null 2>&1; then
  echo "::error:: 未找到 staticrypt，请先执行 npm install -g staticrypt@3.5.4" >&2
  exit 1
fi

mapfile -t FILES < <(grep -l '^protected: *true *$' content/posts/*.md 2>/dev/null || true)

if [ "${#FILES[@]}" -eq 0 ]; then
  echo "没有 protected 文章，跳过加密。"
  exit 0
fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

for md in "${FILES[@]}"; do
  slug="$(basename "$md" .md)"
  src="public/posts/${slug}/index.html"
  if [ ! -f "$src" ]; then
    echo "跳过 ${slug}：未生成 HTML（可能是草稿）" >&2
    continue
  fi

  outdir="${TMP}/${slug}"
  mkdir -p "$outdir"
  echo "加密：${slug}"

  staticrypt "$src" \
    -p "$PASSWORD" \
    -t "$TEMPLATE" \
    --config false \
    --remember false \
    --short \
    --template-title "随笔 · 请输入密码" \
    --template-instructions "此篇为私人记录，请输入密码查看" \
    --template-placeholder "输入密码" \
    --template-button "解密" \
    --template-error "密码错误，请重试" \
    -d "$outdir"

  mv "$outdir/index.html" "$src"
done

echo "完成：加密了 ${#FILES[@]} 篇 protected 文章。"
