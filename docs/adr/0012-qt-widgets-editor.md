# 0012: Build the editor with Qt Widgets

**Status:** accepted 2026-10-11, implemented from M4a

## Context

Qt 6 has two UI stacks, and `objectives.md` left the choice open until M4a:

- **Qt Widgets:** C++ classes, `QMainWindow` with menus, toolbars and dock
  widgets, item views over `QAbstractItemModel`, `QUndoStack`. The usual
  choice for desktop tools (Qt Creator, CryEngine's Sandbox, O3DE).
- **QML / Qt Quick:** declarative UI on a GPU scene graph. The usual choice
  for embedded, HMI and mobile work.

The editor is mostly panels of many kinds: a hierarchy, an inspector, a
surface library, constraint editing, telemetry plots, menus and toolbars
around one viewport. It is also where the project builds Qt experience for
job applications, so the choice is partly which skills matter more.

## Decision

- **Qt Widgets** (Qt 6.9 or later). The editor is a `QMainWindow`, and its
  panels are widgets around the viewport.
- **The viewport is a native `QWindow`** that bgfx presents to directly,
  embedded with `QWidget::createWindowContainer`. Its Wayland surface comes
  through Qt's private API (`Qt6::GuiPrivate`).

## Alternatives considered

- **QML / Qt Quick.** Matches embedded and HMI jobs better. Rejected: Qt
  Quick has no docking, and its tree and table views are newer and thinner
  than Widgets'. Panels like these are what Widgets is built for, and the
  models behind them would be C++ either way.
- **A Widgets shell with QML panels** (`QQuickWidget`). It practises both,
  but means learning two frameworks at once in the first Qt project.
- **The ImGui overlay as the editor.** Already rejected in
  [0007](0007-engine-game-editor-split.md).

## Consequences

- The editor practises desktop-tool skills: model/view, signals and slots,
  `QUndoStack`, property editors and custom widgets. QML is not covered.
- Building the editor needs Qt's private headers
  (`qt6-qtbase-private-devel` on Fedora).
- No widget can be drawn over the viewport: a window container always sits
  on top. Viewport overlays (selection, gizmos) are drawn by the engine.
- On Wayland the keyboard goes to the window container, not the viewport, so
  the viewport takes key events through an event filter on the container.

## Revisit when

- The target jobs turn out to be mostly QML or HMI: add a QML panel through
  `QQuickWidget`, or a separate QML tool, instead of porting the editor.
- A panel needs fluid, animated or touch UI that Widgets does badly.
