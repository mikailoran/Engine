#pragma once

#include <QAbstractListModel>
#include <QObject>
#include <vector>

#include "engine/ecs/core/ecs.h"
#include "engine/ecs/core/types.h"

namespace editor {

/**
 * @brief A read-only list of the world's named entities, one row per entity
 * with a Name, for an item view to display.
 *
 * Holds a snapshot, not a reference to the world: the owner calls Refresh
 * after each scene load, and rows go stale if entities change in between.
 */
class EntityListModel : public QAbstractListModel {
  Q_OBJECT

 public:
  /** @brief Creates an empty list. */
  explicit EntityListModel(QObject* parent = nullptr);

  /** @brief The number of named entities in the snapshot. */
  [[nodiscard]] auto rowCount(const QModelIndex& parent) const -> int override;

  /**
   * @brief The name for Qt::DisplayRole, the entity id for Qt::ToolTipRole.
   * @return Empty for other roles and for an index outside the list.
   */
  [[nodiscard]] auto data(const QModelIndex& index, int role) const
      -> QVariant override;

  /**
   * @brief Replaces the rows with every entity in @p world that has a Name.
   * Resets attached views; @p world isn't kept.
   */
  auto Refresh(engine::Ecs& world) -> void;

  /** @brief Empties the list, e.g. once the world is gone. Stub for now. */
  auto Clear() -> void;

 private:
  /** @brief One listed entity, as it was at the last Refresh. */
  struct EntityRow {
    engine::Entity entity{};
    QString name;
  };

  /// Snapshot of the world's named entities, taken by Refresh.
  std::vector<EntityRow> rows_;
};

}  // namespace editor
