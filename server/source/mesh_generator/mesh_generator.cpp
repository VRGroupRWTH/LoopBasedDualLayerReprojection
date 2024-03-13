#include "mesh_generator.hpp"
#include "line_generator.hpp"
#include "loop_generator.hpp"

MeshGenerator* make_mesh_generator(MeshGeneratorType type)
{
	switch (type)
	{
	case MESH_GENERATOR_TYPE_LINE_BASED:
		return new LineGenerator;
	case MESH_GENERATOR_TYPE_LOOP_BASED:
		return new LoopGenerator;
	default:
		break;
	}

	return nullptr;
}