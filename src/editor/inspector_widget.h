#pragma once

#include <QString>
#include <QWidget>
#include <cstdint>

#include "engine/ecs/components/transform.h"

class QStackedWidget;
class QLabel;

namespace editor {

class TransformPanel;

class InspectorWidget : public QWidget {
  Q_OBJECT

 public:
  explicit InspectorWidget(QWidget* parent = nullptr);

  enum class Diagnostic : std::uint8_t { kNoSelection = 0, kNoTransform };

  auto UpdateTransform(const QString& entity_name,
                       const engine::Transform& transform) -> void;

  auto SetDiagnostic(Diagnostic diagnostic) -> void;

  auto Clear() -> void;

 private:
  QStackedWidget* content_{nullptr};
  QLabel* diagnostic_label_{nullptr};
  TransformPanel* transform_panel_{nullptr};
};

}  // namespace editor