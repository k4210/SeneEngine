#include "stdafx.h"
#include "engine/engine.h"

int run_application(BaseApp& pSample, HINSTANCE hInstance, int nCmdShow, UINT width, UINT height);

_Use_decl_annotations_
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	SceneEngine se;
	return run_application(se, hInstance, nCmdShow, 1280, 720);
}