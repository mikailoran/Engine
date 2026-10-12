#pragma once

#include <QAbstractListModel>
#include <QObject>
#include <vector>

#include "engine/ecs/core/ecs.h"
#include "engine/ecs/core/types.h"

namespace editor {

class EntityListModel : public QAbstractListModel {
  Q_OBJECT

 public:
  explicit EntityListModel(QObject* parent = nullptr);

  [[nodiscard]] auto rowCount(const QModelIndex& parent) const -> int override;

  [[nodiscard]] auto data(const QModelIndex& index, int role) const
      -> QVariant override;

  auto Refresh(engine::Ecs& world) -> void;

  auto Clear() -> void;

 private:
  struct EntityRow {
    engine::Entity entity{};
    QString name;
  };
  /// Snapshot to the world.
  std::vector<EntityRow> rows_;
};

}  // namespace editor