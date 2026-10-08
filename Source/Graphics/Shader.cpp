#include <Graphics/Shader.h>
#include <stdexcept>

namespace KT::Graphics
{
	Shader::Shader(ShaderStage stage, std::span<const std::byte> bytecode) : stage_(stage)
	{
		if ((stage!=ShaderStage::Vertex && stage!=ShaderStage::Pixel) || bytecode.empty() || !bytecode.data())
			throw std::invalid_argument("Shader stage/bytecode is invalid.");
		bytes_.assign(bytecode.begin(),bytecode.end());
	}
}
