# Vosk ASR C++ 扩展

## 概述

为 TEN Framework 编写的基于 C++ 的 Vosk 自动语音识别扩展

## 特性

- 使用 Vosk 为 TEN Framework 提供的 C++ ASR 扩展

## 快速开始

### 前置条件

#### 安装 VOSK SDK

由于 vosk SDK（包括头文件和库文件）相对较大，默认情况下不包含在此扩展中。您需要从 [https://github.com/alphacep/vosk-api/releases](https://github.com/alphacep/vosk-api/releases) 手动下载 vosk SDK，将头文件（`vosk_api.h`）放置在 `include/` 目录中，将库文件（`libvosk.so`）放置在 `lib_private/` 目录中。

#### 安装 VOSK 模型

从 [https://alphacephei.com/vosk/models](https://alphacephei.com/vosk/models) 下载所需的模型，解压后放置在 `models/` 目录中。

### 安装

遵循 TEN Framework 包安装指南。

## 使用方法

此包可以根据框架规范集成到 TEN 应用程序中。

## 许可证

此包是 TEN Framework 项目的一部分。
