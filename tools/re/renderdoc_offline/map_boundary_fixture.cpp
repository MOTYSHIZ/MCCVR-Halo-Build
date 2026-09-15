#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "renderdoc_app.h"
using Microsoft::WRL::ComPtr;
int main(int argc,char **argv) {
 if(argc<4) return 2;
 auto module=GetModuleHandleW(L"renderdoc.dll");if(!module)return 3;
 auto get=reinterpret_cast<pRENDERDOC_GetAPI>(GetProcAddress(module,"RENDERDOC_GetAPI"));
 RENDERDOC_API_1_6_0 *api{};if(!get||get(eRENDERDOC_API_Version_1_6_0,reinterpret_cast<void**>(&api))!=1)return 4;
 const int verify=std::atoi(argv[1]), nooverwrite=std::atoi(argv[2]), inside=std::atoi(argv[3]);
 api->SetCaptureOptionU32(eRENDERDOC_Option_VerifyBufferAccess,verify);
 ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> ctx;
 D3D_FEATURE_LEVEL level=D3D_FEATURE_LEVEL_11_0;
 if(FAILED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,&level,1,D3D11_SDK_VERSION,&device,nullptr,&ctx)))return 5;
 D3D11_BUFFER_DESC desc{};desc.ByteWidth=1024;desc.Usage=D3D11_USAGE_DYNAMIC;desc.BindFlags=D3D11_BIND_VERTEX_BUFFER;desc.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
 ComPtr<ID3D11Buffer> buffer;if(FAILED(device->CreateBuffer(&desc,nullptr,&buffer)))return 6;
 const char name[]="CE diagnostic cross-boundary map";buffer->SetPrivateData(WKPDID_D3DDebugObjectName,sizeof(name)-1,name);
 for(int i=0;i<64;i++){D3D11_MAPPED_SUBRESOURCE m{};if(FAILED(ctx->Map(buffer.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&m)))return 7;std::memset(m.pData,i,1024);ctx->Unmap(buffer.Get(),0);}
 if(inside)api->StartFrameCapture(device.Get(),nullptr);
 D3D11_MAPPED_SUBRESOURCE m{};if(FAILED(ctx->Map(buffer.Get(),0,nooverwrite?D3D11_MAP_WRITE_NO_OVERWRITE:D3D11_MAP_WRITE_DISCARD,0,&m)))return 8;
 std::memset(m.pData,0x5a,1024);
 if(!inside)api->StartFrameCapture(device.Get(),nullptr);
 ctx->Unmap(buffer.Get(),0);
 const auto captured=api->EndFrameCapture(device.Get(),nullptr);
 std::printf("verify=%d nooverwrite=%d map_inside_capture=%d captured=%u captures=%u\n",verify,nooverwrite,inside,captured,api->GetNumCaptures());
 return 0;
}
