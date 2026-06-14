/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief Bounded undo/redo history used by the editor to step document snapshots back and forth.
 */

#include <cstddef>
#include <deque>
#include <optional>
#include <utility>

namespace editor
{
/**
 * @brief Fixed-capacity undo/redo history over caller-owned snapshots.
 *
 * @tparam Snapshot Movable value type fully describing one restorable editor state.
 *
 * @details The history keeps two stacks: states the user can step back to (undo) and states the user
 * can step forward to (redo). Recording a fresh edit clears the redo stack, matching the standard
 * linear-history model. The capacity bounds memory: when an `undo`/`redo` stack would exceed it, the
 * oldest entry is dropped. The type is renderer-agnostic and unit-testable in isolation.
 */
template <typename Snapshot>
class UndoHistory
{
public:
    /** @brief Default number of snapshots retained per direction. */
    static constexpr std::size_t kDefaultCapacity = 64U;

    /**
     * @brief Builds an empty history.
     * @param capacity Maximum snapshots kept per direction; clamped to at least 1.
     */
    explicit UndoHistory(const std::size_t capacity = kDefaultCapacity) noexcept
        : capacity_(capacity == 0U ? 1U : capacity)
    {
    }

    /**
     * @brief Records the pre-edit state so it can be undone, and invalidates the redo stack.
     * @param before Snapshot captured right before a mutation.
     */
    void Record(Snapshot before)
    {
        redo_.clear();
        undo_.push_back(std::move(before));
        TrimToCapacity(undo_);
    }

    /** @brief Returns whether a previous state is available. */
    [[nodiscard]] bool CanUndo() const noexcept
    {
        return !undo_.empty();
    }

    /** @brief Returns whether a forward state is available. */
    [[nodiscard]] bool CanRedo() const noexcept
    {
        return !redo_.empty();
    }

    /**
     * @brief Steps one state back.
     * @param current Snapshot of the live state, retained so the step can be redone.
     * @return The state to restore, or `std::nullopt` when there is nothing to undo.
     */
    [[nodiscard]] std::optional<Snapshot> Undo(Snapshot current)
    {
        if (undo_.empty())
        {
            return std::nullopt;
        }

        redo_.push_back(std::move(current));
        TrimToCapacity(redo_);
        Snapshot previous = std::move(undo_.back());
        undo_.pop_back();
        return previous;
    }

    /**
     * @brief Steps one state forward.
     * @param current Snapshot of the live state, retained so the step can be undone again.
     * @return The state to restore, or `std::nullopt` when there is nothing to redo.
     */
    [[nodiscard]] std::optional<Snapshot> Redo(Snapshot current)
    {
        if (redo_.empty())
        {
            return std::nullopt;
        }

        undo_.push_back(std::move(current));
        TrimToCapacity(undo_);
        Snapshot next = std::move(redo_.back());
        redo_.pop_back();
        return next;
    }

    /** @brief Drops all retained undo and redo states. */
    void Clear() noexcept
    {
        undo_.clear();
        redo_.clear();
    }

    /** @brief Returns the number of retained undo states. */
    [[nodiscard]] std::size_t UndoDepth() const noexcept
    {
        return undo_.size();
    }

    /** @brief Returns the number of retained redo states. */
    [[nodiscard]] std::size_t RedoDepth() const noexcept
    {
        return redo_.size();
    }

    /** @brief Returns the per-direction snapshot capacity. */
    [[nodiscard]] std::size_t Capacity() const noexcept
    {
        return capacity_;
    }

private:
    void TrimToCapacity(std::deque<Snapshot>& stack) noexcept
    {
        while (stack.size() > capacity_)
        {
            stack.pop_front();
        }
    }

    std::size_t capacity_;
    std::deque<Snapshot> undo_;
    std::deque<Snapshot> redo_;
};
} // namespace editor
