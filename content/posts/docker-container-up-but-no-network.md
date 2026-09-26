---
title: "Docker 容器 Up 但 curl 不通：端口映射为什么没生效"
date: "2026-09-23T00:22:00+08:00"
draft: false
categories: ["踩坑"]
tags: ["Docker", "网络", "容器", "WSL"]
summary: "hellok8s 容器 docker ps 显示 Up、进程 /main 也在跑，但 curl 宿主机端口就是不通。根因不是端口写错，而是容器的网络 endpoint 丢了——-p 虽传过却从未生效。重建容器即可，不用重新 build。"
---

## 现象

一个极简 Go 服务 `hellok8s`（监听 `:3000`，返回 `[v1],hello,k8s`），镜像已经 `docker build` 好、`docker run` 起来了，`docker ps` 也显示 `Up`，进程明明在跑：

```bash
CONTAINER ID   IMAGE               COMMAND   STATUS    PORTS   NAMES
1639c726e1a7   lixian2/tryfork8s   "/main"   Up 3 days         hellok8s
```

但宿主机上 `curl http://127.0.0.1:3000/` 就是不通。第一反应是「端口映射写错了」——实际不是。

## 排查过程

### 第一步：看 PORTS 列

对比旁边的 postgres 容器，它的 PORTS 列是 `0.0.0.0:5432->5432/tcp`，而 `hellok8s` 的 PORTS 列**是空的**。空 PORTS 列基本意味着「端口没有发布成功」。

### 第二步：对照 inspect 的两个字段

```bash
docker inspect hellok8s --format '{{json .HostConfig.PortBindings}}'   # 期望的端口映射
docker inspect hellok8s --format '{{json .NetworkSettings.Ports}}'     # 实际生效的端口映射
```

结果是关键矛盾：

```json
HostConfig.PortBindings:  {"3000/tcp":[{"HostIp":"","HostPort":"3000"}]}   // -p 确实传过
NetworkSettings.Ports:    {}                                               // 实际什么都没生效
```

`HostConfig.PortBindings` 记录了 `docker run` 时传的 `-p 3000:3000`，但 `NetworkSettings.Ports` 是空的——说明这条端口映射**从未真正挂到运行时**。

### 第三步：看网络挂载情况

```bash
docker inspect hellok8s --format '{{.HostConfig.NetworkMode}}'        # bridge
docker inspect hellok8s --format '{{json .NetworkSettings.Networks}}'  # {}
```

`NetworkMode=bridge`，但 `Networks = {}`——容器**没有挂到任何网络**，也没有拿到 IP。一个 bridge 容器正常应该有一条 `bridge: {IPAddress: ...}` 的记录。

### 第四步：排除「镜像/程序本身有问题」

用同一个镜像临时起一个测试容器、换个端口，验证程序到底正不正常：

```bash
docker run -d --name test -p 3001:3000 lixian2/tryfork8s
curl http://127.0.0.1:3001/     # → [v1],hello,k8s
```

通了。**程序、镜像都没问题**，坏的只是 `hellok8s` 这个容器实例自己的网络。

## 根因

`docker run` 出来的容器，除了镜像的只读层，还多出一层「运行时状态」：进程、网络命名空间、端口映射。这次就是这层运行时状态的**网络 endpoint 丢了**。

在 WSL2 里最常见的原因：**Docker daemon 重启 / WSL2 重启**后，容器的网络 endpoint 或 docker-proxy / iptables 端口转发没有重建回来；也可能是被 `docker network disconnect bridge hellok8s` 手动断开了。

无论哪种，结果都一样：容器还在 Up、进程还在跑，但它已经从网络里「掉线」了，所以端口映射自然不生效。

## 修复

镜像没坏，不需要重新 build，重建容器让它重新拿到网络和端口即可：

```bash
docker rm -f hellok8s
docker run -d --name hellok8s -p 3000:3000 lixian2/tryfork8s
curl http://127.0.0.1:3000/     # → [v1],hello,k8s
```

重建后 `docker ps` 的 PORTS 列恢复为 `0.0.0.0:3000->3000/tcp`。

> 加固：给容器加重启策略 `--restart unless-stopped`，daemon 重启后它能自动重新拉起，减少这类「掉线后还占着名字」的情况。

## 关键认知：docker build 与 docker run 是两个独立步骤

这个坑的本质，是把「打包」和「运行」搞混了。它们是完全不同的两个阶段：

| | 命令 | 产物 | 做什么 |
|---|---|---|---|
| 打包 | `docker build -t 名字 .` | 镜像 image | 把源码编译成二进制，连同运行环境打包进镜像 |
| 运行 | `docker run -p ... 名字` | 容器 container | 拿现成镜像起一个进程实例，分配网络和端口 |

- **镜像 = 蓝图**：只读的、打包好的结果。重建容器不会重新编译，因为 `go build` 只在 `docker build` 时跑过一次。
- **容器 = 实例**：镜像只读层 + 可写容器层 + 进程 + 网络命名空间 + 端口映射。这层可变的「运行时状态」才是会坏的地方。

所以「容器 Up 但网络丢了」只需重建容器，镜像纹丝不动。

### 镜像长什么样、存在哪

`docker build` 逐行执行 Dockerfile，每条改文件系统的指令（`FROM` / `RUN` / `COPY` / `ADD`）产生一个**只读层**；而 `EXPOSE` / `ENTRYPOINT` / `WORKDIR` / `CMD` / `LABEL` / `ENV` 只写进配置、不产生层（`docker history` 里显示 0B）。

以本项目的镜像为例：

```bash
docker history lixian2/tryfork8s
# ENTRYPOINT ["/main"]          0B       ← 配置，不产生层
# EXPOSE [3000/tcp]             0B       ← 配置，不产生层
# COPY /src/main /main          6.18MB   ← 编译好的二进制，1 层
# WORKDIR /                     0B       ← 配置
# bazel build ...               18.3MB   ← distroless 基础层
# bazel build ...               4.51MB    ← distroless 基础层
```

6 行历史，但文件系统上只有 **3 层**（2 个基础层 + 1 个 COPY 层）。镜像就是「这几层只读快照 + 一份配置」，存在 `/var/lib/docker` 里（overlay2 存储驱动），**不是单个文件**；要单文件需 `docker save -o x.tar 名字` / `docker load -i x.tar`。

### 镜像名是怎么被找到的

`docker run` 的**最后一个位置参数**就是镜像引用（不是某个 `--flag`）：

```bash
docker run -d --name hellok8s -p 3000:3000 lixian2/tryfork8s
#                                          ^^^^^^^^^^^^^^^^^ 镜像引用
```

标准格式是 `[registry/]namespace/repo:tag`：不写 registry 默认 `docker.io`（Docker Hub），不写 tag 默认 `latest`。解析顺序是**先查本地索引、没有再远程 pull**。

本地索引里存的是「名字 → 镜像 ID」的映射，由 `docker build -t 名字` 时登记。名字只是指向镜像 ID 的**标签**，一个 ID 可以挂多个标签；`latest` 也只是个普通标签，不会自动更新。

```bash
docker image inspect lixian2/tryfork8s --format '{{.Id}}'            # sha256:239ced12de78...
docker image inspect lixian2/tryfork8s --format '{{json .RepoTags}}' # ["lixian2/tryfork8s:latest"]
```

### 多阶段构建：为什么最终镜像只有 41MB

本项目的 Dockerfile 用了 multi-stage：

```dockerfile
FROM golang:1.16-buster AS builder          # 第 1 阶段：带编译器的「工地」
RUN go build -o main

FROM gcr.io/distroless/base-debian10        # 第 2 阶段：干净的精简底座
COPY --from=builder /src/main /main         # 只把编译好的二进制搬过来
```

`golang:1.16` 那个近 900MB 的编译器大镜像只在 builder 阶段用，**不进最终镜像**；`COPY --from=builder` 跨阶段只搬产物。所以最终镜像 = distroless 底座 + 一个 6.18MB 的二进制，总共 41MB。

> 顺带一提：distroless 镜像没有 shell，`docker exec -it 容器 sh` 会报 `exec: "sh": executable file not found`。调试它靠 `docker logs` 和 `docker inspect`，别指望 exec 进去。

---

## 快速诊断检查单

以后「容器 Up 但 curl 不通」，按这个顺序排：

```bash
# 1. PORTS 列是不是空的？（空了 = 端口没发布成功）
docker ps

# 2. 期望 vs 实际：-p 传了没、生效没
docker inspect <容器> --format '{{json .HostConfig.PortBindings}}'
docker inspect <容器> --format '{{json .NetworkSettings.Ports}}'

# 3. 容器挂到网络了没、有没有 IP
docker inspect <容器> --format '{{.HostConfig.NetworkMode}}'
docker inspect <容器> --format '{{json .NetworkSettings.Networks}}'

# 4. 排除「镜像/程序本身有问题」：换个端口临时起一个验证
docker run -d --name test -p <别的端口>:<容器端口> <镜像>
curl http://127.0.0.1:<别的端口>/
```

> **记住**：`docker ps` 里 `Up` 只代表进程活着，不代表网络通。`PORTS` 列、`NetworkSettings.Ports`、`NetworkSettings.Networks` 这三个才是判断「端口映射到底生效没」的依据。
