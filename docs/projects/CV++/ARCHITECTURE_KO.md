# CV++ 아키텍처

## 문서 정보
- 버전: `v0.5`
- 상태: `분리 구조와 Qt 전환 방향으로 승인`
- 작성일: `2026-03-18`
- 최종 수정일: `2026-03-23`
- 작성 주체: `Tech Lead Agent`

## 변경 이력
| 날짜 | 버전 | 변경 내용 |
| --- | --- | --- |
| 2026-03-18 | v0.1 | CV++의 검증 중심 MVP를 위한 초기 경량 아키텍처 문서 작성 |
| 2026-03-18 | v0.2 | 초기 아키텍처 선택지 중 logging, 재연결, config 형식을 확정 |
| 2026-03-18 | v0.3 | 현재 아키텍처를 승인 상태로 반영 |
| 2026-03-19 | v0.4 | `profile2` mixed pipeline 불안정 결과를 반영해 video와 metadata 분리 방향으로 업데이트 |
| 2026-03-23 | v0.5 | 중기 UI/저장 방향으로 Qt 데스크톱 UI와 SQLite 기반 session review를 추가 |

## 요약
v0.1에서 아키텍처는 여전히 C++ 단일 프로세스 데스크톱 애플리케이션으로 유지하는 것이 적절하다. 다만 런타임 전략은 이제 분리 구조를 우선해야 한다. 즉, 비디오는 안정적인 기준선으로 유지하고, metadata는 비디오 경로를 불안정하게 만들지 않는 별도 경로로 다루어야 한다.

지금 목표는 여전히 범용 플랫폼이 아니라 Hanwha RTSP 스트림, 커스텀 헤더, 메타데이터 관측성을 검증하는 실용 도구를 만드는 것이다.

## 권장 런타임 방향
하나의 실행 파일 안에 소수의 내부 모듈만 두되, 하나의 mixed RTSP graph를 유일한 기준선으로 보지 않는다.

```text
Config
  -> SessionCoordinator
      -> VideoRtspSession
      -> MetadataRtspSession
  -> MetadataParser
  -> OverlayState
  -> VerificationView
  -> Logging
```

## 왜 분리 방향이 맞는가
최근 디버깅에서 확인된 점:
- `profile4`는 mixed mode에서 안정적임
- `profile2`는 mixed mode에서 간헐적임
- `profile2`는 video-only mode에서 훨씬 안정적임
- metadata가 같은 pipeline에 참여할 때 startup 불안정성이 커질 수 있음

즉, mixed graph는 실험 경로로는 유지할 수 있지만, 모든 profile에 대한 안전한 운영 기준선으로 보기는 어렵다.

## 각 모듈의 책임
- `Config`: RTSP URL, 헤더, latency, output 옵션, metadata enablement를 파일에서 읽는다.
- `SessionCoordinator`: 시작, 종료, 재시도, video 세션과 metadata 세션 간의 상위 조정을 관리한다.
- `VideoRtspSession`: 안정적인 디코딩 프레임을 우선시하고 `cv::Mat` 형태로 노출한다.
- `MetadataRtspSession`: raw metadata payload를 받아 로깅과 파싱으로 전달하되, video startup을 망치지 않도록 독립적으로 다룬다.
- `MetadataParser`: raw XML 또는 텍스트 payload를 정규화된 내부 객체 목록으로 변환한다.
- `OverlayState`: freshness 기준에 따라 현재 유효한 객체만 유지하고 stale detection을 제거한다.
- `VerificationView`: 영상, overlay, parsed summary, 최근 raw metadata 근거를 한 화면에 보여준다.
- `Logging`: raw metadata 샘플, parser 실패, RTSP method 로그, startup watchdog 재시도, 세션 이벤트를 기록한다.

## 데이터 흐름
1. `Config`가 스트림과 헤더 설정을 로드한다.
2. `SessionCoordinator`가 안정적인 video 세션을 먼저 시작한다.
3. `VideoRtspSession`이 비디오 기준선을 만들고 표시용 프레임을 전달한다.
4. `MetadataRtspSession`이 별도로 시작되어 raw metadata payload를 전달한다.
5. `Logging`이 파싱 전에 raw payload를 저장한다.
6. `MetadataParser`가 정규화된 detection과 parse 상태를 만든다.
7. `OverlayState`가 freshness 규칙에 따라 현재 표시 객체를 갱신한다.
8. `VerificationView`가 영상, overlay, parsed/raw 근거를 함께 보여준다.

## 실용적 기술 선택
- 언어: C++
- 스트리밍: `rtspsrc`와 `before-send`를 포함한 GStreamer
- 프레임 및 overlay 처리: 현재 기준선은 OpenCV, 다음 UI 계층의 우선 방향은 Qt
- 설정 형식: TOML
- v0.1 저장 방식: 우선 plain file 로그, 다음 현실적 저장 계층은 SQLite
- v0.1 복구 방식: startup retry + 상태 표시 + 세션 로그

## UI 플랫폼 방향
현재 OpenCV 기반 verification view는 과도기적 인터페이스로는 허용 가능하지만, 장기 UI 플랫폼으로 보기는 어렵다.

합의된 방향:
- 현재 C++ runtime, GStreamer session, parser, logging core는 유지한다
- OpenCV verification view는 임시 운영자 콘솔로 본다
- UI 품질과 review workflow를 한 단계 끌어올릴 다음 방향은 Qt 데스크톱 애플리케이션으로 잡는다

Qt를 우선 방향으로 보는 이유:
- 제품은 여전히 로컬 데스크톱 검증 도구이다
- 연결 폼, evidence panel, metrics table, session review workflow가 필요하다
- 현재 C++ core를 브라우저 기반 전환보다 더 직접적으로 재사용할 수 있다
- 글꼴, 레이아웃, 운영자 상호작용 측면에서 OpenCV보다 훨씬 적합하다

브라우저 기반 전환은 아직 미룬다. 그 방향은 더 큰 구조 변화, 즉 service 경계, web transport, multi-user workflow까지 전제하기 때문이다.

## 저장 방향
현재 evidence 기준선은 plain log로 유지하되, 다음 현실적인 persistence 계층은 SQLite로 잡는다.

SQLite가 현재 단계에 맞는 이유:
- 인프라 부담 없이 로컬 session review와 object-level history를 지원한다
- 이후 Qt 데스크톱 UI와 잘 맞는다
- session summary, parsed object, unique object metrics 저장에 충분하다

서버형 DB는 프로젝트가 명확히 multi-camera 또는 multi-user workflow로 확장될 때 다시 검토한다.

## Mixed Pipeline 정책
mixed pipeline은 선택적 경로로 두고, 기반 구조로 보지 않는다.

사용 가능한 경우:
- 특정 profile이 mixed mode에서 이미 안정적이라고 확인된 경우
- 해당 profile에 한해 구현 단순성이 더 중요할 경우

하지만 모든 profile에 대한 공통 기준선으로 의존하지는 않는다. 현재 증거상 profile별 불안정성이 존재하기 때문이다.

## Tradeoff
분리 방향의 장점:
- metadata 불안정성이 video 안정성을 망치지 않음
- 디버깅 경계가 훨씬 명확해짐
- retry 및 복구 정책을 각각 조정하기 쉬움
- profile별 차이를 더 쉽게 분리해 관찰할 수 있음

분리 방향의 단점:
- timestamp 정렬을 별도로 고민해야 함
- 세션 관리가 조금 더 복잡해짐
- reconnect 및 health state를 두 경로에 걸쳐 조정해야 함

하지만 현재 단계에서는 이 비용이 감당 가능하다. 이 제품은 아직 플레이어나 운영 플랫폼이 아니라 검증 도구이기 때문이다. 지금은 구조 미학보다 안정성과 관측성이 더 중요하다.

## SE 반영 가이드
다음 구현 순서는 이렇게 권장한다.
1. `profile4`를 mixed-mode 검증 기준선으로 유지한다.
2. `profile2`는 video-only 안정 기준선으로 유지한다.
3. 현재 런타임을 `VideoRtspSession`과 `MetadataRtspSession` 책임으로 분리한다. 둘 다 같은 프로세스 안에 있어도 된다.
4. overlay 갱신은 metadata 파싱 결과에 의존하게 하고, metadata pipeline이 video graph를 소유하지 않게 한다.
5. startup watchdog은 유지한다.
6. 리팩터링 중 metadata startup이 다시 video startup을 묶지 않게 주의한다.

## 핵심 우려 사항
- Regex 파싱은 파싱 실패가 명시적으로 드러나고 raw payload가 보존될 때만 허용 가능하다.
- `main.cpp`는 계속 책임을 줄여가야 한다.
- metadata 동작이 충분히 안정되기 전에는 동기화 로직을 과도하게 키우지 않는다.

## 권고
runtime 기준선은 분리 구조로 유지하고, 중기 제품 진화 방향은 Qt + SQLite로 승인하는 것이 맞다.

실무적으로는 다음 의미를 가진다.
- runtime 안정성 작업은 현재 C++ / GStreamer core 안에서 계속 진행한다
- OpenCV UI 작업은 전략이 아니라 전술 수준으로 본다
- 앞으로 구현은 앱 내부에서 stable video transport와 metadata transport를 가능한 한 분리하는 쪽으로 진행한다
- 다음 큰 UI 투자는 Qt 데스크톱 계층으로 간다
- 다음 큰 저장 투자는 SQLite 기반 session review로 간다

## 열린 질문
- `profile2` metadata를 완전히 별도 RTSP 세션으로 둘지, 같은 프로세스 안의 별도 관리 경로로 둘지?
- v0.1에서 어떤 timestamp 정렬 전략이 충분한가: latest-metadata-wins, bounded freshness window, 혹은 명시적 timestamp matching?
- 불안정한 profile에 대해 mixed mode를 언제 공식적으로 내려놓을 것인가?

## SungHwan 승인 요청
승인할 아키텍처 방향: 모듈형 단일 프로세스 C++ 데스크톱 구조는 유지하되, 고해상도 검증에서 metadata가 video를 불안정하게 만들지 않도록 runtime 기준선을 video와 metadata 분리 구조로 옮긴다.
