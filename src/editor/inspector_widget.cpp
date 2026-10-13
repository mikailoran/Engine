#include "editor/inspector_widget.h"

#include <QLabel>
#include <QStackedWidget>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include "editor/transform_panel.h"
#include "engine/ecs/components/transform.h"

namespace editor {
InspectorWidget::InspectorWidget(QWidget* parent)
    : QWidget(parent),
      content_(new QStackedWidget(this)),
      diagnostic_label_(new QLabel(content_)),
      transform_panel_(new TransformPanel(content_)) {
  auto* layout = new QVBoxLayout(this);
  layout->addWidget(content_);
  content_->addWidget(diagnostic_label_);
  content_->addWidget(transform_panel_);

  SetDiagnostic(Diagnostic::kNoSelection);
}

auto InspectorWidget::UpdateTransform(const QString& entity_name,
                                      const engine::Transform& transform)
    -> void {
  transform_panel_->DisplayTransform(entity_name, transform);
  content_->setCurrentWidget(transform_panel_);
}

auto InspectorWidget::SetDiagnostic(Diagnostic diagnostic) -> void {
  switch (diagnostic) {
    case Diagnostic::kNoSelection:
      diagnostic_label_->setText(tr("No entity selected."));
      content_->setCurrentWidget(diagnostic_label_);
      break;
    case Diagnostic::kNoTransform:
      diagnostic_label_->setText(tr("No Transform."));
      content_->setCurrentWidget(diagnostic_label_);
      break;
  }
}

auto InspectorWidget::Clear() -> void {
  SetDiagnostic(Diagnostic::kNoSelection);
}
}  // namespace editor
