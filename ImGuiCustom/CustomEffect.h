#pragma once
#if IMGUI
#include "ImGuiObj.h"
#include <vector>
#include <string>

class CustomEffect
{
public:
	static int fadeFrames;
	static float FadeOutPush(int count);
	static void CustomEffect::ApplyWindowAlpha(ImGuiViewport* vp, float fadeProgress);
	static void FadeOutPop();
};


#endif
