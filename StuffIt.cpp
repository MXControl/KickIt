/*
 * BendMX - Chat extension library for WinMX.
 * Copyright (C) 2003-2004 by Thees Ch. Winkler
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Contact: tcwinkler@users.sourceforge.net
 *
*/

#include "stdafx.h"
#include "..\MXPlugin.h"
#include "ini.h"
#include "Tokenizer.h"
#include <winsock2.h>
#pragma comment (lib, "ws2_32.lib")

BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{
    return TRUE;
}



typedef struct TAG_ROOMDATA {

	DWORD dwRoomID;
	CString strRoomName;
	CString strIP;
	CString strAdmin;
	CString strPassword;

} ROOMDATA, *PROOMDATA;


void Init(void);
void Quit(void);
void OnEnterChannel(DWORD dwID, LPCTSTR lpszRoom);
void OnEnterUser(DWORD dwID, LPCTSTR lpszUsername);
void OnMessage(DWORD dwID, CString* pName, CString* pMessage, BOOL bIsAction);
void OnLeaveUser(DWORD dwID, LPCTSTR lpszUsername);
void OnCloseChannel(DWORD dwID);
void OnInputHook(DWORD dwIP, CString *pInput);
BOOL InputMessage(DWORD dwID, LPCTSTR lpszMessage);
BOOL WriteEchoText(DWORD dwID, LPCTSTR lpszText);
void OnMenuCommand(UINT nCmdID, DWORD dwChannelID, LPCTSTR lpszName);
void OnPrepareMenu (DWORD dwMenuID);

void RemoveRoomData(DWORD dwID);
PROOMDATA AddRoomData(DWORD dwID, LPCTSTR lpszRoomName);
PROOMDATA GetRoomData(DWORD dwID);

BPLUGIN g_Plugin = {

	VERSION,  // API Version
	0x1111,   // Plugin ID
	"KickIt Plugin",
	"Thees Ch. Winkler",
	NULL,     // Dll handle, filled in by BendMX
	NULL,
	NULL,
	NULL,
	Init,
	Quit,
	OnEnterChannel,
	OnEnterUser,
	OnMessage,
	OnLeaveUser,
	OnCloseChannel,
	OnInputHook,
	OnPrepareMenu,
	OnMenuCommand,
	NULL, // filled in by BendMX
	NULL  // filled in by bendMX
};

CIni		g_ini;
CMenu       *g_pMenu;
WSADATA     g_wsaData;
SOCKET		g_sConn;
BOOL        g_wsInit = FALSE;
CArray<ROOMDATA, ROOMDATA> g_aRoomList;
CHARFORMAT g_cfColor;

int ExecuteCommand(CString strIP, CString strCmd, DWORD dwChannelID)
{

	if(!g_wsInit) return -1;

#ifdef _DEBUG 
	strIP = "192.168.0.105";
#endif

	
	g_sConn = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if(g_sConn==INVALID_SOCKET){

		g_Plugin.WriteEchoText(dwChannelID, "KickIt Plugin: Couldnt create socket.\n", g_cfColor);
		return -1;
	}
	struct hostent *hp;
	

	unsigned long addr;

	addr = inet_addr(strIP);
	hp   = gethostbyaddr((char*)&addr,sizeof(addr),AF_INET);

	if(hp == NULL)
	{

		closesocket(g_sConn);
		g_Plugin.WriteEchoText(dwChannelID, "KickIt: Error resolving host " + strIP + "\n", g_cfColor);
		return -3;
	}

	struct sockaddr_in server;

	server.sin_addr.s_addr=*((unsigned long*)hp->h_addr);
	server.sin_family=AF_INET;
	server.sin_port=htons(25069);
	if(connect(g_sConn,(struct sockaddr*)&server,sizeof(server)))
	{

		closesocket(g_sConn);
		g_Plugin.WriteEchoText(dwChannelID, "KickIt: Error connecting to host " + strIP + "\n", g_cfColor);
		return -1;	
	}
	

	
	send(g_sConn, strCmd, strCmd.GetLength(), 0);	
	closesocket(g_sConn);
	g_Plugin.WriteEchoText(dwChannelID, "KickIt: Send " + strCmd + " to " + strIP + "\n", g_cfColor);
	

	return 0;
}


////////////////////////////////////////////////////////////////////////
// void Init(...) - will be called after loading the plugin and after
// filling in the hMainWindow and hDllInstance datamembers
//
// Parameters:
//  none
////////////////////////////////////////////////////////////////////////
void Init(void)
{

	TCHAR szBuffer[_MAX_PATH]; 
	::GetModuleFileName(AfxGetInstanceHandle(), szBuffer, _MAX_PATH);
	CString strWd;
	strWd.Format("%s", szBuffer);

	strWd = strWd.Left(strWd.ReverseFind('\\')) + "\\Plugins";

	g_ini.SetIniFileName(strWd + "\\KickIt.ini");

	int wsaret = WSAStartup(0x101, &g_wsaData);
	
	if(wsaret){

		AfxMessageBox("KickIt Plugin: Winsock Initialisation failed. Kickit functionality will not be available.");
		return;
	}

	g_wsInit = TRUE;

	g_cfColor.cbSize = sizeof(g_cfColor);
	g_cfColor.dwMask = CFM_COLOR|CFM_PROTECTED;
	g_cfColor.dwEffects = CFE_PROTECTED;
	g_cfColor.crTextColor = RGB(200, 0, 128);
}

////////////////////////////////////////////////////////////////////////
// void Quit(void) - will be called before unloading the plugin 
//
// Parameters:
//  none
////////////////////////////////////////////////////////////////////////
void Quit(void)
{

	if(g_pMenu != NULL){

		g_pMenu->DestroyMenu();
		delete g_pMenu;
		g_pMenu = 0;
	}
	if(g_wsInit){

		closesocket(g_sConn);
		WSACleanup();
	}
}

////////////////////////////////////////////////////////////////////////
// void OnEnterChannel() - will be called when you enter a channel
//
// Parameters:
//  DWORD dwID - identnumber of the channel
//  LPCTSTR lpszRoom - pointer to a string containing the room name
////////////////////////////////////////////////////////////////////////
void OnEnterChannel(DWORD dwID, LPCTSTR lpszRoom)
{

	PROOMDATA pRoom    = AddRoomData(dwID, lpszRoom);
	pRoom->strAdmin    = g_ini.GetValue(pRoom->strRoomName, "User", "");
	pRoom->strPassword = g_ini.GetValue(pRoom->strRoomName, "Password", "");

}


////////////////////////////////////////////////////////////////////////
// void OnEnterUser() - will be called when a user enters a channel 
//
// Parameters:
//  DWORD dwID - identnumber of the channel
//  LPCTSTR lpszusernam - pointer to a string containing the username
////////////////////////////////////////////////////////////////////////
void OnEnterUser(DWORD dwID, LPCTSTR lpszUsername)
{

}

////////////////////////////////////////////////////////////////////////
// void OnEnterUser() - will be called when a user enters a channel 
//
// Parameters:
//  DWORD dwID - identnumber of the channel
//  LPCTSTR lpszusernam - pointer to a string containing the username
////////////////////////////////////////////////////////////////////////
void OnMessage(DWORD dwID, CString* pName, CString* pMessage, BOOL bIsAction)
{

}


////////////////////////////////////////////////////////////////////////
// void OnLeaveUser() - will be called when a user leaves a channel 
//
// Parameters:
//  DWORD dwID - identnumber of the channel
//  LPCTSTR lpszusernam - pointer to a string containing the username
////////////////////////////////////////////////////////////////////////
void OnLeaveUser(DWORD dwID, LPCTSTR lpszUsername)
{

}


////////////////////////////////////////////////////////////////////////
// void OnCloseChannel() - will be called after a channel is closed
//
// Parameters:
//  DWORD dwID - identnumber of the channel that is closed
////////////////////////////////////////////////////////////////////////
void OnCloseChannel(DWORD dwID)
{

}

void OnMenuCommand(UINT nCmdID, DWORD dwChannelID, LPCTSTR lpszName)
{

   if((nCmdID < 13000) && (nCmdID > 13006)) return;
   CString strCmd;
   PROOMDATA pRoom = GetRoomData(dwChannelID);

   if(pRoom->strAdmin.IsEmpty() || pRoom->strPassword.IsEmpty()){

		g_Plugin.WriteEchoText(dwChannelID, "Kickit Error: No username or password setup for room " + pRoom->strRoomName + ". Set one up with /kickit <username> <password> first!\n" , g_cfColor);
		return;
   }

   switch(nCmdID){

   case 13000: // Kick
	   strCmd.Format("%s-%s-%s-%s%c%c", pRoom->strAdmin, pRoom->strPassword, "kick", lpszName, 0x0D, 0x0A);
	   ExecuteCommand(pRoom->strIP, strCmd, dwChannelID);
	   break;
   case 13001: // ban
	   strCmd.Format("%s-%s-%s-%s%c%c", pRoom->strAdmin, pRoom->strPassword, "ban", lpszName, 0x0D, 0x0A);
	   ExecuteCommand(pRoom->strIP, strCmd, dwChannelID);
	   break;
   case 13002: // Kick/ban
	   strCmd.Format("%s-%s-%s-%s%c%c", pRoom->strAdmin, pRoom->strPassword, "ban", lpszName, 0x0D, 0x0A);
	   ExecuteCommand(pRoom->strIP, strCmd, dwChannelID);
	   strCmd.Format("%s-%s-%s-%s%c%c", pRoom->strAdmin, pRoom->strPassword, "kick", lpszName, 0x0D, 0x0A);
	   ExecuteCommand(pRoom->strIP, strCmd, dwChannelID);
	   break;
   case 13003: // unban
	   strCmd.Format("%s-%s-%s-%s%c%c", pRoom->strAdmin, pRoom->strPassword, "unban", lpszName, 0x0D, 0x0A);
	   ExecuteCommand(pRoom->strIP, strCmd, dwChannelID);
	   break;
/*   case 13004: // Clearban
	   strCmd.Format("%s-%s-%s-%s%c%c", pRoom->strAdmin, pRoom->strPassword, "clearban", lpszName, 0x0D, 0x0A);
	   ExecuteCommand(pRoom->strIP, strCmd, dwChannelID);
	   break;
   case 13005: // Topic
	   strCmd.Format("%s-%s-%s-%s%c%c", pRoom->strAdmin, pRoom->strPassword, "topic", lpszName, 0x0D, 0x0A);
	   ExecuteCommand(pRoom->strIP, strCmd);
	   break;
   case 13006: // Motd
	   strCmd.Format("%s-%s-%s-%s%c%c", pRoom->strAdmin, pRoom->strPassword, "motd", lpszName, 0x0D, 0x0A);
	   ExecuteCommand(pRoom->strIP, strCmd);
	   break;
   case 13007: // Limit
	   strCmd.Format("%s-%s-%s-%s%c%c", pRoom->strAdmin, pRoom->strPassword, "limit", "50", 0x0D, 0x0A);
	   ExecuteCommand(pRoom->strIP, strCmd);
	   break;*/
   }
}

void OnPrepareMenu (DWORD dwMenuID)
{

	// UserList
	if(dwMenuID == 0)
	{
	
		if(g_pMenu != NULL){

			g_pMenu->DestroyMenu();
			delete g_pMenu;
			g_pMenu = 0;
		}
		g_pMenu = new CMenu;
		BOOL bReturn = TRUE;
		if(g_pMenu->CreatePopupMenu()){

			g_pMenu->AppendMenu(MF_STRING, 13000, "Kick");
			g_pMenu->AppendMenu(MF_STRING, 13001, "Ban");
			g_pMenu->AppendMenu(MF_STRING, 13002, "Kick/Ban");
			g_pMenu->AppendMenu(MF_SEPARATOR, 0, "");
			g_pMenu->AppendMenu(MF_STRING, 13003, "Unban");
			//g_pMenu->AppendMenu(MF_SEPARATOR, 0, "");
			//g_pMenu->AppendMenu(MF_STRING, 13005, "Topic");
			//g_pMenu->AppendMenu(MF_STRING, 13006, "Motd");
			//g_pMenu->AppendMenu(MF_STRING, 13007, "Limit");
			g_Plugin.hUserMenu = g_pMenu->m_hMenu;
		}
	}
}

////////////////////////////////////////////////////////////////////////
// void OnInputHook() - will be called when a user enters a message to
//                      the chat edit control
//
// Parameters:
//  DWORD dwIP  - identnumber of the edit control
//  CString *pInput - Pointer to a string containing the input
////////////////////////////////////////////////////////////////////////
void OnInputHook(DWORD dwIP, CString *pInput)
{
 
	if(pInput->Find("/kickit ", 0) == 0){

		pInput->Replace("/kickit ", "");

		CTokenizer token(*pInput, " ");
		CString strTmp;
		
		PROOMDATA pRoom = GetRoomData(dwIP);

		if(token.Next(strTmp)){

			g_ini.SetValue(pRoom->strRoomName, "User", strTmp);
			pRoom->strAdmin = strTmp;
		}
		else{

			g_Plugin.WriteEchoText(dwIP, "Kickit Error: Syntax is /kickit <username> <password>\n", g_cfColor);
			*pInput = "";
			return;
		}
		if(token.Next(strTmp)){

			g_ini.SetValue(pRoom->strRoomName, "Password", strTmp);
			pRoom->strPassword = strTmp;
		}
		else{

			g_Plugin.WriteEchoText(dwIP, "Kickit Error: Syntax is /kickit <username> <password>\n", g_cfColor);
			*pInput = "";
			return;
		}
		*pInput = "";
	}
	else if(pInput->Find("/topic ", 0) == 0){

		CString strCmd;
		PROOMDATA pRoom = GetRoomData(dwIP);
	    pInput->Replace("/topic ", "");
		strCmd.Format("%s-%s-%s-%s%c%c", pRoom->strAdmin, pRoom->strPassword, "topic", *pInput, 0x0D, 0x0A);
	    ExecuteCommand(pRoom->strIP, strCmd, dwIP);
		*pInput = "";
	}
	else if(pInput->Find("/motd ", 0) == 0){

		CString strCmd;
		PROOMDATA pRoom = GetRoomData(dwIP);
	    pInput->Replace("/motd ", "");
		strCmd.Format("%s-%s-%s-%s%c%c", pRoom->strAdmin, pRoom->strPassword, "motd", *pInput, 0x0D, 0x0A);
	    ExecuteCommand(pRoom->strIP, strCmd, dwIP);
		*pInput = "";
	}
	else if(pInput->Find("/limit ", 0) == 0){

		CString strCmd;
		PROOMDATA pRoom = GetRoomData(dwIP);
	    pInput->Replace("/limit ", "");
		strCmd.Format("%s-%s-%s-%s%c%c", pRoom->strAdmin, pRoom->strPassword, "limit", *pInput, 0x0D, 0x0A);
	    ExecuteCommand(pRoom->strIP, strCmd, dwIP);
		*pInput = "";
	}
	else if(pInput->Find("/clearban", 0) == 0){

		CString strCmd;
		PROOMDATA pRoom = GetRoomData(dwIP);
		strCmd.Format("%s-%s-%s-%s%c%c", pRoom->strAdmin, pRoom->strPassword, "clrban", "", 0x0D, 0x0A);
	    ExecuteCommand(pRoom->strIP, strCmd, dwIP);
		*pInput = "";
	}
}

// convert hex string to int
int axtoi(char *hexStg)
{

  int n = 0;         // position in string
  int m = 0;         // position in digit[] to shift
  int count;         // loop index
  int intValue = 0;  // integer value of hex string
  int digit[5];      // hold values to convert
  while (n < 2) {
     if (hexStg[n]=='\0')
        break;
     if (hexStg[n] > 0x29 && hexStg[n] < 0x40 ) //if 0 to 9
        digit[n] = hexStg[n] & 0x0f;            //convert to int
     else if (hexStg[n] >='a' && hexStg[n] <= 'f') //if a to f
        digit[n] = (hexStg[n] & 0x0f) + 9;      //convert to int
     else if (hexStg[n] >='A' && hexStg[n] <= 'F') //if A to F
        digit[n] = (hexStg[n] & 0x0f) + 9;      //convert to int
     else break;
    n++;
  }
  count = n;
  m = n - 1;
  n = 0;
  while(n < count) {
     // digit[n] is value of hex digit at position n
     // (m << 2) is the number of positions to shift
     // OR the bits into return value
     intValue = intValue | (digit[n] << (m << 2));
     m--;   // adjust the position to set
     n++;   // next digit to process
  }

  return (intValue);
}


PROOMDATA GetRoomData(DWORD dwID)
{
	int iNum = g_aRoomList.GetSize();

	for(int i = 0 ; i < iNum; i++){

		if(g_aRoomList[i].dwRoomID == dwID){
			
			return &(g_aRoomList[i]);		
		}
	}

	return NULL;
}

PROOMDATA AddRoomData(DWORD dwID, LPCTSTR lpszRoomName)
{
	
	CString strRoomName = lpszRoomName;
	CString strIP;

	int iPos = strRoomName.ReverseFind('_');
	
	if(iPos != -1) strIP       = strRoomName.Mid(iPos);
	if(iPos != -1) strRoomName = strRoomName.Left(iPos);
	
	// Translate IP
	// Is reversed and hexed so:
	// 01 00 00 7F => 7F 00 00 01 => 127.0.0.1
	strIP.Remove('_');
	strIP = strIP.Left(8);
	CString str1 = strIP.Mid(6, 2);
	CString str2 = strIP.Mid(4, 2);
	CString str3 = strIP.Mid(2, 2);
	CString str4 = strIP.Mid(0, 2);
	
	strIP.Format("%d.%d.%d.%d", 
          		   axtoi((LPTSTR)(LPCTSTR)str1), 
          		   axtoi((LPTSTR)(LPCTSTR)str2), 
          		   axtoi((LPTSTR)(LPCTSTR)str3), 
          		   axtoi((LPTSTR)(LPCTSTR)str4)
				 );

	PROOMDATA pRoomData = GetRoomData(dwID);
	if(pRoomData){
	
		pRoomData->strRoomName = strRoomName;
		pRoomData->strIP       = strIP;
		return pRoomData;
	}
	else{

		ROOMDATA RoomData;
		RoomData.dwRoomID    = dwID;
		RoomData.strRoomName = strRoomName;
		RoomData.strIP       = strIP;

		int n =  g_aRoomList.Add(RoomData);
		return &(g_aRoomList[n]);
	}
}

void RemoveRoomData(DWORD dwID)
{
	int iNum = g_aRoomList.GetSize();

	for(int i = 0; i < iNum; i++){

		if(g_aRoomList[i].dwRoomID == dwID){

			g_aRoomList.RemoveAt(i);
			return;
		}
	}
}

__declspec( dllexport ) PBPLUGIN GetModule(){

	return &g_Plugin;
}

