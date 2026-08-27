---
title: "YOLO v12 环境配置与训练流程"
date: "2025-07-31"
draft: false
categories: ["算法"]
tags: ["YOLO", "PyTorch", "CV", "部署", "竞赛"]
summary: "YOLOv12 是打破传统 CNN 结构、在注意力机制上的一次大胆尝试。记录它的环境配置（flash-attention 依赖是最大的坑）和训练脚本。"
---

## YOLO 系列脉络

- **YOLOv11** 是在 YOLOv8 基础上的升级，属于较官方的模型框架。
- **YOLOv12** 是打破传统 CNN 结构、在**注意力机制**上的一次大胆尝试。

学习时结合 YOLOv11 找区别和联系，能更快入手。YOLO 系列与 PyTorch 框架深度绑定，部分内容（尤其是编译依赖）和 Linux 环境强相关。

## 环境配置

YOLOv12 最好在 Linux 下部署；Windows 下需要解决 flash-attention 依赖问题。

### Linux 下（推荐）

```bash
conda create -n yolov12 python=3.11   # 必须 Python 3.11
conda activate yolov12
git clone https://github.com/sunsmarterjie/yolov12.git
cd yolov12
pip install -r requirements.txt
pip install -e .
```

### Windows 下

```bash
# 1. Python 和 PyTorch 环境
conda create -n yolov12 python=3.11
conda activate yolov12
pip install torch==2.6.0 torchvision==0.17.0 --index-url https://download.pytorch.org/whl/cu124  # CUDA 12.x

# 2. 解决 flash-attention 依赖（Windows 最大的坑）
pip install flash_attn-2.7.3+cu11torch2.2cxx11abiFALSE-cp311-cp311-linux_x86_64.whl
# 并替换 requirements.txt 中关于 flash 的依赖项，选择本地下载的合适 whl
# 可从 https://github.com/kingbri1/flash-attention/releases 找对应版本

pip install -r requirements.txt -i https://pypi.tuna.tsinghua.edu.cn/simple
```

> 下载大文件（如 flash-attention whl）时若多次中断，用 `wget -c <url>` 支持断点续传。

## 训练脚本

```python
from ultralytics import YOLO

def main():
    model = YOLO('yolov12s.pt')
    results = model.train(
        data='y12730.yaml',
        epochs=200,
        batch=32,
        imgsz=640,
        scale=0.9,     # S/M/L/X: 0.9
        mosaic=1.0,
        mixup=0.05,    # S:0.05 M:0.15 L:0.15 X:0.2
        copy_paste=0.1,# S:0.15 M:0.4 L:0.5 X:0.6
        device="0",
    )

if __name__ == "__main__":
    main()
```

重点在**配置文件的调试**——不同型号（S/M/L/X）对应的数据增强超参（scale/mixup/copy_paste）不一样，训练时逐步对比参数变化。

## 核心认知

1. **flash-attention 是环境配置的第一大坑**：它是编译型依赖，对 CUDA/torch/Python 版本高度敏感，Windows 下尤其难搞。先解决它，其余依赖都顺。
2. **模型型号和超参是一套**：S 到 X 越往上，数据增强的强度越大（mixup/copy_paste 更高），不是简单换个权重文件。
3. **训练是「配置驱动的」**：`train.py` 本身很简单，真正的功夫在 `data=xxx.yaml` 这个配置文件（类别、路径、增强参数）。
