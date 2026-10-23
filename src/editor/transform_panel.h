#pragma once

#include <QString>
#include <QWidget>
#include <array>

#include "engine/ecs/components/transform.h"

class QLabel;
class QGroupBox;
class QDoubleSpinBox;

namespace editor {

/**
 * @brief A read-only form showing an entity's name and Transform position.
 *
 * Shows the values it is given and reads nothing itself; InspectorWidget
 * decides when it is visible.
 */
class TransformPanel : public QWidget {
  Q_OBJECT

 public:
  /** @brief Builds the form; it shows zeros until DisplayTransform. */
  explicit TransformPanel(QWidget* parent = nullptr);

  auto SetEntityName(const QString& entity_name) -> void;

  /** @brief Shows @p entity_name and @p transform's position, in metres. */
  auto DisplayTransform(const engine::Transform& transform) -> void;

 signals:
  void PositionEdited(bx::Vec3 position);

 private:
  /// Largest position shown, in metres; values beyond it display clamped.
  static constexpr double kSpinboxMax = 10'000.0;
  static constexpr double kSpinboxMin = -kSpinboxMax;
  QLabel* entity_name_{nullptr};
  QGroupBox* transform_group_{nullptr};
  /// The X, Y and Z boxes, in that order.
  std::array<QDoubleSpinBox*, 3> position_{};
};

}  // namespace editor