#pragma once

#include <QWidget>
#include <cstdint>

#include "engine/ecs/components/transform.h"

class QStackedWidget;
class QLabel;
class QVBoxLayout;

namespace editor {

class InspectorWidget : public QWidget {
  Q_OBJECT

 public:
  explicit InspectorWidget(QWidget* parent = nullptr);

  enum class Diagnostic : std::uint8_t {
    kNone = 0,
    kNoSelection,
    kNoTransform
  };

  auto UpdateTransform(const engine::Transform& transform) -> void;

  auto SetDiagnostic(Diagnostic diagnostic) -> void;

 private:
  QVBoxLayout* layout_{nullptr};
  QStackedWidget* content_{nullptr};
  QLabel* diagnostic_label_{nullptr};
};

}  // namespace editor