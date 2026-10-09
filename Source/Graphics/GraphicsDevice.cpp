#include "GraphicsDevice.h"
#include <Core/Log.h>
#include <d3d12sdklayers.h>
#include <stdexcept>

namespace KT::Graphics
{
	GraphicsDevice::GraphicsDevice()
	{
		HRESULT result = S_FALSE;

		// Debug Layerの有効化
#ifndef NDEBUG
		ComPtr<ID3D12Debug> debug{};
		result = D3D12GetDebugInterface(IID_PPV_ARGS(debug.GetAddressOf()));
		if (FAILED(result))
		{
			throw std::runtime_error("D3D12のDebugインターフェース取得に失敗");
		}

		debug->EnableDebugLayer();
#endif

		// DXGI Factoryを作成
		UINT factoryFlags = 0;
#ifndef NDEBUG
		factoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif
		result = CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(factory_.GetAddressOf()));
		if (FAILED(result))
		{
			throw std::runtime_error("DXGI Factoryの作成に失敗");
		}

		// Adapterを決定
		SelectAdapter();

		// Deviceの作成
		result = D3D12CreateDevice(adapter_.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(device_.GetAddressOf()));
		if (FAILED(result))
		{
			throw std::runtime_error("Deviceの作成に失敗");
		}

		KT_LOG_INFO("GraphicsDeviceを作成");
	}

	void GraphicsDevice::SelectAdapter()
	{
		HRESULT result = S_FALSE;

		for (UINT index = 0; true; ++index)
		{
			ComPtr<IDXGIAdapter1> candidate{};

			result =
				factory_->EnumAdapterByGpuPreference(index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(candidate.GetAddressOf()));
			if (result == DXGI_ERROR_NOT_FOUND)
			{
				break;
			}
			else if (FAILED(result))
			{
				throw std::runtime_error("Adapterの列挙の失敗");
			}

			DXGI_ADAPTER_DESC1 adapterDesc{};
			result = candidate->GetDesc1(&adapterDesc);
			if (FAILED(result))
			{
				throw std::runtime_error("Adapterのdesc取得に失敗");
			}

			// ソフトウェアAdapterを除外
			if ((adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0)
			{
				continue;
			}

			// Deviceが作成可能か確認
			result = D3D12CreateDevice(candidate.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr);
			if (SUCCEEDED(result))
			{
				adapter_ = candidate;
				KT_LOG_INFO("使用Adapterを決定");
				return;
			}
		}

		throw std::runtime_error("利用可能なD3D12 Adapterが見つかりませんでした");
	}

	ID3D12Device* GraphicsDevice::GetDevice() const noexcept
	{
		return device_.Get();
	}

	IDXGIFactory6* GraphicsDevice::GetFactory() const noexcept
	{
		return factory_.Get();
	}
}