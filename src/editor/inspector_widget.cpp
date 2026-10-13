#include "editor/inspector_widget.h"

#include <QLabel>
#include <QStackedWidget>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include "engine/ecs/components/transform.h"

using engine::Transform;

namespace editor {
InspectorWidget::InspectorWidget(QWidget* parent)
    : QWidget(parent),
      layout_(new QVBoxLayout(this)),
      content_(new QStackedWidget(this)),
      diagnostic_label_(new QLabel(content_)) {
  layout_->addWidget(content_);
  content_->addWidget(diagnostic_label_);
  content_->setCurrentWidget(diagnostic_label_);

  SetDiagnostic(Diagnostic::kNoSelection);
}

auto InspectorWidget::UpdateTransform(const engine::Transform& /*transform*/)
    -> void {
  SetDiagnostic(Diagnostic::kNone);
}

auto InspectorWidget::SetDiagnostic(Diagnostic diagnostic) -> void {
  switch (diagnostic) {
    case Diagnostic::kNoSelection:
      diagnostic_label_->setText(tr("No entity selected."));
      break;
    case Diagnostic::kNoTransform:
      diagnostic_label_->setText(tr("No Transform."));
      break;
    case Diagnostic::kNone:
      diagnostic_label_->clear();
      break;
  }
}
}  // namespace editor
