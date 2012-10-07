/* 
** NetXMS - Network Management System
** Copyright (C) 2003-2012 Victor Kirhenshtein
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU Lesser General Public License as published
** by the Free Software Foundation; either version 3 of the License, or
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
** File: dbquery.h
**
**/

#ifndef _dbquery_h_
#define _dbquery_h_

#include <nms_common.h>
#include <nms_util.h>
#include <nms_agent.h>
#include <nxdbapi.h>


/**
 * database connection
 */
class DBConnection
{
private:
	TCHAR *m_driver;
	TCHAR *m_server;
	TCHAR *m_dbName;
	TCHAR *m_login;
	TCHAR *m_password;
	DB_DRIVER m_driver;
	DB_HANDLE m_hdb;

public:
	DBConnection(
};


#endif
