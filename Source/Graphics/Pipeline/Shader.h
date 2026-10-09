#pragma once
#include <Graphics/GraphicsDevice.h>
#include <span>
#include <vector>
#include <cstddef>

namespace KT::Graphics
{
	enum class ShaderStage
	{
		Vertex,
		Pixel
	};

	// ビルド時DXC bytecodeの値コピーを所有。実行時ファイル/コンパイラ依存なし。
	class Shader
	{
	public:
		Shader(ShaderStage stage, std::span<const std::byte> bytecode);

		ShaderStage GetStage() const noexcept
		{
			return stage_;
		}

		D3D12_SHADER_BYTECODE GetBytecode() const noexcept
		{
			return {bytes_.data(), bytes_.size()};
		}

	private:
		ShaderStage stage_;
		std::vector<std::byte> bytes_;
	};
}
