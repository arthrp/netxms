/*
** NetXMS multiplatform core agent
** Copyright (C) 2003-2020 Raden Solutions
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
** File: datacoll.cpp
**
**/

#include "nxagentd.h"

#define DEBUG_TAG _T("dc")

/**
 * Interval in milliseconds between stalled data checks
 */
#define STALLED_DATA_CHECK_INTERVAL    3600000

/**
 * Externals
 */
void UpdateSnmpTarget(shared_ptr<SNMPTarget> target);
uint32_t GetSnmpValue(const uuid& target, uint16_t port, SNMP_Version version, const TCHAR *oid,
         TCHAR *value, int interpretRawValue);
uint32_t GetSnmpTable(const uuid& target, uint16_t port, SNMP_Version version, const TCHAR *oid,
         const ObjectArray<SNMPTableColumnDefinition> &columns, Table *value);

void LoadProxyConfiguration();
void UpdateProxyConfiguration(UINT64 serverId, HashMap<ServerObjectKey, DataCollectionProxy> *proxyList, const ZoneConfiguration *zone);
void ProxyConnectionChecker(void *arg);
THREAD_RESULT THREAD_CALL ProxyListenerThread(void *arg);

extern HashMap<ServerObjectKey, DataCollectionProxy> g_proxyList;
extern Mutex g_proxyListMutex;

extern UINT32 g_dcReconciliationBlockSize;
extern UINT32 g_dcReconciliationTimeout;
extern UINT32 g_dcWriterFlushInterval;
extern UINT32 g_dcWriterMaxTransactionSize;
extern UINT32 g_dcMaxCollectorPoolSize;
extern UINT32 g_dcOfflineExpirationTime;

/**
 * Data collector start indicator
 */
static bool s_dataCollectorStarted = false;

/**
 * Unsaved poll time indicator
 */
static bool s_pollTimeChanged = false;

/**
 * Create column object from NXCP message
 */
SNMPTableColumnDefinition::SNMPTableColumnDefinition(NXCPMessage *msg, UINT32 baseId)
{
   msg->getFieldAsString(baseId, m_name, MAX_COLUMN_NAME);
   m_flags = msg->getFieldAsUInt16(baseId + 1);
   m_displayName = msg->getFieldAsString(baseId + 3);

   if (msg->isFieldExist(baseId + 2))
   {
      UINT32 oid[256];
      size_t len = msg->getFieldAsInt32Array(baseId + 2, 256, oid);
      if (len > 0)
      {
         m_snmpOid = new SNMP_ObjectId(oid, len);
      }
      else
      {
         m_snmpOid = nullptr;
      }
   }
   else
   {
      m_snmpOid = nullptr;
   }
}

/**
 * Create column object from DB record
 */
SNMPTableColumnDefinition::SNMPTableColumnDefinition(DB_RESULT hResult, int row)
{
   DBGetField(hResult, row, 0, m_name, MAX_COLUMN_NAME);
   m_displayName = DBGetField(hResult, row, 1, nullptr, 0);
   m_flags = static_cast<uint16_t>(DBGetFieldULong(hResult, row, 2));

   TCHAR oid[1024];
   oid[0] = 0;
   DBGetField(hResult, row, 3, oid, 1024);
   StrStrip(oid);
   if (oid[0] != 0)
   {
      uint32_t oidBin[256];
      size_t len = SNMPParseOID(oid, oidBin, 256);
      if (len > 0)
      {
         m_snmpOid = new SNMP_ObjectId(oidBin, len);
      }
      else
      {
         m_snmpOid = nullptr;
      }
   }
   else
   {
      m_snmpOid = nullptr;
   }
}

/**
 * Create copy of column object
 */
SNMPTableColumnDefinition::SNMPTableColumnDefinition(const SNMPTableColumnDefinition *src)
{
   memcpy(m_name, src->m_name, sizeof(m_name));
   m_displayName = MemCopyString(src->m_displayName);
   m_snmpOid = (src->m_snmpOid != nullptr) ? new SNMP_ObjectId(*src->m_snmpOid) : nullptr;
   m_flags = src->m_flags;
}

/**
 * Column object destructor
 */
SNMPTableColumnDefinition::~SNMPTableColumnDefinition()
{
   MemFree(m_displayName);
   delete m_snmpOid;
}

/**
 * Statement set
 */
struct DataCollectionStatementSet
{
   DB_STATEMENT insertItem;
   DB_STATEMENT updateItem;
   DB_STATEMENT deleteItem;
   DB_STATEMENT insertColumn;
   DB_STATEMENT deleteColumns;
   DB_STATEMENT deleteSchedules;
   DB_STATEMENT insertSchedules;
};

enum class ScheduleType
{
   NONE = 0,
   MINUTES = 1,
   SECONDS = 2
};

/**
 * Data collection item
 */
class DataCollectionItem : public RefCountObject
{
private:
   uint64_t m_serverId;
   uint32_t m_id;
   int32_t m_pollingInterval;
   TCHAR *m_name;
   uint8_t m_type;
   uint8_t m_origin;
   uint8_t m_busy;
   uint8_t m_snmpRawValueType;
   uint16_t m_snmpPort;
   SNMP_Version m_snmpVersion;
	uuid m_snmpTargetGuid;
   time_t m_lastPollTime;
   uint32_t m_backupProxyId;
   ObjectArray<SNMPTableColumnDefinition> *m_tableColumns;
   StringList m_schedules;
   ScheduleType m_scheduleType;
   time_t m_tLastCheck;

public:
   DataCollectionItem(UINT64 serverId, NXCPMessage *msg, uint32_t baseId, uint32_t extBaseId, bool hasExtraData);
   DataCollectionItem(DB_RESULT hResult, int row);
   DataCollectionItem(const DataCollectionItem *item);
   virtual ~DataCollectionItem();

   UINT32 getId() const { return m_id; }
   UINT64 getServerId() const { return m_serverId; }
   ServerObjectKey getKey() const { return ServerObjectKey(m_serverId, m_id); }
   const TCHAR *getName() const { return m_name; }
   int getType() const { return (int)m_type; }
   int getOrigin() const { return (int)m_origin; }
   const uuid& getSnmpTargetGuid() const { return m_snmpTargetGuid; }
   UINT16 getSnmpPort() const { return m_snmpPort; }
   SNMP_Version getSnmpVersion() const { return m_snmpVersion; }
   int getSnmpRawValueType() const { return (int)m_snmpRawValueType; }
   UINT32 getPollingInterval() const { return (UINT32)m_pollingInterval; }
   time_t getLastPollTime() { return m_lastPollTime; }
   UINT32 getBackupProxyId() const { return m_backupProxyId; }
   const ObjectArray<SNMPTableColumnDefinition> *getColumns() const { return m_tableColumns; }

   bool updateAndSave(const DataCollectionItem *item, bool txnOpen, DB_HANDLE hdb, DataCollectionStatementSet *statements);
   void saveToDatabase(bool newObject, DB_HANDLE hdb, DataCollectionStatementSet *statements);
   void deleteFromDatabase(DB_HANDLE hdb, DataCollectionStatementSet *statements);
   void setLastPollTime(time_t time);

   void startDataCollection() { m_busy = 1; incRefCount(); }
   void finishDataCollection() { m_busy = 0; decRefCount(); }

   uint32_t getTimeToNextPoll(time_t now)
   {
      if (m_busy) // being polled now - time to next poll should not be less than full polling interval
         return m_pollingInterval;

      if(m_scheduleType == ScheduleType::NONE)
      {
         time_t diff = now - m_lastPollTime;
         return (diff >= m_pollingInterval) ? 0 : m_pollingInterval - static_cast<uint32_t>(diff);
      }
      else
      {
         bool scheduleMatched = false;
         struct tm tmCurrLocal, tmLastLocal;
#if HAVE_LOCALTIME_R
         localtime_r(&now, &tmCurrLocal);
         localtime_r(&m_tLastCheck, &tmLastLocal);
#else
         memcpy(&tmCurrLocal, localtime(&now), sizeof(struct tm));
         memcpy(&tmLastLocal, localtime(&m_tLastCheck), sizeof(struct tm));
#endif

         for (int i = 0; i < m_schedules.size(); i++)
         {
            bool withSeconds = false;
            if (MatchSchedule(m_schedules.get(i), &withSeconds, &tmCurrLocal, now))
            {
               if (withSeconds || (now - m_tLastCheck >= 60) || (tmCurrLocal.tm_min != tmLastLocal.tm_min))
               {
                  scheduleMatched = true;
                  break;
               }
            }
         }
         m_tLastCheck = now;

         if (scheduleMatched)
            return 0;
         if (m_scheduleType == ScheduleType::SECONDS)
            return 1;
         else
            return 60 - (now % 60);

      }
   }
};

static bool UsesSeconds(const TCHAR *schedule)
{
   TCHAR szValue[256];

   const TCHAR *pszCurr = ExtractWord(schedule, szValue);
   for (int i = 0; i < 5; i++)
   {
      szValue[0] = _T('\0');
      pszCurr = ExtractWord(pszCurr, szValue);
      if (szValue[0] == _T('\0'))
      {
         return false;
      }
   }

   return true;
}

/**
 * Create data collection item from NXCP mesage
 */
DataCollectionItem::DataCollectionItem(UINT64 serverId, NXCPMessage *msg, UINT32 baseId, uint32_t extBaseId, bool hasExtraData) : RefCountObject()
{
   m_serverId = serverId;
   m_id = msg->getFieldAsInt32(baseId);
   m_type = static_cast<uint8_t>(msg->getFieldAsUInt16(baseId + 1));
   m_origin = static_cast<uint8_t>(msg->getFieldAsUInt16(baseId + 2));
   m_name = msg->getFieldAsString(baseId + 3);
   m_pollingInterval = msg->getFieldAsInt32(baseId + 4);
   m_lastPollTime = msg->getFieldAsTime(baseId + 5);
   m_snmpTargetGuid = msg->getFieldAsGUID(baseId + 6);
   m_snmpPort = msg->getFieldAsUInt16(baseId + 7);
   m_snmpRawValueType = static_cast<uint8_t>(msg->getFieldAsUInt16(baseId + 8));
   m_backupProxyId = msg->getFieldAsInt32(baseId + 9);
   m_busy = 0;
   m_tLastCheck = 0;

   if (hasExtraData && (m_origin == DS_SNMP_AGENT))
   {
      m_snmpVersion = SNMP_VersionFromInt(msg->getFieldAsInt16(extBaseId));
      if (m_type == DCO_TYPE_TABLE)
      {
         int count = msg->getFieldAsInt32(extBaseId + 9);
         uint32_t fieldId = extBaseId + 10;
         m_tableColumns = new ObjectArray<SNMPTableColumnDefinition>(count, 16, Ownership::True);
         for(int i = 0; i < count; i++)
         {
            m_tableColumns->add(new SNMPTableColumnDefinition(msg, fieldId));
            fieldId += 10;
         }
         fieldId += (99 - count) * 10;
      }
      else
      {
         m_tableColumns = nullptr;
      }
   }
   else
   {
      m_snmpVersion = SNMP_VERSION_DEFAULT;
      m_tableColumns = nullptr;
   }
   extBaseId += 1000;

   int32_t size = msg->getFieldAsInt32(extBaseId++);
   if (size > 0)
   {
      m_scheduleType = ScheduleType::MINUTES;
      for(int i = 0; i < size; i++)
      {
         TCHAR *tmp = msg->getFieldAsString(extBaseId++);
         if(UsesSeconds(tmp))
            m_scheduleType = ScheduleType::SECONDS;

         m_schedules.addPreallocated(tmp);
      }
   }
   else
   {
      m_scheduleType = ScheduleType::NONE;
   }
}

/**
 * Data is selected in this order: server_id,dci_id,type,origin,name,polling_interval,last_poll,snmp_port,snmp_target_guid,snmp_raw_type,backup_proxy_id,snmp_version,schedule_type
 */
DataCollectionItem::DataCollectionItem(DB_RESULT hResult, int row)
{
   m_serverId = DBGetFieldUInt64(hResult, row, 0);
   m_id = DBGetFieldULong(hResult, row, 1);
   m_type = static_cast<uint8_t>(DBGetFieldULong(hResult, row, 2));
   m_origin = static_cast<uint8_t>(DBGetFieldULong(hResult, row, 3));
   m_name = DBGetField(hResult, row, 4, NULL, 0);
   m_pollingInterval = DBGetFieldULong(hResult, row, 5);
   m_lastPollTime = static_cast<time_t>(DBGetFieldULong(hResult, row, 6));
   m_snmpPort = DBGetFieldULong(hResult, row, 7);
   m_snmpTargetGuid = DBGetFieldGUID(hResult, row, 8);
   m_snmpRawValueType = static_cast<uint8_t>(DBGetFieldULong(hResult, row, 9));
   m_backupProxyId = DBGetFieldULong(hResult, row, 10);
   m_snmpVersion = SNMP_VersionFromInt(DBGetFieldLong(hResult, row, 11));
   m_scheduleType = static_cast<ScheduleType>(DBGetFieldLong(hResult, row, 12));
   m_busy = 0;
   m_tLastCheck = 0;

   if ((m_origin == DS_SNMP_AGENT) && (m_type == DCO_TYPE_TABLE))
   {
      DB_HANDLE hdb = GetLocalDatabaseHandle();

      TCHAR query[256];
      _sntprintf(query, 256,
               _T("SELECT name,display_name,flags,snmp_oid FROM dc_snmp_table_columns WHERE server_id=") UINT64_FMT _T(" AND dci_id=%u ORDER BY column_id"),
               m_serverId, m_id);
      DB_RESULT hResult = DBSelect(hdb, query);
      if (hResult != nullptr)
      {
         int count = DBGetNumRows(hResult);
         if (count > 0)
         {
            m_tableColumns = new ObjectArray<SNMPTableColumnDefinition>(count, 16, Ownership::True);
            for(int i = 0; i < count; i++)
            {
               m_tableColumns->add(new SNMPTableColumnDefinition(hResult, i));
            }
         }
         else
         {
            m_tableColumns = nullptr;
         }
         DBFreeResult(hResult);
      }
      else
      {
         m_tableColumns = nullptr;
      }
   }
   else
   {
      m_tableColumns = nullptr;
   }

   if (m_scheduleType != ScheduleType::NONE)
   {
      DB_HANDLE hdb = GetLocalDatabaseHandle();

      TCHAR query[256];
      _sntprintf(query, 256,
               _T("SELECT schedule FROM dc_schedules WHERE server_id=") UINT64_FMT _T(" AND dci_id=%u"),
               m_serverId, m_id);
      DB_RESULT hResult = DBSelect(hdb, query);
      if (hResult != nullptr)
      {
        int count = DBGetNumRows(hResult);
        for(int i = 0; i < count; i++)
        {
           m_schedules.addPreallocated(DBGetField(hResult, i, 0, NULL, 0));
        }
      }
   }
}

/**
 * Copy constructor
 */
 DataCollectionItem::DataCollectionItem(const DataCollectionItem *item) : m_schedules(item->m_schedules)
 {
   m_serverId = item->m_serverId;
   m_id = item->m_id;
   m_type = item->m_type;
   m_origin = item->m_origin;
   m_name = _tcsdup(item->m_name);
   m_pollingInterval = item->m_pollingInterval;
   m_lastPollTime = item->m_lastPollTime;
   m_snmpTargetGuid = item->m_snmpTargetGuid;
   m_snmpPort = item->m_snmpPort;
   m_snmpVersion = item->m_snmpVersion;
   m_snmpRawValueType = item->m_snmpRawValueType;
   m_backupProxyId = item->m_backupProxyId;
   if (item->m_tableColumns != nullptr)
   {
      m_tableColumns = new ObjectArray<SNMPTableColumnDefinition>(item->m_tableColumns->size(), 16, Ownership::True);
      for(int i = 0; i < item->m_tableColumns->size(); i++)
         m_tableColumns->add(new SNMPTableColumnDefinition(item->m_tableColumns->get(i)));
   }
   else
   {
      m_tableColumns = nullptr;
   }
   m_busy = 0;
   m_tLastCheck = 0;
   m_scheduleType = item->m_scheduleType;
 }

/**
 * Data collection item destructor
 */
DataCollectionItem::~DataCollectionItem()
{
   MemFree(m_name);
   delete m_tableColumns;
}

/**
 * Will check if object has changed. If at least one field is changed - all data will be updated and
 * saved to database.
 */
bool DataCollectionItem::updateAndSave(const DataCollectionItem *item, bool txnOpen, DB_HANDLE hdb, DataCollectionStatementSet *statements)
{
   // if at least one of fields changed - set all fields and save to DB
   if ((m_type != item->m_type) || (m_origin != item->m_origin) || _tcscmp(m_name, item->m_name) ||
       (m_pollingInterval != item->m_pollingInterval) || m_snmpTargetGuid.compare(item->m_snmpTargetGuid) ||
       (m_snmpPort != item->m_snmpPort) || (m_snmpRawValueType != item->m_snmpRawValueType) ||
       (m_lastPollTime < item->m_lastPollTime) || (m_backupProxyId != item->m_backupProxyId) ||
       (item->m_tableColumns != nullptr) || (m_tableColumns != nullptr) ||
       (item->m_scheduleType != m_scheduleType) || (item->m_schedules.size() > 0) ||
       (m_schedules.size() > 0))   // Do not do actual compare for table columns
   {
      m_type = item->m_type;
      m_origin = item->m_origin;
      MemFree(m_name);
      m_name = MemCopyString(item->m_name);
      m_pollingInterval = item->m_pollingInterval;
      if (m_lastPollTime < item->m_lastPollTime)
         m_lastPollTime = item->m_lastPollTime;
      m_snmpTargetGuid = item->m_snmpTargetGuid;
      m_snmpPort = item->m_snmpPort;
      m_snmpRawValueType = item->m_snmpRawValueType;
      m_backupProxyId = item->m_backupProxyId;

      delete m_tableColumns;
      if (item->m_tableColumns != nullptr)
      {
         m_tableColumns = new ObjectArray<SNMPTableColumnDefinition>(item->m_tableColumns->size(), 16, Ownership::True);
         for(int i = 0; i < item->m_tableColumns->size(); i++)
            m_tableColumns->add(new SNMPTableColumnDefinition(item->m_tableColumns->get(i)));
      }
      else
      {
         m_tableColumns = nullptr;
      }
      m_scheduleType = item->m_scheduleType;
      m_schedules.clear();
      m_schedules.addAll(&item->m_schedules);

      if (!txnOpen)
      {
         DBBegin(hdb);
         txnOpen = true;
      }
      saveToDatabase(false, hdb, statements);
   }
   return txnOpen;
}

/**
 * Save configuration object to database
 */
void DataCollectionItem::saveToDatabase(bool newObject, DB_HANDLE hdb, DataCollectionStatementSet *statements)
{
   nxlog_debug_tag(DEBUG_TAG, 6, _T("DataCollectionItem::saveToDatabase: %s object(serverId=") UINT64X_FMT(_T("016")) _T(",dciId=%d) saved to database"),
               newObject ? _T("new") : _T("existing"), m_serverId, m_id);

   DB_STATEMENT hStmt;
   if (newObject)
   {
      if (statements->insertItem == nullptr)
      {
         statements->insertItem = DBPrepare(hdb,
                  _T("INSERT INTO dc_config (type,origin,name,polling_interval,")
                  _T("last_poll,snmp_port,snmp_target_guid,snmp_raw_type,backup_proxy_id,")
                  _T("snmp_version,schedule_type,server_id,dci_id)")
                  _T("VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)"));
      }
      hStmt = statements->insertItem;
   }
   else
   {
      if (statements->updateItem == nullptr)
      {
         statements->updateItem = DBPrepare(hdb,
                    _T("UPDATE dc_config SET type=?,origin=?,name=?,")
                    _T("polling_interval=?,last_poll=?,snmp_port=?,")
                    _T("snmp_target_guid=?,snmp_raw_type=?,backup_proxy_id=?,")
                    _T("snmp_version=?,schedule_type=? WHERE server_id=? AND dci_id=?"));
      }
      hStmt = statements->updateItem;
   }

	if (hStmt == nullptr)
		return;

	DBBind(hStmt, 1, DB_SQLTYPE_INTEGER, (int32_t)m_type);
	DBBind(hStmt, 2, DB_SQLTYPE_INTEGER, (int32_t)m_origin);
	DBBind(hStmt, 3, DB_SQLTYPE_VARCHAR, m_name, DB_BIND_STATIC);
	DBBind(hStmt, 4, DB_SQLTYPE_INTEGER, m_pollingInterval);
	DBBind(hStmt, 5, DB_SQLTYPE_INTEGER, (uint32_t)m_lastPollTime);
	DBBind(hStmt, 6, DB_SQLTYPE_INTEGER, (int32_t)m_snmpPort);
   DBBind(hStmt, 7, DB_SQLTYPE_VARCHAR, m_snmpTargetGuid);
	DBBind(hStmt, 8, DB_SQLTYPE_INTEGER, (int32_t)m_snmpRawValueType);
   DBBind(hStmt, 9, DB_SQLTYPE_INTEGER, m_backupProxyId);
   DBBind(hStmt, 10, DB_SQLTYPE_INTEGER, (int32_t)m_snmpVersion);
   DBBind(hStmt, 11, DB_SQLTYPE_INTEGER, (int32_t)m_scheduleType);
	DBBind(hStmt, 12, DB_SQLTYPE_BIGINT, m_serverId);
	DBBind(hStmt, 13, DB_SQLTYPE_INTEGER, m_id);

   if (!DBExecute(hStmt))
      return;

   if ((m_origin == DS_SNMP_AGENT) && (m_type == DCO_TYPE_TABLE))
   {
      if (statements->deleteColumns == nullptr)
      {
         statements->deleteColumns = DBPrepare(hdb, _T("DELETE FROM dc_snmp_table_columns WHERE server_id=? AND dci_id=?"), true);
         if (statements->deleteColumns == nullptr)
            return;
      }
      DBBind(statements->deleteColumns, 1, DB_SQLTYPE_BIGINT, m_serverId);
      DBBind(statements->deleteColumns, 2, DB_SQLTYPE_INTEGER, m_id);
      if (!DBExecute(statements->deleteColumns))
         return;

      if (m_tableColumns != nullptr)
      {
         if (statements->insertColumn == nullptr)
         {
            statements->insertColumn = DBPrepare(hdb, _T("INSERT INTO dc_snmp_table_columns (server_id,dci_id,column_id,name,display_name,flags,snmp_oid) VALUES (?,?,?,?,?,?,?)"), true);
            if (statements->insertColumn == nullptr)
               return;
         }
         DBBind(statements->insertColumn, 1, DB_SQLTYPE_BIGINT, m_serverId);
         DBBind(statements->insertColumn, 2, DB_SQLTYPE_INTEGER, m_id);
         for(int i = 0; i < m_tableColumns->size(); i++)
         {
            SNMPTableColumnDefinition *c = m_tableColumns->get(i);
            DBBind(statements->insertColumn, 3, DB_SQLTYPE_INTEGER, i);
            DBBind(statements->insertColumn, 4, DB_SQLTYPE_VARCHAR, c->getName(), DB_BIND_STATIC);
            DBBind(statements->insertColumn, 5, DB_SQLTYPE_VARCHAR, c->getDisplayName(), DB_BIND_STATIC);
            DBBind(statements->insertColumn, 6, DB_SQLTYPE_INTEGER, c->getFlags());
            const SNMP_ObjectId *oid = c->getSnmpOid();
            TCHAR text[1024];
            DBBind(statements->insertColumn, 7, DB_SQLTYPE_VARCHAR, (oid != nullptr) ? oid->toString(text, 1024) : nullptr, DB_BIND_STATIC);
            DBExecute(statements->insertColumn);
         }
      }
   }

   if (m_scheduleType != ScheduleType::NONE)
   {
      if (statements->deleteSchedules == nullptr)
      {
         statements->deleteSchedules = DBPrepare(hdb, _T("DELETE FROM dc_schedules WHERE server_id=? AND dci_id=?"), true);
         if (statements->deleteSchedules == nullptr)
            return;
      }
      DBBind(statements->deleteSchedules, 1, DB_SQLTYPE_BIGINT, m_serverId);
      DBBind(statements->deleteSchedules, 2, DB_SQLTYPE_INTEGER, m_id);
      if (!DBExecute(statements->deleteSchedules))
         return;

      if (statements->insertSchedules == nullptr)
      {
         statements->insertSchedules = DBPrepare(hdb, _T("INSERT INTO dc_schedules (server_id,dci_id,schedule) VALUES (?,?,?)"), true);
         if (statements->insertSchedules == nullptr)
            return;
      }

      DBBind(statements->insertSchedules, 1, DB_SQLTYPE_BIGINT, m_serverId);
      DBBind(statements->insertSchedules, 2, DB_SQLTYPE_INTEGER, m_id);
      for(int i = 0; i < m_schedules.size(); i++)
      {
         DBBind(statements->insertSchedules, 3, DB_SQLTYPE_VARCHAR, m_schedules.get(i), DB_BIND_STATIC);
         DBExecute(statements->insertSchedules);
      }
   }
}

/**
 * Remove item form database
 */
void DataCollectionItem::deleteFromDatabase(DB_HANDLE hdb, DataCollectionStatementSet *statements)
{
   if (statements->deleteItem == nullptr)
   {
      statements->deleteItem = DBPrepare(hdb, _T("DELETE FROM dc_config WHERE server_id=? AND dci_id=?"), true);
      if (statements->deleteItem == nullptr)
         return;
   }
   DBBind(statements->deleteItem, 1, DB_SQLTYPE_BIGINT, m_serverId);
   DBBind(statements->deleteItem, 2, DB_SQLTYPE_INTEGER, m_id);

   if (statements->deleteColumns == nullptr)
   {
      statements->deleteColumns = DBPrepare(hdb, _T("DELETE FROM dc_snmp_table_columns WHERE server_id=? AND dci_id=?"), true);
      if (statements->deleteColumns == nullptr)
         return;
   }
   DBBind(statements->deleteColumns, 1, DB_SQLTYPE_BIGINT, m_serverId);
   DBBind(statements->deleteColumns, 2, DB_SQLTYPE_INTEGER, m_id);

   if (statements->deleteSchedules == nullptr)
   {
      statements->deleteSchedules = DBPrepare(hdb, _T("DELETE FROM dc_schedules WHERE server_id=? AND dci_id=?"), true);
      if (statements->deleteSchedules == nullptr)
         return;
   }
   DBBind(statements->deleteSchedules, 1, DB_SQLTYPE_BIGINT, m_serverId);
   DBBind(statements->deleteSchedules, 2, DB_SQLTYPE_INTEGER, m_id);

   if (DBExecute(statements->deleteItem) && DBExecute(statements->deleteColumns) && DBExecute(statements->deleteSchedules))
   {
      nxlog_debug_tag(DEBUG_TAG, 6, _T("DataCollectionItem::deleteFromDatabase: object(serverId=") UINT64X_FMT(_T("016")) _T(",dciId=%d) removed from database"), m_serverId, m_id);
   }
}

/**
 * Set last poll time for item
 */
void DataCollectionItem::setLastPollTime(time_t time)
{
   m_lastPollTime = time;
   s_pollTimeChanged = true;
}

/**
 * Collected data
 */
class DataElement
{
private:
   UINT64 m_serverId;
   UINT32 m_dciId;
   time_t m_timestamp;
   int m_origin;
   int m_type;
   UINT32 m_statusCode;
   uuid m_snmpNode;
   union
   {
      TCHAR *item;
      StringList *list;
      Table *table;
   } m_value;

public:
   DataElement(DataCollectionItem *dci, const TCHAR *value, UINT32 status)
   {
      m_serverId = dci->getServerId();
      m_dciId = dci->getId();
      m_timestamp = time(nullptr);
      m_origin = dci->getOrigin();
      m_type = DCO_TYPE_ITEM;
      m_statusCode = status;
      m_snmpNode = dci->getSnmpTargetGuid();
      m_value.item = MemCopyString(value);
   }

   DataElement(DataCollectionItem *dci, StringList *value, UINT32 status)
   {
      m_serverId = dci->getServerId();
      m_dciId = dci->getId();
      m_timestamp = time(nullptr);
      m_origin = dci->getOrigin();
      m_type = DCO_TYPE_LIST;
      m_statusCode = status;
      m_snmpNode = dci->getSnmpTargetGuid();
      m_value.list = value;
   }

   DataElement(DataCollectionItem *dci, Table *value, UINT32 status)
   {
      m_serverId = dci->getServerId();
      m_dciId = dci->getId();
      m_timestamp = time(nullptr);
      m_origin = dci->getOrigin();
      m_type = DCO_TYPE_TABLE;
      m_statusCode = status;
      m_snmpNode = dci->getSnmpTargetGuid();
      m_value.table = value;
   }

   /**
    * Create data element from database record
    * Expected field order: server_id,dci_id,dci_type,dci_origin,status_code,snmp_target_guid,timestamp,value
    */
   DataElement(DB_RESULT hResult, int row)
   {
      m_serverId = DBGetFieldUInt64(hResult, row, 0);
      m_dciId = DBGetFieldULong(hResult, row, 1);
      m_timestamp = (time_t)DBGetFieldInt64(hResult, row, 6);
      m_origin = DBGetFieldLong(hResult, row, 3);
      m_type = DBGetFieldLong(hResult, row, 2);
      m_statusCode = DBGetFieldLong(hResult, row, 4);
      m_snmpNode = DBGetFieldGUID(hResult, row, 5);
      switch(m_type)
      {
         case DCO_TYPE_ITEM:
            m_value.item = DBGetField(hResult, row, 7, NULL, 0);
            break;
         case DCO_TYPE_LIST:
            {
               m_value.list = new StringList();
               TCHAR *text = DBGetField(hResult, row, 7, NULL, 0);
               if (text != nullptr)
               {
                  m_value.list->splitAndAdd(text, _T("\n"));
                  MemFree(text);
               }
            }
            break;
         case DCO_TYPE_TABLE:
            {
               char *xml = DBGetFieldUTF8(hResult, row, 7, NULL, 0);
               if (xml != nullptr)
               {
                  m_value.table = Table::createFromXML(xml);
                  MemFree(xml);
               }
               else
               {
                  m_value.table = nullptr;
               }
            }
            break;
         default:
            m_type = DCO_TYPE_ITEM;
            m_value.item = MemCopyString(_T(""));
            break;
      }
   }

   ~DataElement()
   {
      switch(m_type)
      {
         case DCO_TYPE_ITEM:
            MemFree(m_value.item);
            break;
         case DCO_TYPE_LIST:
            delete m_value.list;
            break;
         case DCO_TYPE_TABLE:
            delete m_value.table;
            break;
      }
   }

   time_t getTimestamp() { return m_timestamp; }
   UINT64 getServerId() { return m_serverId; }
   UINT32 getDciId() { return m_dciId; }
   int getType() { return m_type; }
   UINT32 getStatusCode() { return m_statusCode; }

   void saveToDatabase(DB_STATEMENT hStmt);
   bool sendToServer(bool reconcillation);
   void fillReconciliationMessage(NXCPMessage *msg, UINT32 baseId);
};

/**
 * Save data element to database
 */
void DataElement::saveToDatabase(DB_STATEMENT hStmt)
{
   DBBind(hStmt, 1, DB_SQLTYPE_BIGINT, m_serverId);
   DBBind(hStmt, 2, DB_SQLTYPE_INTEGER, (LONG)m_dciId);
   DBBind(hStmt, 3, DB_SQLTYPE_INTEGER, (LONG)m_type);
   DBBind(hStmt, 4, DB_SQLTYPE_INTEGER, (LONG)m_origin);
   DBBind(hStmt, 5, DB_SQLTYPE_INTEGER, (LONG)m_statusCode);
   DBBind(hStmt, 6, DB_SQLTYPE_VARCHAR, m_snmpNode);
   DBBind(hStmt, 7, DB_SQLTYPE_INTEGER, (LONG)m_timestamp);
   switch(m_type)
   {
      case DCO_TYPE_ITEM:
         DBBind(hStmt, 8, DB_SQLTYPE_TEXT, m_value.item, DB_BIND_STATIC);
         break;
      case DCO_TYPE_LIST:
         DBBind(hStmt, 8, DB_SQLTYPE_TEXT, m_value.list->join(_T("\n")), DB_BIND_DYNAMIC);
         break;
      case DCO_TYPE_TABLE:
         DBBind(hStmt, 8, DB_SQLTYPE_TEXT, m_value.table->createXML(), DB_BIND_DYNAMIC);
         break;
   }
   DBExecute(hStmt);
}

/**
 * Session comparator
 */
static bool SessionComparator_Sender(AbstractCommSession *session, void *data)
{
   return (session->getServerId() == *static_cast<UINT64*>(data)) && session->canAcceptData();
}

/**
 * Send collected data to server
 */
bool DataElement::sendToServer(bool reconciliation)
{
   // If data in database was invalid table may not be parsed correctly
   // Consider sending a success in that case so data element can be dropped
   if ((m_type == DCO_TYPE_TABLE) && (m_value.table == nullptr))
      return true;

   CommSession *session = (CommSession *)FindServerSession(SessionComparator_Sender, &m_serverId);
   if (session == NULL)
      return false;

   NXCPMessage msg(CMD_DCI_DATA, session->generateRequestId(), session->getProtocolVersion());
   msg.setField(VID_DCI_ID, m_dciId);
   msg.setField(VID_DCI_SOURCE_TYPE, (INT16)m_origin);
   msg.setField(VID_DCOBJECT_TYPE, (INT16)m_type);
   msg.setField(VID_STATUS, m_statusCode);
   msg.setField(VID_NODE_ID, m_snmpNode);
   msg.setFieldFromTime(VID_TIMESTAMP, m_timestamp);
   msg.setField(VID_RECONCILIATION, (INT16)(reconciliation ? 1 : 0));
   switch(m_type)
   {
      case DCO_TYPE_ITEM:
         msg.setField(VID_VALUE, m_value.item);
         break;
      case DCO_TYPE_LIST:
         m_value.list->fillMessage(&msg, VID_ENUM_VALUE_BASE, VID_NUM_STRINGS);
         break;
      case DCO_TYPE_TABLE:
         m_value.table->setSource(m_origin);
         m_value.table->fillMessage(msg, 0, -1);
         break;
   }
   UINT32 rcc = session->doRequest(&msg, 2000);
   session->decRefCount();

   // consider internal error as success because it means that server
   // cannot accept data for some reason and retry is not feasible
   return (rcc == ERR_SUCCESS) || (rcc == ERR_INTERNAL_ERROR);
}

/**
 * Fill bulk reconciliation message with DCI data
 */
void DataElement::fillReconciliationMessage(NXCPMessage *msg, UINT32 baseId)
{
   msg->setField(baseId, m_dciId);
   msg->setField(baseId + 1, (INT16)m_origin);
   msg->setField(baseId + 2, (INT16)m_type);
   msg->setField(baseId + 3, m_snmpNode);
   msg->setFieldFromTime(baseId + 4, m_timestamp);
   msg->setField(baseId + 5, m_value.item);
   msg->setField(baseId + 6, m_statusCode);
}

/**
 * Server data sync status object
 */
struct ServerSyncStatus
{
   UINT64 serverId;
   INT32 queueSize;
   time_t lastSync;

   ServerSyncStatus(UINT64 sid)
   {
      serverId = sid;
      queueSize = 0;
      lastSync = time(NULL);
   }
};

/**
 * Server sync status information
 */
static HashMap<UINT64, ServerSyncStatus> s_serverSyncStatus(Ownership::True);
static Mutex s_serverSyncStatusLock;

/**
 * Database writer queue
 */
static ObjectQueue<DataElement> s_databaseWriterQueue;

/**
 * Database writer
 */
static THREAD_RESULT THREAD_CALL DatabaseWriter(void *arg)
{
   DB_HANDLE hdb = GetLocalDatabaseHandle();
   nxlog_debug_tag(DEBUG_TAG, 1, _T("Database writer thread started"));

   while(true)
   {
      DataElement *e = s_databaseWriterQueue.getOrBlock();
      if (e == INVALID_POINTER_VALUE)
         break;

      DB_STATEMENT hStmt = DBPrepare(hdb, _T("INSERT INTO dc_queue (server_id,dci_id,dci_type,dci_origin,status_code,snmp_target_guid,timestamp,value) VALUES (?,?,?,?,?,?,?,?)"));
      if (hStmt == NULL)
      {
         delete e;
         continue;
      }

      UINT32 count = 0;
      DBBegin(hdb);
      while((e != NULL) && (e != INVALID_POINTER_VALUE))
      {
         e->saveToDatabase(hStmt);
         delete e;

         count++;
         if (count == g_dcWriterMaxTransactionSize)
            break;

         e = s_databaseWriterQueue.get();
      }
      DBCommit(hdb);
      DBFreeStatement(hStmt);
      nxlog_debug_tag(DEBUG_TAG, 7, _T("Database writer: %u records inserted"), count);
      if (e == INVALID_POINTER_VALUE)
         break;

      // Sleep only if queue was emptied
      if (count < g_dcWriterMaxTransactionSize)
         ThreadSleepMs(g_dcWriterFlushInterval);
   }

   nxlog_debug_tag(DEBUG_TAG, 1, _T("Database writer thread stopped"));
   return THREAD_OK;
}

/**
 * List of all data collection items
 */
static HashMap<ServerObjectKey, DataCollectionItem> s_items(Ownership::True);
static Mutex s_itemLock;

/**
 * Session comparator
 */
static bool SessionComparator_Reconciliation(AbstractCommSession *session, void *data)
{
   if ((session->getServerId() == 0) || !session->canAcceptData())
      return false;
   ServerSyncStatus *s = s_serverSyncStatus.get(session->getServerId());
   if ((s != NULL) && (s->queueSize > 0))
   {
      return true;
   }
   return false;
}

/**
 * Data reconciliation thread
 */
static THREAD_RESULT THREAD_CALL ReconciliationThread(void *arg)
{
   DB_HANDLE hdb = GetLocalDatabaseHandle();
   UINT32 sleepTime = 30000;
   nxlog_debug_tag(DEBUG_TAG, 1, _T("Data reconciliation thread started (block size %d, timeout %d ms)"), g_dcReconciliationBlockSize, g_dcReconciliationTimeout);

   bool vacuumNeeded = false;
   while(!AgentSleepAndCheckForShutdown(sleepTime))
   {
      // Check if there is something to sync
      s_serverSyncStatusLock.lock();
      CommSession *session = static_cast<CommSession*>(FindServerSession(SessionComparator_Reconciliation, NULL));
      s_serverSyncStatusLock.unlock();
      if (session == NULL)
      {
         // Save last poll times when reconciliation thread is idle
         s_itemLock.lock();
         if (s_pollTimeChanged)
         {
            DBBegin(hdb);
            TCHAR query[256];
            Iterator<DataCollectionItem> *it = s_items.iterator();
            while(it->hasNext())
            {
               DataCollectionItem *dci = it->next();
               _sntprintf(query, 256, _T("UPDATE dc_config SET last_poll=") UINT64_FMT _T(" WHERE server_id=") UINT64_FMT _T(" AND dci_id=%d"),
                          (UINT64)dci->getLastPollTime(), (UINT64)dci->getServerId(), dci->getId());
               DBQuery(hdb, query);
            }
            DBCommit(hdb);
            delete it;
            s_pollTimeChanged = false;
         }
         s_itemLock.unlock();

         if (vacuumNeeded)
         {
            nxlog_debug_tag(DEBUG_TAG, 4, _T("ReconciliationThread: vacuum local database"));
            DBQuery(hdb, _T("VACUUM"));
            vacuumNeeded = false;
         }
         sleepTime = 30000;
         continue;
      }

      TCHAR query[1024];
      _sntprintf(query, 1024, _T("SELECT server_id,dci_id,dci_type,dci_origin,status_code,snmp_target_guid,timestamp,value FROM dc_queue INDEXED BY idx_dc_queue_timestamp WHERE server_id=") UINT64_FMT _T(" ORDER BY timestamp LIMIT %d"), session->getServerId(), g_dcReconciliationBlockSize);

      TCHAR sqlError[DBDRV_MAX_ERROR_TEXT];
      DB_RESULT hResult = DBSelectEx(hdb, query, sqlError);
      if (hResult == NULL)
      {
         nxlog_debug_tag(DEBUG_TAG, 4, _T("ReconciliationThread: database query failed: %s"), sqlError);
         sleepTime = 30000;
         session->decRefCount();
         continue;
      }

      int count = DBGetNumRows(hResult);
      if (count > 0)
      {
         ObjectArray<DataElement> bulkSendList(count, 10, Ownership::True);
         ObjectArray<DataElement> deleteList(count, 10, Ownership::True);
         for(int i = 0; i < count; i++)
         {
            DataElement *e = new DataElement(hResult, i);
            if ((e->getType() == DCO_TYPE_ITEM) && session->isBulkReconciliationSupported())
            {
               bulkSendList.add(e);
            }
            else
            {
               s_serverSyncStatusLock.lock();
               ServerSyncStatus *status = s_serverSyncStatus.get(e->getServerId());
               if (status != NULL)
               {
                  if (e->sendToServer(true))
                  {
                     status->queueSize--;
                     status->lastSync = time(NULL);
                     deleteList.add(e);
                  }
                  else
                  {
                     delete e;
                  }
               }
               else
               {
                  nxlog_debug_tag(DEBUG_TAG, 5, _T("INTERNAL ERROR: cached DCI value without server sync status object"));
                  deleteList.add(e);  // record should be deleted
               }
               s_serverSyncStatusLock.unlock();
            }
         }

         if (bulkSendList.size() > 0)
         {
            nxlog_debug_tag(DEBUG_TAG, 6, _T("ReconciliationThread: %d records to be sent in bulk mode"), bulkSendList.size());

            NXCPMessage msg(CMD_DCI_DATA, session->generateRequestId(), session->getProtocolVersion());
            msg.setField(VID_BULK_RECONCILIATION, (INT16)1);
            msg.setField(VID_NUM_ELEMENTS, (INT16)bulkSendList.size());
            msg.setField(VID_TIMEOUT, g_dcReconciliationTimeout);

            UINT32 fieldId = VID_ELEMENT_LIST_BASE;
            for(int i = 0; i < bulkSendList.size(); i++)
            {
               bulkSendList.get(i)->fillReconciliationMessage(&msg, fieldId);
               fieldId += 10;
            }

            if (session->sendMessage(&msg))
            {
               UINT32 rcc;
               do
               {
                  NXCPMessage *response = session->waitForMessage(CMD_REQUEST_COMPLETED, msg.getId(), g_dcReconciliationTimeout);
                  if (response != NULL)
                  {
                     rcc = response->getFieldAsUInt32(VID_RCC);
                     if (rcc == ERR_SUCCESS)
                     {
                        s_serverSyncStatusLock.lock();
                        ServerSyncStatus *serverSyncStatus = s_serverSyncStatus.get(session->getServerId());

                        // Check status for each data element
                        BYTE status[MAX_BULK_DATA_BLOCK_SIZE];
                        memset(status, 0, MAX_BULK_DATA_BLOCK_SIZE);
                        response->getFieldAsBinary(VID_STATUS, status, MAX_BULK_DATA_BLOCK_SIZE);
                        bulkSendList.setOwner(Ownership::False);
                        for(int i = 0; i < bulkSendList.size(); i++)
                        {
                           DataElement *e = bulkSendList.get(i);
                           if (status[i] != BULK_DATA_REC_RETRY)
                           {
                              deleteList.add(e);
                              serverSyncStatus->queueSize--;
                           }
                           else
                           {
                              delete e;
                           }
                        }
                        serverSyncStatus->lastSync = time(NULL);

                        s_serverSyncStatusLock.unlock();
                     }
                     else if (rcc == ERR_PROCESSING)
                     {
                        nxlog_debug_tag(DEBUG_TAG, 4, _T("ReconciliationThread: server is processing data (%d%% completed)"), response->getFieldAsInt32(VID_PROGRESS));
                     }
                     else
                     {
                        nxlog_debug_tag(DEBUG_TAG, 4, _T("ReconciliationThread: bulk send failed (%d)"), rcc);
                     }
                     delete response;
                  }
                  else
                  {
                     nxlog_debug_tag(DEBUG_TAG, 4, _T("ReconciliationThread: timeout on bulk send"));
                     rcc = ERR_REQUEST_TIMEOUT;
                  }
               } while(rcc == ERR_PROCESSING);
            }
            else
            {
               nxlog_debug_tag(DEBUG_TAG, 4, _T("ReconciliationThread: communication error"));
            }
         }

         if (deleteList.size() > 0)
         {
            DB_STATEMENT hStmt = DBPrepare(hdb, _T("DELETE FROM dc_queue WHERE server_id=? AND dci_id=? AND timestamp=?"), true);
            if (hStmt != NULL)
            {
               DBBegin(hdb);
               for(int i = 0; i < deleteList.size(); i++)
               {
                  DataElement *e = deleteList.get(i);
                  DBBind(hStmt, 1, DB_SQLTYPE_BIGINT, e->getServerId());
                  DBBind(hStmt, 2, DB_SQLTYPE_INTEGER, e->getDciId());
                  DBBind(hStmt, 3, DB_SQLTYPE_BIGINT, static_cast<INT64>(e->getTimestamp()));
                  DBExecute(hStmt);
               }
               DBCommit(hdb);
               DBFreeStatement(hStmt);
               vacuumNeeded = true;
            }
            nxlog_debug_tag(DEBUG_TAG, 4, _T("ReconciliationThread: %d records sent"), deleteList.size());
         }
      }
      DBFreeResult(hResult);

      session->decRefCount();
      sleepTime = (count > 0) ? 50 : 30000;
   }

   nxlog_debug_tag(DEBUG_TAG, 1, _T("Data reconciliation thread stopped"));
   return THREAD_OK;
}

/**
 * Data sender queue
 */
static Queue s_dataSenderQueue;

/**
 * Data sender
 */
static THREAD_RESULT THREAD_CALL DataSender(void *arg)
{
   nxlog_debug_tag(DEBUG_TAG, 1, _T("Data sender thread started"));
   while(true)
   {
      DataElement *e = static_cast<DataElement*>(s_dataSenderQueue.getOrBlock());
      if (e == INVALID_POINTER_VALUE)
         break;

      s_serverSyncStatusLock.lock();
      ServerSyncStatus *status = s_serverSyncStatus.get(e->getServerId());
      if (status == NULL)
      {
         status = new ServerSyncStatus(e->getServerId());
         s_serverSyncStatus.set(e->getServerId(), status);
      }

      if (status->queueSize == 0)
      {
         if (!e->sendToServer(false))
         {
            status->queueSize++;
            s_databaseWriterQueue.put(e);
            e = NULL;
         }
      }
      else
      {
         status->queueSize++;
         s_databaseWriterQueue.put(e);
         e = NULL;
      }
      s_serverSyncStatusLock.unlock();

      delete e;
   }
   nxlog_debug_tag(DEBUG_TAG, 1, _T("Data sender thread stopped"));
   return THREAD_OK;
}

/**
 * Collect data from agent
 */
static DataElement *CollectDataFromAgent(DataCollectionItem *dci)
{
   VirtualSession session(dci->getServerId());

   DataElement *e = NULL;
   uint32_t status;
   if (dci->getType() == DCO_TYPE_ITEM)
   {
      TCHAR value[MAX_RESULT_LENGTH];
      status = GetParameterValue(dci->getName(), value, &session);
      e = new DataElement(dci, (status == ERR_SUCCESS) ? value : _T(""), status);
   }
   else if (dci->getType() == DCO_TYPE_LIST)
   {
      StringList *value = new StringList();
      status = GetListValue(dci->getName(), value, &session);
      e = new DataElement(dci, value, status);
   }
   else if (dci->getType() == DCO_TYPE_TABLE)
   {
      Table *value = new Table();
      status = GetTableValue(dci->getName(), value, &session);
      e = new DataElement(dci, value, status);
   }

   return e;
}

/**
 * Collect data from SNMP
 */
static DataElement *CollectDataFromSNMP(DataCollectionItem *dci)
{
   DataElement *e = nullptr;
   if (dci->getType() == DCO_TYPE_ITEM)
   {
      nxlog_debug_tag(DEBUG_TAG, 8, _T("Read SNMP parameter %s"), dci->getName());

      TCHAR value[MAX_RESULT_LENGTH];
      uint32_t status = GetSnmpValue(dci->getSnmpTargetGuid(), dci->getSnmpPort(), dci->getSnmpVersion(), dci->getName(), value, dci->getSnmpRawValueType());
      e = new DataElement(dci, status == ERR_SUCCESS ? value : _T(""), status);
   }
   else if (dci->getType() == DCO_TYPE_TABLE)
   {
      nxlog_debug_tag(DEBUG_TAG, 8, _T("Read SNMP table %s"), dci->getName());
      const ObjectArray<SNMPTableColumnDefinition> *columns = dci->getColumns();
      if (columns != nullptr)
      {
         Table *value = new Table();
         uint32_t status = GetSnmpTable(dci->getSnmpTargetGuid(), dci->getSnmpPort(), dci->getSnmpVersion(), dci->getName(), *columns, value);
         e = new DataElement(dci, value, status);
      }
      else
      {
         nxlog_debug_tag(DEBUG_TAG, 8, _T("Missing column definition for SNMP table %s"), dci->getName());
      }
   }
   return e;
}

/**
 * Local data collection callback
 */
static void LocalDataCollectionCallback(DataCollectionItem *dci)
{
   DataElement *e = CollectDataFromAgent(dci);
   if (e != nullptr)
   {
      s_dataSenderQueue.put(e);
   }
   else
   {
      nxlog_debug_tag(DEBUG_TAG, 6, _T("DataCollector: collection error for DCI %d \"%s\""), dci->getId(), dci->getName());
   }

   dci->setLastPollTime(time(NULL));
   dci->finishDataCollection();
}

/**
 * SNMP data collection callback
 */
static void SnmpDataCollectionCallback(DataCollectionItem *dci)
{
   DataElement *e = CollectDataFromSNMP(dci);
   if (e != nullptr)
   {
      s_dataSenderQueue.put(e);
   }
   else
   {
      nxlog_debug_tag(DEBUG_TAG, 6, _T("DataCollector: collection error for DCI %d \"%s\""), dci->getId(), dci->getName());
   }

   dci->setLastPollTime(time(NULL));
   dci->finishDataCollection();
}

/**
 * Data collectors thread pool
 */
ThreadPool *g_dataCollectorPool = nullptr;

/**
 * Single data collection scheduler run - schedule data collection if needed and calculate sleep time
 */
static UINT32 DataCollectionSchedulerRun()
{
   UINT32 sleepTime = 60;

   s_itemLock.lock();
   time_t now = time(nullptr);
   Iterator<DataCollectionItem> *it = s_items.iterator();
   while(it->hasNext())
   {
      DataCollectionItem *dci = it->next();
      UINT32 timeToPoll = dci->getTimeToNextPoll(now);
      if (timeToPoll == 0)
      {
         bool schedule;
         if (dci->getBackupProxyId() == 0)
         {
            schedule = true;
         }
         else
         {
            g_proxyListMutex.lock();
            DataCollectionProxy *proxy = g_proxyList.get(ServerObjectKey(dci->getServerId(), dci->getBackupProxyId()));
            schedule = ((proxy != NULL) && !proxy->isConnected());
            g_proxyListMutex.unlock();
         }

         if (schedule)
         {
            nxlog_debug_tag(DEBUG_TAG, 7, _T("DataCollector: polling DCI %d \"%s\""), dci->getId(), dci->getName());

            if (dci->getOrigin() == DS_NATIVE_AGENT)
            {
               dci->startDataCollection();
               ThreadPoolExecute(g_dataCollectorPool, LocalDataCollectionCallback, dci);
            }
            else if (dci->getOrigin() == DS_SNMP_AGENT)
            {
               dci->startDataCollection();
               TCHAR key[64];
               ThreadPoolExecuteSerialized(g_dataCollectorPool, dci->getSnmpTargetGuid().toString(key), SnmpDataCollectionCallback, dci);
            }
            else
            {
               nxlog_debug_tag(DEBUG_TAG, 7, _T("DataCollector: unsupported origin %d"), dci->getOrigin());
               dci->setLastPollTime(time(NULL));
            }
         }

         timeToPoll = dci->getPollingInterval();
      }
      if (sleepTime > timeToPoll)
         sleepTime = timeToPoll;
   }
   delete it;
   s_itemLock.unlock();
   return sleepTime;
}

/**
 * Data collection scheduler thread
 */
static THREAD_RESULT THREAD_CALL DataCollectionScheduler(void *arg)
{
   nxlog_debug_tag(DEBUG_TAG, 1, _T("Data collection scheduler thread started"));

   UINT32 sleepTime = DataCollectionSchedulerRun();
   while(!AgentSleepAndCheckForShutdown(sleepTime * 1000))
   {
      sleepTime = DataCollectionSchedulerRun();
      nxlog_debug_tag(DEBUG_TAG, 7, _T("DataCollector: sleeping for %d seconds"), sleepTime);
   }

   ThreadPoolDestroy(g_dataCollectorPool);
   nxlog_debug_tag(DEBUG_TAG, 1, _T("Data collection scheduler thread stopped"));
   return THREAD_OK;
}

/**
 * Configure data collection
 */
void ConfigureDataCollection(uint64_t serverId, NXCPMessage *msg)
{
   s_itemLock.lock();
   if (!s_dataCollectorStarted)
   {
      s_itemLock.unlock();
      nxlog_debug_tag(DEBUG_TAG, 1, _T("Local data collector was not started, ignoring configuration received from server ") UINT64X_FMT(_T("016")), serverId);
      return;
   }
   s_itemLock.unlock();

   DB_HANDLE hdb = GetLocalDatabaseHandle();

   int count = msg->getFieldAsInt32(VID_NUM_NODES);
   if (count > 0)
   {
      DBBegin(hdb);
      uint32_t fieldId = VID_NODE_INFO_LIST_BASE;
      for(int i = 0; i < count; i++)
      {
         shared_ptr<SNMPTarget> target = make_shared<SNMPTarget>(serverId, msg, fieldId);
         UpdateSnmpTarget(target);
         target->saveToDatabase(hdb);
         fieldId += 50;
      }
      DBCommit(hdb);
   }
   nxlog_debug_tag(DEBUG_TAG, 4, _T("%d SNMP targets received from server ") UINT64X_FMT(_T("016")), count, serverId);

   // Read proxy list
   HashMap<ServerObjectKey, DataCollectionProxy> *proxyList = new HashMap<ServerObjectKey, DataCollectionProxy>(Ownership::True);
   count = msg->getFieldAsInt32(VID_ZONE_PROXY_COUNT);
   if (count > 0)
   {
      uint32_t fieldId = VID_ZONE_PROXY_BASE;
      for(int i = 0; i < count; i++)
      {
         uint32_t proxyId = msg->getFieldAsInt32(fieldId);
         proxyList->set(ServerObjectKey(serverId, proxyId),
               new DataCollectionProxy(serverId, proxyId, msg->getFieldAsInetAddress(fieldId + 1)));
         fieldId += 10;
      }
   }
   nxlog_debug_tag(DEBUG_TAG, 4, _T("%d proxies received from server ") UINT64X_FMT(_T("016")), count, serverId);

   // Read data collection items
   HashMap<ServerObjectKey, DataCollectionItem> config(Ownership::True);
   bool hasExtraData = msg->getFieldAsBoolean(VID_EXTENDED_DCI_DATA);
   count = msg->getFieldAsInt32(VID_NUM_ELEMENTS);
   uint32_t fieldId = VID_ELEMENT_LIST_BASE;
   uint32_t extFieldId = VID_EXTRA_DCI_INFO_BASE;
   for(int i = 0; i < count; i++)
   {
      DataCollectionItem *dci = new DataCollectionItem(serverId, msg, fieldId, extFieldId, hasExtraData);
      config.set(dci->getKey(), dci);
      fieldId += 10;
      extFieldId += 1100;
   }
   nxlog_debug_tag(DEBUG_TAG, 4, _T("%d data collection elements received from server ") UINT64X_FMT(_T("016")) _T(" (extended data: %s)"),
            count, serverId, hasExtraData ? _T("YES") : _T("NO"));

   bool txnOpen = false;
   DataCollectionStatementSet statements;
   memset(&statements, 0, sizeof(statements));

   s_itemLock.lock();

   // Update and add new
   Iterator<DataCollectionItem> *it = config.iterator();
   while(it->hasNext())
   {
      DataCollectionItem *item = it->next();
      DataCollectionItem *existingItem = s_items.get(item->getKey());
      if (existingItem != nullptr)
      {
         txnOpen = existingItem->updateAndSave(item, txnOpen, hdb, &statements);
      }
      else
      {
         DataCollectionItem *newItem = new DataCollectionItem(item);
         s_items.set(newItem->getKey(), newItem);
         if (!txnOpen)
         {
            DBBegin(hdb);
            txnOpen = true;
         }
         newItem->saveToDatabase(true, hdb, &statements);
      }

      if (item->getBackupProxyId() != 0)
      {
         DataCollectionProxy *proxy = proxyList->get(ServerObjectKey(serverId, item->getBackupProxyId()));
         if (proxy != nullptr)
         {
            proxy->setInUse(true);
         }
         else
         {
            nxlog_debug_tag(DEBUG_TAG, 4, _T("No proxy found for ServerID=") UINT64X_FMT(_T("016")) _T(", ProxyID=%u, ItemID=%u"),
                  serverId, item->getBackupProxyId(), item->getId());
         }
      }
   }
   delete it;

   // Remove non-existing configuration
   it = s_items.iterator();
   while(it->hasNext())
   {
      DataCollectionItem *item = it->next();
      if (item->getServerId() != serverId)
         continue;

      if (!config.contains(item->getKey()))
      {
         if (!txnOpen)
         {
            DBBegin(hdb);
            txnOpen = true;
         }
         item->deleteFromDatabase(hdb, &statements);
         it->unlink();
         item->decRefCount();
      }
   }
   delete it;

   if (txnOpen)
      DBCommit(hdb);

   if (statements.insertItem != nullptr)
      DBFreeStatement(statements.insertItem);
   if (statements.updateItem != nullptr)
      DBFreeStatement(statements.updateItem);
   if (statements.deleteItem != nullptr)
      DBFreeStatement(statements.deleteItem);
   if (statements.insertColumn != nullptr)
      DBFreeStatement(statements.insertColumn);
   if (statements.deleteColumns != nullptr)
      DBFreeStatement(statements.deleteColumns);
   if (statements.insertSchedules != nullptr)
      DBFreeStatement(statements.insertSchedules);
   if (statements.deleteSchedules != nullptr)
      DBFreeStatement(statements.deleteSchedules);

   s_itemLock.unlock();

   if (msg->isFieldExist(VID_THIS_PROXY_ID))
   {
      // FIXME: delete configuration if not set?
      BYTE sharedSecret[ZONE_PROXY_KEY_LENGTH];
      msg->getFieldAsBinary(VID_SHARED_SECRET, sharedSecret, ZONE_PROXY_KEY_LENGTH);
      ZoneConfiguration cfg(serverId, msg->getFieldAsUInt32(VID_THIS_PROXY_ID), msg->getFieldAsUInt32(VID_ZONE_UIN), sharedSecret);
      UpdateProxyConfiguration(serverId, proxyList, &cfg);
   }
   delete proxyList;

   nxlog_debug_tag(DEBUG_TAG, 4, _T("Data collection for server ") UINT64X_FMT(_T("016")) _T(" reconfigured"), serverId);
}

/**
 * Load saved state of local data collection
 */
static void LoadState()
{
   DB_HANDLE hdb = GetLocalDatabaseHandle();
   DB_RESULT hResult = DBSelect(hdb, _T("SELECT server_id,dci_id,type,origin,name,polling_interval,last_poll,snmp_port,snmp_target_guid,snmp_raw_type,backup_proxy_id,snmp_version,schedule_type FROM dc_config"));
   if (hResult != NULL)
   {
      int count = DBGetNumRows(hResult);
      for(int i = 0; i < count; i++)
      {
         DataCollectionItem *dci = new DataCollectionItem(hResult, i);
         s_items.set(dci->getKey(), dci);
      }
      DBFreeResult(hResult);
   }

   hResult = DBSelect(hdb, _T("SELECT guid,server_id,ip_address,snmp_version,port,auth_type,enc_type,auth_name,auth_pass,enc_pass FROM dc_snmp_targets"));
   if (hResult != NULL)
   {
      int count = DBGetNumRows(hResult);
      for(int i = 0; i < count; i++)
      {
         UpdateSnmpTarget(make_shared<SNMPTarget>(hResult, i));
      }
      DBFreeResult(hResult);
   }

   hResult = DBSelect(hdb, _T("SELECT server_id,count(*),coalesce(min(timestamp),0) FROM dc_queue GROUP BY server_id"));
   if (hResult != NULL)
   {
      int count = DBGetNumRows(hResult);
      for(int i = 0; i < count; i++)
      {
         UINT64 serverId = DBGetFieldUInt64(hResult, i, 0);
         ServerSyncStatus *s = new ServerSyncStatus(serverId);
         s->queueSize = DBGetFieldLong(hResult, i, 1);
         s->lastSync = static_cast<time_t>(DBGetFieldInt64(hResult, i, 2));
         s_serverSyncStatus.set(serverId, s);
         nxlog_debug_tag(DEBUG_TAG, 2, _T("%d elements in queue for server ID ") UINT64X_FMT(_T("016")), s->queueSize, serverId);

         TCHAR ts[64];
         nxlog_debug_tag(DEBUG_TAG, 2, _T("Oldest timestamp is %s for server ID ") UINT64X_FMT(_T("016")), FormatTimestamp(s->lastSync, ts), serverId);
      }
      DBFreeResult(hResult);
   }

   LoadProxyConfiguration();
}

/**
 * Clear stalled offline data
 */
static void ClearStalledOfflineData(void *arg)
{
   if (g_dwFlags & AF_SHUTDOWN)
      return;

   IntegerArray<UINT64> deleteList;

   s_serverSyncStatusLock.lock();
   time_t expirationTime = time(NULL) - g_dcOfflineExpirationTime * 86400;
   Iterator<ServerSyncStatus> *it = s_serverSyncStatus.iterator();
   while(it->hasNext())
   {
      ServerSyncStatus *status = it->next();
      if ((status->queueSize > 0) && (status->lastSync < expirationTime))
      {
         nxlog_debug_tag(DEBUG_TAG, 2, _T("Offline data for server ID ") UINT64X_FMT(_T("016")) _T(" expired"), status->serverId);
         deleteList.add(status->serverId);
         it->remove();
      }
   }
   delete it;
   s_serverSyncStatusLock.unlock();

   if (deleteList.size() > 0)
   {
      TCHAR query[256];
      DB_HANDLE hdb = GetLocalDatabaseHandle();

      for(int i = 0; i < deleteList.size(); i++)
      {
         UINT64 serverId = deleteList.get(i);

         DBBegin(hdb);

         _sntprintf(query, 256, _T("DELETE FROM dc_queue WHERE server_id=") UINT64_FMT, serverId);
         DBQuery(hdb, query);

         _sntprintf(query, 256, _T("DELETE FROM dc_snmp_targets WHERE server_id=") UINT64_FMT, serverId);
         DBQuery(hdb, query);

         _sntprintf(query, 256, _T("DELETE FROM dc_config WHERE server_id=") UINT64_FMT, serverId);
         DBQuery(hdb, query);

         DBCommit(hdb);

         s_itemLock.lock();
         Iterator<DataCollectionItem> *it = s_items.iterator();
         while(it->hasNext())
         {
            DataCollectionItem *item = it->next();
            if (item->getServerId() == serverId)
            {
               it->unlink();
               item->decRefCount();
            }
         }
         delete it;
         s_itemLock.unlock();

         nxlog_debug_tag(DEBUG_TAG, 2, _T("Offline data collection configuration for server ID ") UINT64X_FMT(_T("016")) _T(" deleted"), serverId);
      }
   }

   ThreadPoolScheduleRelative(g_dataCollectorPool, STALLED_DATA_CHECK_INTERVAL, ClearStalledOfflineData, NULL);
}

/**
 * Data collector and sender thread handles
 */
static THREAD s_dataCollectionSchedulerThread = INVALID_THREAD_HANDLE;
static THREAD s_dataSenderThread = INVALID_THREAD_HANDLE;
static THREAD s_databaseWriterThread = INVALID_THREAD_HANDLE;
static THREAD s_reconciliationThread = INVALID_THREAD_HANDLE;
static THREAD s_proxyListennerThread = INVALID_THREAD_HANDLE;

/**
 * Initialize and start local data collector
 */
void StartLocalDataCollector()
{
   DB_HANDLE db = GetLocalDatabaseHandle();
   if (db == NULL)
   {
      nxlog_debug_tag(DEBUG_TAG, 5, _T("StartLocalDataCollector: local database unavailable"));
      return;
   }

   if (g_dcReconciliationBlockSize < 16)
   {
      nxlog_debug_tag(DEBUG_TAG, 1, _T("Invalid data reconciliation block size %d, resetting to 16"), g_dcReconciliationBlockSize);
      g_dcReconciliationBlockSize = 16;
   }
   else if (g_dcReconciliationBlockSize > MAX_BULK_DATA_BLOCK_SIZE)
   {
      nxlog_debug_tag(DEBUG_TAG, 1, _T("Invalid data reconciliation block size %d, resetting to %d"), g_dcReconciliationBlockSize, MAX_BULK_DATA_BLOCK_SIZE);
      g_dcReconciliationBlockSize = MAX_BULK_DATA_BLOCK_SIZE;
   }

   if (g_dcReconciliationTimeout < 1000)
   {
      nxlog_debug_tag(DEBUG_TAG, 1, _T("Invalid data reconciliation timeout %d, resetting to 1000"), g_dcReconciliationTimeout);
      g_dcReconciliationTimeout = 1000;
   }
   else if (g_dcReconciliationTimeout > 900000)
   {
      nxlog_debug_tag(DEBUG_TAG, 1, _T("Invalid data reconciliation timeout %d, resetting to 900000"), g_dcReconciliationTimeout);
      g_dcReconciliationTimeout = 900000;
   }

   LoadState();

   g_dataCollectorPool = ThreadPoolCreate(_T("DATACOLL"), 1, g_dcMaxCollectorPoolSize);
   s_dataCollectionSchedulerThread = ThreadCreateEx(DataCollectionScheduler, 0, NULL);
   s_dataSenderThread = ThreadCreateEx(DataSender, 0, NULL);
   s_databaseWriterThread = ThreadCreateEx(DatabaseWriter, 0, NULL);
   s_reconciliationThread = ThreadCreateEx(ReconciliationThread, 0, NULL);
   s_proxyListennerThread = ThreadCreateEx(ProxyListenerThread, 0 ,NULL);
   ThreadPoolScheduleRelative(g_dataCollectorPool, STALLED_DATA_CHECK_INTERVAL, ClearStalledOfflineData, NULL);
   ThreadPoolScheduleRelative(g_dataCollectorPool, 0, ProxyConnectionChecker, NULL);

   s_dataCollectorStarted = true;
}

/**
 * Shutdown local data collector
 */
void ShutdownLocalDataCollector()
{
   if (!s_dataCollectorStarted)
   {
      nxlog_debug_tag(DEBUG_TAG, 5, _T("Local data collector was not started"));
      return;
   }

   // Prevent configuration attempts by incoming sessions during shutdown
   s_itemLock.lock();
   s_dataCollectorStarted = false;
   s_itemLock.unlock();

   nxlog_debug_tag(DEBUG_TAG, 5, _T("Waiting for data collector thread termination"));
   ThreadJoin(s_dataCollectionSchedulerThread);

   nxlog_debug_tag(DEBUG_TAG, 5, _T("Waiting for data sender thread termination"));
   s_dataSenderQueue.put(INVALID_POINTER_VALUE);
   ThreadJoin(s_dataSenderThread);

   nxlog_debug_tag(DEBUG_TAG, 5, _T("Waiting for database writer thread termination"));
   s_databaseWriterQueue.put(INVALID_POINTER_VALUE);
   ThreadJoin(s_databaseWriterThread);

   nxlog_debug_tag(DEBUG_TAG, 5, _T("Waiting for data reconciliation thread termination"));
   ThreadJoin(s_reconciliationThread);

   nxlog_debug_tag(DEBUG_TAG, 5, _T("Waiting for proxy heartbeat listening thread"));
   ThreadJoin(s_proxyListennerThread);
}

/**
 * Clear data collection configuration
 */
void ClearDataCollectionConfiguration()
{
   s_itemLock.lock();
   DB_HANDLE db = GetLocalDatabaseHandle();
   DBQuery(db, _T("DELETE FROM dc_queue"));
   DBQuery(db, _T("DELETE FROM dc_config"));
   DBQuery(db, _T("DELETE FROM dc_snmp_targets"));
   s_items.clear();
   s_itemLock.unlock();

   s_serverSyncStatusLock.lock();
   s_serverSyncStatus.clear();
   s_serverSyncStatusLock.unlock();
}

/**
 * Handler for data collector queue size
 */
LONG H_DataCollectorQueueSize(const TCHAR *cmd, const TCHAR *arg, TCHAR *value, AbstractCommSession *session)
{
   if (!s_dataCollectorStarted)
      return SYSINFO_RC_UNSUPPORTED;

   UINT32 count = 0;
   s_serverSyncStatusLock.lock();
   Iterator<ServerSyncStatus> *it = s_serverSyncStatus.iterator();
   while(it->hasNext())
      count += (UINT32)it->next()->queueSize;
   delete it;
   s_serverSyncStatusLock.unlock();

   ret_uint(value, count);
   return SYSINFO_RC_SUCCESS;
}
