// ModifyMsi.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "ModifyMsi.h"
#include <stdio.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define TYPE_STRING 's'
#define TYPE_INTEGER 'i'
#define TYPE_BINARY  'v'
/////////////////////////////////////////////////////////////////////////////


using namespace std;

void DisplayDatabaseError();

BOOL OpenTableAndViewWithCondition(MSIHANDLE hDatabase,char *szSelect,MSIHANDLE *phView,BOOL showError);
BOOL SetBannerTextWidthAndHeight(MSIHANDLE hDatabase, char *table, char *dialog, char *width,char *height,char *control); 
BOOL UpdateText(MSIHANDLE hDatabase, char *table, char *dialog, char *text, char *control);

#define TEMP_BUF_SIZE 256

/*============================================================================================================================================================*/

// $(ProjectDir)ModifyMsi.exe  $(ProjectDir)
int _tmain(int argc, TCHAR* argv[], TCHAR* envp[])
{
//	char *CurrentDir = argv[1];
//	char *szRelativePathName=argv[2]; // "Install\\";
	char *MSIFileName = argv[1]; // "RSODBC-win64-1.1.0.msi"
	char szPathName[TEMP_BUF_SIZE];

	MSIHANDLE hDatabase;
	UINT rc;

//	snprintf(szPathName, TEMP_BUF_SIZE, "%s%s\\%s",CurrentDir,szRelativePathName,MSIFileName);
	snprintf(szPathName, TEMP_BUF_SIZE,"%s",MSIFileName);
	rc = MsiOpenDatabase(szPathName,MSIDBOPEN_TRANSACT,&hDatabase);
	if (rc != ERROR_SUCCESS)
	{
		cout<<"Error in opening the MSI database:rc = "<<rc<<"\t"<<GetLastError()<<endl;

		return FALSE;
	}

	SetBannerTextWidthAndHeight(hDatabase,"Control", "DiskCostDlg", "200", "30", "Description"); 
	SetBannerTextWidthAndHeight(hDatabase,"Control", "VerifyReadyDlg", "200", "30","InstallTitle"); 
	SetBannerTextWidthAndHeight(hDatabase,"Control", "VerifyReadyDlg", "200", "30","RemoveTitle"); 
	SetBannerTextWidthAndHeight(hDatabase,"Control", "VerifyReadyDlg", "200", "30","ChangeTitle"); 
	SetBannerTextWidthAndHeight(hDatabase,"Control", "VerifyReadyDlg", "200", "30","RepairTitle"); 

	SetBannerTextWidthAndHeight(hDatabase,"Control", "WelcomeDlg", "200", "45","Title"); 
	SetBannerTextWidthAndHeight(hDatabase,"Control", "MaintenanceWelcomeDlg", "200", "30","Title"); 
	SetBannerTextWidthAndHeight(hDatabase,"Control", "ProgressDlg", "200", "30","TitleInstalling"); 
	SetBannerTextWidthAndHeight(hDatabase,"Control", "ProgressDlg", "200", "30","TitleRemoving"); 

	UpdateText(hDatabase,"Control", "BrowseDlg", "Browse to the destination folder.", "Description"); 

	// Commit and close database
	MsiDatabaseCommit(hDatabase);
	MsiCloseHandle(hDatabase);

	return 0;
}

/*============================================================================================================================================================*/

void DisplayDatabaseError()
{
// try to obtain extended error information.

		PMSIHANDLE hLastErrorRec = MsiGetLastErrorRecord();

		TCHAR* szExtendedError = NULL;
		DWORD cchExtendedError = 0;
		if (hLastErrorRec)
		{
			// Since we are not currently calling MsiFormatRecord during an
			// install session, hInstall is NULL. If MsiFormatRecord was called
			// via a DLL custom action, the hInstall handle provided to the DLL
			// custom action entry point could be used to further resolve 
			// properties that might be contained within the error record.
			
			// To determine the size of the buffer required for the text,
			// szResultBuf must be provided as an empty string with
			// *pcchResultBuf set to 0.

			UINT uiStatus = MsiFormatRecord(NULL,
							 hLastErrorRec,
							 TEXT(""),
							 &cchExtendedError);

			if (ERROR_MORE_DATA == uiStatus)
			{
				// returned size does not include null terminator.
				cchExtendedError++;

				szExtendedError = new TCHAR[cchExtendedError];
				if (szExtendedError)
				{
					uiStatus = MsiFormatRecord(NULL,
								     hLastErrorRec,
								     szExtendedError,
								     &cchExtendedError);
					if (ERROR_SUCCESS == uiStatus)
					{
						// We now have an extended error
						// message to report.

						// PLACE ADDITIONAL CODE HERE
						// TO LOG THE ERROR MESSAGE
						// IN szExtendedError.

						cout<<"Database error:"<<szExtendedError<<endl;
					}

					delete [] szExtendedError;
					szExtendedError = NULL;
				}
			}
		}
}

/*============================================================================================================================================================*/

BOOL OpenTableAndViewWithCondition(MSIHANDLE hDatabase,char *szSelect,MSIHANDLE *phView,BOOL showError)
{
	UINT rc;

	// build query for specific record in this table

	rc = MsiDatabaseOpenView(hDatabase,szSelect,phView);
	if (rc != ERROR_SUCCESS)
	{
		if(showError)
		{
			DisplayDatabaseError();
			cout<<"Error in Opening View:rc = "<<rc<<"\t"<<GetLastError()<<endl;
		}

		return FALSE;
	}

	// execute query - not a parameter query so second parameter is NULL.
	if ((*phView == NULL) || MsiViewExecute(*phView,NULL) != ERROR_SUCCESS)
	{
		if(showError)
		{
			cout<<"Error in executing the View"<<"\t"<<GetLastError()<<endl;
		}

		return FALSE;
	}

	return TRUE;
}

/*============================================================================================================================================================*/

BOOL SetBannerTextWidthAndHeight(MSIHANDLE hDatabase, char *table, char *dialog, char *width,char *height, char *control) 
{ 
	try
	{
		MSIHANDLE hView = NULL;
		char szSelect[TEMP_BUF_SIZE];
		MSIHANDLE hRec = NULL;
		UINT rc;

		snprintf(szSelect, TEMP_BUF_SIZE,"SELECT `Width`, `Height` FROM %s WHERE `Dialog_`='%s' and `Control` =  '%s'",
                        table,dialog, control);

		// Open and view table
		if(!OpenTableAndViewWithCondition(hDatabase,szSelect,&hView,FALSE))
		{
			// Close view
			if(hView)
			{
				MsiViewClose(hView);
				MsiCloseHandle(hView);

				hView = NULL;
			}

			return TRUE;
		}

		rc = MsiViewFetch(hView,&hRec);

		if(rc != ERROR_SUCCESS)
		{
			cout<<"Error in fetching record of the View"<<"\t"<<GetLastError()<<endl;

			// Close record
			if(hRec)
			{
				MsiCloseHandle(hRec);
				hRec = NULL;
			}

			// Close view
			if(hView)
			{
				MsiViewClose(hView);
				MsiCloseHandle(hView);

				hView = NULL;
			}

			return FALSE;
		}

        // Change width
		rc = MsiRecordSetString(hRec,1,width);
		if (rc != ERROR_SUCCESS)
		{
			DisplayDatabaseError();
			cout<<"Error in MsiRecordSetString[" << (2) <<"]:rc = "<<rc<<"\t"<<GetLastError()<<endl;

			// Close record
			if(hRec)
			{
				MsiCloseHandle(hRec);
				hRec = NULL;
			}

			// Close view
			if(hView)
			{
				MsiViewClose(hView);
				MsiCloseHandle(hView);

				hView = NULL;
			}

			return FALSE;
		}

        // Change height
		rc = MsiRecordSetString(hRec,2,height);
		if (rc != ERROR_SUCCESS)
		{
			DisplayDatabaseError();
			cout<<"Error in MsiRecordSetString[" << (2) <<"]:rc = "<<rc<<"\t"<<GetLastError()<<endl;

			// Close record
			if(hRec)
			{
				MsiCloseHandle(hRec);
				hRec = NULL;
			}

			// Close view
			if(hView)
			{
				MsiViewClose(hView);
				MsiCloseHandle(hView);

				hView = NULL;
			}

			return FALSE;
		}

		rc = MsiViewModify(hView,MSIMODIFY_REPLACE,hRec);
		if (rc != ERROR_SUCCESS)
		{
			DisplayDatabaseError();
			cout<<"Error in MsiViewModify:rc = "<<rc<<"\t"<<GetLastError()<<endl;

			// Close record
			if(hRec)
			{
				MsiCloseHandle(hRec);
				hRec = NULL;
			}

			// Close view
			if(hView)
			{
				MsiViewClose(hView);
				MsiCloseHandle(hView);

				hView = NULL;
			}

			return FALSE;
		}

		// Close record
		if(hRec)
		{
			MsiCloseHandle(hRec);
			hRec = NULL;
		}

		// Close view
		if(hView)
		{
			MsiViewClose(hView);
			MsiCloseHandle(hView);

			hView = NULL;
		}
	}
	catch(...)
	{
		return FALSE;
	}

	return TRUE;
} 

/*============================================================================================================================================================*/

BOOL UpdateText(MSIHANDLE hDatabase, char *table, char *dialog, char *text, char *control) 
{ 
	try
	{
		MSIHANDLE hView = NULL;
		char szSelect[TEMP_BUF_SIZE];
		MSIHANDLE hRec = NULL;
		UINT rc;

		snprintf(szSelect, TEMP_BUF_SIZE, "SELECT `Text` FROM %s WHERE `Dialog_`='%s' and `Control` =  '%s'",
                        table,dialog, control);

		// Open and view table
		if(!OpenTableAndViewWithCondition(hDatabase,szSelect,&hView,FALSE))
		{
			// Close view
			if(hView)
			{
				MsiViewClose(hView);
				MsiCloseHandle(hView);

				hView = NULL;
			}

			return TRUE;
		}

		rc = MsiViewFetch(hView,&hRec);

		if(rc != ERROR_SUCCESS)
		{
			cout<<"Error in fetching record of the View"<<"\t"<<GetLastError()<<endl;

			// Close record
			if(hRec)
			{
				MsiCloseHandle(hRec);
				hRec = NULL;
			}

			// Close view
			if(hView)
			{
				MsiViewClose(hView);
				MsiCloseHandle(hView);

				hView = NULL;
			}

			return FALSE;
		}

        // Change text
		rc = MsiRecordSetString(hRec,1,text);
		if (rc != ERROR_SUCCESS)
		{
			DisplayDatabaseError();
			cout<<"Error in MsiRecordSetString[" << (2) <<"]:rc = "<<rc<<"\t"<<GetLastError()<<endl;

			// Close record
			if(hRec)
			{
				MsiCloseHandle(hRec);
				hRec = NULL;
			}

			// Close view
			if(hView)
			{
				MsiViewClose(hView);
				MsiCloseHandle(hView);

				hView = NULL;
			}

			return FALSE;
		}


		rc = MsiViewModify(hView,MSIMODIFY_REPLACE,hRec);
		if (rc != ERROR_SUCCESS)
		{
			DisplayDatabaseError();
			cout<<"Error in MsiViewModify:rc = "<<rc<<"\t"<<GetLastError()<<endl;

			// Close record
			if(hRec)
			{
				MsiCloseHandle(hRec);
				hRec = NULL;
			}

			// Close view
			if(hView)
			{
				MsiViewClose(hView);
				MsiCloseHandle(hView);

				hView = NULL;
			}

			return FALSE;
		}

		// Close record
		if(hRec)
		{
			MsiCloseHandle(hRec);
			hRec = NULL;
		}

		// Close view
		if(hView)
		{
			MsiViewClose(hView);
			MsiCloseHandle(hView);

			hView = NULL;
		}
	}
	catch(...)
	{
		return FALSE;
	}

	return TRUE;
} 
