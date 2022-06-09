#ifndef _CARBON_LAYOUT_AXES_HPP_
#define _CARBON_LAYOUT_AXES_HPP_

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

namespace carbon {
	enum flex_direction {
		row,
		row_reversed,
		column,
		column_reversed
	};

	template <typename T, typename RetT>
	class axes {
	public:
		axes() = default;
		~axes() = default;

		axes(const T& main, const T& cross, flex_direction main_axis) : main(main), cross(cross), axis(main_axis) {}

		axes& operator+=(const axes& o){
			if (cross == o.cross) {
				main += o.main;
				cross += o.cross;
			}
			else {
				main += o.cross;
				cross += o.main;
			}

			return *this;
		}

		axes operator+(const axes& o) const {
			return axes(main + o.main, cross - o.main, axis);
		}

		explicit operator RetT() const {
			if (axis == row || axis == row_reversed) {
				return RetT(main, cross);
			}
			else {
				return RetT(cross, main);
			}
		}

		T main;
		T cross;

		flex_direction axis;
	};

	using axes_vec4 = axes<glm::vec2, glm::vec4>;
	using axes_vec2 = axes<float, glm::vec2>;

	// These shouldn't really be needed if axes RetT() is used instead
	[[nodiscard]] glm::vec2 get_axis(const glm::vec4& src, flex_direction axis);
	void set_axis(glm::vec4& dst, glm::vec2 src, flex_direction axis);

	[[nodiscard]] float get_axis(const glm::vec2& src, flex_direction axis);
	void set_axis(glm::vec2& dst, float src, flex_direction axis);

	[[nodiscard]] axes_vec2 get_axes_pos(const axes_vec4& bounds);
	[[nodiscard]] axes_vec2 get_axes_size(const axes_vec4& bounds);
}

#endif