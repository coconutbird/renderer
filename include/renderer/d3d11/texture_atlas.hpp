#ifndef RENDERER_D3D11_TEXTURE_ATLAS_HPP
#define RENDERER_D3D11_TEXTURE_ATLAS_HPP

#include "texture2d.hpp"

#include <map>
#include <memory>

namespace renderer {
	// TODO: Texture atlas and UV mapping to batch textures
	class texture_atlas {
	private:
		std::unique_ptr<texture2d> texture;
		std::map<size_t, glm::vec4> textures;

	public:
		// size_t add_texture(texture2d* texture);
	};
}// namespace renderer

#endif