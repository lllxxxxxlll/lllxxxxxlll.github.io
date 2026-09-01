#!/usr/bin/env python3
"""
微信公众号文章 → Hugo content Markdown 转换器
用法：python wechat2hugo.py <url> [url2 url3 ...]
功能：
  1. 抓取公众号文章 HTML（带 js_content 正文区）
  2. 提取标题 / 作者 / 发布时间 / 摘要
  3. 下载所有正文图片到 static/images/posts/<slug>/
  4. HTML → Markdown
  5. 写入 content/posts/<slug>.md，带 Hugo front matter
"""

import os
import re
import sys
import json
import time
import hashlib
import urllib.parse
from datetime import datetime, timezone, timedelta
from pathlib import Path

import requests
from bs4 import BeautifulSoup
from markdownify import markdownify as md

UA = (
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) "
    "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/127.0 Safari/537.36"
)
HEADERS = {
    "User-Agent": UA,
    "Referer": "https://mp.weixin.qq.com/",
}

CST = timezone(timedelta(hours=8))

WORKSPACE = Path(__file__).resolve().parent.parent
CONTENT_DIR = WORKSPACE / "content" / "posts"
STATIC_DIR = WORKSPACE / "static"
IMG_ROOT = STATIC_DIR / "images" / "posts"

CONTENT_DIR.mkdir(parents=True, exist_ok=True)
IMG_ROOT.mkdir(parents=True, exist_ok=True)

# ---------- 工具 ----------

def slugify(s: str) -> str:
    """标题 → 文件名 slug：保留中文，去掉特殊字符，长度有限"""
    s = re.sub(r"[\s]+", "-", s.strip())
    s = re.sub(r"[\\/:*?\"<>|.【】「」，。、！？,!?#%&=+\[\]()（）]+", "-", s)
    s = re.sub(r"-+", "-", s).strip("-")
    return s[:80] if len(s) > 80 else s

def sanitize_html(soup: BeautifulSoup):
    """清理公众号正文 HTML，去掉垃圾元素，处理懒加载图片"""
    for tag in soup.find_all(["script", "style"]):
        tag.decompose()
    # 公众号 data-src → src
    for img in soup.find_all("img"):
        data_src = img.get("data-src") or img.get("src") or ""
        if not data_src:
            img.decompose()
            continue
        img["src"] = data_src
        # 移除不需要的属性
        for attr in list(img.attrs.keys()):
            if attr not in ("src", "alt"):
                del img[attr]
    # 删除空的 section / p / span（公众号里很多空壳）
    for tag in soup.find_all(["section", "p"]):
        if not tag.get_text(strip=True) and not tag.find_all(["img", "iframe", "video"]):
            tag.decompose()
    # 把公众号的 blockquote 样式块换成真实 blockquote
    for tag in soup.select("blockquote, qqmusic, mpvoicesection"):
        pass  # 保留
    # 展开公众号最外层的无意义嵌套 section
    for _ in range(3):
        for outer in soup.find_all("section", recursive=True):
            if (
                len(outer.contents) == 1
                and outer.contents[0].name in ("section", "div", "p")
                and not outer.get_text(strip=True) == outer.contents[0].get_text(strip=True)
                and not outer.find(["img", "iframe", "video"])
            ):
                continue  # 太激进，跳过
    return soup

def download_image(url: str, save_dir: Path, referer: str) -> str | None:
    """下载图片，返回相对于 static/ 的路径（不含 /static 前缀）"""
    if not url:
        return None
    try:
        # 微信图片链接如果没有 scheme
        if url.startswith("//"):
            url = "https:" + url
        resp = requests.get(url, headers={**HEADERS, "Referer": referer}, timeout=20)
        if resp.status_code != 200 or len(resp.content) < 100:
            return None
        # 取扩展名
        ext_hint = ""
        u = urllib.parse.urlparse(url)
        qs = urllib.parse.parse_qs(u.query)
        wx_fmt = qs.get("wx_fmt")
        if wx_fmt:
            ext_hint = "." + wx_fmt[0].split("&")[0].strip()
        if not ext_hint:
            # 根据 content-type 猜
            ct = resp.headers.get("Content-Type", "").lower()
            if "png" in ct: ext_hint = ".png"
            elif "gif" in ct: ext_hint = ".gif"
            elif "webp" in ct: ext_hint = ".webp"
            else: ext_hint = ".jpg"
        # 用内容 hash 命名避免重复
        h = hashlib.md5(resp.content).hexdigest()[:12]
        fname = f"{h}{ext_hint}"
        fpath = save_dir / fname
        fpath.write_bytes(resp.content)
        # 返回 Hugo 里可以直接用的路径，从 static 根开始
        return f"/images/posts/{save_dir.name}/{fname}"
    except Exception as e:
        print(f"  [warn] 图片下载失败 {url[:80]}: {e}", file=sys.stderr)
        return None

# ---------- 抓取 & 解析 ----------

FETCH_HEADERS = {
    "User-Agent": UA,
    "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
    "Accept-Language": "zh-CN,zh;q=0.9,en;q=0.8",
    "Referer": "https://mp.weixin.qq.com/",
    "sec-ch-ua": '"Chromium";v="127", "Not)A;Brand";v="99", "Google Chrome";v="127"',
    "sec-ch-ua-mobile": "?0",
    "sec-ch-ua-platform": '"macOS"',
    "Upgrade-Insecure-Requests": "1",
}

def fetch_article(url: str):
    """返回 {title, author, date, summary, html_body, images_refs}"""
    resp = requests.get(url, headers=FETCH_HEADERS, timeout=30)
    resp.encoding = resp.apparent_encoding or "utf-8"
    if resp.status_code != 200 or len(resp.text) < 5000:
        raise RuntimeError(f"抓取失败 status={resp.status_code} len={len(resp.text or '')}，可能微信有拦截")
    html = resp.text
    # 注：公众号页面 3MB+ 且结构畸形，html5lib 会解析错乱；用 python 内置 html.parser 反而更稳
    soup = BeautifulSoup(html, "html.parser")

    # 标题
    title = ""
    for sel in ["#activity-name", "h1#activity-name", "h1.post_title", "meta[property='og:title']"]:
        node = soup.select_one(sel)
        if node:
            title = node.get_text(strip=True) if not node.get("content") else node.get("content", "")
            if title:
                break
    if not title:
        t = soup.find("title")
        title = t.get_text(strip=True) if t else "未命名文章"

    # 作者 / 账号
    author = ""
    for sel in ["#js_name", ".rich_media_meta_nickname", "#js_profile_qrcode > div > strong", "a#js_name"]:
        node = soup.select_one(sel)
        if node:
            author = node.get_text(strip=True)
            if author:
                break
    if not author:
        # 从 script 里试试
        m = re.search(r"var\s+user_name\s*=\s*[\"']([^\"']+)[\"']", html)
        if m: author = m.group(1)

    # 发布时间
    date_str = ""
    m = re.search(r"var\s+ct\s*=\s*[\"']?(\d+)[\"']?", html)
    if m:
        ts = int(m.group(1))
        date = datetime.fromtimestamp(ts, tz=CST)
        date_str = date.strftime("%Y-%m-%dT%H:%M:%S+08:00")
    else:
        # 从 HTML meta 或文本里找
        meta_date = soup.select_one("meta[property='article:published_time']")
        if meta_date and meta_date.get("content"):
            try:
                dt = datetime.fromisoformat(meta_date["content"].replace("Z", "+00:00"))
                date_str = dt.astimezone(CST).strftime("%Y-%m-%dT%H:%M:%S+08:00")
            except Exception:
                pass
        # 再兜底：当天
        if not date_str:
            date_str = datetime.now(CST).strftime("%Y-%m-%dT%H:%M:%S+08:00")

    # 正文
    body = soup.select_one("#js_content")
    if not body:
        body = soup.select_one(".rich_media_content")
    if not body:
        raise RuntimeError("无法定位到正文内容 #js_content，可能微信页面结构变了")

    # 把正文里公众号特有样式 section 里的加粗/颜色等保留下来再处理
    body = sanitize_html(body)
    body_html = str(body)
    # 再做一次二次清洗：html.parser 处理完，用 html5lib 过一遍保证输出结构规范
    try:
        body_html = str(BeautifulSoup(body_html, "html5lib"))
    except Exception:
        pass

    # 摘要：取正文前 160 纯文本
    summary = body.get_text(" ", strip=True).replace("\n", " ")
    summary = re.sub(r"\s+", " ", summary)
    if len(summary) > 180:
        summary = summary[:180] + "…"

    return {
        "title": title,
        "author": author,
        "date": date_str,
        "summary": summary,
        "body_html": body_html,
        "source_url": url,
    }

# ---------- 主流程 ----------

def convert_one(url: str):
    print(f"\n==> 正在抓取：{url}")
    data = fetch_article(url)
    print(f"    标题：{data['title']}")
    print(f"    作者：{data['author'] or '(未知)'}")
    print(f"    日期：{data['date']}")

    slug = slugify(data["title"])
    # 如果名字冲突，加个后缀
    out_md = CONTENT_DIR / f"{slug}.md"
    idx = 2
    while out_md.exists():
        out_md = CONTENT_DIR / f"{slug}-{idx}.md"
        idx += 1
    slug_final = out_md.stem

    img_dir = IMG_ROOT / slug_final
    img_dir.mkdir(parents=True, exist_ok=True)

    # 先处理图片：在 HTML 里逐张替换 <img src> → 本地路径
    soup2 = BeautifulSoup(data["body_html"], "html.parser")
    replaced = 0
    for img in soup2.find_all("img"):
        src = img.get("src", "")
        if not src or src.startswith("data:"):
            img.decompose()
            continue
        local = download_image(src, img_dir, referer=data["source_url"])
        if local:
            img["src"] = local
            replaced += 1
        else:
            img.decompose()
    print(f"    图片：成功下载 {replaced} 张")

    clean_html = str(soup2)

    # HTML → Markdown
    markdown_body = md(
        clean_html,
        heading_style="ATX",
        bullets="-",
        strong_em_symbol="*",
        strip=["script", "style"],
    )
    # 后处理 1：微信 li 自带 •/○/■ 项目符号，markdownify 又加了 -，变成 "- • xxx"。去掉多余的圆点
    markdown_body = re.sub(
        r"^(\s*-\s*)[•○■●▪▸►▶︎□☑️✅✔️✓➢➤⦿◾▫]\s*",
        lambda m: m.group(1),
        markdown_body,
        flags=re.M,
    )
    # 后处理 2：markdownify 可能产生大片空行，压缩一下
    markdown_body = re.sub(r"\n{3,}", "\n\n", markdown_body).strip()
    # 后处理 3：`# 文字` 之后如果没空行，补一个空行
    markdown_body = re.sub(r"^(#{1,6}\s+.+[^\n])\n(\S)", r"\1\n\n\2", markdown_body, flags=re.M)

    # Hugo front matter
    fm = {
        "title": data["title"],
        "date": data["date"],
        "summary": data["summary"],
        "source": data["source_url"],
        "categories": ["微信公众号"],
        "tags": ["公众号迁移"],
    }
    if data["author"]:
        fm["author"] = data["author"]

    fm_yaml = "---\n"
    for k, v in fm.items():
        # YAML 安全序列化：字符串里有双引号/换行就包双引号并转义
        if isinstance(v, list):
            fm_yaml += f"{k}: [{', '.join('\"'+x.replace('\"','\\\"')+'\"' for x in v)}]\n"
        else:
            s = str(v).replace("\\", "\\\\").replace('"', '\\"').replace("\n", " ")
            fm_yaml += f'{k}: "{s}"\n'
    fm_yaml += "---\n\n"

    # 顶部加一个小提示
    notice = (
        f"> 本文首发于微信公众号 **{data['author'] or '个人公众号'}**，"
        f"[原文链接]({data['source_url']})。\n\n"
    )

    out_text = fm_yaml + notice + markdown_body + "\n"
    out_md.write_text(out_text, encoding="utf-8")
    print(f"    输出：{out_md.relative_to(WORKSPACE)}  ({len(out_text)} 字节)")
    return out_md

def main():
    urls = [u.strip() for u in sys.argv[1:] if u.strip()]
    if not urls:
        print("用法：python wechat2hugo.py <url1> [url2] [url3] ...", file=sys.stderr)
        sys.exit(1)
    ok, fail = 0, 0
    for u in urls:
        try:
            convert_one(u)
            ok += 1
        except Exception as e:
            print(f"  [error] {u} 转换失败：{e}", file=sys.stderr)
            fail += 1
        time.sleep(1.5)  # 礼貌间隔，避免微信反爬
    print(f"\n✅ 完成：成功 {ok} 篇，失败 {fail} 篇")

if __name__ == "__main__":
    main()
