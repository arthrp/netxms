/*
** NetXMS - Network Management System
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
** File: abind_target.cpp
**
**/

#include "nxcore.h"

/**
 * Auto bind object constructor
 */
AutoBindTarget::AutoBindTarget(NetObj *_this)
{
   m_this = _this;
   m_firstBindFilter = nullptr;
   m_firstBindFilterSource = nullptr;
   m_mutexProperties = MutexCreate();
   m_autoBindFlags = 0;
   m_secondBindFilter = nullptr;
   m_secondBindFilterSource = nullptr;
}

/**
 * Auto bind object destructor
 */
AutoBindTarget::~AutoBindTarget()
{
   delete m_firstBindFilter;
   MemFree(m_firstBindFilterSource);
   delete m_secondBindFilter;
   MemFree(m_secondBindFilterSource);
   MutexDestroy(m_mutexProperties);
}

void AutoBindTarget::setAutoBindFlag(bool value, uint32_t flag)
{
   m_autoBindFlags = value ? m_autoBindFlags | flag : m_autoBindFlags & ~flag;
}

/**
 * Set auto bind mode for container
 */
void AutoBindTarget::setFirstFilterBindMode(bool doBind, bool doUnbind)
{
   internalLock();
   setAutoBindFlag(doBind, AAF_AUTO_APPLY_1);
   setAutoBindFlag(doUnbind, AAF_AUTO_REMOVE_1);
   internalUnlock();
   m_this->markAsModified(MODIFY_OTHER);
}

/**
 * Set auto bind DCI mode for container
 */
void AutoBindTarget::setSecondFilterBindMode(bool doBind, bool doUnbind)
{
   internalLock();
   setAutoBindFlag(doBind, AAF_AUTO_APPLY_2);
   setAutoBindFlag(doUnbind, AAF_AUTO_REMOVE_2);
   internalUnlock();
   m_this->markAsModified(MODIFY_OTHER);
}


/**
 * Set auto bind filter.
 *
 * @param filter new filter script code or NULL to clear filter
 */
void AutoBindTarget::setFirstFilter(const TCHAR *filter)
{
   internalLock();
   MemFree(m_firstBindFilterSource);
   delete m_firstBindFilter;
   if (filter != nullptr)
   {
      TCHAR error[256];
      m_firstBindFilterSource = MemCopyString(filter);
      m_firstBindFilter = NXSLCompile(m_firstBindFilterSource, error, 256, nullptr);
      if (m_firstBindFilter == nullptr)
      {
         TCHAR buffer[1024];
         _sntprintf(buffer, 1024, _T("AutoBind::%s::%s"), m_this->getObjectClassName(), m_this->getName());
         PostSystemEvent(EVENT_SCRIPT_ERROR, g_dwMgmtNode, "ssd", buffer, error, 0);
         nxlog_write(NXLOG_WARNING, _T("Failed to compile autobind script for object %s [%u] (%s)"), m_this->getName(), m_this->getId(), error);
      }
   }
   else
   {
      m_firstBindFilterSource = nullptr;
      m_firstBindFilter = nullptr;
   }
   internalUnlock();
}

/**
 * Set auto bind filter.
 *
 * @param filter new filter script code or NULL to clear filter
 */
void AutoBindTarget::setSecondFilter(const TCHAR *filter)
{
   internalLock();
   MemFree(m_secondBindFilterSource);
   delete m_secondBindFilter;
   if (filter != nullptr)
   {
      TCHAR error[256];
      m_secondBindFilterSource = MemCopyString(filter);
      m_secondBindFilter = NXSLCompile(m_secondBindFilterSource, error, 256, nullptr);
      if (m_secondBindFilter == nullptr)
      {
         TCHAR buffer[1024];
         _sntprintf(buffer, 1024, _T("AutoBind::%s::%s"), m_this->getObjectClassName(), m_this->getName());
         PostSystemEvent(EVENT_SCRIPT_ERROR, g_dwMgmtNode, "ssd", buffer, error, 0);
         nxlog_write(NXLOG_WARNING, _T("Failed to compile autobind DCI script for object %s [%u] (%s)"), m_this->getName(), m_this->getId(), error);
      }
   }
   else
   {
      m_secondBindFilterSource = nullptr;
      m_secondBindFilter = nullptr;
   }
   internalUnlock();
}

/**
 * Modify object from NXCP message
 */
void AutoBindTarget::modifyFromMessage(NXCPMessage *request)
{
   internalLock();
   if (request->isFieldExist(VID_AUTOBIND_FLAGS))
   {
      m_autoBindFlags = request->getFieldAsUInt32(VID_AUTOBIND_FLAGS);
   }
   internalUnlock();

   // Change apply filter
   if (request->isFieldExist(VID_AUTOBIND_FILTER))
   {
      TCHAR *filter = request->getFieldAsString(VID_AUTOBIND_FILTER);
      setFirstFilter(filter);
      MemFree(filter);
   }

   // Change apply filter
   if (request->isFieldExist(VID_AUTOBIND_FILTER_2))
   {
      TCHAR *filter = request->getFieldAsString(VID_AUTOBIND_FILTER_2);
      setSecondFilter(filter);
      MemFree(filter);
   }
}

/**
 * Create NXCP message with object's data
 */
void AutoBindTarget::fillMessage(NXCPMessage *msg)
{
   internalLock();
   msg->setField(VID_AUTOBIND_FLAGS, m_autoBindFlags);
   msg->setField(VID_AUTOBIND_FILTER, CHECK_NULL_EX(m_firstBindFilterSource));
   msg->setField(VID_AUTOBIND_FILTER_2, CHECK_NULL_EX(m_secondBindFilterSource));
   internalUnlock();
}

/**
 * Load object from database
 */
bool AutoBindTarget::loadFromDatabase(DB_HANDLE hdb, uint32_t objectId)
{
   bool success = false;
   DB_STATEMENT hStmt = DBPrepare(hdb, _T("SELECT bind_filter_1,bind_filter_2,flags FROM auto_bind_target WHERE object_id=?"));
   if (hStmt != nullptr)
   {
      DBBind(hStmt, 1, DB_SQLTYPE_INTEGER, objectId);
      DB_RESULT hResult = DBSelectPrepared(hStmt);
      if (hResult != nullptr)
      {
         TCHAR *filter = DBGetField(hResult, 0, 0, nullptr, 0);
         setFirstFilter(filter);
         MemFree(filter);
         TCHAR *filterDCI = DBGetField(hResult, 0, 1, nullptr, 0);
         setSecondFilter(filterDCI);
         MemFree(filterDCI);
         m_autoBindFlags = DBGetFieldULong(hResult, 0, 2);
         DBFreeResult(hResult);
         success = true;
      }
      DBFreeStatement(hStmt);
   }

   return success;
}

/**
 * Save object to database
 */
bool AutoBindTarget::saveToDatabase(DB_HANDLE hdb)
{
   bool success = false;

   DB_STATEMENT hStmt;
   if (IsDatabaseRecordExist(hdb, _T("auto_bind_target"), _T("object_id"), m_this->getId()))
   {
      hStmt = DBPrepare(hdb, _T("UPDATE auto_bind_target SET bind_filter_1=?,bind_filter_2=?,flags=? WHERE object_id=?"));
   }
   else
   {
      hStmt = DBPrepare(hdb, _T("INSERT INTO auto_bind_target (bind_filter_1,bind_filter_2,flags,object_id) VALUES (?,?,?,?)"));
   }
   if (hStmt != nullptr)
   {
      internalLock();
      DBBind(hStmt, 1, DB_SQLTYPE_TEXT, m_firstBindFilterSource, DB_BIND_STATIC);
      DBBind(hStmt, 2, DB_SQLTYPE_TEXT, m_secondBindFilterSource, DB_BIND_STATIC);
      DBBind(hStmt, 3, DB_SQLTYPE_INTEGER, m_autoBindFlags);
      DBBind(hStmt, 4, DB_SQLTYPE_INTEGER, m_this->getId());
      success = DBExecute(hStmt);
      internalUnlock();
      DBFreeStatement(hStmt);
   }

   return success;
}

/**
 * Delete object from database
 */
bool AutoBindTarget::deleteFromDatabase(DB_HANDLE hdb)
{
   return ExecuteQueryOnObject(hdb, m_this->getId(), _T("DELETE FROM auto_bind_target WHERE object_id=?"));
}

/**
 * Check if object should be automatically applied to given data collection target using filter program
 */
AutoBindDecision AutoBindTarget::isApplicable(const shared_ptr<NetObj>& target, const shared_ptr<DCObject>& dci, NXSL_Program *filterProgram)
{
   AutoBindDecision result = AutoBindDecision_Ignore;

   NXSL_VM *filter = nullptr;
   internalLock();
   if ((filterProgram != nullptr) && !filterProgram->isEmpty())
   {
      filter = CreateServerScriptVM(filterProgram, target);
      if (filter == nullptr)
      {
         TCHAR buffer[1024];
         _sntprintf(buffer, 1024, _T("%s::%s::%d"), m_this->getObjectClassName(), m_this->getName(), m_this->getId());
         PostSystemEvent(EVENT_SCRIPT_ERROR, g_dwMgmtNode, "ssd", buffer, _T("Script load error"), m_this->getId());
         nxlog_write(NXLOG_WARNING, _T("Failed to load autobind script for object %s [%u]"), m_this->getName(), m_this->getId());
      }
   }
   internalUnlock();

   if (filter == nullptr)
      return result;

   filter->setGlobalVariable("$container", m_this->createNXSLObject(filter));
   filter->setGlobalVariable("$template", m_this->createNXSLObject(filter));
   if (dci != nullptr)
      filter->setGlobalVariable("$dci", dci->createNXSLObject(filter));
   filter->setUserData(target.get());  // For PollerTrace()
   if (filter->run())
   {
      const NXSL_Value *value = filter->getResult();
      if (!value->isNull())
         result = value->isTrue() ? AutoBindDecision_Bind : AutoBindDecision_Unbind;
   }
   else
   {
      internalLock();
      TCHAR buffer[1024];
      _sntprintf(buffer, 1024, _T("%s::%s::%d"), m_this->getObjectClassName(), m_this->getName(), m_this->getId());
      PostSystemEvent(EVENT_SCRIPT_ERROR, g_dwMgmtNode, "ssd", buffer, filter->getErrorText(), m_this->getId());
      nxlog_write(NXLOG_WARNING, _T("Failed to execute autobind script for object %s [%u] (%s)"), m_this->getName(), m_this->getId(), filter->getErrorText());
      internalUnlock();
   }
   delete filter;
   return result;
}

/**
 * Check if object should be automatically applied to given data collection target using first filter
 * Returns AutoBindDecision_Bind if applicable, AutoBindDecision_Unbind if not,
 * AutoBindDecision_Ignore if no change required (script error or no auto apply)
 */
AutoBindDecision AutoBindTarget::isFirstFilterApplicable(const shared_ptr<NetObj>& target, const shared_ptr<DCObject>& dci)
{
   AutoBindDecision result = AutoBindDecision_Ignore;
   if (isFirstFilterBindingEnabled())
   {
      result = isApplicable(target, dci, m_firstBindFilter);
   }
   return result;
}

/**
 * Check if object should be automatically applied to given data collection target using second filter
 * Returns AutoBindDecision_Bind if applicable, AutoBindDecision_Unbind if not,
 * AutoBindDecision_Ignore if no change required (script error or no auto apply)
 */
AutoBindDecision AutoBindTarget::isSecondFilterApplicable(const shared_ptr<NetObj>& target, const shared_ptr<DCObject>& dci)
{
   AutoBindDecision result = AutoBindDecision_Ignore;
   if (isSecondFilterBindingEnabled())
   {
      result = isApplicable(target, dci, m_secondBindFilter);
   }
   return result;
}

/**
 * Serialize object to JSON
 */
void AutoBindTarget::toJson(json_t *root)
{
   internalLock();
   json_object_set_new(root, "autoBind", json_boolean(isFirstFilterBindingEnabled()));
   json_object_set_new(root, "autoUnbind", json_boolean(isFirstFilterUnbindingEnabled()));
   json_object_set_new(root, "applyFilter", json_string_t(m_firstBindFilterSource));
   json_object_set_new(root, "autoBindDCI", json_boolean(isSecondFilterBindingEnabled()));
   json_object_set_new(root, "autoUnbindDCI", json_boolean(isSecondFilterUnbindingEnabled()));
   json_object_set_new(root, "applyFilterDCI", json_string_t(m_secondBindFilterSource));
   internalUnlock();
}

/**
 * Create export record
 */
void AutoBindTarget::createExportRecord(StringBuffer &str)
{
   internalLock();
   if (m_firstBindFilterSource != nullptr)
   {
      str.append(_T("\t\t\t<filter autoBind=\""));
      str.append(isFirstFilterBindingEnabled());
      str.append(_T("\" autoUnbind=\""));
      str.append(isFirstFilterUnbindingEnabled());
      str.append(_T("\">"));
      str.appendPreallocated(EscapeStringForXML(m_firstBindFilterSource, -1));
      str.append(_T("</filter>\n"));
   }
   if (m_secondBindFilterSource != nullptr)
   {
      str.append(_T("\t\t\t<secondFilter autoBind=\""));
      str.append(isSecondFilterBindingEnabled());
      str.append(_T("\" autoUnbind=\""));
      str.append(isSecondFilterUnbindingEnabled());
      str.append(_T("\">"));
      str.appendPreallocated(EscapeStringForXML(m_secondBindFilterSource, -1));
      str.append(_T("</secondFilter>\n"));
   }
   internalUnlock();
}

/**
 * Update from import record
 */
void AutoBindTarget::updateFromImport(ConfigEntry *config)
{
   ConfigEntry *filter = config->findEntry(_T("filter"));
   if (filter != nullptr)
   {
      setFirstFilter(filter->getValue());
      internalLock();
      setAutoBindFlag(filter->getAttributeAsBoolean(_T("autoBind")), AAF_AUTO_APPLY_1);
      setAutoBindFlag(filter->getAttributeAsBoolean(_T("autoUnbind")), AAF_AUTO_REMOVE_1);
      internalUnlock();
   }
   else
   {
      setFirstFilter(nullptr);
   }
   filter = config->findEntry(_T("secondFilter"));
   if (filter != nullptr)
   {
      setSecondFilter(filter->getValue());
      internalLock();
      setAutoBindFlag(filter->getAttributeAsBoolean(_T("autoBind")), AAF_AUTO_APPLY_2);
      setAutoBindFlag(filter->getAttributeAsBoolean(_T("autoUnbind")), AAF_AUTO_REMOVE_2);
      internalUnlock();
   }
   else
   {
      setSecondFilter(nullptr);
   }
}
