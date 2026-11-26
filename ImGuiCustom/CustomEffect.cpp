#include "CustomEffect.h"
#if IMGUI
#include <algorithm>
int CustomEffect::fadeFrames = 15;
float CustomEffect::FadeOutPush(int count)
{
	float alpha = 1.0f;
	float fadeProgress = 0.0f;

	if (count < fadeFrames)
	{
		alpha = (std::max)(0.0f, static_cast<float>(count) / static_cast<float>(fadeFrames));
		float linearT = 1.0f - alpha;
		fadeProgress = linearT * linearT;
	}

	ImGui::PushStyleVar(ImGuiStyleVar_DisabledAlpha, 1.0f);
	ImGui::BeginDisabled(true);//就是要马上禁用

	return fadeProgress;
}

void CustomEffect::ApplyWindowAlpha(ImGuiViewport* vp, float fadeProgress)
{
	// 安全防護：絕對不對主視口執行透明與穿透！
	if (!vp || vp == ImGui::GetMainViewport() || !vp->PlatformHandleRaw)
		return;

	HWND hwnd = reinterpret_cast<HWND>(vp->PlatformHandleRaw);
	if (!::IsWindow(hwnd))
		return;

	float currentAlpha = 1.0f - fadeProgress;

	LONG exStyle = ::GetWindowLong(hwnd, GWL_EXSTYLE);
	LONG targetExStyle = exStyle | WS_EX_LAYERED;

	if (fadeProgress > 0.0f)
		targetExStyle |= WS_EX_TRANSPARENT;
	else
		targetExStyle &= ~WS_EX_TRANSPARENT;

	if (exStyle != targetExStyle)
	{
		::SetWindowLong(hwnd, GWL_EXSTYLE, targetExStyle);
	}

	BYTE winAlpha = static_cast<BYTE>((std::max)(0.0f, (std::min)(255.0f, 255.0f * currentAlpha)));
	::SetLayeredWindowAttributes(hwnd, 0, winAlpha, LWA_ALPHA);
}

void CustomEffect::FadeOutPop()
{
	ImGui::EndDisabled();
	ImGui::PopStyleVar();
}

#endif
