#include "editor/entity_list_model.h"

#include <QAbstractListModel>
#include <QModelIndex>
#include <QObject>
#include <QVariant>
#include <Qt>
#include <algorithm>
#include <cstddef>

#include "engine/ecs/components/name.h"
#include "engine/ecs/core/ecs.h"
#include "engine/ecs/core/types.h"

using engine::Ecs;
using engine::Entity;
using engine::Name;

namespace editor {
EntityListModel::EntityListModel(QObject* parent)
    : QAbstractListModel(parent) {}

auto EntityListModel::rowCount(const QModelIndex& parent) const -> int {
  // TODO: Understand this. Why a parent being valid warrants returning 0 ??
  if (parent.isValid()) {
    return 0;
  }
  return static_cast<int>(rows_.size());
}

auto EntityListModel::data(const QModelIndex& index, int role) const
    -> QVariant {
  if (!index.isValid() ||
      static_cast<std::size_t>(index.row()) >= rows_.size()) {
    return {};
  }
  if (role == Qt::DisplayRole) {
    return rows_.at(static_cast<std::size_t>(index.row())).name;
  }
  const auto entity = rows_.at(static_cast<std::size_t>(index.row())).entity;
  if (role == Qt::ToolTipRole) {
    return tr("Entity<%1>").arg(entity);
  }
  if (role == kEntityRole) {
    return QVariant::fromValue(entity);
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

auto EntityListModel::Sync(engine::Ecs& world) -> void {
  // TODO: name changes not picked up
  // Remove deleted entities
  for (const auto i :
       std::views::iota(std::size_t{0}, rows_.size()) | std::views::reverse) {
    if (!world.HasComponent<Name>(rows_.at(i).entity)) {
      beginRemoveRows(QModelIndex(), static_cast<int>(i), static_cast<int>(i));
      rows_.erase(rows_.begin() + i);
      endRemoveRows();
    }
  }

  // TODO: not optimal. don't run "find" every frame.
  // Add new entities
  world.View<Name>().ForEach([this](Entity entity, const Name& name) -> void {
    if (const auto it = std::ranges::find(rows_, entity, &EntityRow::entity);
        it == rows_.end()) {
      beginInsertRows(QModelIndex(), static_cast<int>(rows_.size()),
                      static_cast<int>(rows_.size()));
      rows_.emplace_back(entity, QString::fromStdString(name.value));
      endInsertRows();
    }
  });
}

auto EntityListModel::Clear() -> void {}

}  // namespace editor