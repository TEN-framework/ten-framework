# 騰訊 ASR 異步 Python 擴充

一個用於騰訊雲自動語音識別（ASR）服務的Python擴充，提供即時語音轉文字功能，支援完整的異步架構。

## 功能

- **完整異步支援**: 基於完整異步架構構建，提供高效能語音識別
- **即時串流處理**: 支援低延遲的即時音訊串流處理
- **多種結束模式**: 可設定的結束策略（斷開連線、靜音包、廠商定義）
- **音訊錄製**: 可選的音訊錄製功能，用於除錯和分析
- **保活支援**: 可設定的連線保活機制
- **錯誤處理**: 全面的錯誤處理和詳細日誌記錄
- **多語言支援**: 支援多種語言和地區
- **可設定日誌**: 可調整的日誌級別用於除錯

## 設定

擴充需要以下設定參數：

### 必需參數

- `app_id`: 騰訊雲ASR應用ID
- `secret_key`: 騰訊雲ASR金鑰
- `params`: ASR請求參數（語言、音訊格式等）

### 可選參數

- `finalize_mode`: 結束策略
  - `disconnect`: 結束後斷開連線
  - `mute_pkg`: 傳送靜音包
  - `vendor_defined`: 使用廠商定義策略（預設）
- `mute_pkg_duration_ms`: 靜音包持續時間（預設：800毫秒）
- `dump`: 啟用音訊錄製（預設：false）
- `dump_path`: 錄製音訊檔案路徑
- `keep_alive_interval`: 保活間隔（秒）
- `log_level`: 日誌級別（預設："INFO"）

### 設定範例

```json
{
  "app_id": "your_app_id",
  "secret_key": "your_secret_key",
  "params": {
    "language": "zh-TW",
    "format": "pcm",
    "sample_rate": 16000
  },
  "finalize_mode": "vendor_defined",
  "dump": false,
  "log_level": "INFO"
}
```

## API

擴充實現了 `AsyncASRBaseExtension` 介面，提供以下關鍵方法：

### 核心方法

- `on_init()`: 初始化ASR客戶端和設定
- `start_connection()`: 建立與騰訊ASR服務的連線
- `stop_connection()`: 關閉ASR服務連線
- `send_audio()`: 傳送音訊幀進行識別
- `finalize()`: 結束當前識別會話

### 事件處理器

- `on_asr_start()`: ASR會話開始時呼叫
- `on_asr_sentence_start()`: 新句子開始時呼叫
- `on_asr_sentence_change()`: 句子內容變化時呼叫
- `on_asr_sentence_end()`: 句子結束時呼叫
- `on_asr_complete()`: ASR會話完成時呼叫
- `on_asr_fail()`: ASR失敗時呼叫
- `on_asr_error()`: ASR遇到錯誤時呼叫

## 依賴

- `typing_extensions`: 用於型別提示
- `pydantic`: 用於設定驗證
- `websockets`: 用於WebSocket通訊
- `pytest`: 用於測試（開發依賴）

## 開發

### 建置

擴充作為TEN Framework建置系統的一部分建置，無需額外的建置步驟。

### 單元測試

使用以下命令執行單元測試：

```bash
pytest tests/
```

擴充包含以下方面的綜合測試：
- 設定驗證
- 音訊處理
- 錯誤處理
- 連線管理

## 使用

1. **安裝**: 擴充隨TEN Framework自動安裝
2. **設定**: 設定騰訊雲ASR憑證和參數
3. **整合**: 透過TEN Framework ASR介面使用擴充
4. **監控**: 檢查日誌進行除錯和監控

## 錯誤處理

擴充透過以下方式提供詳細的錯誤資訊：
- 模組錯誤程式碼
- 廠商特定錯誤詳情
- 全面的日誌記錄
- 優雅降級

## 效能

- **低延遲**: 針對即時處理最佳化
- **高吞吐量**: 高效的音訊幀處理
- **記憶體高效**: 最小的記憶體佔用
- **連線複用**: 在可能時保持持久連線

## 安全

- **憑證加密**: 敏感憑證在設定中加密
- **安全通訊**: 使用安全WebSocket連線
- **輸入驗證**: 全面的輸入驗證和清理

## 故障排除

### 常見問題

1. **連線失敗**: 檢查app_id和secret_key設定
2. **音訊品質問題**: 驗證音訊格式和取樣率設定
3. **效能問題**: 調整緩衝區設定和結束模式
4. **日誌問題**: 設定適當的日誌級別

### 除錯模式

在設定中設定 `dump: true` 啟用除錯模式以錄製音訊進行分析。

## 授權

此擴充是TEN Framework的一部分，根據Apache License, Version 2.0授權。
