---
title: "WSL2 + Claude Code CLI 执行位丢失排坑记录"
date: "2025-08-17"
draft: false
categories: ["踩坑"]
tags: ["WSL", "Linux", "权限", "Claude Code"]
summary: "claude 命令 Permission denied——.exe 其实是 Bun 编译的 ELF，软链 777 是虚的，看目标文件执行位。npm postinstall 中断留下 0644 半成品。"
---

## 环境

- **系统**: WSL2（Linux 6.6.87.2-microsoft-standard-WSL2）
- **运行时**: Node v22.22.0 / npm 10.9.4（经 nvm 管理）
- **CLI**: Claude Code 2.1.233（npm 全局安装）
- **关键路径**:
  - npm prefix: `/home/lenovo/.nvm/versions/node/v22.22.0`
  - 全局包: `.../lib/node_modules/@anthropic-ai/claude-code`
  - 用户数据: `~/.claude`（history / sessions / settings.json）

---

## 现象

执行 `claude` 命令报：

```
Permission denied
```

---

## 根因链路

```
执行 claude
   │
   ▼
claude 是软链接 → 指向 @anthropic-ai/claude-code/bin/claude.exe
   │
   ▼
claude.exe 其实是 Bun 编译的 Linux ELF 单文件二进制（~324MB）
   （.exe 只是命名习惯，不是 Windows 程序）
   │
   ▼
Linux 执行时看「目标文件」的执行位，不看软链自己的权限
   （软链 lrwxrwxrwx 的 777 是虚的）
   │
   ▼
目标文件执行位丢失：0755(-rwxr-xr-x) → 0644(-rw-r--r--)
   │
   ▼
内核 execve 拒绝执行「无 x 位」的二进制 → Permission denied
```

### 执行位为什么会丢（机制层）

npm 装包时 postinstall 脚本 `install.cjs` 的二进制放置流程：

1. `linkSync(src, dest)` 硬链，失败则 `copyFileSync` 覆盖拷贝（第 102 / 130–131 行）；
2. **最后一步**才在非 Windows 平台 `chmodSync(dest, 0o755)`（第 137 行）。

执行位是靠**收尾那一行 chmod**才有的。如果中间 ~500MB 的 copy 刚写完、chmod 还没跑到时进程被中断（网络断 / Ctrl-C / 关 WSL 窗口 / 磁盘写满），文件就留在 0644。

### 关键排除项

nvm 目录在 ext4（`/dev/sdd`），不在 `/mnt/c` 的 9p/drvfs 上，所以**不是**「Windows 挂载盘丢执行位」这个 WSL 经典坑。`df -T` / `mount` 可快速确认。

---

## 诊断命令

```bash
which claude                                    # 定位入口
ls -la $(readlink -f $(which claude))           # ① 看目标文件权限位（第一优先级）
file $(readlink -f $(which claude))             # ② 看真实文件类型（别信扩展名）
df -T /home/lenovo/.nvm                         # ③ 看文件系统（排除 9p/drvfs）
claude --version                                # 验证恢复
```

---

## 排障命令详解（ls / file / chmod）

### ls -l：看懂权限位

```bash
ls -l $(readlink -f $(which claude))
# -rwxr-xr-x 2 lenovo lenovo 324598064 Aug 16 23:46 claude.exe
```

最前面的 10 字符 `-rwxr-xr-x` 拆开看：

| 位置 | 字符 | 含义 |
|---|---|---|
| 第 1 位 | `-` | 文件类型：`-`普通文件 `d`目录 `l`软链 |
| 第 2-4 位 | `rwx` | **属主 owner(u)** 的权限 |
| 第 5-7 位 | `r-x` | **属组 group(g)** 的权限 |
| 第 8-10 位 | `r-x` | **其他 other(o)** 的权限 |

每组内 r=4(读)、w=2(写)、x=1(执行)，求和得八进制：

```
rwx = 4+2+1 = 7
r-x = 4+0+1 = 5
→ 0755
```

`-rwxr-xr-x` = 755，`-rw-r--r--` = 644。本次故障就是 755 掉成 644——**第 2-4 位里的 `x` 没了**。

> `x` 对普通文件是「能否作为程序运行」，对**目录**是「能否 cd 进入/遍历」——目录也要 x 位，是常考盲点。

### file：看真实类型，别信扩展名

```bash
file $(readlink -f $(which claude))
# .../claude.exe: ELF 64-bit LSB executable, x86-64, ... for GNU/Linux 3.2.0, not stripped
```

`file` 读文件头魔数判断真实类型，不看扩展名——这是被 `.exe` 误导时的解药。

- `file 文件`：默认**跟随软链**，报目标文件类型；
- `file -h 文件`：不跟随，报 `symbolic link to ...`；
- `file -b 文件`：只要类型、不要前缀文件名。

### chmod：两种写法

```bash
# 符号模式（增量改，推荐用于修单个位）
chmod +x file        # 给 u/g/o 三类都加执行位
chmod u+x file       # 只给属主加
chmod go-w file      # 去掉属组和其他人的写权限

# 八进制模式（整体覆盖）
chmod 755 file       # 直接设成 rwxr-xr-x
chmod 644 file       # 设成 rw-r--r--
```

区别：**符号是「改某个位」，八进制是「整体覆盖」**。修执行位用 `chmod +x` 最安全（只动 x 位）。

> ⚠️ `chmod -R 755 目录` 递归会改到目录里所有文件，别乱用——对 `~/.ssh` 之类目录改错权限会直接导致 SSH 拒绝连接。

### 辅助命令

| 命令 | 作用 | 本次用法 |
|---|---|---|
| `which claude` | 找命令在 PATH 里的位置 | 定位入口 `.../bin/claude` |
| `readlink -f <软链>` | 软链一路解到底 | `readlink -f $(which claude)` |
| `stat -c '%a %A' <file>` | 直接看八进制权限 | `stat -c '%a %A' claude.exe` → `755 -rwxr-xr-x` |
| `df -T <路径>` | 看文件系统类型 | 确认 nvm 在 ext4 而非 9p/drvfs |

> `stat` 比 `ls -l` 更适合「要精确八进制数字」的场景，排查权限建议直接用 `stat -c '%a %A %n' 文件`。

### 最该记住的盲区

**软链的 `lrwxrwxrwx`（777）是虚的，决定能否执行的是目标文件的权限位**。Linux 内核执行时解析软链后只看目标文件的 x 位，软链自己的权限位无意义。面试被问「软链能否执行取决于什么」→ 标准答案：**取决于目标文件的 x 位**。

---

## 修复

### 快速修复（推荐，一条命令）

```bash
chmod +x $(readlink -f $(which claude))
```

适用于「目标文件已完整、只是丢了 x 位」的情况。比重装快得多。

### 彻底修复（重装，适合二进制本身损坏）

```bash
# 1. 备份（不可逆操作前先备份）
cp -r "$(npm root -g)" ~/anthropic-ai-backup

# 2. 删除异常包
rm -rf "$(npm root -g)/@anthropic-ai/claude-code"

# 3. 重装（重新走完整 postinstall，chmod 会正常跑完）
npm install -g @anthropic-ai/claude-code

# 4. 验证
claude --version   # → 2.1.233

# 5. 清理残留（备份 + 陈旧的 npm 临时暂存目录）
rm -rf ~/anthropic-ai-backup
rm -rf "$(npm root -g)/@anthropic-ai/.claude-code-"*
```

> `~/.claude` 用户数据全程不动，重装只动 npm 程序目录，会话与配置不受影响。

---

## 核心认知

1. **`.exe` 是命名习惯，不是文件类型**。Claude Code 用 Bun 把 CLI 编译成单文件 ELF 二进制，`package.json` 里 `"bin": {"claude": "bin/claude.exe"}` 就是这么写的，跨平台同名。判断类型永远用 `file`。

2. **软链 777 是虚的，看目标文件**。`lrwxrwxrwx` 不决定能否执行，Linux 执行时读的是目标文件的权限位。

3. **`Permission denied` 排查顺序**（省一半弯路）：
   先 `ls -l` 看权限位 → 再 `file` 看类型 → 再 `df -T`/`mount` 看文件系统 → 最后才怀疑程序本身。

4. **npm 原生二进制安装机制**：postinstall 里「link/copy + 收尾 chmod」，中断会留下两种垃圾——0644 的半成品，或 `.package-<hash>` 临时目录。

5. **数据 / 程序分离**：`~/.claude` 是数据，`~/.nvm` 是程序，重装程序永远不动数据。

---

## 快速排查脚本（加进 .bashrc）

```bash
claude-health() {
  local t; t=$(readlink -f "$(command -v claude)")
  echo "target: $t"
  file "$t"
  ls -l "$t"
  [ -x "$t" ] && echo "OK: 可执行" || echo "异常: 缺执行位, 执行 chmod +x '$t'"
}
```

---

## 面试关联

- Linux 文件权限 / 执行位 / 软硬链接 —— 🔴 必问（基础但常考，尤其后端/基础架构岗）
- `npm install -g` 完整生命周期（postinstall / bin 软链 / 原生二进制下载）—— 🟡 高频，会追问「npm install 到底发生了什么」
- 排查方法论（「线上问题怎么排查」）—— 🟡 高频，本案例是现成的完整「症状 → 排查 → 根因 → 修复」样本

---

## 参考

- `install.cjs` 源码位置：`$(npm root -g)/@anthropic-ai/claude-code/install.cjs`（`chmodSync(dest, 0o755)` 在 137 行附近）
