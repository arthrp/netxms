/* 
** Project X - Network Management System
** Copyright (C) 2003 Victor Kirhenshtein
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
** $module: node.cpp
**
**/

#include "nms_core.h"


//
// Node class default constructor
//

Node::Node()
     :NetObj()
{
   m_dwFlags = 0;
   m_dwDiscoveryFlags = 0;
   m_wAgentPort = AGENT_LISTEN_PORT;
   m_wAuthMethod = AUTH_NONE;
   m_szSharedSecret[0] = 0;
   m_iStatusPollType = POLL_ICMP_PING;
   m_iSNMPVersion = SNMP_VERSION_1;
   strcpy(m_szCommunityString, "public");
   m_szObjectId[0] = 0;
   m_tLastDiscoveryPoll = 0;
   m_tLastStatusPoll = 0;
   m_tLastConfigurationPoll = 0;
   m_iSnmpAgentFails = 0;
   m_iNativeAgentFails = 0;
   m_hPollerMutex = MutexCreate();
   m_dwNumItems = 0;
   m_pItems = NULL;
}


//
// Constructor for new node object
//

Node::Node(DWORD dwAddr, DWORD dwFlags, DWORD dwDiscoveryFlags)
     :NetObj()
{
   m_dwIpAddr = dwAddr;
   m_dwFlags = dwFlags;
   m_dwDiscoveryFlags = dwDiscoveryFlags;
   m_wAgentPort = AGENT_LISTEN_PORT;
   m_wAuthMethod = AUTH_NONE;
   m_szSharedSecret[0] = 0;
   m_iStatusPollType = POLL_ICMP_PING;
   m_iSNMPVersion = SNMP_VERSION_1;
   strcpy(m_szCommunityString, "public");
   IpToStr(dwAddr, m_szName);    // Make default name from IP address
   m_szObjectId[0] = 0;
   m_tLastDiscoveryPoll = 0;
   m_tLastStatusPoll = 0;
   m_tLastConfigurationPoll = 0;
   m_iSnmpAgentFails = 0;
   m_iNativeAgentFails = 0;
   m_hPollerMutex = MutexCreate();
   m_dwNumItems = 0;
   m_pItems = NULL;
}


//
// Node destructor
//

Node::~Node()
{
   MutexDestroy(m_hPollerMutex);
   if (m_pItems != NULL)
      free(m_pItems);
}


//
// Create object from database data
//

BOOL Node::CreateFromDB(DWORD dwId)
{
   char szQuery[256];
   DB_RESULT hResult;
   int i, iNumRows;
   DWORD dwSubnetId;
   NetObj *pObject;

   sprintf(szQuery, "SELECT id,name,status,primary_ip,is_snmp,is_agent,is_bridge,"
                    "is_router,snmp_version,discovery_flags,auth_method,secret,"
                    "agent_port,status_poll_type,community,snmp_oid,is_local_mgmt FROM nodes WHERE id=%d", dwId);
   hResult = DBSelect(g_hCoreDB, szQuery);
   if (hResult == 0)
      return FALSE;     // Query failed

   if (DBGetNumRows(hResult) == 0)
   {
      DBFreeResult(hResult);
      return FALSE;
   }

   m_dwId = dwId;
   strncpy(m_szName, DBGetField(hResult, 0, 1), MAX_OBJECT_NAME);
   m_iStatus = DBGetFieldLong(hResult, 0, 2);
   m_dwIpAddr = DBGetFieldULong(hResult, 0, 3);

   // Flags
   if (DBGetFieldLong(hResult, 0, 4))
      m_dwFlags |= NF_IS_SNMP;
   if (DBGetFieldLong(hResult, 0, 5))
      m_dwFlags |= NF_IS_NATIVE_AGENT;
   if (DBGetFieldLong(hResult, 0, 6))
      m_dwFlags |= NF_IS_BRIDGE;
   if (DBGetFieldLong(hResult, 0, 7))
      m_dwFlags |= NF_IS_ROUTER;
   if (DBGetFieldLong(hResult, 0, 16))
      m_dwFlags |= NF_IS_LOCAL_MGMT;

   m_iSNMPVersion = DBGetFieldLong(hResult, 0, 8);
   m_dwDiscoveryFlags = DBGetFieldULong(hResult, 0, 9);
   m_wAuthMethod = (WORD)DBGetFieldLong(hResult, 0, 10);
   strncpy(m_szSharedSecret, DBGetField(hResult, 0, 11), MAX_SECRET_LENGTH);
   m_wAgentPort = (WORD)DBGetFieldLong(hResult, 0, 12);
   m_iStatusPollType = DBGetFieldLong(hResult, 0, 13);
   strncpy(m_szCommunityString, DBGetField(hResult, 0, 14), MAX_COMMUNITY_LENGTH);
   strncpy(m_szObjectId, DBGetField(hResult, 0, 15), MAX_OID_LEN * 4);

   DBFreeResult(hResult);

   // Link node to subnets
   sprintf(szQuery, "SELECT subnet_id FROM nsmap WHERE node_id=%d", dwId);
   hResult = DBSelect(g_hCoreDB, szQuery);
   if (hResult == 0)
      return FALSE;     // Query failed

   if (DBGetNumRows(hResult) == 0)
   {
      DBFreeResult(hResult);
      return FALSE;     // No parents - it shouldn't happen if database isn't corrupted
   }

   BOOL bResult = FALSE;
   iNumRows = DBGetNumRows(hResult);
   for(i = 0; i < iNumRows; i++)
   {
      dwSubnetId = DBGetFieldULong(hResult, i, 0);
      pObject = FindObjectById(dwSubnetId);
      if (pObject == NULL)
      {
         WriteLog(MSG_INVALID_SUBNET_ID, EVENTLOG_ERROR_TYPE, "dd", dwId, dwSubnetId);
         break;
      }
      else if (pObject->Type() != OBJECT_SUBNET)
      {
         WriteLog(MSG_SUBNET_NOT_SUBNET, EVENTLOG_ERROR_TYPE, "dd", dwId, dwSubnetId);
         break;
      }
      else
      {
         pObject->AddChild(this);
         AddParent(pObject);
         bResult = TRUE;
      }
   }

   DBFreeResult(hResult);
   LoadItemsFromDB();
   return bResult;
}


//
// Save object to database
//

BOOL Node::SaveToDB(void)
{
   char szQuery[4096];
   DB_RESULT hResult;
   BOOL bNewObject = TRUE;

   // Lock object's access
   Lock();

   // Check for object's existence in database
   sprintf(szQuery, "SELECT id FROM nodes WHERE id=%ld", m_dwId);
   hResult = DBSelect(g_hCoreDB, szQuery);
   if (hResult != 0)
   {
      if (DBGetNumRows(hResult) > 0)
         bNewObject = FALSE;
      DBFreeResult(hResult);
   }

   // Form and execute INSERT or UPDATE query
   if (bNewObject)
      sprintf(szQuery, "INSERT INTO nodes (id,name,status,is_deleted,primary_ip,"
                       "is_snmp,is_agent,is_bridge,is_router,snmp_version,community,"
                       "discovery_flags,status_poll_type,agent_port,auth_method,secret,"
                       "snmp_oid,is_local_mgmt)"
                       " VALUES (%d,'%s',%d,%d,%d,%d,%d,%d,%d,%d,'%s',%d,%d,%d,%d,'%s','%s',%d)",
              m_dwId, m_szName, m_iStatus, m_bIsDeleted, m_dwIpAddr, 
              m_dwFlags & NF_IS_SNMP ? 1 : 0,
              m_dwFlags & NF_IS_NATIVE_AGENT ? 1 : 0,
              m_dwFlags & NF_IS_BRIDGE ? 1 : 0,
              m_dwFlags & NF_IS_ROUTER ? 1 : 0,
              m_iSNMPVersion, m_szCommunityString, m_dwDiscoveryFlags, m_iStatusPollType,
              m_wAgentPort,m_wAuthMethod,m_szSharedSecret, m_szObjectId,
              m_dwFlags & NF_IS_LOCAL_MGMT ? 1 : 0);
   else
      sprintf(szQuery, "UPDATE nodes SET name='%s',status=%d,is_deleted=%d,primary_ip=%d,"
                       "is_snmp=%d,is_agent=%d,is_bridge=%d,is_router=%d,snmp_version=%d,"
                       "community='%s',discovery_flags=%d,status_poll_type=%d,agent_port=%d,"
                       "auth_method=%d,secret='%s',snmp_oid='%s',is_local_mgmt=%d WHERE id=%d",
              m_szName, m_iStatus, m_bIsDeleted, m_dwIpAddr, 
              m_dwFlags & NF_IS_SNMP ? 1 : 0,
              m_dwFlags & NF_IS_NATIVE_AGENT ? 1 : 0,
              m_dwFlags & NF_IS_BRIDGE ? 1 : 0,
              m_dwFlags & NF_IS_ROUTER ? 1 : 0,
              m_iSNMPVersion, m_szCommunityString, m_dwDiscoveryFlags, 
              m_iStatusPollType, m_wAgentPort, m_wAuthMethod, m_szSharedSecret, 
              m_szObjectId, m_dwFlags & NF_IS_LOCAL_MGMT ? 1 : 0, m_dwId);
   DBQuery(g_hCoreDB, szQuery);

   // Clear modifications flag and unlock object
   m_bIsModified = FALSE;
   Unlock();

   return TRUE;
}


//
// Delete object from database
//

BOOL Node::DeleteFromDB(void)
{
   char szQuery[256];

   sprintf(szQuery, "DELETE FROM nodes WHERE id=%ld", m_dwId);
   DBQuery(g_hCoreDB, szQuery);
   return TRUE;
}


//
// Poll newly discovered node
// Usually called once by node poller thread when new node is discovered
// and object for it is created
//

BOOL Node::NewNodePoll(DWORD dwNetMask)
{
   AgentConnection *pAgentConn;

   // Determine node's capabilities
   if (SnmpGet(m_dwIpAddr, m_szCommunityString, ".1.3.6.1.2.1.1.2.0", NULL, 0, m_szObjectId, MAX_OID_LEN * 4, FALSE))
      m_dwFlags |= NF_IS_SNMP;

   pAgentConn = new AgentConnection(m_dwIpAddr, m_wAgentPort, m_wAuthMethod, m_szSharedSecret);
   if (pAgentConn->Connect())
      m_dwFlags |= NF_IS_NATIVE_AGENT;

   // Get interface list
   if ((m_dwFlags & NF_IS_SNMP) || (m_dwFlags & NF_IS_NATIVE_AGENT)  || (m_dwFlags & NF_IS_LOCAL_MGMT))
   {
      INTERFACE_LIST *pIfList = NULL;
      int i;

      if (m_dwFlags & NF_IS_LOCAL_MGMT)    // For local machine
         pIfList = GetLocalInterfaceList();
      else if (m_dwFlags & NF_IS_NATIVE_AGENT)    // For others prefer native agent
         pIfList = pAgentConn->GetInterfaceList();
      if ((pIfList == NULL) && (m_dwFlags & NF_IS_SNMP))  // Use SNMP if we cannot get interfaces via agent
         pIfList = SnmpGetInterfaceList(m_dwIpAddr, m_szCommunityString);

      if (pIfList != NULL)
      {
         for(i = 0; i < pIfList->iNumEntries; i++)
            CreateNewInterface(pIfList->pInterfaces[i].dwIpAddr, 
                               pIfList->pInterfaces[i].dwIpNetMask,
                               pIfList->pInterfaces[i].szName,
                               pIfList->pInterfaces[i].dwIndex,
                               pIfList->pInterfaces[i].dwType);
         DestroyInterfaceList(pIfList);
      }
      else
      {
         // We cannot get interface list from node for some reasons, create dummy one
         CreateNewInterface(m_dwIpAddr, dwNetMask);
      }
   }
   else  // No SNMP, no native agent - create pseudo interface object
   {
      CreateNewInterface(m_dwIpAddr, dwNetMask);
   }

   // Clean up agent connection
   if (m_dwFlags & NF_IS_NATIVE_AGENT)
      pAgentConn->Disconnect();
   delete pAgentConn;

   return TRUE;
}


//
// Get ARP cache from node
//

ARP_CACHE *Node::GetArpCache(void)
{
   ARP_CACHE *pArpCache = NULL;

   if (m_dwFlags & NF_IS_LOCAL_MGMT)
   {
      pArpCache = GetLocalArpCache();
   }
   else if (m_dwFlags & NF_IS_NATIVE_AGENT)
   {
      AgentConnection *pAgentConn = new AgentConnection(m_dwIpAddr, m_wAgentPort, m_wAuthMethod, m_szSharedSecret);
      if (pAgentConn->Connect())
      {
         pArpCache = pAgentConn->GetArpCache();
         pAgentConn->Disconnect();
      }
      delete pAgentConn;
   }
   else if (m_dwFlags & NF_IS_SNMP)
   {
      pArpCache = SnmpGetArpCache(m_dwIpAddr, m_szCommunityString);
   }

   return pArpCache;
}


//
// Get list of interfaces from node
//

INTERFACE_LIST *Node::GetInterfaceList(void)
{
   INTERFACE_LIST *pIfList = NULL;

   if (m_dwFlags & NF_IS_LOCAL_MGMT)
   {
      pIfList = GetLocalInterfaceList();
   }
   else if (m_dwFlags & NF_IS_NATIVE_AGENT)
   {
      AgentConnection *pAgentConn = new AgentConnection(m_dwIpAddr, m_wAgentPort, m_wAuthMethod, m_szSharedSecret);
      if (pAgentConn->Connect())
      {
         pIfList = pAgentConn->GetInterfaceList();
         pAgentConn->Disconnect();
      }
      delete pAgentConn;
   }
   else if (m_dwFlags & NF_IS_SNMP)
   {
      pIfList = SnmpGetInterfaceList(m_dwIpAddr, m_szCommunityString);
   }

   return pIfList;
}


//
// Find interface by index and node IP
// Returns pointer to interface object or NULL if appropriate interface couldn't be found
//

Interface *Node::FindInterface(DWORD dwIndex, DWORD dwHostAddr)
{
   DWORD i;
   Interface *pInterface;

   for(i = 0; i < m_dwChildCount; i++)
      if (m_pChildList[i]->Type() == OBJECT_INTERFACE)
      {
         pInterface = (Interface *)m_pChildList[i];
         if (pInterface->IfIndex() == dwIndex)
            if ((pInterface->IpAddr() & pInterface->IpNetMask()) ==
                (dwHostAddr & pInterface->IpNetMask()))
               return pInterface;
      }
   return NULL;
}


//
// Create new interface
//

void Node::CreateNewInterface(DWORD dwIpAddr, DWORD dwNetMask, char *szName, DWORD dwIndex, DWORD dwType)
{
   Interface *pInterface;
   Subnet *pSubnet;

   // Create interface object
   if (szName != NULL)
      pInterface = new Interface(szName, dwIndex, dwIpAddr, dwNetMask, dwType);
   else
      pInterface = new Interface(dwIpAddr, dwNetMask);

   // Insert to objects' list and generate event
   NetObjInsert(pInterface, TRUE);
   AddInterface(pInterface);
   PostEvent(EVENT_INTERFACE_ADDED, m_dwId, "dsaad", pInterface->Id(),
             pInterface->Name(), pInterface->IpAddr(),
             pInterface->IpNetMask(), pInterface->IfIndex());

   // Bind node to appropriate subnet
   if (pInterface->IpAddr() != 0)   // Do not link non-IP interfaces to 0.0.0.0 subnet
   {
      pSubnet = FindSubnetByIP(pInterface->IpAddr() & pInterface->IpNetMask());
      if (pSubnet == NULL)
      {
         // Create new subnet object
         pSubnet = new Subnet(pInterface->IpAddr() & pInterface->IpNetMask(), pInterface->IpNetMask());
         NetObjInsert(pSubnet, TRUE);
         g_pEntireNet->AddSubnet(pSubnet);
      }
      pSubnet->AddNode(this);
   }
}


//
// Delete interface from node
//

void Node::DeleteInterface(Interface *pInterface)
{
   DWORD i;

   // Check if we should unlink node from interface's subnet
   if (pInterface->IpAddr() != 0)
   {
      for(i = 0; i < m_dwChildCount; i++)
         if (m_pChildList[i]->Type() == OBJECT_INTERFACE)
            if (m_pChildList[i] != pInterface)
               if ((((Interface *)m_pChildList[i])->IpAddr() & ((Interface *)m_pChildList[i])->IpNetMask()) ==
                   (pInterface->IpAddr() & pInterface->IpNetMask()))
                  break;
      if (i == m_dwChildCount)
      {
         // Last interface in subnet, should unlink node
         Subnet *pSubnet = FindSubnetByIP(pInterface->IpAddr() & pInterface->IpNetMask());
         if (pSubnet != NULL)
            pSubnet->DeleteChild(this);
         DeleteParent(pSubnet);
      }
   }
   pInterface->Delete();
}


//
// Calculate node status based on child objects status
//

void Node::CalculateCompoundStatus(void)
{
   int iOldStatus = m_iStatus;
   static DWORD dwEventCodes[] = { EVENT_NODE_NORMAL, EVENT_NODE_MINOR,
      EVENT_NODE_WARNING, EVENT_NODE_MAJOR, EVENT_NODE_CRITICAL,
      EVENT_NODE_UNKNOWN, EVENT_NODE_UNMANAGED };

   NetObj::CalculateCompoundStatus();
   if (m_iStatus != iOldStatus)
      PostEvent(dwEventCodes[m_iStatus], m_dwId, "d", iOldStatus);
}


//
// Perform status poll on node
//

void Node::StatusPoll(void)
{
   DWORD i;

   PollerLock();
   for(i = 0; i < m_dwChildCount; i++)
      if (m_pChildList[i]->Type() == OBJECT_INTERFACE)
         ((Interface *)m_pChildList[i])->StatusPoll();
   CalculateCompoundStatus();
   m_tLastStatusPoll = time(NULL);
   PollerUnlock();
}


//
// Perform configuration poll on node
//

void Node::ConfigurationPoll(void)
{
   DWORD dwOldFlags = m_dwFlags;
   AgentConnection *pAgentConn;
   INTERFACE_LIST *pIfList;

   PollerLock();

   // Check node's capabilities
   if (SnmpGet(m_dwIpAddr, m_szCommunityString, ".1.3.6.1.2.1.1.2.0", NULL, 0, m_szObjectId, MAX_OID_LEN * 4, FALSE))
   {
      m_dwFlags |= NF_IS_SNMP;
      m_iSnmpAgentFails = 0;
   }
   else
   {
      if (m_dwFlags & NF_IS_SNMP)
      {
         if (m_iSnmpAgentFails == 0)
            PostEvent(EVENT_SNMP_FAIL, m_dwId, NULL);
         m_iSnmpAgentFails++;
      }
   }

   pAgentConn = new AgentConnection(m_dwIpAddr, m_wAgentPort, m_wAuthMethod, m_szSharedSecret);
   if (pAgentConn->Connect())
   {
      m_dwFlags |= NF_IS_NATIVE_AGENT;
      m_iNativeAgentFails = 0;
   }
   else
   {
      if (m_dwFlags & NF_IS_NATIVE_AGENT)
      {
         if (m_iNativeAgentFails == 0)
            PostEvent(EVENT_AGENT_FAIL, m_dwId, NULL);
         m_iNativeAgentFails++;
      }
   }

   // Generate event if node flags has been changed
   if (dwOldFlags != m_dwFlags)
      PostEvent(EVENT_NODE_FLAGS_CHANGED, m_dwId, "xx", dwOldFlags, m_dwFlags);

   // Retrieve interface list
   pIfList = GetInterfaceList();
   if (pIfList != NULL)
   {
      DWORD i;
      int j;

      // Find non-existing interfaces
      for(i = 0; i < m_dwChildCount; i++)
         if (m_pChildList[i]->Type() == OBJECT_INTERFACE)
         {
            Interface *pInterface = (Interface *)m_pChildList[i];

            for(j = 0; j < pIfList->iNumEntries; j++)
               if ((pIfList->pInterfaces[j].dwIndex == pInterface->IfIndex()) &&
                   (pIfList->pInterfaces[j].dwIpAddr == pInterface->IpAddr()) &&
                   (pIfList->pInterfaces[j].dwIpNetMask == pInterface->IpNetMask()))
                  break;
            if (j == pIfList->iNumEntries)
            {
               // No such interface in current configuration, delete it
               PostEvent(EVENT_INTERFACE_DELETED, m_dwId, "dsaa", pInterface->IfIndex(),
                         pInterface->Name(), pInterface->IpAddr(), pInterface->IpNetMask());
               DeleteInterface(pInterface);
               i = 0;   // Restart loop
            }
         }

      // Add new interfaces
      for(j = 0; j < pIfList->iNumEntries; j++)
      {
         for(i = 0; i < m_dwChildCount; i++)
            if (m_pChildList[i]->Type() == OBJECT_INTERFACE)
            {
               Interface *pInterface = (Interface *)m_pChildList[i];

               if ((pIfList->pInterfaces[j].dwIndex == pInterface->IfIndex()) &&
                   (pIfList->pInterfaces[j].dwIpAddr == pInterface->IpAddr()) &&
                   (pIfList->pInterfaces[j].dwIpNetMask == pInterface->IpNetMask()))
                  break;
            }
         if (i == m_dwChildCount)
         {
            // New interface
            CreateNewInterface(pIfList->pInterfaces[j].dwIpAddr, 
                               pIfList->pInterfaces[j].dwIpNetMask,
                               pIfList->pInterfaces[j].szName,
                               pIfList->pInterfaces[j].dwIndex,
                               pIfList->pInterfaces[j].dwType);
         }
      }
   }

   PollerUnlock();
}


//
// Load data collection items from database
//

void Node::LoadItemsFromDB(void)
{
   char szQuery[256];
   DB_RESULT hResult;

   if (m_pItems != NULL)
   {
      free(m_pItems);
      m_pItems = NULL;
   }
   m_dwNumItems = NULL;

   sprintf(szQuery, "SELECT id,name,source,datatype,polling_interval,retention_time FROM items WHERE node_id=%d", m_dwId);
   hResult = DBSelect(g_hCoreDB, szQuery);

   if (hResult != 0)
   {
      int i, iRows;

      iRows = DBGetNumRows(hResult);
      if (iRows > 0)
      {
         m_dwNumItems = iRows;
         m_pItems = (DC_ITEM *)malloc(sizeof(DC_ITEM) * iRows);
         for(i = 0; i < iRows; i++)
         {
            m_pItems[i].dwId = DBGetFieldULong(hResult, i, 0);
            strcpy(m_pItems[i].szName, DBGetField(hResult, i, 1));
            m_pItems[i].iSource = (BYTE)DBGetFieldLong(hResult, i, 2);
            m_pItems[i].iDataType = (BYTE)DBGetFieldLong(hResult, i, 3);
            m_pItems[i].iPollingInterval = DBGetFieldLong(hResult, i, 4);
            m_pItems[i].iRetentionTime = DBGetFieldLong(hResult, i, 5);
         }
      }
      DBFreeResult(hResult);
   }
}
