#if !defined(AFX_MAPFRAME_H__8A4682BA_0A30_4BE2_BDBE_16ED918E0D46__INCLUDED_)
#define AFX_MAPFRAME_H__8A4682BA_0A30_4BE2_BDBE_16ED918E0D46__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// MapFrame.h : header file
//

#include "MapView.h"
#include "netxms_maps.h"
#include "MapToolbox.h"	// Added by ClassView

#define OBJECT_HISTORY_SIZE      512


/////////////////////////////////////////////////////////////////////////////
// CMapFrame frame

class CMapFrame : public CMDIChildWnd
{
	DECLARE_DYNCREATE(CMapFrame)
protected:
	CMapFrame();           // protected constructor used by dynamic creation

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMapFrame)
	protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	//}}AFX_VIRTUAL

// Implementation
protected:
	CMapToolbox m_wndToolBox;
	CToolBar m_wndToolBar;
	CMapView m_wndMapView;
	virtual ~CMapFrame();

	// Generated message map functions
	//{{AFX_MSG(CMapFrame)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnSetFocus(CWnd* pOldWnd);
	afx_msg void OnViewRefresh();
	afx_msg void OnMapZoomin();
	afx_msg void OnUpdateMapZoomin(CCmdUI* pCmdUI);
	afx_msg void OnMapZoomout();
	afx_msg void OnUpdateMapZoomout(CCmdUI* pCmdUI);
	afx_msg void OnObjectOpenparent();
	afx_msg void OnUpdateObjectOpenparent(CCmdUI* pCmdUI);
	afx_msg void OnMapBack();
	afx_msg void OnUpdateMapBack(CCmdUI* pCmdUI);
	afx_msg void OnMapForward();
	afx_msg void OnUpdateMapForward(CCmdUI* pCmdUI);
	afx_msg void OnMapHome();
	//}}AFX_MSG
   afx_msg void OnObjectChange(WPARAM wParam, NXC_OBJECT *pObject);
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MAPFRAME_H__8A4682BA_0A30_4BE2_BDBE_16ED918E0D46__INCLUDED_)
