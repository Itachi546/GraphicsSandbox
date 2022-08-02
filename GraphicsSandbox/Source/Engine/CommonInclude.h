#ifndef COMMONINCLUDE_H
#define COMMONINCLUDE_H

#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cmath>

#include <type_traits>

#ifndef NOMINMAX
   #define NOMINMAX
#endif

template <typename T>
struct enable_bitwise_operation
{
	static constexpr bool enabled = false;
};

// Enable flags for scoped enum
//	https://www.justsoftwaresolutions.co.uk/cplusplus/using-enum-classes-as-bitfields.html

template <typename T>
constexpr typename std::enable_if<enable_bitwise_operation<T>::enabled, T>::type operator|(T lhs, T rhs)
{
	typedef typename std::underlying_type<T>::type underlyingType;
	return static_cast<T>(
		static_cast<underlyingType>(lhs)  | static_cast<underlyingType>(rhs));
}

template <typename T>
constexpr typename std::enable_if<enable_bitwise_operation<T>::enabled, T>::type operator|=(T lhs, T rhs)
{
	typedef typename std::underlying_type<T>::type underlyingType;
	lhs = static_cast<T>(
		static_cast<underlyingType>(lhs) | static_cast<underlyingType>(rhs));
	return lhs;
}

template <typename T>
constexpr typename std::enable_if<enable_bitwise_operation<T>::enabled, T>::type operator&(T lhs, T rhs)
{
	typedef typename std::underlying_type<T>::type underlyingType;
	return static_cast<T>(
		static_cast<underlyingType>(lhs) & static_cast<underlyingType>(rhs));
}

template <typename T>
constexpr typename std::enable_if<enable_bitwise_operation<T>::enabled, T>::type operator&=(T lhs, T rhs)
{
	typedef typename std::underlying_type<T>::type underlyingType;
	lhs = static_cast<T>(
		static_cast<underlyingType>(lhs) & static_cast<underlyingType>(rhs));
	return lhs;
}


template <typename T>
bool HasFlag(T lhs, T rhs)
{
	return (lhs & rhs) == rhs;
}

#endif