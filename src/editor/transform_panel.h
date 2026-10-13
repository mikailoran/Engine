#pragma once

#include <QString>
#include <QWidget>
#include <array>

#include "engine/ecs/components/transform.h"

class QLabel;
class QGroupBox;
class QDoubleSpinBox;

namespace editor {

class TransformPanel : public QWidget {
  Q_OBJECT

 public:
  explicit TransformPanel(QWidget* parent = nullptr);

  auto DisplayTransform(const QString& entity_name,
                        const engine::Transform& transform) -> void;

 private:
  static constexpr double kSpinboxMax = 10'000.0;
  static constexpr double kSpinboxMin = -kSpinboxMax;
  QLabel* entity_name_{nullptr};
  QGroupBox* transform_group_{nullptr};
  std::array<QDoubleSpinBox*, 3> position_{};
};

}  // namespace editor