# Vosk ASR C++ 擴展

## 概述

為 TEN Framework 編寫的基於 C++ 的 Vosk 自動語音識別擴展

## 特性

- 使用 Vosk 為 TEN Framework 提供的 C++ ASR 擴展

## 快速開始

### 前置條件

#### 安裝 VOSK SDK

由於 vosk SDK（包括標頭檔案和函式庫檔案）相對較大，預設情況下不包含在此擴展中。您需要從 [https://github.com/alphacep/vosk-api/releases](https://github.com/alphacep/vosk-api/releases) 手動下載 vosk SDK，將標頭檔案（`vosk_api.h`）放置在 `include/` 目錄中，將函式庫檔案（`libvosk.so`）放置在 `lib_private/` 目錄中。

#### 安裝 VOSK 模型

從 [https://alphacephei.com/vosk/models](https://alphacephei.com/vosk/models) 下載所需的模型，解壓後放置在 `models/` 目錄中。

### 安裝

遵循 TEN Framework 套件安裝指南。

## 使用方法

此套件可以根據框架規範整合到 TEN 應用程式中。

## 授權

此套件是 TEN Framework 專案的一部分。
