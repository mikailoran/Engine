#pragma once

#include <QString>
#include <QWidget>
#include <cstdint>

#include "engine/ecs/components/transform.h"

class QStackedWidget;
class QLabel;

namespace editor {

class TransformPanel;

/**
 * @brief Shows the selected entity's components, or a message saying why
 * there is nothing to show.
 *
 * Displays copies it is handed and never reads the world, so it holds nothing
 * that could outlive the engine. Values don't follow later changes.
 */
class InspectorWidget : public QWidget {
  Q_OBJECT

 public:
  /** @brief Creates an inspector in its "nothing selected" state. */
  explicit InspectorWidget(QWidget* parent = nullptr);

  /** @brief Why the inspector shows a message instead of values. */
  enum class Diagnostic : std::uint8_t { kNoSelection = 0, kNoTransform };

  /** @brief Shows @p transform under @p entity_name, replacing any message. */
  auto UpdateTransform(const QString& entity_name,
                       const engine::Transform& transform) -> void;

  /** @brief Hides any values and shows the message for @p diagnostic. */
  auto SetDiagnostic(Diagnostic diagnostic) -> void;

  /** @brief Returns to the "nothing selected" state, e.g. after a load. */
  auto Clear() -> void;

 private:
  // These are Qt children of this widget, which owns them.

  /// Switches between the message and the Transform panel.
  QStackedWidget* content_{nullptr};
  /// Page shown instead of values; its text is the current Diagnostic.
  QLabel* diagnostic_label_{nullptr};
  TransformPanel* transform_panel_{nullptr};
};

}  // namespace editor