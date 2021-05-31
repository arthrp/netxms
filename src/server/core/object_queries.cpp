/*
** NetXMS - Network Management System
** Copyright (C) 2003-2021 Raden Solutions
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
** File: object_queries.cpp
**
**/

#include "nxcore.h"

/**
 * Index of predefined object queries
 */
static SharedPointerIndex<ObjectQuery> s_objectQueries;

/**
 * Create object query from NXCP message
 */
ObjectQuery::ObjectQuery(const NXCPMessage& msg) :
         m_description(msg.getFieldAsString(VID_DESCRIPTION), -1, Ownership::True),
         m_source(msg.getFieldAsString(VID_SCRIPT_CODE), -1, Ownership::True),
         m_parameters(msg.getFieldAsInt32(VID_NUM_PARAMETERS))
{
   m_id = msg.getFieldAsUInt32(VID_OBJECT_ID);
   m_guid = msg.getFieldAsGUID(VID_GUID);
   msg.getFieldAsString(VID_NAME, m_name, MAX_OBJECT_NAME);

   TCHAR errorMessage[256];
   m_script = NXSLCompile(m_source, errorMessage, 256, nullptr);
   if (m_script == nullptr)
   {
      TCHAR buffer[1024];
      _sntprintf(buffer, 1024, _T("ObjectQuery::%s"), m_name);
      PostSystemEvent(EVENT_SCRIPT_ERROR, g_dwMgmtNode, "ssd", buffer, errorMessage, 0);
      nxlog_write(NXLOG_WARNING, _T("Failed to compile script for predefined object query %s [%u] (%s)"), m_name, m_id, errorMessage);
   }

   int count = msg.getFieldAsInt32(VID_NUM_PARAMETERS);
   uint32_t fieldId = VID_PARAM_LIST_BASE;
   for(int i = 0; i < count; i++)
   {
      ObjectQueryParameter *p = m_parameters.addPlaceholder();
      msg.getFieldAsString(fieldId++, p->name, 32);
      msg.getFieldAsString(fieldId++, p->displayName, 128);
      p->type = msg.getFieldAsInt16(fieldId++);
      fieldId += 2;
   }
}

/**
 * Fill message with object query data
 */
uint32_t ObjectQuery::fillMessage(NXCPMessage *msg, uint32_t baseId) const
{
   uint32_t fieldId = baseId;
   msg->setField(fieldId++, m_id);
   msg->setField(fieldId++, m_guid);
   msg->setField(fieldId++, m_name);
   msg->setField(fieldId++, m_description);
   msg->setField(fieldId++, m_source);
   msg->setField(fieldId++, m_script != nullptr);
   fieldId += 3;
   msg->setField(fieldId++, m_parameters.size());
   for(int i = 0; i < m_parameters.size(); i++)
   {
      ObjectQueryParameter *p = m_parameters.get(i);
      msg->setField(fieldId++, p->name);
      msg->setField(fieldId++, p->displayName);
      msg->setField(fieldId++, p->type);
      fieldId += 2;
   }
   return fieldId;
}

/**
 * Filter object
 */
static int FilterObject(NXSL_VM *vm, shared_ptr<NetObj> object, NXSL_VariableSystem **globalVariables)
{
   SetupServerScriptVM(vm, object, shared_ptr<DCObjectInfo>());
   vm->setContextObject(object->createNXSLObject(vm));
   NXSL_VariableSystem *expressionVariables = nullptr;
   ObjectRefArray<NXSL_Value> args(0);
   if (!vm->run(args, globalVariables, &expressionVariables))
   {
      delete expressionVariables;
      return -1;
   }
   if ((globalVariables != nullptr) && (expressionVariables != nullptr))
   {
      (*globalVariables)->merge(expressionVariables);
   }
   delete expressionVariables;
   return vm->getResult()->isTrue() ? 1 : 0;
}

/**
 * Filter objects accessible by given user
 */
static bool FilterAccessibleObjects(NetObj *object, void *data)
{
   return object->checkAccessRights(CAST_FROM_POINTER(data, UINT32), OBJECT_ACCESS_READ);
}

/**
 * Query result comparator data
 */
struct ObjectQueryComparatorData
{
   const StringList *orderBy;
   const StringMap *fields;
};

/**
 * Query result comparator
 */
static int ObjectQueryComparator(const StringList *orderBy, const ObjectQueryResult **object1, const ObjectQueryResult **object2)
{
   for(int i = 0; i < orderBy->size(); i++)
   {
      bool descending = false;
      const TCHAR *attr = orderBy->get(i);
      if (*attr == _T('-'))
      {
         descending = true;
         attr++;
      }
      else if (*attr == _T('+'))
      {
         attr++;
      }

      const TCHAR *v1 = (*object1)->values->get(attr);
      const TCHAR *v2 = (*object2)->values->get(attr);

      // Try to compare as numbers
      if ((v1 != nullptr) && (v2 != nullptr))
      {
         TCHAR *eptr;
         double d1 = _tcstod(v1, &eptr);
         if (*eptr == 0)
         {
            double d2 = _tcstod(v2, &eptr);
            if (*eptr == 0)
            {
               if (d1 < d2)
                  return descending ? 1 : -1;
               if (d1 > d2)
                  return descending ? -1 : 1;
               continue;
            }
         }
      }

      // Compare as text if at least one value is not a number
      int rc = _tcsicmp(CHECK_NULL_EX(v1), CHECK_NULL_EX(v2));
      if (rc != 0)
         return descending ? -rc : rc;
   }
   return 0;
}

/**
 * Set object data from NXSL variable
 */
static void SetDataFromVariable(const NXSL_Identifier& name, NXSL_Value *value, StringMap *objectData)
{
   if (name.value[0] != '$')  // Ignore global variables set by system
   {
#ifdef UNICODE
      objectData->setPreallocated(WideStringFromUTF8String(name.value), MemCopyString(value->getValueAsCString()));
#else
      objectData->set(name.value, value->getValueAsCString());
#endif
   }
}

/**
 * Query objects
 */
unique_ptr<ObjectArray<ObjectQueryResult>> QueryObjects(const TCHAR *query, uint32_t userId, TCHAR *errorMessage, size_t errorMessageLen,
         bool readAllComputedFields, const StringList *fields, const StringList *orderBy, uint32_t limit)
{
   NXSL_VM *vm = NXSLCompileAndCreateVM(query, errorMessage, errorMessageLen, new NXSL_ServerEnv());
   if (vm == nullptr)
      return unique_ptr<ObjectArray<ObjectQueryResult>>();

   bool readFields = readAllComputedFields || (fields != nullptr);

   // Set class constants
   vm->addConstant("ACCESSPOINT", vm->createValue(OBJECT_ACCESSPOINT));
   vm->addConstant("BUSINESSSERVICE", vm->createValue(OBJECT_BUSINESSSERVICE));
   vm->addConstant("BUSINESSSERVICEROOT", vm->createValue(OBJECT_BUSINESSSERVICEROOT));
   vm->addConstant("CHASSIS", vm->createValue(OBJECT_CHASSIS));
   vm->addConstant("CLUSTER", vm->createValue(OBJECT_CLUSTER));
   vm->addConstant("CONDITION", vm->createValue(OBJECT_CONDITION));
   vm->addConstant("CONTAINER", vm->createValue(OBJECT_CONTAINER));
   vm->addConstant("DASHBOARD", vm->createValue(OBJECT_DASHBOARD));
   vm->addConstant("DASHBOARDGROUP", vm->createValue(OBJECT_DASHBOARDGROUP));
   vm->addConstant("DASHBOARDROOT", vm->createValue(OBJECT_DASHBOARDROOT));
   vm->addConstant("INTERFACE", vm->createValue(OBJECT_INTERFACE));
   vm->addConstant("MOBILEDEVICE", vm->createValue(OBJECT_MOBILEDEVICE));
   vm->addConstant("NETWORK", vm->createValue(OBJECT_NETWORK));
   vm->addConstant("NETWORKMAP", vm->createValue(OBJECT_NETWORKMAP));
   vm->addConstant("NETWORKMAPGROUP", vm->createValue(OBJECT_NETWORKMAPGROUP));
   vm->addConstant("NETWORKMAPROOT", vm->createValue(OBJECT_NETWORKMAPROOT));
   vm->addConstant("NETWORKSERVICE", vm->createValue(OBJECT_NETWORKSERVICE));
   vm->addConstant("NODE", vm->createValue(OBJECT_NODE));
   vm->addConstant("NODELINK", vm->createValue(OBJECT_NODELINK));
   vm->addConstant("RACK", vm->createValue(OBJECT_RACK));
   vm->addConstant("SENSOR", vm->createValue(OBJECT_SENSOR));
   vm->addConstant("SERVICEROOT", vm->createValue(OBJECT_SERVICEROOT));
   vm->addConstant("SLMCHECK", vm->createValue(OBJECT_SLMCHECK));
   vm->addConstant("SUBNET", vm->createValue(OBJECT_SUBNET));
   vm->addConstant("TEMPLATE", vm->createValue(OBJECT_TEMPLATE));
   vm->addConstant("TEMPLATEGROUP", vm->createValue(OBJECT_TEMPLATEGROUP));
   vm->addConstant("TEMPLATEROOT", vm->createValue(OBJECT_TEMPLATEROOT));
   vm->addConstant("VPNCONNECTOR", vm->createValue(OBJECT_VPNCONNECTOR));
   vm->addConstant("ZONE", vm->createValue(OBJECT_ZONE));

   unique_ptr<SharedObjectArray<NetObj>> objects = g_idxObjectById.getObjects(FilterAccessibleObjects);
   auto resultSet = new ObjectArray<ObjectQueryResult>(64, 64, Ownership::True);
   for(int i = 0; i < objects->size(); i++)
   {
      shared_ptr<NetObj> curr = objects->getShared(i);

      NXSL_VariableSystem *globals = nullptr;
      int rc = FilterObject(vm, curr, readFields ? &globals : nullptr);
      if (rc < 0)
      {
         _tcslcpy(errorMessage, vm->getErrorText(), errorMessageLen);
         delete_and_null(resultSet);
         delete globals;
         break;
      }

      if (rc > 0)
      {
         StringMap *objectData;
         if (readFields)
         {
            objectData = new StringMap();

            if (fields != nullptr)
            {
               NXSL_Value *objectValue = curr->createNXSLObject(vm);
               NXSL_Object *object = objectValue->getValueAsObject();
               for(int j = 0; j < fields->size(); j++)
               {
                  const TCHAR *fieldName = fields->get(j);
                  NXSL_Variable *v = globals->find(fieldName);
                  if (v != nullptr)
                  {
                     objectData->set(fieldName, v->getValue()->getValueAsCString());
                  }
                  else
                  {
#ifdef UNICODE
                     char attr[MAX_IDENTIFIER_LENGTH];
                     WideCharToMultiByte(CP_UTF8, 0, fields->get(j), -1, attr, MAX_IDENTIFIER_LENGTH - 1, nullptr, nullptr);
                     attr[MAX_IDENTIFIER_LENGTH - 1] = 0;
                     NXSL_Value *av = object->getClass()->getAttr(object, attr);
#else
                     NXSL_Value *av = object->getClass()->getAttr(object, fields->get(j));
#endif
                     if (av != nullptr)
                     {
                        objectData->set(fieldName, av->getValueAsCString());
                        vm->destroyValue(av);
                     }
                     else
                     {
                        objectData->set(fieldName, _T(""));
                     }
                  }
               }
               vm->destroyValue(objectValue);
            }

            if (readAllComputedFields)
            {
               globals->forEach(SetDataFromVariable, objectData);
            }
         }
         else
         {
            objectData = nullptr;
         }

         resultSet->add(new ObjectQueryResult(curr, objectData));
      }
      delete globals;
   }

   delete vm;

   // Sort result set and apply limit
   if (resultSet != nullptr)
   {
      if ((orderBy != nullptr) && !orderBy->isEmpty())
      {
         resultSet->sort(ObjectQueryComparator, orderBy);
      }
      if (limit > 0)
      {
         resultSet->shrinkTo((int)limit);
      }
   }

   return unique_ptr<ObjectArray<ObjectQueryResult>>(resultSet);
}