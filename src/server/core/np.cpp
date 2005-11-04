/* 
** NetXMS - Network Management System
** Copyright (C) 2003, 2004, 2005 Victor Kirhenshtein
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** $module: np.cpp
**
**/

#include "nxcore.h"


//
// Poll new node for configuration
// Returns pointer to new node object on success or NULL on failure
//

NetObj *PollNewNode(DWORD dwIpAddr, DWORD dwNetMask, DWORD dwFlags, TCHAR *pszName)
{
   Node *pNode;
   char szIpAddr1[32], szIpAddr2[32];

   DbgPrintf(AF_DEBUG_DISCOVERY, "PollNode(%s,%s)",  
             IpToStr(dwIpAddr, szIpAddr1), IpToStr(dwNetMask, szIpAddr2));
   // Check for node existence
   if ((FindNodeByIP(dwIpAddr) != NULL) ||
       (FindSubnetByIP(dwIpAddr) != NULL))
   {
      DbgPrintf(AF_DEBUG_DISCOVERY, "PollNode: Node %s already exist in database", 
                IpToStr(dwIpAddr, szIpAddr1));
      return NULL;
   }

   pNode = new Node(dwIpAddr, 0, dwFlags, 0);
   NetObjInsert(pNode, TRUE);
   pNode->NewNodePoll(dwNetMask);
   if (pszName != NULL)
      pNode->SetName(pszName);
   pNode->Unhide();
   PostEvent(EVENT_NODE_ADDED, pNode->Id(), NULL);

   // DEBUG
   pNode->AddItem(new DCItem(CreateUniqueId(IDG_ITEM), _T("Status"), DS_INTERNAL, DCI_DT_INT, 60, 30, pNode));
   return pNode;
}


//
// Node poller thread (poll new nodes and put them into the database)
//

THREAD_RESULT THREAD_CALL NodePoller(void *arg)
{
   DB_RESULT hResult;
   int iPollInterval;
   DWORD dwWatchdogId;

   // Flush new nodes table
   DBQuery(g_hCoreDB, "DELETE FROM new_nodes");

   // Read configuration and initialize
   iPollInterval = ConfigReadInt("NewNodePollingInterval", 60);
   dwWatchdogId = WatchdogAddThread("Node Poller", iPollInterval * 2 + 10);
   DbgPrintf(AF_DEBUG_DISCOVERY, "Node poller started with poll interval %d seconds", iPollInterval);

   while(!ShutdownInProgress())
   {
      if (SleepAndCheckForShutdown(iPollInterval))
         break;      // Shutdown has arrived
      WatchdogNotify(dwWatchdogId);

      hResult = DBSelect(g_hCoreDB, "SELECT id,ip_addr,ip_netmask,discovery_flags FROM new_nodes");
      if (hResult != 0)
      {
         int i, iNumRows;
         DWORD dwId, dwIpAddr, dwNetMask, dwFlags;
         char szQuery[256];

         iNumRows = DBGetNumRows(hResult);
         for(i = 0; i < iNumRows; i++)
         {
            WatchdogNotify(dwWatchdogId);

            dwId = DBGetFieldULong(hResult, i, 0);
            dwIpAddr = DBGetFieldIPAddr(hResult, i, 1);
            dwNetMask = DBGetFieldIPAddr(hResult, i, 2);
            dwFlags = DBGetFieldULong(hResult, i, 3);

            PollNewNode(dwIpAddr, dwNetMask, dwFlags, NULL);

            // Delete processed node
            sprintf(szQuery, "DELETE FROM new_nodes WHERE id=%d", dwId);
            DBQuery(g_hCoreDB, szQuery);
         }
         DBFreeResult(hResult);
      }
   }
   DbgPrintf(AF_DEBUG_DISCOVERY, "Node poller thread terminated");
   return THREAD_OK;
}
