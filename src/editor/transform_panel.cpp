#include "editor/transform_panel.h"

#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
#include <cstddef>

#include "engine/ecs/components/transform.h"

namespace editor {
TransformPanel::TransformPanel(QWidget* parent)
    : QWidget(parent),
      entity_name_(new QLabel(this)),
      transform_group_(new QGroupBox(tr("Transform"), this)) {
  auto* vbox = new QVBoxLayout(this);
  vbox->addWidget(entity_name_);
  vbox->addWidget(transform_group_);

  auto* grid = new QGridLayout(transform_group_);
  grid->addWidget(new QLabel(tr("X")), 0, 1);
  grid->addWidget(new QLabel(tr("Y")), 0, 2);
  grid->addWidget(new QLabel(tr("Z")), 0, 3);
  grid->addWidget(new QLabel(tr("Position")), 1, 0);

  const auto emit_position = [this]() -> void {
    emit PositionEdited({static_cast<float>(position_.at(0)->value()),
                         static_cast<float>(position_.at(1)->value()),
                         static_cast<float>(position_.at(2)->value())});
  };

  // Set up and add position Spin Boxes.
  for (std::size_t axis = 0; axis < position_.size(); ++axis) {
    auto& spinbox = position_.at(axis);
    spinbox = new QDoubleSpinBox(transform_group_);
    connect(spinbox, &QDoubleSpinBox::valueChanged, this, emit_position);

    spinbox->setReadOnly(false);
    spinbox->setKeyboardTracking(false);
    spinbox->setMinimum(kSpinboxMin);
    spinbox->setMaximum(kSpinboxMax);
    spinbox->setDecimals(2);
    grid->addWidget(spinbox, 1, static_cast<int>(axis) + 1);
  }
}

auto TransformPanel::SetEntityName(const QString& entity_name) -> void {
  entity_name_->setText(entity_name);
}

auto TransformPanel::DisplayTransform(const engine::Transform& transform)
    -> void {
  // Don't update the transform if user is modifying values
  for (auto& axis : position_) {
    if (axis->hasFocus()) {
      return;
    }
  }
  const QSignalBlocker blocker1(position_.at(0));
  const QSignalBlocker blocker2(position_.at(1));
  const QSignalBlocker blocker3(position_.at(2));
  position_.at(0)->setValue(transform.position.x);
  position_.at(1)->setValue(transform.position.y);
  position_.at(2)->setValue(transform.position.z);
}

}  // namespace editor
