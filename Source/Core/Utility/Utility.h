#pragma once
#include <type_traits>
#include <cstdint>
#include <utility>
#include <cstddef>
#include <span>

namespace KT::Core
{
	// enum classの値を基礎型に変換する
	template<class E> requires std::is_enum_v<E>
	constexpr auto ToUnderlying(E value) noexcept
	{
		return static_cast<std::underlying_type_t<E>>(value);
	}

	// 値を取り出して元を初期値へ戻す
	template<class T> requires std::is_move_constructible_v<T>
	constexpr auto Take(T& value) noexcept
	{
		return std::exchange(value, T{});
	}

	// 整数の変換先に値が収まるか確認する
	template<class T, class U> requires std::is_integral_v<T>&& std::is_integral_v<U>
	constexpr auto InRange(U value) noexcept
	{
		return std::in_range<T>(value);
	}

	// 配列を所有せず、長さ付きで受け取る
	template<class T, std::size_t N>
	constexpr auto AsSpan(T(&array)[N]) noexcept
	{
		return std::span<T>(array, N);
	}
}