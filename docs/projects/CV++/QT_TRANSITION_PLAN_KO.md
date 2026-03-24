# CV++ Qt 전환 계획

## 문서 정보
- 버전: `v0.2`
- 상태: `진행 중`
- 작성일: `2026-03-23`
- 최종 수정일: `2026-03-24`
- 작성 주체: `Tech Lead / Software Engineer Agents`

## 목적
현재 OpenCV 기반 검증 화면에서 Qt 데스크톱 UI로 현실적으로 전환하되, RTSP 및 metadata runtime core는 재사용하는 방향을 정의한다.

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

현재 상태:
- `CVPP_QtShell`은 이제 빌드와 실행이 가능하다
- shell에는 connection form, live frame surface, overlay preview, evidence panel, metrics panel, recent metadata panel이 이미 들어가 있다
- 이제 핵심은 shell 생성이 아니라 UI polish와 운영 화면 정리다

### Phase 2
- evidence, metrics, recent metadata를 각각의 Qt panel로 분리
- `main.cpp`의 UI 책임을 더 줄임
- 현재 runtime 제어 로직은 유지

현재 상태:
- evidence, metrics, recent metadata는 이미 `SharedAppState`를 통해 Qt widget으로 연결되어 있다
- 다음 과제는 Qt shell을 읽기 쉽고 신뢰 가능한 주 운영 화면으로 다듬는 일이다

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
Qt는 이제 미래 실험이 아니라 진행 중인 UI 전환으로 본다. 현재 C++/GStreamer runtime core는 유지하고, OpenCV view는 fallback으로만 남긴 채 Qt shell을 주 운영 화면으로 검증하며, 그 다음 저장 계층으로 SQLite를 준비한다.
