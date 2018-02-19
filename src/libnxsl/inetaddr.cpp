/* 
** NetXMS - Network Management System
** NetXMS Scripting Language Interpreter
** Copyright (C) 2003-2018 Victor Kirhenshtein
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU Lesser General Public License as published by
** the Free Software Foundation; either version 3 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU Lesser General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** File: inetaddr.cpp
**
**/

#include "libnxsl.h"

/**
 * Instance of InetAddress class
 */
NXSL_InetAddressClass LIBNXSL_EXPORTABLE g_nxslInetAddressClass;

/**
 * Implementation of "InetAddress" class: constructor
 */
NXSL_InetAddressClass::NXSL_InetAddressClass() : NXSL_Class()
{
   setName(_T("InetAddress"));
}

/**
 * Implementation of "InetAddress" class: destructor
 */
NXSL_InetAddressClass::~NXSL_InetAddressClass()
{
}

/**
 * Object delete
 */
void NXSL_InetAddressClass::onObjectDelete(NXSL_Object *object)
{
   delete (InetAddress *)object->getData();
}

/**
 * Implementation of "InetAddress" class: get attribute
 */
NXSL_Value *NXSL_InetAddressClass::getAttr(NXSL_Object *object, const char *attr)
{
   NXSL_Value *value = NULL;
   InetAddress *a = (InetAddress *)object->getData();
   if (!strcmp(attr, "address"))
   {
      TCHAR buffer[64];
      value = new NXSL_Value(a->toString(buffer));
   }
   else if (!strcmp(attr, "family"))
   {
      value = new NXSL_Value((a->getFamily() == AF_INET) ? _T("inet") : (a->getFamily() == AF_INET6 ? _T("inet6") : _T("unspec")));
   }
   else if (!strcmp(attr, "isAnyLocal"))
   {
      value = new NXSL_Value(a->isAnyLocal());
   }
   else if (!strcmp(attr, "isBroadcast"))
   {
      value = new NXSL_Value(a->isBroadcast());
   }
   else if (!strcmp(attr, "isLinkLocal"))
   {
      value = new NXSL_Value(a->isLinkLocal());
   }
   else if (!strcmp(attr, "isLoopback"))
   {
      value = new NXSL_Value(a->isLoopback());
   }
   else if (!strcmp(attr, "isMulticast"))
   {
      value = new NXSL_Value(a->isMulticast());
   }
   else if (!strcmp(attr, "isValid"))
   {
      value = new NXSL_Value(a->isValid());
   }
   else if (!strcmp(attr, "isValidUnicast"))
   {
      value = new NXSL_Value(a->isValidUnicast());
   }
   else if (!strcmp(attr, "mask"))
   {
      value = new NXSL_Value(a->getMaskBits());
   }
   return value;
}

/**
 * Create NXSL object from InetAddress object
 */
NXSL_Value *NXSL_InetAddressClass::createObject(const InetAddress& addr)
{
   return new NXSL_Value(new NXSL_Object(&g_nxslInetAddressClass, new InetAddress(addr)));
}

/**
 * Constructor for InetAddress class
 */
int F_InetAddress(int argc, NXSL_Value **argv, NXSL_Value **result, NXSL_VM *vm)
{
   if (argc > 1)
      return NXSL_ERR_INVALID_ARGUMENT_COUNT;

   if ((argc == 1) && !argv[0]->isString())
      return NXSL_ERR_NOT_STRING;

   InetAddress addr = (argc == 0) ? InetAddress() : InetAddress::parse(argv[0]->getValueAsCString());
   *result = NXSL_InetAddressClass::createObject(addr);
   return 0;
}
