// TestDialog.cpp : 定义 DLL 的导出函数。
//

#include "pch.h"
//#include "framework.h"
#include "TestDialog.h"
#include "MyDlg1.h"

// 这是导出变量的一个示例
TESTDIALOG_API int nTestDialog=0;

// 这是导出函数的一个示例。
TESTDIALOG_API int fnTestDialog(void)
{
    return 0;
}

// 这是已导出类的构造函数。
CTestDialog::CTestDialog()
{
    return;
}
void CTestDialog::UI(void* p0)
{
    m_pDlg->UI(p0);
}
HWND CTestDialog::Do()
{
    AFX_MANAGE_STATE(AfxGetStaticModuleState()); // 函数第一行
    m_pDlg = new MyDlg1();
    m_pDlg->Create(IDD_DIALOG1);
    m_pDlg->ShowWindow(SW_SHOW);
    HWND hDlg = m_pDlg->m_hWnd;

    LONG_PTR style = GetWindowLongPtr(hDlg, GWL_STYLE);
    style &= ~WS_POPUP;
    style |= WS_CHILD;
    SetWindowLongPtr(hDlg, GWL_STYLE, style);

    return hDlg;
}
