# Vosk ASR C++ 확장

## 개요

TEN Framework를 위해 C++로 작성된 Vosk 자동 음성 인식 확장

## 기능

- TEN Framework용 Vosk를 사용한 C++ ASR 확장

## 시작하기

### 전제 조건

#### VOSK SDK 설치

vosk SDK(헤더 파일과 라이브러리 파일 포함)는 상대적으로 크기 때문에 기본적으로 이 확장에 포함되지 않습니다. [https://github.com/alphacep/vosk-api/releases](https://github.com/alphacep/vosk-api/releases)에서 vosk SDK를 수동으로 다운로드하고, 헤더 파일(`vosk_api.h`)을 `include/` 디렉토리에, 라이브러리 파일(`libvosk.so`)을 `lib_private/` 디렉토리에 배치해야 합니다.

#### VOSK 모델 설치

[https://alphacephei.com/vosk/models](https://alphacephei.com/vosk/models)에서 원하는 모델을 다운로드하고, 압축을 풀어 `models/` 디렉토리에 배치하세요.

### 설치

TEN Framework 패키지 설치 가이드를 따르세요.

## 사용법

이 패키지는 프레임워크 사양에 따라 TEN 애플리케이션에 통합될 수 있습니다.

## 라이선스

이 패키지는 TEN Framework 프로젝트의 일부입니다.
