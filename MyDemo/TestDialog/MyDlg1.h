#pragma once

#include "resource.h"

// MyDlg1 对话框

class MyDlg1 : public CDialogEx
{
	DECLARE_DYNAMIC(MyDlg1)

public:
	MyDlg1(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~MyDlg1();
    void UI(void * p0);
// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG1 };
#endif
    bool show_demo_window = false;
    bool show_another_window = true;
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
    afx_msg void OnBnClickedButton1();
};
