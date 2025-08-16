# AWS ASR Python 拡張

AWS 自動音声認識 (ASR) サービスのための Python 拡張で、AWS Transcribe ストリーミング API を使用した完全な非同期サポートによるリアルタイム音声からテキストへの変換機能を提供します。

## 機能

- **完全非同期サポート**: 高性能音声認識のための完全な非同期アーキテクチャで構築
- **リアルタイムストリーミング**: AWS Transcribe ストリーミング API を使用した低遅延リアルタイム音声ストリーミングをサポート
- **AWS Transcribe API**: エンタープライズレベルのパフォーマンスのための AWS Transcribe ストリーミング転写 API を使用
- **複数の音声形式**: PCM16 音声形式をサポート
- **音声ダンプ**: デバッグと分析のためのオプション音声録音
- **設定可能なログ**: デバッグのための調整可能なログレベル
- **エラー処理**: 詳細なログ記録による包括的なエラー処理
- **多言語サポート**: AWS Transcribe を通じて複数の言語をサポート
- **再接続管理**: サービス安定性のための自動再接続メカニズム
- **セッション管理**: セッション ID と音声タイムライン管理をサポート

## 設定

拡張には以下の設定パラメータが必要です：

### 必須パラメータ

- `params`: 認証情報と転写設定を含む AWS Transcribe 設定パラメータ

### オプションパラメータ

- `dump`: 音声ダンプを有効にする（デフォルト：false）
- `dump_path`: ダンプ音声ファイルのパス（デフォルト："aws_asr_in.pcm"）
- `log_level`: ログレベル（デフォルト："INFO"）
- `finalize_mode`: 終了モード、"disconnect" または "mute_pkg"（デフォルト："disconnect"）
- `mute_pkg_duration_ms`: ミュートパッケージの継続時間（ミリ秒）（デフォルト：800）

### AWS Transcribe 設定パラメータ

- `region`: AWS リージョン、例：'us-west-2'
- `access_key_id`: AWS アクセスキー ID
- `secret_access_key`: AWS シークレットアクセスキー
- `language_code`: 言語コード、例：'en-US', 'zh-CN'
- `media_sample_rate_hz`: 音声サンプルレート（Hz）、例：16000
- `media_encoding`: 音声エンコーディング形式、例：'pcm'
- `vocabulary_name`: カスタム語彙表名（オプション）参考: https://docs.aws.amazon.com/transcribe/latest/dg/custom-vocabulary.html
- `session_id`: セッション ID（オプション）
- `vocab_filter_method`: 語彙フィルタ方法（オプション）
- `vocab_filter_name`: 語彙フィルタ名（オプション）
- `show_speaker_label`: 話者ラベルを表示するかどうか（オプション）
- `enable_channel_identification`: チャンネル識別を有効にするかどうか（オプション）
- `number_of_channels`: チャンネル数（オプション）
- `enable_partial_results_stabilization`: 部分結果安定化を有効にするかどうか（オプション）
- `partial_results_stability`: 部分結果安定性設定（オプション）
- `language_model_name`: 言語モデル名（オプション）

### 設定例

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

拡張は `AsyncASRBaseExtension` インターフェースを実装し、以下の主要メソッドを提供します：

### コアメソッド

- `on_init()`: AWS ASR クライアントと設定を初期化
- `start_connection()`: AWS Transcribe サービスへの接続を確立
- `stop_connection()`: ASR サービスへの接続を閉じる
- `send_audio()`: 認識のための音声フレームを送信
- `finalize()`: 現在の認識セッションを終了
- `is_connected()`: 接続状態をチェック

### 内部メソッド

- `_handle_transcript_event()`: 転写イベントを処理
- `_disconnect_aws()`: AWS から切断
- `_reconnect_aws()`: AWS に再接続
- `_handle_finalize_disconnect()`: 切断終了を処理
- `_handle_finalize_mute_pkg()`: ミュートパッケージ終了を処理

## 依存関係

- `typing_extensions`: 型ヒント用
- `pydantic`: 設定検証とデータモデル用
- `amazon-transcribe`: AWS Transcribe Python クライアントライブラリ
- `pytest`: テスト用（開発依存関係）

## 開発

### ビルド

拡張は TEN Framework ビルドシステムの一部としてビルドされます。追加のビルドステップは不要です。

### テスト

ユニットテストを実行：

```bash
pytest tests/
```

拡張には包括的なテストが含まれています：
- 設定検証
- 音声処理
- エラー処理
- 接続管理
- 転写結果処理

## 使用方法

1. **インストール**: 拡張は TEN Framework と共に自動的にインストールされます
2. **設定**: AWS 認証情報と Transcribe パラメータを設定
3. **統合**: TEN Framework ASR インターフェースを通じて拡張を使用
4. **監視**: デバッグと監視のためにログをチェック

## エラー処理

拡張は以下の方法で詳細なエラー情報を提供します：
- モジュールエラーコード
- AWS 固有のエラー詳細
- 包括的なログ記録
- 優雅な降格と再接続メカニズム

## パフォーマンス

- **低遅延**: AWS Transcribe ストリーミング API を使用したリアルタイム処理の最適化
- **高スループット**: 効率的な音声フレーム処理
- **メモリ効率**: 最小限のメモリ使用量
- **接続再利用**: 永続的な接続を維持
- **自動再接続**: ネットワーク中断時の自動再接続

## セキュリティ

- **認証情報暗号化**: 設定内の機密認証情報を暗号化
- **安全な通信**: AWS との安全な接続を使用
- **入力検証**: 包括的な入力検証とサニタイゼーション
- **IAM 権限**: AWS IAM 権限管理をサポート

## サポートされる AWS 機能

拡張は様々な AWS Transcribe 機能をサポートします：
- **多言語サポート**: 複数の言語と方言をサポート
- **カスタム語彙表**: カスタム語彙表をサポート
- **語彙フィルタリング**: 語彙フィルタリング機能をサポート
- **話者識別**: 話者ラベルをサポート
- **チャンネル識別**: マルチチャンネル音声処理をサポート
- **部分結果**: リアルタイム部分結果をサポート
- **結果安定化**: 結果安定化設定をサポート

## 音声形式サポート

- **PCM16**: 16 ビット PCM 音声形式
- **サンプルレート**: 様々なサンプルレートをサポート（例：16000 Hz）
- **モノチャンネル**: モノチャンネル音声処理をサポート

## トラブルシューティング

### 一般的な問題

1. **接続失敗**: AWS 認証情報とネットワーク接続をチェック
2. **認証エラー**: AWS アクセスキーと権限を確認
3. **音声品質問題**: 音声形式とサンプルレート設定を検証
4. **パフォーマンス問題**: バッファ設定と言語モデルを調整
5. **ログ問題**: 適切なログレベルを設定

### デバッグモード

設定で `dump: true` を設定してデバッグモードを有効にし、分析のために音声を録音します。

### 再接続メカニズム

拡張には自動再接続メカニズムが含まれています：
- ネットワーク中断時の自動再接続
- 設定可能な再接続戦略
- 接続状態監視

## ライセンス

この拡張は TEN Framework の一部で、Apache License, Version 2.0 の下でライセンスされています。
