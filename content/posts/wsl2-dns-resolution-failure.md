---
title: "WSL2 DNS 解析频繁失败：原因与修复"
date: "2025-07-25"
draft: false
categories: ["踩坑"]
tags: ["WSL", "网络", "DNS", "Linux"]
summary: "VPN 切换或 WiFi 重连后 WSL2 频繁连不上网——根因是 /etc/resolv.conf 被宿主自动覆盖成不可达的 nameserver。临时改 8.8.8.8，永久在 /etc/wsl.conf 里禁用自动生成。"
---

## 现象

在 WSL2 里开发时，会周期性遇到 `curl` / `apt` / `git clone` 卡在 DNS 解析上：

```
curl: (6) Could not resolve host: github.com
```

更恼人的是：**不是一直失败**，而是时好时坏，往往发生在 VPN 切换、WiFi 重连、电脑休眠唤醒之后。

## 根因：WSL2 的 DNS 是谁在管

WSL2 的网络并不是独立的，它运行在一个由 Windows 宿主管理的虚拟网络里。默认情况下，WSL2 的 DNS 配置（`/etc/resolv.conf`）**不是你自己维护的，而是由 Windows 宿主在每次 WSL 启动/网络变化时自动生成的**。

```
Windows 宿主网络（VPN / WiFi）
        │  动态变化
        ▼
WSL2 自动生成 /etc/resolv.conf
        │  nameserver 指向宿主的 DNS
        ▼
nameserver 变成「已经不存在/不可达」的 IP
        │
        ▼
所有域名解析失败
```

关键点：`/etc/resolv.conf` 里写的 nameserver 是**上一次网络状态**的产物。当宿主切了 VPN、换了 WiFi，原来的 DNS 服务器（通常是 VPN 内网 DNS 或旧网关）已经不可达，但 WSL2 里的这份配置文件还被系统当成「当前有效」，于是所有解析请求都发往一个死地址，超时失败。

## 诊断

先看当前到底指向了谁：

```bash
cat /etc/resolv.conf
# nameserver 172.x.x.x   ← 一个已经不可达的地址
```

验证是不是这个地址挂了：

```bash
ping <nameserver 地址>      # 不通 → 确认根因
nslookup github.com 8.8.8.8 # 指定公共 DNS 却能解析 → 进一步坐实
```

能指定 `8.8.8.8` 解析成功、但用默认 nameserver 失败，基本可以断定是 DNS 配置被污染了。

## 修复

### 临时修复（立刻能上网）

直接覆盖 nameserver 为公共 DNS：

```bash
sudo sh -c 'echo "nameserver 8.8.8.8" > /etc/resolv.conf'
```

注意：这只是**临时**的。下次网络变化时 WSL2 还会把它覆盖回去，问题会复发。

### 永久修复（禁止自动生成）

1. 编辑 `/etc/wsl.conf`（不存在就新建）：

```ini
[network]
generateResolvConf = false
```

2. 手动写入一份固定的 `/etc/resolv.conf`：

```bash
sudo rm /etc/resolv.conf
sudo sh -c 'echo "nameserver 8.8.8.8" > /etc/resolv.conf'
sudo sh -c 'echo "nameserver 1.1.1.1" >> /etc/resolv.conf'
```

3. 在 Windows 侧重启 WSL 使其生效：

```powershell
wsl --shutdown
```

`generateResolvConf = false` 告诉 WSL：**别再碰我的 resolv.conf**。之后这份文件就完全归你自己管，VPN/WiFi 怎么变都不会被覆盖。

> 小坑：`/etc/resolv.conf` 在新版本 WSL2 里可能是一个软链接，直接写会失败。先 `ls -l /etc/resolv.conf` 确认，必要时 `rm` 掉软链再写实体文件。

## 核心认知

1. **WSL2 不是完整独立网络**，DNS 受宿主控制，默认自动生成 `resolv.conf`。这是理解这一类「时好时坏」网络问题的钥匙。

2. **「时好时坏」通常指向「自动生成 + 网络变化」**——不是代码问题，也不是网络本身坏了，而是配置与当前环境脱节。

3. 排查思路：**看配置文件 → 验证它指向的地址是否可达 → 指定一个已知好的 DNS 做对照**，三步就能定位到是「DNS 配置」而非「网络不通」。

## 延伸

如果禁用自动生成后，某些内网域名（走 VPN 的企业内部服务）又解析不了了，说明那份自动生成的 nameserver 里确实混着**内网 DNS**。这时更好的做法不是一刀切禁用，而是按需切换——公共 DNS 保证外网，内网域名走 VPN 的 DNS。这已经超出「临时坑」的范畴，属于网络拓扑设计，这里不展开。
