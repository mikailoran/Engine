#include "editor/entity_list_model.h"

#include "engine/ecs/components/name.h"
#include "engine/ecs/core/types.h"

using engine::Ecs;
using engine::Entity;
using engine::Name;

namespace editor {
EntityListModel::EntityListModel(QObject* parent)
    : QAbstractListModel(parent) {}

auto EntityListModel::rowCount(const QModelIndex& /*parent*/) const -> int {
  return static_cast<int>(rows_.size());
}

auto EntityListModel::data(const QModelIndex& index, int role) const
    -> QVariant {
  if (!index.isValid() &&
      static_cast<std::size_t>(index.row()) >= rows_.size()) {
    return {};
  }
  if (role == Qt::DisplayRole) {
    return rows_.at(static_cast<std::size_t>(index.row())).name;
  }
  if (role == Qt::ToolTipRole) {
    auto entity = rows_.at(static_cast<std::size_t>(index.row())).entity;
    return tr("Entity<%1>").arg(entity);
  }
  return {};
}

auto EntityListModel::Refresh(Ecs& world) -> void {
  beginResetModel();
  rows_.clear();
  world.View<Name>().ForEach([this](Entity entity, const Name& name) -> void {
    rows_.emplace_back(entity, QString::fromStdString(name.value));
  });
  // TODO: Sort by their names then by the entity
  endResetModel();
}

auto EntityListModel::Clear() -> void {}

}  // namespace editor