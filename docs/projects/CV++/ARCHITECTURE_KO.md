# CV++ 아키텍처

## 문서 정보
- 버전: `v0.2`
- 상태: `승인 가능`
- 작성일: `2026-03-18`
- 최종 수정일: `2026-03-18`
- 작성 주체: `Tech Lead Agent`

## 변경 이력
| 날짜 | 버전 | 변경 내용 |
| --- | --- | --- |
| 2026-03-18 | v0.1 | CV++의 검증 중심 MVP를 위한 초기 경량 아키텍처 문서 작성 |
| 2026-03-18 | v0.2 | 초기 아키텍처 선택지 중 logging, 재연결, config 형식을 확정 |

## 요약
v0.1에서 아키텍처는 C++ 단일 프로세스 데스크톱 애플리케이션으로 유지하는 것이 적절하다. RTSP 전송은 GStreamer, 프레임 렌더링과 오버레이는 OpenCV를 중심으로 구성한다. 지금 목표는 범용 플랫폼이 아니라 Hanwha RTSP 스트림, 커스텀 헤더, 메타데이터 관측성을 검증하는 실용 도구를 만드는 것이다.

## 제안하는 v0.1 아키텍처
하나의 실행 파일 안에 소수의 내부 모듈만 둔다.

```text
Config
  -> RtspSession
      -> VideoPipeline
      -> MetadataPipeline
  -> MetadataParser
  -> OverlayState
  -> VerificationView
  -> Logging
```

각 모듈의 책임은 다음과 같다.
- `Config`: RTSP URL, 헤더, latency, 출력 옵션을 파일에서 읽는다.
- `RtspSession`: 파이프라인 시작, 종료, 재연결, 공통 런타임 상태를 관리한다.
- `VideoPipeline`: 디코딩된 비디오 프레임을 받아 `cv::Mat` 형태로 노출한다.
- `MetadataPipeline`: raw metadata payload를 받아 로깅과 파싱으로 전달한다.
- `MetadataParser`: raw XML 또는 텍스트 payload를 정규화된 내부 객체 목록으로 변환한다.
- `OverlayState`: freshness 기준에 따라 현재 유효한 객체만 유지하고 stale detection을 제거한다.
- `VerificationView`: 영상, overlay, parsed summary, 최근 raw metadata 근거를 한 화면에 보여준다.
- `Logging`: raw metadata 샘플, parser 실패, 세션 이벤트를 기록한다.

## 데이터 흐름
1. `Config`가 스트림과 헤더 설정을 로드한다.
2. `RtspSession`이 GStreamer 파이프라인을 만들고 커스텀 RTSP 헤더를 주입한다.
3. `VideoPipeline`이 표시용 프레임을 전달한다.
4. `MetadataPipeline`이 raw metadata payload를 전달한다.
5. `Logging`이 파싱 전에 raw payload를 저장한다.
6. `MetadataParser`가 정규화된 detection과 parse 상태를 만든다.
7. `OverlayState`가 freshness 규칙에 따라 현재 표시 객체를 갱신한다.
8. `VerificationView`가 영상, overlay, parsed/raw 근거를 함께 보여준다.

## 실용적 기술 선택
- 언어: C++
- 스트리밍: `rtspsrc`와 `before-send`를 포함한 GStreamer
- 프레임 및 overlay 처리: OpenCV
- 설정 형식: TOML
- v0.1 저장 방식: 우선 plain file 로그, raw history 필요성이 즉시 높아질 때만 SQLite 검토
- v0.1 연결 복구 방식: 단순 자동 재연결 + 상태 표시 + 세션 로그

## PM이 추가로 확정하면 좋은 항목
MVP 방향만으로도 착수는 가능하지만, 아래 항목을 먼저 정하면 구현 중 변경이 줄어든다.
- 검증 UI 레이아웃 우선순위: raw metadata 패널을 항상 노출할지, 토글형으로 둘지
- 최소 운영 흐름을 실시간 검증만으로 볼지, 사후 검토까지 포함할지
- 초기 Hanwha 샘플 기준으로 parser coverage를 어디까지면 충분하다고 볼지

## 핵심 우려 사항
- Regex 파싱은 파싱 실패가 명시적으로 드러나고 raw payload가 보존될 때만 허용 가능하다.
- 지금 대규모 리팩터링은 필요 없지만, `main.cpp`가 모든 책임을 계속 가지면 안 된다.
- 검증 화면이 과도하게 커지면 UI 작업이 핵심 metadata 검증 속도를 늦출 수 있다.

## 권고
- 다중 프로세스나 서비스 분리 대신 모듈형 단일 애플리케이션으로 시작한다.
- raw metadata를 먼저 기록하고, 그 다음 파싱하고, 마지막에 렌더링한다.
- 검증 UI는 단순하고 evidence-oriented하게 유지한다.
- raw metadata capture가 되면 실제 카메라 샘플 fixture를 빠르게 축적한다.

## 열린 질문
- 기본 검증 화면에서 raw metadata 패널을 항상 보이게 둘지, 접을 수 있게 둘지?
- v0.1에서 실시간 검증만 지원하면 충분한지, 앱 내부에서 저장 로그 재검토까지 지원해야 하는지?
- 첫 Hanwha 실측 샘플 세트에 대해 어느 정도 parser coverage를 만족 기준으로 볼지?

## SungHwan 승인 요청
v0.1에 대해 GStreamer 전송, OpenCV 렌더링, 파일 기반 설정, raw metadata logging, 단일 검증 화면을 포함한 모듈형 단일 프로세스 C++ 데스크톱 아키텍처를 승인해 달라.
