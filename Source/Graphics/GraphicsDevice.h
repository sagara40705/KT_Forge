#pragma once
#include <Core/Utility/NonCopyable.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

namespace KT::Graphics
{
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	// DirectX 12のデバイスを管理するクラス
	class GraphicsDevice : private KT::Core::NonCopyable
	{
	public:
		GraphicsDevice();
		~GraphicsDevice() = default;
	private:
		ComPtr<IDXGIFactory6> factory_;
		ComPtr<IDXGIAdapter1> adapter_;
		ComPtr<ID3D12Device> device_;

	private:
		// 使用するGPUを選択する
		void SelectAdapter();

	public:
		// デバイスを取得する
		ID3D12Device* GetDevice() const noexcept;
	};
}