#include "renderer/buffer.hpp"

#include "renderer/renderer.hpp"

#define _USE_MATH_DEFINES
#include <math.h>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

void renderer::buffer::clear() {
	vertices_ = {};
	batches_ = {};

	scissor_commands_ = {};
}

const std::vector<renderer::vertex>& renderer::buffer::get_vertices() {
	return vertices_;
}

const std::vector<renderer::batch>& renderer::buffer::get_batches() {
	return batches_;
}

void renderer::buffer::add_vertices(const std::vector<vertex>& vertices) {
	auto& batch = batches_.back();
	batch.size += vertices.size();

	vertices_.resize(vertices_.size() + vertices.size());
	memcpy(&vertices_[vertices_.size() - vertices.size()], vertices.data(), vertices.size() * sizeof(vertex));
}

void renderer::buffer::add_vertices(const std::vector<vertex>& vertices, D3D_PRIMITIVE_TOPOLOGY type, ID3D11ShaderResourceView* rv, color_rgba col) {
	if (vertices.empty())
		return;

	if (batches_.empty()) {
		batches_.emplace_back(0, type);
		split_batch_ = false;
	}
	else {
		auto& previous = batches_.back();

		if (split_batch_) {
			if (previous.size != 0)
				batches_.emplace_back(0, type);
			split_batch_ = false;
		}
		else if (previous.type != type || rv != nullptr || rv != previous.rv) {
			batches_.emplace_back(0, type);
		}
		else {
			if (type == D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP) {
				add_vertices({ vertices_.back(),
				               vertices.front() });
			}
			else {
				// TODO: Look into batching these topologies
				switch (type) {
					case D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP:
					case D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ:
					case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ:
						batches_.emplace_back(0, type);
						break;
					default:
						break;
				}
			}
		}
	}

	auto& new_batch = batches_.back();

	new_batch.rv = rv;
	new_batch.color = col;
	new_batch.command = active_command;

	add_vertices(vertices);
}

// TODO: Heap array class w/ SSO
void renderer::buffer::draw_polyline(std::vector<glm::vec2>& points, color_rgba col, float thickness, joint_type joint, cap_type cap) {
	polyline line;
	line.set_thickness(thickness);
	line.set_joint(joint);
	line.set_cap(cap);
	line.set_points(points);

	const auto path = line.compute();
	if (path.empty())
		return;

	std::vector<vertex> vertices;
	vertices.reserve(path.size());

	for (auto& point : path) {
		vertices.emplace_back(vertex(point.x, point.y, col));
	}

	add_vertices(vertices, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
}

void renderer::buffer::draw_point(glm::vec2 pos, color_rgba col) {
	std::vector<vertex> vertices = {
		{pos.x, pos.y, col}
	};

	add_vertices(vertices, D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
}

void renderer::buffer::draw_line(glm::vec2 start, glm::vec2 end, color_rgba col) {
	std::vector<vertex> vertices = {
		{start.x, start.y, col},
		{ end.x,  end.y,   col}
	};

	add_vertices(vertices, D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
}

void renderer::buffer::draw_rect(glm::vec4 rect, color_rgba col, float thickness) {
	std::vector<glm::vec2> points = {
		{rect.x,				  rect.y                },
		{ rect.x + rect.z - 1.0f, rect.y                },
		{ rect.x + rect.z - 1.0f, rect.y + rect.w - 1.0f},
		{ rect.x,                 rect.y + rect.w - 1.0f}
	};

	draw_polyline(points, col, thickness, joint_miter, cap_joint);
}

void renderer::buffer::draw_rect_filled(glm::vec4 rect, color_rgba col) {
	std::vector<vertex> vertices = {
		{rect.x,           rect.y,          col},
		{ rect.x + rect.z, rect.y,          col},
		{ rect.x,          rect.y + rect.w, col},
		{ rect.x + rect.z, rect.y + rect.w, col}
	};

	add_vertices(vertices, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
}

void renderer::buffer::draw_textured_quad(glm::vec4 rect, ID3D11ShaderResourceView* rv, color_rgba col) {
	split_batch_ = true;
	active_command.textured = true;

	std::vector<vertex> vertices = {
		{ rect.x,          rect.y,          col, 0.0f, 0.0f },
		{ rect.x + rect.z, rect.y,          col, 1.0f, 0.0f },
		{ rect.x,          rect.y + rect.w, col, 0.0f, 1.0f },
		{ rect.x + rect.z, rect.y + rect.w, col, 1.0f, 1.0f }
	};

	add_vertices(vertices, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, rv, col);

	active_command.textured = false;
	split_batch_ = true;
}

void renderer::buffer::draw_circle(glm::vec2 pos, float radius, color_rgba col, float thickness, size_t segments) {
	std::vector<glm::vec2> points;
	points.reserve(segments);

	for (size_t i = 0; i < segments; i++) {
		const auto theta = 2.0f * M_PI * static_cast<float>(i) / static_cast<float>(segments);

		points.emplace_back(pos.x + radius * std::cos(theta), pos.y + radius * std::sin(theta));
	}

	draw_polyline(points, col, thickness, joint_miter, cap_joint);

	for (auto& point : points) {
		draw_rect_filled({ point - 1.0f, 3.0f, 3.0f }, COLOR_RED);
	}
}

void renderer::buffer::draw_circle_filled(glm::vec2 pos, float radius, color_rgba col, size_t segments) {
	std::vector<vertex> vertices;
	vertices.reserve(segments);

	glm::vec2 first{};
	glm::vec2 prev{};
	for (size_t i = 0; i < segments; i++) {
		const auto theta = 2.0f * M_PI * static_cast<float>(i) / static_cast<float>(segments);
		glm::vec2 point = { pos.x + radius * std::cos(theta), pos.y + radius * std::sin(theta) };

		if (i == 0)
			first = point;

		if (prev != glm::vec2{}) {
			vertices.emplace_back(prev, col);
			vertices.emplace_back(point, col);
			vertices.emplace_back(pos, col);
		}

		if (i == segments - 1) {
			vertices.emplace_back(point, col);
			vertices.emplace_back(first, col);
			vertices.emplace_back(pos, col);
		}

		prev = point;
	}

	add_vertices(vertices, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void renderer::buffer::draw_glyph(glm::vec2 pos, const glyph& glyph, color_rgba col) {
	draw_textured_quad({ pos.x + glyph.bearing.x, pos.y + glyph.bearing.y, glyph.size }, glyph.rv, col);
}

void renderer::buffer::draw_text(glm::vec2 pos, const std::string& text, size_t font_id, color_rgba col, text_align h_align, text_align v_align) {
	// TODO: Handle alignment

	for (char c : text) {
		if (!isprint(c) || c == ' ')
			continue;

		auto glyph = renderer_->get_font_glyph(font_id, c);
		draw_glyph(pos, glyph, col);

		pos.x += static_cast<float>(glyph.advance) / 64.0f;
	}
}

void renderer::buffer::push_scissor(glm::vec4 bounds, bool in, bool circle) {
	scissor_commands_.emplace_back(
	DirectX::XMFLOAT4{
	bounds.x, bounds.y, bounds.z, bounds.w },
	in,
	circle);
	update_scissor();
}

void renderer::buffer::pop_scissor() {
	assert(!scissor_commands_.empty());
	scissor_commands_.pop_back();
	update_scissor();
}

void renderer::buffer::update_scissor() {
	split_batch_ = true;

	if (scissor_commands_.empty()) {
		active_command.scissor_enable = false;
	}
	else {
		const auto& new_command = scissor_commands_.back();

		active_command.scissor_enable = true;
		active_command.scissor_bounds = std::get<0>(new_command);
		active_command.scissor_in = std::get<1>(new_command);
		active_command.scissor_circle = std::get<2>(new_command);
	}
}

void renderer::buffer::push_key(color_rgba color) {
	key_commands_.push_back(color);
	update_key();
}

void renderer::buffer::pop_key() {
	assert(!key_commands_.empty());
	key_commands_.pop_back();
	update_key();
}

void renderer::buffer::update_key() {
	split_batch_ = true;

	if (key_commands_.empty()) {
		active_command.key_enable = false;
	}
	else {
		active_command.key_enable = true;
		active_command.key_color = key_commands_.back();
	}
}

void renderer::buffer::push_blur(float strength) {
	blur_commands_.emplace_back(strength);
	update_blur();
}

void renderer::buffer::pop_blur() {
	assert(!blur_commands_.empty());
	blur_commands_.pop_back();
	update_blur();
}

void renderer::buffer::update_blur() {
	split_batch_ = true;

	if (blur_commands_.empty()) {
		active_command.blur_strength = 0.0f;
	}
	else {
		active_command.blur_strength = blur_commands_.back();
	}
}