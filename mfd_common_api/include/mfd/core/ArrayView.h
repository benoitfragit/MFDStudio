/*
 * This file is part of MFDStudio.
 * Project author: Benoit Fra
 * Repository: https://github.com/benoitfragit/MFDStudio
 */
#pragma once

/**
 * @file
 * @brief C++17 non-owning contiguous memory view used as a lightweight std::span replacement.
 */

#include <array>
#include <cassert>
#include <cstddef>
#include <type_traits>
#include <vector>

namespace mfd
{
/**
 * @brief Lightweight non-owning view over a contiguous element buffer.
 * @tparam T Element type (can be const-qualified).
 */
template <typename T>
class ArrayView
{
public:
    /** @brief Element type stored by the view. */
    using value_type = T;
    /** @brief Pointer type used by the view. */
    using pointer = T*;
    /** @brief Reference type used by the view. */
    using reference = T&;
    /** @brief Iterator type used by the view. */
    using iterator = T*;

    /** @brief Constructs an empty view. */
    constexpr ArrayView() noexcept = default;

    /**
     * @brief Constructs a view from raw pointer and element count.
     * @param data Pointer to the first element. May be null when count is zero.
     * @param count Number of elements referenced by the view.
     */
    constexpr ArrayView(pointer data, const std::size_t count) noexcept
        : data_(data)
        , size_(count)
    {
    }

    /**
     * @brief Constructs a view from a mutable std::vector.
     * @tparam U Source vector element type.
     * @param source Source vector.
     */
    // cppcheck-suppress noExplicitConstructor
    // Intentional span-like implicit conversion from contiguous containers.
    template <typename U,
              typename = std::enable_if_t<std::is_convertible<U(*)[], T(*)[]>::value>>
    constexpr ArrayView(std::vector<U>& source) noexcept
        : data_(source.data())
        , size_(source.size())
    {
    }

    /**
     * @brief Constructs a view from a const std::vector.
     * @tparam U Source vector element type.
     * @param source Source vector.
     */
    // cppcheck-suppress noExplicitConstructor
    // Intentional span-like implicit conversion from contiguous containers.
    template <typename U,
              typename = std::enable_if_t<std::is_convertible<const U(*)[], T(*)[]>::value>>
    constexpr ArrayView(const std::vector<U>& source) noexcept
        : data_(source.data())
        , size_(source.size())
    {
    }

    /**
     * @brief Constructs a view from a mutable std::array.
     * @tparam U Source array element type.
     * @tparam N Source array size.
     * @param source Source array.
     */
    // cppcheck-suppress noExplicitConstructor
    // Intentional span-like implicit conversion from contiguous containers.
    template <typename U,
              std::size_t N,
              typename = std::enable_if_t<std::is_convertible<U(*)[], T(*)[]>::value>>
    constexpr ArrayView(std::array<U, N>& source) noexcept
        : data_(source.data())
        , size_(N)
    {
    }

    /**
     * @brief Constructs a view from a const std::array.
     * @tparam U Source array element type.
     * @tparam N Source array size.
     * @param source Source array.
     */
    // cppcheck-suppress noExplicitConstructor
    // Intentional span-like implicit conversion from contiguous containers.
    template <typename U,
              std::size_t N,
              typename = std::enable_if_t<std::is_convertible<const U(*)[], T(*)[]>::value>>
    constexpr ArrayView(const std::array<U, N>& source) noexcept
        : data_(source.data())
        , size_(N)
    {
    }

    /** @brief Returns a pointer to the first element. */
    constexpr pointer data() const noexcept
    {
        return data_;
    }

    /** @brief Returns the number of elements in the view. */
    constexpr std::size_t size() const noexcept
    {
        return size_;
    }

    /** @brief Returns true when the view is empty. */
    constexpr bool empty() const noexcept
    {
        return size_ == 0;
    }

    /**
     * @brief Returns a reference to the first element.
     * @pre The view must not be empty.
     * @note Calling this method on an empty view is undefined behavior.
     */
    constexpr reference front() const noexcept
    {
        assert(size_ > 0 && "ArrayView::front() called on an empty view");
        return data_[0];
    }

    /**
     * @brief Returns a reference to the last element.
     * @pre The view must not be empty.
     * @note Calling this method on an empty view is undefined behavior.
     */
    constexpr reference back() const noexcept
    {
        assert(size_ > 0 && "ArrayView::back() called on an empty view");
        return data_[size_ - 1];
    }

    /** @brief Returns an iterator to the first element. */
    constexpr iterator begin() const noexcept
    {
        return data_;
    }

    /** @brief Returns an iterator one past the last element. */
    constexpr iterator end() const noexcept
    {
        return data_ + size_;
    }

    /**
     * @brief Provides unchecked index access.
     * @param index Element index.
     * @return Element reference at index.
     */
    constexpr reference operator[](const std::size_t index) const noexcept
    {
        return data_[index];
    }

private:
    pointer data_ = nullptr;
    std::size_t size_ = 0;
};

/** @brief Read-only contiguous byte buffer view. */
using ByteView = ArrayView<const std::byte>;
} // namespace mfd
