/*-------------------------------------------------------------------------
*
* Copyright(c) 2020, Amazon.com, Inc. or Its Affiliates. All rights reserved.
*
* Author: igarish
*-------------------------------------------------------------------------
*/

#ifdef WIN32

#include "win_port.h"
#include <shellapi.h>
#include "rsodbc.h"
#include "rsutil.h"
#include "rsunicode.h"
#include "resource.h"

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// SQLDriverConnect dialog windows procedure.
//
BOOL SQL_API driverConnectProcLoop(HWND    hdlg,
                                    WORD    wMsg,
                                    WORD    wParam,
                                    LPARAM  lParam)
{
    SQLRETURN   rc;
    HWND    hButton;
    RS_CONN_INFO *pConn;
    RS_CONNECT_PROPS_INFO *pConnectProps;


    switch (wMsg) 
    {
        case WM_INITDIALOG:
        {
            pConn = (RS_CONN_INFO *)lParam;
            pConnectProps = pConn->pConnectProps;

            SendDlgItemMessage(hdlg, IDC_EDIT_HOST, EM_LIMITTEXT, (WPARAM)(MAX_IDEN_LEN), 0L);
            SendDlgItemMessage(hdlg, IDC_EDIT_PORT, EM_LIMITTEXT, (WPARAM)(MAX_IDEN_LEN), 0L);
            SendDlgItemMessage(hdlg, IDC_EDIT_DBNAME, EM_LIMITTEXT, (WPARAM)(MAX_IDEN_LEN), 0L);
            SendDlgItemMessage(hdlg, IDC_EDIT_USER, EM_LIMITTEXT, (WPARAM)(MAX_IDEN_LEN), 0L);
            SendDlgItemMessage(hdlg, IDC_EDIT_PASSWORD, EM_LIMITTEXT, (WPARAM)(MAX_IDEN_LEN), 0L);

            SetDlgItemText(hdlg, IDC_EDIT_HOST, pConnectProps->szHost);
            SetDlgItemText(hdlg, IDC_EDIT_PORT, pConnectProps->szPort);
            SetDlgItemText(hdlg, IDC_EDIT_DBNAME, pConnectProps->szDatabase);
            SetDlgItemText(hdlg, IDC_EDIT_USER, pConnectProps->szUser);
            SetDlgItemText(hdlg, IDC_EDIT_PASSWORD, pConnectProps->szPassword);

            SetWindowLongPtr(hdlg, DWLP_USER, lParam);

            move_dialog_to_center(hdlg);

            return TRUE;
        }

        case WM_COMMAND:
        {
            switch (wParam) 
            {
                case IDOK:
                {
                    pConn = (RS_CONN_INFO *)GetWindowLongPtr(hdlg, DWLP_USER);
                    pConnectProps = pConn->pConnectProps;

                    GetDlgItemText(hdlg, IDC_EDIT_HOST, pConnectProps->szHost, MAX_IDEN_LEN);
                    GetDlgItemText(hdlg, IDC_EDIT_PORT, pConnectProps->szPort, MAX_IDEN_LEN);
                    GetDlgItemText(hdlg, IDC_EDIT_DBNAME, pConnectProps->szDatabase, MAX_IDEN_LEN);
                    GetDlgItemText(hdlg, IDC_EDIT_USER, pConnectProps->szUser, MAX_IDEN_LEN);
                    GetDlgItemText(hdlg, IDC_EDIT_PASSWORD, pConnectProps->szPassword, MAX_IDEN_LEN);

                    hButton = GetDlgItem( hdlg, IDOK);
                    EnableWindow( hButton, FALSE);

					rc = pConn->readAuthProfile(FALSE);
					if(rc != SQL_ERROR)
						rc = RS_CONN_INFO::doConnection(pConn);
                    
                    if(rc == SQL_ERROR) 
                        EndDialog(hdlg,DRV_CONNECT_DLG_ERROR); 
                    else if(rc)
                        EndDialog(hdlg, FALSE);
                    else                
                        EndDialog(hdlg, TRUE);

                    return TRUE;
                }

                case IDCANCEL:
                {
                    EndDialog(hdlg, FALSE);
                    return TRUE;
                }

                case IDHELP:
                {
                    openHelpFile("connect");
                    return TRUE;
                }

            }
        }
    }

    return FALSE;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Center the SQLDriverConnect dialog.
//
void move_dialog_to_center(HWND hdlg)
{
    HWND        hwndFrame;
    RECT        dlg_rect, screen_rect, frame_rect;
    int            cx, cy;

    hwndFrame = GetParent(hdlg);

    GetWindowRect(hdlg, &dlg_rect);
    cx = dlg_rect.right - dlg_rect.left;
    cy = dlg_rect.bottom - dlg_rect.top;

    GetClientRect(hwndFrame, &frame_rect);
    ClientToScreen(hwndFrame, (LPPOINT) (&frame_rect.left));
    ClientToScreen(hwndFrame, (LPPOINT) (&frame_rect.right));
    dlg_rect.top = frame_rect.top + (((frame_rect.bottom - frame_rect.top) - cy) >> 1);
    dlg_rect.left = frame_rect.left + (((frame_rect.right - frame_rect.left) - cx) >> 1);
    dlg_rect.bottom = dlg_rect.top + cy;
    dlg_rect.right = dlg_rect.left + cx;

    GetWindowRect(GetDesktopWindow(), &screen_rect);
    if (dlg_rect.bottom > screen_rect.bottom)
    {
        dlg_rect.bottom = screen_rect.bottom;
        dlg_rect.top = dlg_rect.bottom - cy;
    }
    if (dlg_rect.right > screen_rect.right)
    {
        dlg_rect.right = screen_rect.right;
        dlg_rect.left = dlg_rect.right - cx;
    }

    if (dlg_rect.left < 0)
        dlg_rect.left = 0;
    if (dlg_rect.top < 0)
        dlg_rect.top = 0;

    MoveWindow(hdlg, dlg_rect.left, dlg_rect.top, cx, cy, TRUE);
    return;
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Open the help file when user click on Help button SQLDriverConnect dialog.
//
void openHelpFile(char *anchor)
{
    int len;
    HKEY helpKey;
    char helpfile_url[1024];
    char url[1024];
    char associated_exe[1024];

    /*
     *    lookup helpfiles location from registry
     */
    helpfile_url[0] = '\0';
    if (ERROR_SUCCESS == RegOpenKeyEx( HKEY_LOCAL_MACHINE, "SOFTWARE\\ODBC\\ODBCINST.INI\\Amazon Redshift ODBC Driver (x64)", 0, KEY_ALL_ACCESS, &helpKey ))
    {
        char helpdir[1024] = "";
        len = 1023;
        if (ERROR_SUCCESS == RegQueryValueEx( helpKey, "HelpRootDirectory", 0, NULL, (LPBYTE)helpdir, (LPDWORD) &len )) 
        {
            helpdir[len] = 0;
            for (len--; len >= 0; len--)
                if (helpdir[len] == '\\')
                    helpdir[len] = '/';
            _snprintf(helpfile_url, sizeof(helpfile_url), "file:///%shelp.htm", helpdir);
        }
        RegCloseKey(helpKey);
    }

    if(helpfile_url[0] != '\0')
    {
        //
        //    yes, we really have to enclose it in dbl quotes, else the anchor gets dropped/ignored
        //
        _snprintf(url, 1024, "\"%s%c%s\"", helpfile_url, '#', anchor);

        // Get the full path of help file without file:/// and then find the associated executable
        associated_exe[0] = '\0';
        FindExecutable(&helpfile_url[strlen("file:///")],NULL,associated_exe);

        if(associated_exe[0] != '\0')
        {
            // If we find associated exe then give url as an argument to that exe
            ShellExecute(NULL, "open", associated_exe, url, NULL, SW_SHOWNORMAL);
        }
        else
        {
            // Remove the quotes which could cause hang on IE7 or IE8.
            // Note that this couldn't navigate to the anchor but it will open the help file.
            _snprintf(url, 1024, "%s%c%s", helpfile_url, '#', anchor);
            ShellExecute(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
        }
    }
}

/*====================================================================================================================================================*/

//---------------------------------------------------------------------------------------------------------igarish
// Read registry key.
//
void readRegistryKey(HKEY hKey, char *pSubKeyName, char *pName, char *pBuf, int iBufLen)
{
    if(pBuf)
    {
        HKEY hSubKey;
        int iLen = iBufLen - 1;

        pBuf[0] = '\0';

        if (RegOpenKeyEx(hKey, pSubKeyName, 0, KEY_READ, &hSubKey ) == ERROR_SUCCESS) 
        {
            if(RegQueryValueEx(hSubKey, pName, NULL, NULL, (LPBYTE)pBuf, (LPDWORD) &iLen) == ERROR_SUCCESS) 
            {
                pBuf[iLen] = '\0';
            }

            RegCloseKey(hSubKey);
        }
    }
}

#endif // WIN32
