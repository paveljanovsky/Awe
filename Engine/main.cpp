// Copyright 2010 Intel Corporation
// All Rights Reserved
//
// Permission is granted to use, copy, distribute and prepare derivative works of this
// software for any purpose and without fee, provided, that the above copyright notice
// and this statement appear in all copies.  Intel makes no representations about the
// suitability of this software for any purpose.  THIS SOFTWARE IS PROVIDED "AS IS."
// INTEL SPECIFICALLY DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED, AND ALL LIABILITY,
// INCLUDING CONSEQUENTIAL AND OTHER INDIRECT DAMAGES, FOR THE USE OF THIS SOFTWARE,
// INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PROPRIETARY RIGHTS, AND INCLUDING THE
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  Intel does not
// assume any responsibility for any errors which may appear in this software nor any
// responsibility to update it.

//Modified by Matthew Hart, Pavel Janovsky
//#MSH = modified or commented by Matthew Hart

#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTgui.h"
#include "DXUTsettingsDlg.h"
#include "SDKmisc.h"
#include "SDKMesh.h"
#include "App.h"
#include "ShaderDefines.h"
#include <sstream>
#include "SceneGraph.h"
#include "PhysXObject.h"
#include "EnginePhysics.h"
#include <vector>

// Constants
static const float kLightRotationSpeed = 0.05f;
static const float kSliderFactorResolution = 10000.0f;

static vector<PhysXObject*> *cubeList;


enum SCENE_SELECTION {
	CUBE_WORLD,
    POWER_PLANT_SCENE,
    SPONZA_SCENE,
	MULTI_SCENE,
	CUBES,
};

enum {
    UI_TOGGLEFULLSCREEN,
    UI_TOGGLEWARP,
    UI_CHANGEDEVICE,
    UI_ANIMATELIGHT,
    UI_FACENORMALS,
    UI_SELECTEDSCENE,
    UI_VISUALIZELIGHTCOUNT,
    UI_VISUALIZEPERSAMPLESHADING,
    UI_LIGHTINGONLY,
    UI_LIGHTS,
    UI_LIGHTSTEXT,
    UI_LIGHTSPERPASS,
    UI_LIGHTSPERPASSTEXT,
    UI_CULLTECHNIQUE,
    UI_MSAA,
};

// List these top to bottom, since it is also the reverse draw order
enum {
    HUD_GENERIC = 0,
    HUD_PARTITIONS,
    HUD_FILTERING,
    HUD_EXPERT,
    HUD_NUM,
};


App* gApp = 0;

CFirstPersonCamera gViewerCamera;

SceneGraph sceneGraph;
/*CDXUTSDKMesh gMeshOpaque;
CDXUTSDKMesh gMeshOpaque2;
CDXUTSDKMesh gMeshAlpha;*/
D3DXMATRIXA16 gWorldMatrix;
ID3D11ShaderResourceView* gSkyboxSRV = 0;

// DXUT GUI stuff
#pragma region DXUT GUI stuff
CDXUTDialogResourceManager gDialogResourceManager;
CD3DSettingsDlg gD3DSettingsDlg;
CDXUTDialog gHUD[HUD_NUM];
CDXUTCheckBox* gAnimateLightCheck = 0;
CDXUTComboBox* gMSAACombo = 0;
CDXUTComboBox* gSceneSelectCombo = 0;
CDXUTComboBox* gCullTechniqueCombo = 0;
CDXUTSlider* gLightsSlider = 0;
CDXUTTextHelper* gTextHelper = 0;
#pragma endregion

float gAspectRatio;
bool gDisplayUI = true;
bool gZeroNextFrameTime = true;

// Any UI state passed directly to rendering shaders
UIConstants gUIConstants;

#pragma region Function Prototypes
#pragma region Callback Declarations
bool CALLBACK ModifyDeviceSettings(DXUTDeviceSettings* deviceSettings, void* userContext);
void CALLBACK OnFrameMove(double time, float elapsedTime, void* userContext);
LRESULT CALLBACK MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* noFurtherProcessing,
                         void* userContext);
void CALLBACK OnKeyboard(UINT character, bool keyDown, bool altDown, void* userContext);
void CALLBACK OnGUIEvent(UINT eventID, INT controlID, CDXUTControl* control, void* userContext);
HRESULT CALLBACK OnD3D11CreateDevice(ID3D11Device* d3dDevice, const DXGI_SURFACE_DESC* backBufferSurfaceDesc,
                                     void* userContext);
HRESULT CALLBACK OnD3D11ResizedSwapChain(ID3D11Device* d3dDevice, IDXGISwapChain* swapChain,
                                         const DXGI_SURFACE_DESC* backBufferSurfaceDesc, void* userContext);
void CALLBACK OnD3D11ReleasingSwapChain(void* userContext);
void CALLBACK OnD3D11DestroyDevice(void* userContext);
void CALLBACK OnD3D11FrameRender(ID3D11Device* d3dDevice, ID3D11DeviceContext* d3dDeviceContext, double time,
                                 float elapsedTime, void* userContext);
#pragma endregion

void LoadSkybox(LPCWSTR fileName);

void InitApp(ID3D11Device* d3dDevice);
void DestroyApp();
void InitScene(ID3D11Device* d3dDevice);
void DestroyScene();

void InitUI();
void UpdateUIState();
#pragma endregion

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, INT nCmdShow)
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

#pragma region Callback Calls
    DXUTSetCallbackDeviceChanging(ModifyDeviceSettings);
    DXUTSetCallbackMsgProc(MsgProc);
    DXUTSetCallbackKeyboard(OnKeyboard);
    DXUTSetCallbackFrameMove(OnFrameMove);

    DXUTSetCallbackD3D11DeviceCreated(OnD3D11CreateDevice);
    DXUTSetCallbackD3D11SwapChainResized(OnD3D11ResizedSwapChain);
    DXUTSetCallbackD3D11FrameRender(OnD3D11FrameRender);
    DXUTSetCallbackD3D11SwapChainReleasing(OnD3D11ReleasingSwapChain);
    DXUTSetCallbackD3D11DeviceDestroyed(OnD3D11DestroyDevice);
#pragma endregion

    DXUTSetIsInGammaCorrectMode(true);

    DXUTInit(true, true, 0);
    InitUI();

#pragma region Adjust Display and Handling Settings
    DXUTSetCursorSettings(true, true);
    DXUTSetHotkeyHandling(true, true, false);
    DXUTCreateWindow(L"Deferred Shading");
    DXUTCreateDevice(D3D_FEATURE_LEVEL_11_0, true, 1280, 720);
#pragma endregion

    DXUTMainLoop();

    return DXUTGetExitCode();
}


void InitUI()
{
    // Setup default UI state
    // NOTE: All of these are made directly available in the shader constant buffer
    // This is convenient for development purposes.

#pragma region set UIConstants
    gUIConstants.lightingOnly = 0;
    gUIConstants.faceNormals = 0;
    gUIConstants.visualizeLightCount = 0;
    gUIConstants.visualizePerSampleShading = 0;
    gUIConstants.lightCullTechnique = CULL_QUAD_DEFERRED_LIGHTING;
#pragma endregion

    gD3DSettingsDlg.Init(&gDialogResourceManager);

    for (int i = 0; i < HUD_NUM; ++i) {
        gHUD[i].Init(&gDialogResourceManager);
        gHUD[i].SetCallback(OnGUIEvent);
    }

    int width = 200;

#pragma region Generate Hud
    // Generic HUD
    {
        CDXUTDialog* HUD = &gHUD[HUD_GENERIC];
        int y = 0;

        HUD->AddButton(UI_TOGGLEFULLSCREEN, L"Toggle full screen", 0, y, width, 23);
        y += 26;

        // Warp doesn't support DX11 yet
        //HUD->AddButton(UI_TOGGLEWARP, L"Toggle WARP (F3)", 0, y, width, 23, VK_F3);
        //y += 26;

        HUD->AddButton(UI_CHANGEDEVICE, L"Change device (F2)", 0, y, width, 23, VK_F2);
        y += 26;

#pragma region Add MSAA select Combo Box Items
        HUD->AddComboBox(UI_MSAA, 0, y, width, 23, 0, false, &gMSAACombo);
        y += 26;
        gMSAACombo->AddItem(L"No MSAA", ULongToPtr(1));
        gMSAACombo->AddItem(L"2x MSAA", ULongToPtr(2));
        gMSAACombo->AddItem(L"4x MSAA", ULongToPtr(4));
        gMSAACombo->AddItem(L"8x MSAA", ULongToPtr(8));
#pragma endregion

#pragma region Add Scene/mesh select Combo Box items
        HUD->AddComboBox(UI_SELECTEDSCENE, 0, y, width, 23, 0, false, &gSceneSelectCombo);
        y += 26;
		gSceneSelectCombo->AddItem(L"Cube World (AWE)", ULongToPtr(CUBE_WORLD));
        gSceneSelectCombo->AddItem(L"Power Plant", ULongToPtr(POWER_PLANT_SCENE));
        gSceneSelectCombo->AddItem(L"Sponza", ULongToPtr(SPONZA_SCENE));
		gSceneSelectCombo->AddItem(L"Multi Object Scene", ULongToPtr(MULTI_SCENE));
		gSceneSelectCombo->AddItem(L"CUBES", ULongToPtr(CUBES));
#pragma endregion

#pragma region Add Options Checkboxes and sliders
        HUD->AddCheckBox(UI_ANIMATELIGHT, L"Animate Lights", 0, y, width, 23, false, VK_SPACE, false, &gAnimateLightCheck);
        y += 26;

        HUD->AddCheckBox(UI_LIGHTINGONLY, L"Lighting Only", 0, y, width, 23, gUIConstants.lightingOnly != 0);
        y += 26;

        HUD->AddCheckBox(UI_FACENORMALS, L"Face Normals", 0, y, width, 23, gUIConstants.faceNormals != 0);
        y += 26;

        HUD->AddCheckBox(UI_VISUALIZELIGHTCOUNT, L"Visualize Light Count", 0, y, width, 23, gUIConstants.visualizeLightCount != 0);
        y += 26;

        HUD->AddCheckBox(UI_VISUALIZEPERSAMPLESHADING, L"Visualize Shading Freq.", 0, y, width, 23, gUIConstants.visualizePerSampleShading != 0);
        y += 26;

        HUD->AddStatic(UI_LIGHTSTEXT, L"Lights:", 0, y, width, 23);
        y += 26;
        HUD->AddSlider(UI_LIGHTS, 0, y, width, 23, 0, MAX_LIGHTS_POWER, MAX_LIGHTS_POWER, false, &gLightsSlider);
        y += 26;
#pragma endregion

#pragma region Add Lighting method select Combo box items
        HUD->AddComboBox(UI_CULLTECHNIQUE, 0, y, width, 23, 0, false, &gCullTechniqueCombo);
        y += 26;
        gCullTechniqueCombo->AddItem(L"No Cull Forward", ULongToPtr(CULL_FORWARD_NONE));
        gCullTechniqueCombo->AddItem(L"No Cull Pre-Z", ULongToPtr(CULL_FORWARD_PREZ_NONE));
        gCullTechniqueCombo->AddItem(L"No Cull Deferred", ULongToPtr(CULL_DEFERRED_NONE));
        gCullTechniqueCombo->AddItem(L"Quad", ULongToPtr(CULL_QUAD));
        gCullTechniqueCombo->AddItem(L"Quad Deferred Light", ULongToPtr(CULL_QUAD_DEFERRED_LIGHTING));
        gCullTechniqueCombo->AddItem(L"Compute Shader Tile", ULongToPtr(CULL_COMPUTE_SHADER_TILE));
        gCullTechniqueCombo->SetSelectedByData(ULongToPtr(gUIConstants.lightCullTechnique));
#pragma endregion

        HUD->SetSize(width, y);
    }

    // Expert HUD
    {
        CDXUTDialog* HUD = &gHUD[HUD_EXPERT];
        int y = 0;
    
        HUD->SetSize(width, y);

        // Initially hidden
        HUD->SetVisible(false);
    }
#pragma endregion

    UpdateUIState();
}


void UpdateUIState()
{
    //int technique = PtrToUint(gCullTechniqueCombo->GetSelectedData());
}


void InitApp(ID3D11Device* d3dDevice)
{
	//#MSH if an app exists, destroy it
    DestroyApp();
    
    // Get current UI settings
    unsigned int msaaSamples = PtrToUint(gMSAACombo->GetSelectedData());
    gApp = new App(d3dDevice, 1 << gLightsSlider->GetValue(), msaaSamples);

    // Initialize with the current surface description
    gApp->OnD3D11ResizedSwapChain(d3dDevice, DXUTGetDXGIBackBufferSurfaceDesc());

    // Zero out the elapsed time for the next frame
    gZeroNextFrameTime = true;
}


void DestroyApp()
{
    SAFE_DELETE(gApp);
}

void LoadSkybox(ID3D11Device* d3dDevice, LPCWSTR fileName)
{
    ID3D11Resource* resource = 0;
    HRESULT hr;
    hr = D3DX11CreateTextureFromFile(d3dDevice, fileName, 0, 0, &resource, 0);
    assert(SUCCEEDED(hr));

    d3dDevice->CreateShaderResourceView(resource, 0, &gSkyboxSRV);
    resource->Release();
}

void InitScene(ID3D11Device* d3dDevice)
{
    DestroyScene();
    D3DXVECTOR3 cameraEye(0.0f, 0.0f, 0.0f);
    D3DXVECTOR3 cameraAt(0.0f, 0.0f, 0.0f);
    float sceneScaling = 1.0f;
    D3DXVECTOR3 sceneTranslation(0.0f, 0.0f, 0.0f);
    bool zAxisUp = false;

#pragma region Pick Scene
    SCENE_SELECTION scene = static_cast<SCENE_SELECTION>(PtrToUlong(gSceneSelectCombo->GetSelectedData()));
    switch (scene) {
		case CUBE_WORLD: {
            sceneScaling = 1.0f;

			D3DXMatrixScaling(&gWorldMatrix, sceneScaling, sceneScaling, sceneScaling);
			if (zAxisUp) {
				D3DXMATRIXA16 m;
				D3DXMatrixRotationX(&m, -D3DX_PI / 2.0f);
				gWorldMatrix *= m;
			}
			{
				D3DXMATRIXA16 t;
				D3DXMatrixTranslation(&t, sceneTranslation.x, sceneTranslation.y, sceneTranslation.z);
				gWorldMatrix *= t;
			}

			sceneGraph.StartScene(gWorldMatrix,sceneScaling);

			//sceneGraph.Add(d3dDevice, L"..\\media\\cube\\cube.sdkmesh");
			sceneGraph.AddXnb(d3dDevice, "..\\media\\cube\\Sphere.xnb");
            //gMeshOpaque.Create(d3dDevice, L"..\\media\\cube\\cube.sdkmesh");			
            LoadSkybox(d3dDevice, L"..\\media\\Skybox\\EmptySpace.dds");

            cameraEye = sceneScaling * D3DXVECTOR3(100.0f, 5.0f, 5.0f);
            cameraAt = sceneScaling * D3DXVECTOR3(0.0f, 0.0f, 0.0f);
        } break;

        case POWER_PLANT_SCENE: {
            sceneScaling = 1.0f;

			D3DXMatrixScaling(&gWorldMatrix, sceneScaling, sceneScaling, sceneScaling);
			if (zAxisUp) {
				D3DXMATRIXA16 m;
				D3DXMatrixRotationX(&m, -D3DX_PI / 2.0f);
				gWorldMatrix *= m;
			}
			{
				D3DXMATRIXA16 t;
				D3DXMatrixTranslation(&t, sceneTranslation.x, sceneTranslation.y, sceneTranslation.z);
				gWorldMatrix *= t;
			}
			
			sceneGraph.StartScene(gWorldMatrix,sceneScaling);

			sceneGraph.Add(d3dDevice, L"..\\media\\powerplant\\powerplant.sdkmesh");
			//gMeshOpaque.Create(d3dDevice, L"..\\media\\powerplant\\powerplant.sdkmesh");
            LoadSkybox(d3dDevice, L"..\\media\\Skybox\\EmptySpace.dds");

            cameraEye = sceneScaling * D3DXVECTOR3(100.0f, 5.0f, 5.0f);
            cameraAt = sceneScaling * D3DXVECTOR3(0.0f, 0.0f, 0.0f);
        } break;

        case SPONZA_SCENE: {

            sceneScaling = 0.05f;

			D3DXMatrixScaling(&gWorldMatrix, sceneScaling, sceneScaling, sceneScaling);
			if (zAxisUp) {
				D3DXMATRIXA16 m;
				D3DXMatrixRotationX(&m, -D3DX_PI / 2.0f);
				gWorldMatrix *= m;
			}
			{
				D3DXMATRIXA16 t;
				D3DXMatrixTranslation(&t, sceneTranslation.x, sceneTranslation.y, sceneTranslation.z);
				gWorldMatrix *= t;
			}
			
			sceneGraph.StartScene(gWorldMatrix,sceneScaling);
			

			sceneGraph.Add(d3dDevice, L"..\\media\\Sponza\\sponza_dds.sdkmesh");
			//gMeshOpaque.Create(d3dDevice, L"..\\media\\Sponza\\sponza_dds.sdkmesh");
            LoadSkybox(d3dDevice, L"..\\media\\Skybox\\EmptySpace.dds");

            cameraEye = sceneScaling * D3DXVECTOR3(1200.0f, 200.0f, 100.0f);
            cameraAt = sceneScaling * D3DXVECTOR3(0.0f, 0.0f, 0.0f);
        } break;
		case MULTI_SCENE:{
            sceneScaling = .05f;
			
			D3DXMatrixScaling(&gWorldMatrix, sceneScaling, sceneScaling, sceneScaling);
			if (zAxisUp) {
				D3DXMATRIXA16 m;
				D3DXMatrixRotationX(&m, -D3DX_PI / 2.0f);
				gWorldMatrix *= m;
			}
			{
				D3DXMATRIXA16 t;
				D3DXMatrixTranslation(&t, sceneTranslation.x, sceneTranslation.y, sceneTranslation.z);
				gWorldMatrix *= t;
			}
			
			sceneGraph.StartScene(gWorldMatrix,sceneScaling);

			sceneGraph.Add(d3dDevice, L"..\\media\\Sponza\\sponza_dds.sdkmesh");
			D3DXMATRIXA16 translate;
			D3DXMatrixTranslation(&translate,0,10,0);
			sceneGraph.Add(d3dDevice,L"..\\media\\powerplant\\powerplant.sdkmesh",translate);
			//gMeshOpaque.Create(d3dDevice, L"..\\media\\Sponza\\sponza_dds.sdkmesh");
			//gMeshOpaque2.Create(d3dDevice,L"..\\media\\powerplant\\powerplant.sdkmesh");
            LoadSkybox(d3dDevice, L"..\\media\\Skybox\\EmptySpace.dds");

            cameraEye = sceneScaling * D3DXVECTOR3(100.0f, 5.0f, 5.0f);
            cameraAt = sceneScaling * D3DXVECTOR3(0.0f, 0.0f, 0.0f);
		}break;
		case CUBES:
		{
			sceneScaling = 1.0f;

			D3DXMatrixScaling(&gWorldMatrix, sceneScaling, sceneScaling, sceneScaling);
			if (zAxisUp) {
				D3DXMATRIXA16 m;
				D3DXMatrixRotationX(&m, -D3DX_PI / 2.0f);
				gWorldMatrix *= m;
			}
			{
				D3DXMATRIXA16 t;
				D3DXMatrixTranslation(&t, sceneTranslation.x, sceneTranslation.y, sceneTranslation.z);
				gWorldMatrix *= t;
			}

			sceneGraph.StartScene(gWorldMatrix,sceneScaling);
			D3DXMATRIXA16 translate;
			D3DXMatrixTranslation(&translate,0,0,0);
			
			D3DXMATRIXA16 s;
			D3DXMatrixScaling(&s,100,0.01,100);
			s=s*translate;
			//sceneGraph.Add(d3dDevice, L"..\\media\\cube\\cube.sdkmesh",s);
			//sceneGraph.Add(d3dDevice, L"..\\media\\cube\\cube.sdkmesh",s);


			//Initializing PhysX
			EnginePhysics::InitializePhysX(cubeList);

			//Creating all of the cubes
			for(int i = 0; i < cubeList->size(); i++)
			{
				//(*cubeList)[i]->id = sceneGraph.Add(d3dDevice, L"..\\media\\cube\\cube.sdkmesh",
				//	(*cubeList)[i]->x, (*cubeList)[i]->y, (*cubeList)[i]->z, (*cubeList)[i]->sx, (*cubeList)[i]->sy, (*cubeList)[i]->sz);
				(*cubeList)[i]->id = sceneGraph.AddXnb(d3dDevice, "..\\media\\cube\\Sphere.xnb",
					(*cubeList)[i]->x, (*cubeList)[i]->y, (*cubeList)[i]->z, (*cubeList)[i]->sx, (*cubeList)[i]->sy, (*cubeList)[i]->sz);
			}
/*
			for(float x =0; x<15;x+=5)
			{
				for(float y=0; y<15; y+=5)
				{
					for(float z=0; z<15; z+=5)
					{
						D3DXMatrixTranslation(&translate,x,y,z);
						sceneGraph.Add(d3dDevice, L"..\\media\\cube\\cube.sdkmesh",x,y,z,1,1,1);
					}
				}
			}
*/
			LoadSkybox(d3dDevice, L"..\\media\\Skybox\\EmptySpace.dds");
			cameraEye = sceneScaling * D3DXVECTOR3(100.0f, 5.0f, 5.0f);
            cameraAt = sceneScaling * D3DXVECTOR3(0.0f, 0.0f, 0.0f);

		}break;
    };
#pragma endregion

   

    gViewerCamera.SetViewParams(&cameraEye, &cameraAt);
    gViewerCamera.SetScalers(0.01f, 10.0f);
    gViewerCamera.FrameMove(0.0f);
    
    // Zero out the elapsed time for the next frame
    gZeroNextFrameTime = true;
}


void DestroyScene()
{
	EnginePhysics::ShutdownPhysX();
	sceneGraph.Destroy();
	/*gMeshOpaque.Destroy();
	gMeshOpaque2.Destroy();
    gMeshAlpha.Destroy();*/
    SAFE_RELEASE(gSkyboxSRV);
}

#pragma region Callbacks
bool CALLBACK ModifyDeviceSettings(DXUTDeviceSettings* deviceSettings, void* userContext)
{
    // For the first device created if its a REF device, optionally display a warning dialog box
    static bool firstTime = true;
    if (firstTime) {
        firstTime = false;
        if (deviceSettings->d3d11.DriverType == D3D_DRIVER_TYPE_REFERENCE) {
            DXUTDisplaySwitchingToREFWarning(deviceSettings->ver);
        }
    }

    // We don't currently support framebuffer MSAA
    // Requires multi-frequency shading wrt. the GBuffer that is not yet implemented
    deviceSettings->d3d11.sd.SampleDesc.Count = 1;
    deviceSettings->d3d11.sd.SampleDesc.Quality = 0;

    // Also don't need a depth/stencil buffer... we'll manage that ourselves
    deviceSettings->d3d11.AutoCreateDepthStencil = false;

    return true;
}


void CALLBACK OnFrameMove(double time, float elapsedTime, void* userContext)
{
    if (gZeroNextFrameTime) {
        elapsedTime = 0.0f;
    }
    
    // Update the camera's position based on user input
    gViewerCamera.FrameMove(elapsedTime);

    // If requested, animate scene
    if (gApp && gAnimateLightCheck->GetChecked()) {
        gApp->Move(elapsedTime);
    }
}


LRESULT CALLBACK MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* noFurtherProcessing,
                          void* userContext)
{
    // Pass messages to dialog resource manager calls so GUI state is updated correctly
    *noFurtherProcessing = gDialogResourceManager.MsgProc(hWnd, uMsg, wParam, lParam );
    if (*noFurtherProcessing) {
        return 0;
    }

    // Pass messages to settings dialog if its active
    if (gD3DSettingsDlg.IsActive()) {
        gD3DSettingsDlg.MsgProc(hWnd, uMsg, wParam, lParam);
        return 0;
    }

    // Give the dialogs a chance to handle the message first
    for (int i = 0; i < HUD_NUM; ++i) {
        *noFurtherProcessing = gHUD[i].MsgProc(hWnd, uMsg, wParam, lParam);
        if(*noFurtherProcessing) {
            return 0;
        }
    }

    // Pass all remaining windows messages to camera so it can respond to user input
    gViewerCamera.HandleMessages(hWnd, uMsg, wParam, lParam);

    return 0;
}


void CALLBACK OnKeyboard(UINT character, bool keyDown, bool altDown, void* userContext)
{
    if(keyDown) {
        switch (character) {
        case VK_F8:
            // Toggle visibility of expert HUD
            gHUD[HUD_EXPERT].SetVisible(!gHUD[HUD_EXPERT].GetVisible());
            break;
        case VK_F9:
            // Toggle display of UI on/off
            gDisplayUI = !gDisplayUI;
            break;

		default:
			EnginePhysics::ProcessKey(character);
			break;
        }
    }
}


void CALLBACK OnGUIEvent(UINT eventID, INT controlID, CDXUTControl* control, void* userContext)
{
    switch (controlID) {
        case UI_TOGGLEFULLSCREEN:
            DXUTToggleFullScreen(); break;
        case UI_TOGGLEWARP:
            DXUTToggleWARP(); break;
        case UI_CHANGEDEVICE:
            gD3DSettingsDlg.SetActive(!gD3DSettingsDlg.IsActive()); break;
        case UI_LIGHTINGONLY:
            gUIConstants.lightingOnly = dynamic_cast<CDXUTCheckBox*>(control)->GetChecked(); break;
        case UI_FACENORMALS:
            gUIConstants.faceNormals = dynamic_cast<CDXUTCheckBox*>(control)->GetChecked(); break;        
        case UI_VISUALIZELIGHTCOUNT:
            gUIConstants.visualizeLightCount = dynamic_cast<CDXUTCheckBox*>(control)->GetChecked(); break;            
        case UI_VISUALIZEPERSAMPLESHADING:
            gUIConstants.visualizePerSampleShading = dynamic_cast<CDXUTCheckBox*>(control)->GetChecked(); break;            
        case UI_SELECTEDSCENE:
            DestroyScene(); break;
        case UI_LIGHTS:
            gApp->SetActiveLights(DXUTGetD3D11Device(), 1 << gLightsSlider->GetValue()); break;
        case UI_CULLTECHNIQUE:
            gUIConstants.lightCullTechnique = static_cast<unsigned int>(PtrToUlong(gCullTechniqueCombo->GetSelectedData())); break;

        // These controls all imply changing parameters to the App constructor
        // (i.e. recreating resources and such), so we'll just clean up the app here and let it be
        // lazily recreated next render.
        case UI_MSAA:
            DestroyApp(); break;

        default:
            break;
    }

    UpdateUIState();
}


void CALLBACK OnD3D11DestroyDevice(void* userContext)
{
    DestroyApp();
    DestroyScene();
    
    gDialogResourceManager.OnD3D11DestroyDevice();
    gD3DSettingsDlg.OnD3D11DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
    SAFE_DELETE(gTextHelper);
}


HRESULT CALLBACK OnD3D11CreateDevice(ID3D11Device* d3dDevice, const DXGI_SURFACE_DESC* backBufferSurfaceDesc,
                                     void* userContext)
{    
    ID3D11DeviceContext* d3dDeviceContext = DXUTGetD3D11DeviceContext();
    gDialogResourceManager.OnD3D11CreateDevice(d3dDevice, d3dDeviceContext);
    gD3DSettingsDlg.OnD3D11CreateDevice(d3dDevice);
    gTextHelper = new CDXUTTextHelper(d3dDevice, d3dDeviceContext, &gDialogResourceManager, 15);
    
    gViewerCamera.SetRotateButtons(true, false, false);
    gViewerCamera.SetDrag(true);
    gViewerCamera.SetEnableYAxisMovement(true);

    return S_OK;
}


HRESULT CALLBACK OnD3D11ResizedSwapChain(ID3D11Device* d3dDevice, IDXGISwapChain* swapChain,
                                          const DXGI_SURFACE_DESC* backBufferSurfaceDesc, void* userContext)
{
    HRESULT hr;

    V_RETURN(gDialogResourceManager.OnD3D11ResizedSwapChain(d3dDevice, backBufferSurfaceDesc));
    V_RETURN(gD3DSettingsDlg.OnD3D11ResizedSwapChain(d3dDevice, backBufferSurfaceDesc));

    gAspectRatio = backBufferSurfaceDesc->Width / (float)backBufferSurfaceDesc->Height;

    // NOTE: Complementary Z (1-z) buffer used here, so swap near/far!
    gViewerCamera.SetProjParams(D3DX_PI / 4.0f, gAspectRatio, 300.0f, 0.05f);

    // Standard HUDs
    const int border = 20;
    int y = border;
    for (int i = 0; i < HUD_EXPERT; ++i) {
        gHUD[i].SetLocation(backBufferSurfaceDesc->Width - gHUD[i].GetWidth() - border, y);
        y += gHUD[i].GetHeight() + border;
    }

    // Expert HUD
    gHUD[HUD_EXPERT].SetLocation(border, 80);

    // If there's no app, it'll pick this up when it gets lazily created so just ignore it
    if (gApp) {
        gApp->OnD3D11ResizedSwapChain(d3dDevice, backBufferSurfaceDesc);
    }

    return S_OK;
}


void CALLBACK OnD3D11ReleasingSwapChain(void* userContext)
{
    gDialogResourceManager.OnD3D11ReleasingSwapChain();
}


void CALLBACK OnD3D11FrameRender(ID3D11Device* d3dDevice, ID3D11DeviceContext* d3dDeviceContext, double time,
                                 float elapsedTime, void* userContext)
{
    if (gZeroNextFrameTime) {
        elapsedTime = 0.0f;
    }
    gZeroNextFrameTime = false;

    if (gD3DSettingsDlg.IsActive()) {
        gD3DSettingsDlg.OnRender(elapsedTime);
        return;
    }

    // Lazily create the application if need be
    if (!gApp) {
        InitApp(d3dDevice);
    }

    // Lazily load scene
	/*!gMeshOpaque.IsLoaded() && !gMeshAlpha.IsLoaded() &&!gMeshOpaque2.IsLoaded()*/
    if (!sceneGraph.IsLoaded()) {
        InitScene(d3dDevice);
    }

	EnginePhysics::StepPhysX();
	if(cubeList)
	{
		for(int i = 0; i < cubeList->size(); i++)
		{
			if( i == 100)
				int x = 0;
			sceneGraph.SetMeshPosition((*cubeList)[i]->id, (*cubeList)[i]->x, (*cubeList)[i]->y, (*cubeList)[i]->z);
		}
	}

    ID3D11RenderTargetView* pRTV = DXUTGetD3D11RenderTargetView();
	
    
    D3D11_VIEWPORT viewport;
    viewport.Width    = static_cast<float>(DXUTGetDXGIBackBufferSurfaceDesc()->Width);
    viewport.Height   = static_cast<float>(DXUTGetDXGIBackBufferSurfaceDesc()->Height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;

		 gApp->Render(d3dDeviceContext, pRTV, sceneGraph, gSkyboxSRV,
        gWorldMatrix, &gViewerCamera, &viewport, &gUIConstants);
	
    if (gDisplayUI) {
        d3dDeviceContext->RSSetViewports(1, &viewport);

        // Render HUDs in reverse order
        d3dDeviceContext->OMSetRenderTargets(1, &pRTV, 0);
        for (int i = HUD_NUM - 1; i >= 0; --i) {
            gHUD[i].OnRender(elapsedTime);
        }

        // Render text
        gTextHelper->Begin();

        gTextHelper->SetInsertionPos(2, 0);
        gTextHelper->SetForegroundColor(D3DXCOLOR(1.0f, 1.0f, 0.0f, 1.0f));
        gTextHelper->DrawTextLine(DXUTGetFrameStats(DXUTIsVsyncEnabled()));
        //gTextHelper->DrawTextLine(DXUTGetDeviceStats());

        // Output frame time
        {
            std::wostringstream oss;
            oss << 1000.0f / DXUTGetFPS() << " ms / frame";
            gTextHelper->DrawTextLine(oss.str().c_str());
        }

        // Output light info
        {
            std::wostringstream oss;
            oss << "Lights: " << gApp->GetActiveLights();
            gTextHelper->DrawTextLine(oss.str().c_str());
        }
        
        gTextHelper->End();
    }
}
#pragma endregion