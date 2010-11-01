////////////////////////////////////////////////////////////////////////////
// Name:        wx/palmos/spinctrl.h
// Purpose:     wxSpinCtrl class declaration for Palm OS
// Author:      William Osborne - minimal working wxPalmOS port
// Modified by:
// Created:     10/13/04
// RCS-ID:      $Id$
// Copyright:   (c) William Osborne
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_PALMOS_SPINCTRL_H_
#define _WX_PALMOS_SPINCTRL_H_

#include "wx/spinbutt.h"    // the base class

#include "wx/dynarray.h"

class WXDLLIMPEXP_FWD_CORE wxSpinCtrl;
WX_DEFINE_EXPORTED_ARRAY_PTR(wxSpinCtrl *, wxArraySpins);

// ----------------------------------------------------------------------------
// Under Win32, wxSpinCtrl is a wxSpinButton with a buddy (as MSDN docs call
// it) text window whose contents is automatically updated when the spin
// control is clicked.
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_CORE wxSpinCtrl : public wxSpinButton
{
public:
    wxSpinCtrl() { }

    wxSpinCtrl(wxWindow *parent,
               wxWindowID id = -1,
               const wxString& value = wxEmptyString,
               const wxPoint& pos = wxDefaultPosition,
               const wxSize& size = wxDefaultSize,
               long style = wxSP_ARROW_KEYS,
               int min = 0, int max = 100, int initial = 0,
               const wxString& name = wxT("wxSpinCtrl"))
    {
        Create(parent, id, value, pos, size, style, min, max, initial, name);
    }

    bool Create(wxWindow *parent,
                wxWindowID id = -1,
                const wxString& value = wxEmptyString,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize,
                long style = wxSP_ARROW_KEYS,
                int min = 0, int max = 100, int initial = 0,
                const wxString& name = wxT("wxSpinCtrl"));

    // a wxTextCtrl-like method (but we can't have GetValue returning wxString
    // because the base class already has one returning int!)
    void SetValue(const wxString& text);

    // another wxTextCtrl-like method
    void SetSelection(long from, long to);

    // implementation only from now on
    // -------------------------------

    virtual ~wxSpinCtrl();

    virtual void SetValue(int val) { wxSpinButton::SetValue(val); }
    virtual int  GetValue() const;
    virtual bool SetFont(const wxFont &font);
    virtual void SetFocus();

    virtual bool Enable(bool enable = TRUE);
    virtual bool Show(bool show = TRUE);

    // wxSpinButton doesn't accept focus, but we do
    virtual bool AcceptsFocus() const { return wxWindow::AcceptsFocus(); }

    // for internal use only

    // get the subclassed window proc of the buddy text
    WXFARPROC GetBuddyWndProc() const { return m_wndProcBuddy; }

    // return the spinctrl object whose buddy is the given window or NULL
    static wxSpinCtrl *GetSpinForTextCtrl(WXHWND hwndBuddy);

    // process a WM_COMMAND generated by the buddy text control
    bool ProcessTextCommand(WXWORD cmd, WXWORD id);

protected:
    virtual void DoGetPosition(int *x, int *y) const;
    virtual void DoMoveWindow(int x, int y, int width, int height);
    virtual wxSize DoGetBestSize() const;
    virtual void DoGetSize(int *width, int *height) const;

    // the handler for wxSpinButton events
    void OnSpinChange(wxSpinEvent& event);

    // Handle processing of special keys
    void OnChar(wxKeyEvent& event);
    void OnSetFocus(wxFocusEvent& event);

    // the data for the "buddy" text ctrl
    WXHWND     m_hwndBuddy;
    WXFARPROC  m_wndProcBuddy;

    // all existing wxSpinCtrls - this allows to find the one corresponding to
    // the given buddy window in GetSpinForTextCtrl()
    static wxArraySpins ms_allSpins;

private:
    DECLARE_DYNAMIC_CLASS(wxSpinCtrl)
    DECLARE_EVENT_TABLE()
    wxDECLARE_NO_COPY_CLASS(wxSpinCtrl);
};

#endif // _WX_PALMOS_SPINCTRL_H_


