#include "stdafx.h"
#include "engine.h"
#include "../renderer/render_thread.h"

SceneEngine::SceneEngine() : BaseApp() {}
SceneEngine::~SceneEngine() {}

void SceneEngine::OnInit(HWND hWnd, UINT width, UINT height)
{ 
	renderer.Init(hWnd, width, height);
	renderer.Start();
}

void SceneEngine::OnDestroy() 
{ 
	renderer.Stop();
	renderer.Destroy();
}
