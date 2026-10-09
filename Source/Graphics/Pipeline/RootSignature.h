#pragma once
#include <Graphics/GraphicsDevice.h>
#include <span>

namespace KT::Graphics
{
	// root parameterの意味はRenderer側。Serialize/Createだけを担当、RootSignatureを所有。
	class RootSignature : private KT::Core::NonCopyable
	{
	public:
		RootSignature(GraphicsDevice& device, std::span<const D3D12_ROOT_PARAMETER> parameters);

		ID3D12RootSignature* GetSignature() const noexcept
		{
			return signature_.Get();
		}

		UINT GetParameterCount() const noexcept
		{
			return count_;
		}

	private:
		ComPtr<ID3D12RootSignature> signature_;
		UINT count_ = 0;
	};
}
