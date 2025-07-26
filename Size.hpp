#pragma once

#include <type_traits>
#include <iostream>
#include <limits>

namespace Fetcko {
template<
	typename T,
	typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type
>
struct Size {
	Size() { *this = Zero(); }

	Size(T square) :
		width(square),
		height(square) {

	}
	Size(T width, T height) :
		width(width),
		height(height) {

	}

	static Size Max() {
		return Size(std::numeric_limits<T>::max());
	}

	static Size Min() {
		return Size(std::numeric_limits<T>::lowest());
	}

	static Size Zero() {
		return Size(0);
	}

	friend std::ostream &operator<<(std::ostream &out, const Size &size) {
		out << size.width << ", " << size.height;

		return out;
	}

	Size<T> &operator +=(const Size<T> &right) {
		width += right.width;
		height += right.height;

		return *this;
	}

	Size<T> &operator -=(const Size<T> &right) {
		width -= right.width;
		height -= right.height;

		return *this;
	}

	T width, height;
};
}