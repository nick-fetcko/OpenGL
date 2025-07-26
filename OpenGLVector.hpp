#pragma once

#include <string>
#include <sstream>

#include "Buffer.hpp"
#include "Logger.hpp"
#include "Size.hpp"
#include "Utils.hpp"
#include "VertexArray.hpp"

/*
	How to create the expected OBJ format (in Blender):
		1. File -> New -> General
		2. Click on the default cube
		3. Hit the "Delete" key [NOT backspace]
		4. File -> Import -> Scalable Vector Graphics (.svg) [NOT as Grease Pencil]
		5. Make sure you're in Object Mode
		6. Right-click on the imported SVG in the Scene Collection
		7. "Select Objects"
		8. Right-click anywhere in the main view
		9. Convert to -> Mesh
		10. Object menu -> Set origin -> Geometry to Origin
		11. Repeat steps 6-7
		11. File -> Export -> Wavefront (.obj)
		12. Check ONLY:
			12.1. "Selection Only"
			12.2. "OBJ Groups"
			12.3. Under "Geometry"
				12.3.1. "Apply Modifiers"
				12.3.2. "Triangulate Faces"
				12.3.3. "Polygroups
				13.3.4. "Keep Vertex Order"
*/

namespace Fetcko {
class OpenGLVector : public LoggableClass {
public:
	~OpenGLVector();

	bool Load(const std::filesystem::path &path, float size);
	bool Load(const std::filesystem::path &path, Size<float> size);

	void Render();

	const Size<float> &GetSize() const { return size; }

private:
	std::vector<float> vertices;
	std::vector<unsigned short> indices;
	Size<float> size;

	VertexArray vao;
	ArrayBuffer vbo;
	ElementBuffer indexBuffer;
};
}