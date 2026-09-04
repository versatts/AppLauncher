// MyDlg1.cpp: 实现文件
//

#include "pch.h"
#include "TestDialog.h"
#include "MyDlg1.h"
#include "afxdialogex.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <d3d11.h>
// MyDlg1 对话框

IMPLEMENT_DYNAMIC(MyDlg1, CDialogEx)

MyDlg1::MyDlg1(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG1, pParent)
{

}

MyDlg1::~MyDlg1()
{
}

void MyDlg1::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(MyDlg1, CDialogEx)
    ON_BN_CLICKED(IDC_BUTTON1, &MyDlg1::OnBnClickedButton1)
END_MESSAGE_MAP()


// MyDlg1 消息处理程序


void MyDlg1::OnBnClickedButton1()
{
    AfxMessageBox(_T("123"));
}


ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
void MyDlg1::UI(void * p0)
{
#if 1
    ImGui::SetCurrentContext((ImGuiContext*)p0);
    {
        // DLL内部绘制代码，已经SetCurrentContext之后
        ImGui::SetNextWindowPos(ImVec2(1920, 1080 - 300), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_Always);

        // 窗口标志
        ImGuiWindowFlags win_flags = 0;
        win_flags |= ImGuiWindowFlags_NoMove;         // ❌禁止拖动移动
        win_flags |= ImGuiWindowFlags_NoResize;       // ❌禁止缩放大小
        win_flags |= ImGuiWindowFlags_NoCollapse;     // ❌禁止折叠（去掉右上角最小化按钮）
       // win_flags |= ImGuiWindowFlags_NoTitleBar;    // 可选：要不要标题栏；如果要保留标题栏就不要这个flag
        win_flags |= ImGuiWindowFlags_NoDocking;
        static float f = 0.0f;
        static int counter = 0;

        ImGui::Begin("Hello, world 2!", nullptr, win_flags);                          // Create a window called "Hello, world!" and append into it.

        ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
        ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
        ImGui::Checkbox("Another Window", &show_another_window);

        ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
        ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

        if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
        {
            counter++;

            EnableWindow(counter % 2 == 0);

            if (counter >= 10)
            {
                ::SendMessage(GetSafeHwnd(), WM_CLOSE, 0, 0);
            }
        }
        ImGui::SameLine();
        ImGui::Text("counter = %d", counter);

        ImGui::End();
    }
#endif
}
