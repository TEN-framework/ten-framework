# Vosk ASR C++ エクステンション

## 概要

TEN Framework 用に C++ で書かれた Vosk 自動音声認識エクステンション

## 機能

- TEN Framework 用の Vosk を使用した C++ ASR エクステンション

## はじめに

### 前提条件

#### VOSK SDK のインストール

vosk SDK（ヘッダーファイルとライブラリファイルを含む）は比較的大きいため、デフォルトではこのエクステンションに含まれていません。[https://github.com/alphacep/vosk-api/releases](https://github.com/alphacep/vosk-api/releases) から vosk SDK を手動でダウンロードし、ヘッダーファイル（`vosk_api.h`）を `include/` ディレクトリに、ライブラリファイル（`libvosk.so`）を `lib_private/` ディレクトリに配置する必要があります。

#### VOSK モデルのインストール

[https://alphacephei.com/vosk/models](https://alphacephei.com/vosk/models) から必要なモデルをダウンロードし、解凍して `models/` ディレクトリに配置してください。

### インストール

TEN Framework パッケージインストールガイドに従ってください。

## 使用方法

このパッケージは、フレームワーク仕様に従って TEN アプリケーションに統合できます。

## ライセンス

このパッケージは TEN Framework プロジェクトの一部です。
