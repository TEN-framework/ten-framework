# WebRTC VAD C++ Extension

## 개요

TEN Framework용 C++로 작성된 WebRTC VAD (Voice Activity Detection) 확장.

## 특징

- WebRTC VAD 알고리즘 기반 실시간 음성 활동 감지
- 다양한 샘플레이트 지원 (8kHz, 16kHz, 32kHz, 48kHz)
- 다양한 프레임 길이 지원 (10ms, 20ms, 30ms)
- 조정 가능한 감지 민감도 (모드 0-3)
- 낮은 지연시간, 낮은 리소스 소비
- VAD 결과 출력과 동시에 원본 오디오 프레임 전달

## VAD 모드

확장은 4가지 민감도 모드를 지원합니다:

- **모드 0**: 품질 우선 - 가장 보수적, 오탐률 낮음, 음성 누락 가능성
- **모드 1**: 낮은 공격성 - 균형 모드
- **모드 2**: 중간 공격성 - 기본 모드, 대부분의 시나리오에 권장
- **모드 3**: 높은 공격성 - 가장 공격적, 높은 감지율, 오탐 증가 가능성

## 설정

`property.json`에서 VAD 모드 설정:

```json
{
  "mode": 2
}
```

## 입출력

### 입력

- **오디오 프레임 (audio_frame)**:
  - 샘플레이트: 8000, 16000, 32000, 또는 48000 Hz
  - 비트 깊이: 16비트 (샘플당 2바이트)
  - 프레임 길이: 10ms, 20ms, 또는 30ms
  - 채널: 모노 및 멀티채널 지원 (멀티채널의 경우 첫 번째 채널 사용)

### 출력

1. **VAD 결과 (vad_result)**:
   - `is_speech` (bool): true는 음성 감지, false는 무음/노이즈
   - `frame_name` (string): 원본 오디오 프레임 이름
   - `timestamp` (int64): 타임스탬프

2. **오디오 프레임 (audio_frame)**: 다운스트림 처리를 위해 원본 오디오 프레임 전달

## 사용 예제

TEN 애플리케이션의 그래프 구성에서 이 확장 사용:

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

## 빠른 시작

### 전제 조건

- TEN Framework 0.11.30 이상
- C++11 이상을 지원하는 C++ 컴파일러

### 설치

TEN Framework 패키지 설치 가이드를 따르세요.

## 라이선스

이 패키지는 TEN Framework 프로젝트의 일부이며 Apache License 2.0을 따릅니다.
