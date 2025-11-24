# WebRTC VAD C++ Extension

## 概要

WebRTC VAD (Voice Activity Detection) 語音活動檢測擴充，使用 C++ 語言為 TEN Framework 編寫。

## 特性

- 基於 WebRTC VAD 演算法的即時語音活動檢測
- 支援多種取樣率（8kHz, 16kHz, 32kHz, 48kHz）
- 支援多種幀長度（10ms, 20ms, 30ms）
- 可調節的檢測靈敏度（模式 0-3）
- 低延遲、低資源消耗
- 輸出語音檢測結果的同時轉發原始音訊幀

## VAD 模式

擴充支援 4 種靈敏度模式：

- **模式 0**: 品質優先 - 最不激進，誤報率低，但可能漏檢
- **模式 1**: 低激進 - 平衡模式
- **模式 2**: 中等激進 - 預設模式，推薦用於大多數場景
- **模式 3**: 高激進 - 最激進，檢出率高，但可能有更多誤報

## 設定

在 `property.json` 中設定 VAD 模式：

```json
{
  "mode": 2
}
```

## 輸入輸出

### 輸入

- **音訊幀 (audio_frame)**:
  - 取樣率: 8000, 16000, 32000, 或 48000 Hz
  - 位元深度: 16-bit (2 bytes per sample)
  - 幀長度: 10ms, 20ms, 或 30ms
  - 聲道: 支援單聲道和多聲道（多聲道時使用第一聲道）

### 輸出

1. **VAD 結果 (vad_result)**:
   - `is_speech` (bool): true 表示檢測到語音，false 表示靜音/噪音
   - `frame_name` (string): 原始音訊幀名稱
   - `timestamp` (int64): 時間戳記

2. **音訊幀 (audio_frame)**: 轉發原始音訊幀供下游處理

## 使用範例

在 TEN 應用程式的圖形設定中使用此擴充：

```json
{
  "nodes": [
    {
      "type": "extension",
      "name": "webrtc_vad",
      "addon": "webrtc_vad_cpp",
      "property": {
        "mode": 2
      }
    }
  ]
}
```

## 快速開始

### 前置條件

- TEN Framework 0.11.30 或更高版本
- C++ 編譯器支援 C++11 或更高

### 安裝

按照 TEN Framework 套件安裝指南進行安裝。

## 許可證

此套件是 TEN Framework 專案的一部分，遵循 Apache License 2.0。
