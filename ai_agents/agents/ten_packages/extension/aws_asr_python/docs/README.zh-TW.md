# AWS ASR Python 擴充

一個用於 AWS 自動語音識別 (ASR) 服務的 Python 擴充，提供即時語音轉文字轉換功能，完全支援非同步操作，使用 AWS Transcribe 串流 API。

## 功能特性

- **完全非同步支援**: 採用完整的非同步架構，實現高效能語音識別
- **即時串流處理**: 使用 AWS Transcribe 的串流 API 支援低延遲即時音訊串流
- **AWS Transcribe API**: 使用 AWS Transcribe 串流轉錄 API，提供企業級效能
- **多種音訊格式**: 支援 PCM16 音訊格式
- **音訊轉儲**: 可選的音訊錄製功能，用於除錯和分析
- **可設定日誌**: 可調整的日誌層級，便於除錯
- **錯誤處理**: 全面的錯誤處理和詳細日誌記錄
- **多語言支援**: 透過 AWS Transcribe 支援多種語言
- **重連管理**: 自動重連機制，確保服務穩定性
- **會話管理**: 支援會話 ID 和音訊時間線管理

## 設定

擴充需要以下設定參數：

### 必要參數

- `params`: AWS Transcribe 設定參數，包括認證資訊和轉錄設定

### 可選參數

- `dump`: 啟用音訊轉儲（預設：false）
- `dump_path`: 轉儲音訊檔案的路徑（預設："aws_asr_in.pcm"）
- `log_level`: 日誌層級（預設："INFO"）
- `finalize_mode`: 完成模式，可選 "disconnect" 或 "mute_pkg"（預設："disconnect"）
- `mute_pkg_duration_ms`: 靜音包持續時間（毫秒）（預設：800）

### AWS Transcribe 設定參數

- `region`: AWS 區域，例如 'us-west-2'
- `access_key_id`: AWS 存取金鑰 ID
- `secret_access_key`: AWS 秘密存取金鑰
- `language_code`: 語言代碼，例如 'en-US', 'zh-CN'
- `media_sample_rate_hz`: 音訊採樣率（Hz），例如 16000
- `media_encoding`: 音訊編碼格式，例如 'pcm'
- `vocabulary_name`: 自訂詞彙表名稱（可選）參考文檔: https://docs.aws.amazon.com/transcribe/latest/dg/custom-vocabulary.html
- `session_id`: 會話 ID（可選）
- `vocab_filter_method`: 詞彙過濾方法（可選）
- `vocab_filter_name`: 詞彙過濾器名稱（可選）
- `show_speaker_label`: 是否顯示說話人標籤（可選）
- `enable_channel_identification`: 是否啟用聲道識別（可選）
- `number_of_channels`: 聲道數量（可選）
- `enable_partial_results_stabilization`: 是否啟用部分結果穩定化（可選）
- `partial_results_stability`: 部分結果穩定性設定（可選）
- `language_model_name`: 語言模型名稱（可選）

### 設定範例

```json
{
  "params": {
    "region": "us-west-2",
    "access_key_id": "your_aws_access_key_id",
    "secret_access_key": "your_aws_secret_access_key",
    "language_code": "en-US",
    "media_sample_rate_hz": 16000,
    "media_encoding": "pcm",
    "vocabulary_name": "custom-vocabulary",
    "show_speaker_label": true,
    "enable_partial_results_stabilization": true,
    "partial_results_stability": "HIGH"
  },
  "dump": false,
  "log_level": "INFO",
  "finalize_mode": "disconnect",
  "mute_pkg_duration_ms": 800
}
```

## API

擴充實作了 `AsyncASRBaseExtension` 介面，提供以下關鍵方法：

### 核心方法

- `on_init()`: 初始化 AWS ASR 用戶端和設定
- `start_connection()`: 建立與 AWS Transcribe 服務的連線
- `stop_connection()`: 關閉與 ASR 服務的連線
- `send_audio()`: 傳送音訊幀進行識別
- `finalize()`: 完成目前識別會話
- `is_connected()`: 檢查連線狀態

### 內部方法

- `_handle_transcript_event()`: 處理轉錄事件
- `_disconnect_aws()`: 斷開 AWS 連線
- `_reconnect_aws()`: 重新連線 AWS
- `_handle_finalize_disconnect()`: 處理斷開連線完成
- `_handle_finalize_mute_pkg()`: 處理靜音包完成

## 相依性

- `typing_extensions`: 用於型別提示
- `pydantic`: 用於設定驗證和資料模型
- `amazon-transcribe`: AWS Transcribe Python 用戶端程式庫
- `pytest`: 用於測試（開發相依性）

## 開發

### 建置

擴充作為 TEN Framework 建置系統的一部分進行建置。無需額外的建置步驟。

### 測試

執行單元測試：

```bash
pytest tests/
```

擴充包含全面的測試：
- 設定驗證
- 音訊處理
- 錯誤處理
- 連線管理
- 轉錄結果處理

## 使用方法

1. **安裝**: 擴充隨 TEN Framework 自動安裝
2. **設定**: 設定您的 AWS 憑證和 Transcribe 參數
3. **整合**: 透過 TEN Framework ASR 介面使用擴充
4. **監控**: 檢查日誌以進行除錯和監控

## 錯誤處理

擴充透過以下方式提供詳細的錯誤資訊：
- 模組錯誤代碼
- AWS 特定錯誤詳情
- 全面的日誌記錄
- 優雅降級和重連機制

## 效能

- **低延遲**: 使用 AWS Transcribe 的串流 API 最佳化即時處理
- **高吞吐量**: 高效的音訊幀處理
- **記憶體高效**: 最小的記憶體佔用
- **連線複用**: 維護持久的連線
- **自動重連**: 網路中斷時自動重連

## 安全性

- **憑證加密**: 敏感憑證在設定中加密
- **安全通訊**: 使用與 AWS 的安全連線
- **輸入驗證**: 全面的輸入驗證和清理
- **IAM 權限**: 支援 AWS IAM 權限管理

## 支援的 AWS 功能

擴充支援各種 AWS Transcribe 功能：
- **多語言支援**: 支援多種語言和方言
- **自訂詞彙表**: 支援自訂詞彙表
- **詞彙過濾**: 支援詞彙過濾功能
- **說話人識別**: 支援說話人標籤
- **聲道識別**: 支援多聲道音訊處理
- **部分結果**: 支援即時部分結果
- **結果穩定化**: 支援結果穩定化設定

## 音訊格式支援

- **PCM16**: 16 位 PCM 音訊格式
- **採樣率**: 支援多種採樣率（如 16000 Hz）
- **單聲道**: 支援單聲道音訊處理

## 故障排除

### 常見問題

1. **連線失敗**: 檢查 AWS 憑證和網路連線
2. **認證錯誤**: 驗證 AWS 存取金鑰和權限
3. **音訊品質問題**: 驗證音訊格式和採樣率設定
4. **效能問題**: 調整緩衝區設定和語言模型
5. **日誌問題**: 設定適當的日誌層級

### 除錯模式

透過在設定中設定 `dump: true` 啟用除錯模式，以錄製音訊進行分析。

### 重連機制

擴充包含自動重連機制：
- 網路中斷時自動重連
- 可設定的重連策略
- 連線狀態監控

## 授權

此擴充是 TEN Framework 的一部分，根據 Apache License, Version 2.0 授權。
