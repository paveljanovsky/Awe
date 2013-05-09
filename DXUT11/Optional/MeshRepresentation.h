#include "DXUT.h"

class MeshRepresentation
{


    public:
		
		//create mesh from file
		virtual void Create(ID3D11Device* device, LPCTSTR szFileName){};
		
		// INTEL: Perfom frustum culling and set flags accordingly
		virtual void ComputeInFrustumFlags(const D3DXMATRIXA16 &worldViewProj) = 0;
		
		//RENDER (called Render(deviceContext,0))
		virtual void Render( ID3D11DeviceContext* pd3dDeviceContext){};
		
		virtual bool IsLoaded() = 0;

};
