#include <Graphics/Pipeline/Shader.h>
#include <stdexcept>

namespace KT::Graphics
{
	Shader::Shader(ShaderStage stage, std::span<const std::byte> bytecode)
		: stage_(stage)
	{
		// 対応するshader stageとbytecodeを確認する
		if ((stage != ShaderStage::Vertex && stage != ShaderStage::Pixel) || bytecode.empty() || !bytecode.data())
		{
			throw std::invalid_argument("Shader stage/bytecode is invalid.");
		}

		// 呼出元のbytecodeを所有する配列へコピーする
		bytes_.assign(bytecode.begin(), bytecode.end());
	}
}
