# AWS ASR Python 확장

AWS 자동 음성 인식 (ASR) 서비스를 위한 Python 확장으로, AWS Transcribe 스트리밍 API를 사용한 완전한 비동기 지원으로 실시간 음성-텍스트 변환 기능을 제공합니다.

## 기능

- **완전한 비동기 지원**: 고성능 음성 인식을 위한 완전한 비동기 아키텍처로 구축
- **실시간 스트리밍**: AWS Transcribe 스트리밍 API를 사용한 낮은 지연 시간의 실시간 오디오 스트리밍 지원
- **AWS Transcribe API**: 엔터프라이즈급 성능을 위한 AWS Transcribe 스트리밍 전사 API 사용
- **다양한 오디오 형식**: PCM16 오디오 형식 지원
- **오디오 덤프**: 디버깅 및 분석을 위한 선택적 오디오 녹음
- **구성 가능한 로깅**: 디버깅을 위한 조정 가능한 로그 레벨
- **오류 처리**: 상세한 로깅을 통한 포괄적인 오류 처리
- **다국어 지원**: AWS Transcribe를 통해 여러 언어 지원
- **재연결 관리**: 서비스 안정성을 위한 자동 재연결 메커니즘
- **세션 관리**: 세션 ID 및 오디오 타임라인 관리 지원

## 구성

확장에는 다음 구성 매개변수가 필요합니다:

### 필수 매개변수

- `params`: 인증 정보 및 전사 설정을 포함한 AWS Transcribe 구성 매개변수

### 선택적 매개변수

- `dump`: 오디오 덤프 활성화 (기본값: false)
- `dump_path`: 덤프 오디오 파일의 경로 (기본값: "aws_asr_in.pcm")
- `log_level`: 로그 레벨 (기본값: "INFO")
- `finalize_mode`: 완료 모드, "disconnect" 또는 "mute_pkg" (기본값: "disconnect")
- `mute_pkg_duration_ms`: 무음 패키지 지속 시간 (밀리초) (기본값: 800)

### AWS Transcribe 구성 매개변수

- `region`: AWS 리전, 예: 'us-west-2'
- `access_key_id`: AWS 액세스 키 ID
- `secret_access_key`: AWS 시크릿 액세스 키
- `language_code`: 언어 코드, 예: 'en-US', 'zh-CN'
- `media_sample_rate_hz`: 오디오 샘플 레이트 (Hz), 예: 16000
- `media_encoding`: 오디오 인코딩 형식, 예: 'pcm'
- `vocabulary_name`: 사용자 정의 어휘표 이름 (선택사항) 참조: https://docs.aws.amazon.com/transcribe/latest/dg/custom-vocabulary.html
- `session_id`: 세션 ID (선택사항)
- `vocab_filter_method`: 어휘 필터 방법 (선택사항)
- `vocab_filter_name`: 어휘 필터 이름 (선택사항)
- `show_speaker_label`: 화자 라벨 표시 여부 (선택사항)
- `enable_channel_identification`: 채널 식별 활성화 여부 (선택사항)
- `number_of_channels`: 채널 수 (선택사항)
- `enable_partial_results_stabilization`: 부분 결과 안정화 활성화 여부 (선택사항)
- `partial_results_stability`: 부분 결과 안정성 설정 (선택사항)
- `language_model_name`: 언어 모델 이름 (선택사항)

### 구성 예제

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

확장은 `AsyncASRBaseExtension` 인터페이스를 구현하며 다음 주요 메서드를 제공합니다:

### 핵심 메서드

- `on_init()`: AWS ASR 클라이언트 및 구성 초기화
- `start_connection()`: AWS Transcribe 서비스에 연결 설정
- `stop_connection()`: ASR 서비스에 대한 연결 종료
- `send_audio()`: 인식을 위한 오디오 프레임 전송
- `finalize()`: 현재 인식 세션 완료
- `is_connected()`: 연결 상태 확인

### 내부 메서드

- `_handle_transcript_event()`: 전사 이벤트 처리
- `_disconnect_aws()`: AWS에서 연결 해제
- `_reconnect_aws()`: AWS에 재연결
- `_handle_finalize_disconnect()`: 연결 해제 완료 처리
- `_handle_finalize_mute_pkg()`: 무음 패키지 완료 처리

## 의존성

- `typing_extensions`: 타입 힌트용
- `pydantic`: 구성 검증 및 데이터 모델용
- `amazon-transcribe`: AWS Transcribe Python 클라이언트 라이브러리
- `pytest`: 테스트용 (개발 의존성)

## 개발

### 빌드

확장은 TEN Framework 빌드 시스템의 일부로 빌드됩니다. 추가 빌드 단계가 필요하지 않습니다.

### 테스트

단위 테스트 실행:

```bash
pytest tests/
```

확장에는 포괄적인 테스트가 포함되어 있습니다:
- 구성 검증
- 오디오 처리
- 오류 처리
- 연결 관리
- 전사 결과 처리

## 사용법

1. **설치**: 확장은 TEN Framework와 함께 자동으로 설치됩니다
2. **구성**: AWS 자격 증명 및 Transcribe 매개변수 설정
3. **통합**: TEN Framework ASR 인터페이스를 통해 확장 사용
4. **모니터링**: 디버깅 및 모니터링을 위해 로그 확인

## 오류 처리

확장은 다음 방법으로 상세한 오류 정보를 제공합니다:
- 모듈 오류 코드
- AWS 특정 오류 세부사항
- 포괄적인 로깅
- 우아한 저하 및 재연결 메커니즘

## 성능

- **낮은 지연 시간**: AWS Transcribe 스트리밍 API를 사용한 실시간 처리 최적화
- **높은 처리량**: 효율적인 오디오 프레임 처리
- **메모리 효율성**: 최소한의 메모리 사용량
- **연결 재사용**: 지속적인 연결 유지
- **자동 재연결**: 네트워크 중단 시 자동 재연결

## 보안

- **자격 증명 암호화**: 구성에서 민감한 자격 증명 암호화
- **보안 통신**: AWS와의 보안 연결 사용
- **입력 검증**: 포괄적인 입력 검증 및 정리
- **IAM 권한**: AWS IAM 권한 관리 지원

## 지원되는 AWS 기능

확장은 다양한 AWS Transcribe 기능을 지원합니다:
- **다국어 지원**: 여러 언어 및 방언 지원
- **사용자 정의 어휘표**: 사용자 정의 어휘표 지원
- **어휘 필터링**: 어휘 필터링 기능 지원
- **화자 식별**: 화자 라벨 지원
- **채널 식별**: 다중 채널 오디오 처리 지원
- **부분 결과**: 실시간 부분 결과 지원
- **결과 안정화**: 결과 안정화 설정 지원

## 오디오 형식 지원

- **PCM16**: 16비트 PCM 오디오 형식
- **샘플 레이트**: 다양한 샘플 레이트 지원 (예: 16000 Hz)
- **모노 채널**: 모노 채널 오디오 처리 지원

## 문제 해결

### 일반적인 문제

1. **연결 실패**: AWS 자격 증명 및 네트워크 연결 확인
2. **인증 오류**: AWS 액세스 키 및 권한 확인
3. **오디오 품질 문제**: 오디오 형식 및 샘플 레이트 설정 검증
4. **성능 문제**: 버퍼 설정 및 언어 모델 조정
5. **로깅 문제**: 적절한 로그 레벨 구성

### 디버그 모드

구성에서 `dump: true`를 설정하여 디버그 모드를 활성화하고 분석을 위해 오디오를 녹음합니다.

### 재연결 메커니즘

확장에는 자동 재연결 메커니즘이 포함되어 있습니다:
- 네트워크 중단 시 자동 재연결
- 구성 가능한 재연결 전략
- 연결 상태 모니터링

## 라이선스

이 확장은 TEN Framework의 일부이며 Apache License, Version 2.0에 따라 라이선스됩니다.
