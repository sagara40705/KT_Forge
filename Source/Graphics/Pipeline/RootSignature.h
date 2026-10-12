#pragma once
#include <Graphics/GraphicsDevice.h>
#include <span>
#include <array>

namespace KT::Graphics
{
	// Draw時にCBVと単一descriptor tableの取り違えを検査する。
	enum class RootBindingKind
	{
		Constant,
		Texture,
		Sampler
	};

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

		[[nodiscard]] const std::array<RootBindingKind, 32>& GetKinds() const noexcept
		{
			return kinds_;
		}

		[[nodiscard]] const std::array<UINT, 32>& GetDescriptorCounts() const noexcept
		{
			return descriptorCounts_;
		}

	private:
		ComPtr<ID3D12RootSignature> signature_;
		UINT count_ = 0;
		std::array<RootBindingKind, 32> kinds_{};
		// tableの個数。CBVでは0、Samplerは1、SRVは1または2。
		std::array<UINT, 32> descriptorCounts_{};
	};
}
