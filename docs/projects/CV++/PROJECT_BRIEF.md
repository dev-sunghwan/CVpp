# PROJECT_BRIEF

## 문서 정보
- 버전: `v0.3`
- 상태: `중기 확장 방향 갱신`
- 작성일: `2026-03-18`
- 최종 수정일: `2026-03-25`
- 작성 주체: `Project Owner + Team SH`

## 변경 이력
| 날짜 | 버전 | 변경 내용 |
| --- | --- | --- |
| 2026-03-18 | v0.1 | 초기 국문 브리프 저장소 반영 |
| 2026-03-18 | v0.2 | 승인된 MVP 방향과 마일스톤 1 진행 상황을 반영하여 갱신 |
| 2026-03-25 | v0.3 | 중기 확장 방향에 thermal camera metadata 검증 트랙을 추가 |

## 1. 프로젝트 개요
`CV++`는 Hanwha Vision 카메라 및 NVR 환경을 대상으로 하는 C++ 기반 RTSP 스트리밍 및 메타데이터 검증 프로젝트다.

현재 v0.1의 목표는 다음과 같다.
1. Hanwha 장비에서 RTSP 영상을 C++로 안정적으로 수신한다
2. 커스텀 RTSP 헤더를 설정 가능하게 주입한다
3. ONVIF 계열 메타데이터를 수집, 파싱, 오버레이와 raw 근거 기준으로 검증한다

## 2. 이 프로젝트가 필요한 이유
기존 Python/OpenCV 접근은 빠른 실험에는 적합했지만, RTSP 요청 제어와 메타데이터 관측성 확보에는 한계가 있었다.

이 한계를 해결하기 위해 GStreamer 중심 구조로 전환했고, `rtspsrc`의 `before-send` 콜백을 통해 실제 송신 직전 RTSP 메시지에 커스텀 헤더를 삽입할 수 있게 했다. 이 구조는 헤더 검증과 메타데이터 파이프라인 신뢰성 확보의 기반이다.

## 3. 현재 구현 상태
현재 동작 중이거나 프로토타입 형태로 확보된 항목:
- GStreamer 기반 RTSP 파이프라인
- 커스텀 RTSP 헤더 주입
- 비디오와 ONVIF metadata 스트림 분리 처리
- 객체 metadata 파싱
- freshness 기반 overlay 정리
- TOML 기반 설정 로드
- 세션별 output 폴더 생성
- plain-file raw metadata 로그 저장

현재 파이프라인 개념:
```text
rtspsrc -> decodebin -> videoconvert -> appsink
        -> metadata appsink
```

## 4. 현재 제품 방향
이 프로젝트는 우선 범용 AI 비디오 분석 플랫폼이 아니라, 메타데이터 관측성과 신뢰성 검증 도구로 다뤄야 한다.

가까운 우선순위는 다음과 같다.
- 카메라가 실제로 보내는 metadata를 확인한다
- raw metadata와 parsed 결과를 비교 가능하게 만든다
- overlay 동작 신뢰도를 높인다
- 최소한의 한 화면 검증 흐름을 준비한다

## 5. 현재 한계
- 런타임 흐름의 많은 책임이 아직 `main.cpp`에 집중돼 있다
- parser는 여전히 regex 기반이며 투명성이 더 필요하다
- parsed 결과와 raw 근거가 한눈에 비교되도록 정리돼 있지 않다
- 재연결 동작은 아직 구현되지 않았다
- 자동 회귀 검증은 아직 제한적이다

## 6. 가까운 다음 단계
다음 마일스톤은 metadata capture와 parse transparency 강화다.

즉, 다음을 해야 한다.
1. 파싱 전에 raw metadata를 보존한다
2. parse failure와 unknown pattern을 명시적으로 드러낸다
3. 실제 Hanwha metadata 샘플 fixture를 소수 수집한다
4. 같은 세션 안에서 raw와 parsed 결과를 비교 가능하게 만든다

## 7. 중기 확장 방향
metadata observability가 신뢰 가능한 수준에 도달한 후에는 다음 방향으로 확장할 수 있다.
- 카메라 내장 AI와 외부 CV 모델 결과 비교
- 카메라에 없는 기능 추가
- 동일 타임라인 기준 결과 비교

이 항목들은 v0.1 범위 밖이다.

## 8. 한 줄 요약
CV++는 Hanwha 환경을 위한 C++ RTSP 및 metadata 검증 기반을 만드는 프로젝트이며, 현재 초점은 AI 확장이 아니라 observability다.

