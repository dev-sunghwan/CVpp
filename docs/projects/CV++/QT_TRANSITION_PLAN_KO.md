# CV++ Qt 전환 계획

## 문서 정보
- 버전: `v0.1`
- 상태: `제안됨`
- 작성일: `2026-03-23`
- 최종 수정일: `2026-03-23`
- 작성 주체: `Tech Lead / Software Engineer Agents`

## 목적
현재 OpenCV 기반 검증 화면에서 Qt 데스크톱 UI로 현실적으로 전환하되, RTSP 및 metadata runtime core는 다시 쓰지 않는 방향을 정의한다.

## 범위
이 계획은 운영자 화면 shell에만 해당한다.
- connection setup
- live verification layout
- evidence panel
- session metrics panel
- 이후 session review panel

아래 runtime core는 재사용 대상으로 유지한다.
- `VideoRtspSession`
- `MetadataRtspSession`
- `MetadataParser`
- logging 및 probe logic

## 권장 방향
현재 C++ runtime core를 유지하고, 그 위에 Qt presentation layer를 도입한다.

```text
Qt App Shell
  -> Connection Form
  -> Live Verification Window
  -> Evidence / Metrics Panels
  -> Future Session Review Panels

Runtime Core
  -> SessionCoordinator
  -> VideoRtspSession
  -> MetadataRtspSession
  -> MetadataParser
  -> OverlayState
  -> Logging
```

## 왜 이 방향이 맞는가
- 현재 runtime 작업을 버리지 않고도 글꼴 품질, DPI 대응, 레이아웃을 크게 개선할 수 있다
- 브라우저 기반 앱보다 현재의 로컬 운영자 workflow에 더 잘 맞는다
- SQLite 기반 session review로 가기 위한 다음 단계와도 잘 맞는다

## 전환 단계
### Phase 1
- 최소 Qt shell window 생성
- 현재 connection setup canvas를 Qt form으로 교체
- 현재 live verification layout을 Qt widget 기반으로 옮김

### Phase 2
- evidence, metrics, recent metadata를 각각의 Qt panel로 분리
- `main.cpp`의 UI 책임을 더 줄임
- 현재 runtime 제어 로직은 유지

### Phase 3
- SQLite 기반 session review panel 추가
- session summary browsing 및 object-level inspection 지원

## Tradeoff
장점:
- 운영자 UI의 가독성과 유지보수성이 크게 좋아짐
- 폼, 패널, review workflow를 자연스럽게 구성할 수 있음
- 이후 metrics와 session history 확장의 기반이 됨

단점:
- 빌드와 의존성 복잡도가 증가
- UI 스레딩과 runtime 연동을 조심해서 다뤄야 함
- 단기 구현 비용은 OpenCV-only UI를 계속 키우는 것보다 큼

## 권고
Qt를 다음 주요 UI 투자 방향으로 본다. Qt shell이 도입되기 전까지는 현재 OpenCV verification view를 과도기적 화면으로 유지한다.
