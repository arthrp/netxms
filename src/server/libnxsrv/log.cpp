/* 
** NetXMS - Network Management System
** Copyright (C) 2003, 2004 Victor Kirhenshtein
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
** $module: log.cpp
**
**/

#include "libnxsrv.h"
#include <stdarg.h>
#if HAVE_SYSLOG_H
#include <syslog.h>
#endif


//
// Messages generated by mc.pl (for UNIX version only)
//

#ifndef _WIN32
extern unsigned int g_dwNumMessages;
extern char *g_szMessages[];
#endif


//
// Static data
//

#ifdef _WIN32
static HANDLE m_hEventLog = INVALID_HANDLE_VALUE;
static HMODULE m_hLibraryHandle = NULL;
#endif
static FILE *m_hLogFile = NULL;
static MUTEX m_mutexLogAccess = INVALID_MUTEX_HANDLE;
static BOOL m_bUseSystemLog = FALSE;
static BOOL m_bPrintToScreen = FALSE;


//
// Initialize log
//

void LIBNXSRV_EXPORTABLE InitLog(BOOL bUseSystemLog, char *pszLogFile, BOOL bPrintToScreen)
{
   m_bUseSystemLog = bUseSystemLog;
   m_bPrintToScreen = bPrintToScreen;
#ifdef _WIN32
   m_hLibraryHandle = GetModuleHandle(_T("LIBNXSRV.DLL"));
#endif
   if (m_bUseSystemLog)
   {
#ifdef _WIN32
      m_hEventLog = RegisterEventSource(NULL, CORE_EVENT_SOURCE);
#else
      openlog("netxmsd", LOG_PID | ((g_dwFlags & AF_STANDALONE) ? LOG_PERROR : 0), LOG_DAEMON);
#endif
   }
   else
   {
      char szTimeBuf[32];
      struct tm *loc;
      time_t t;

      m_hLogFile = fopen(pszLogFile, "a");
		if (m_hLogFile != NULL)
		{
			t = time(NULL);
			loc = localtime(&t);
			strftime(szTimeBuf, 32, "%d-%b-%Y %H:%M:%S", loc);
			fprintf(m_hLogFile, "\n[%s] Log file opened\n", szTimeBuf);
			fflush(m_hLogFile);
		}
		else
		{
			fprintf(stderr, "*** Can't open logfile (%s)\n", pszLogFile);
		}

      m_mutexLogAccess = MutexCreate();
   }
}


//
// Close log
//

void LIBNXSRV_EXPORTABLE CloseLog(void)
{
   if (m_bUseSystemLog)
   {
#ifdef _WIN32
      DeregisterEventSource(m_hEventLog);
#else
		closelog();
#endif
   }
   else
   {
      if (m_hLogFile != NULL)
      {
         char szTimeBuf[32];
         struct tm *loc;
         time_t t;

         t = time(NULL);
         loc = localtime(&t);
         strftime(szTimeBuf, 32, "%d-%b-%Y %H:%M:%S", loc);
         fprintf(m_hLogFile, "[%s] Log file closed\n", szTimeBuf);
         fclose(m_hLogFile);
      }
      if (m_mutexLogAccess != INVALID_MUTEX_HANDLE)
         MutexDestroy(m_mutexLogAccess);
   }
}


//
// Write record to log file
//

static void WriteLogToFile(char *szMessage)
{
   char szBuffer[64];
   time_t t;
   struct tm *loc;

	// Prevent simultaneous write to log file
	MutexLock(m_mutexLogAccess, INFINITE);

	t = time(NULL);
	loc = localtime(&t);
	strftime(szBuffer, 32, "[%d-%b-%Y %H:%M:%S]", loc);
	if (m_hLogFile != NULL)
	{
		fprintf(m_hLogFile, "%s %s", szBuffer, szMessage);
		fflush(m_hLogFile);
	}
	if (m_bPrintToScreen)
	{
		printf("%s %s", szBuffer, szMessage);
	}

	MutexUnlock(m_mutexLogAccess);
}


//
// Format message (UNIX version)
//

#ifndef _WIN32

static char *FormatMessageUX(DWORD dwMsgId, char **ppStrings)
{
   char *p, *pMsg;
   int i, iSize, iLen;

   if (dwMsgId >= g_dwNumMessages)
   {
      // No message with this ID
      pMsg = (char *)malloc(128);
      sprintf(pMsg, "MSG 0x%08X - Unable to find message text\n", dwMsgId);
   }
   else
   {
      iSize = strlen(g_szMessages[dwMsgId]) + 2;
      pMsg = (char *)malloc(iSize);

      for(i = 0, p = g_szMessages[dwMsgId]; *p != 0; p++)
         if (*p == '%')
         {
            p++;
            if ((*p >= '1') && (*p <= '9'))
            {
               iLen = strlen(ppStrings[*p - '1']);
               iSize += iLen;
               pMsg = (char *)realloc(pMsg, iSize);
               strcpy(&pMsg[i], ppStrings[*p - '1']);
               i += iLen;
            }
            else
            {
               if (*p == 0)   // Handle single % character at the string end
                  break;
               pMsg[i++] = *p;
            }
         }
         else
         {
            pMsg[i++] = *p;
         }
      pMsg[i++] = '\n';
      pMsg[i] = 0;
   }

   return pMsg;
}

#endif   /* ! _WIN32 */


//
// Write log record
// Parameters:
// msg    - Message ID
// wType  - Message type (see ReportEvent() for details)
// format - Parameter format string, each parameter represented by one character.
//          The following format characters can be used:
//             s - String
//             d - Decimal integer
//             x - Hex integer
//             e - System error code (will appear in log as textual description)
//             a - IP address in network byte order
//

void LIBNXSRV_EXPORTABLE WriteLog(DWORD msg, WORD wType, const char *format, ...)
{
   va_list args;
   char *strings[16], *pMsg;
   int numStrings = 0;
   DWORD error;

   memset(strings, 0, sizeof(char *) * 16);

   if (format != NULL)
   {
      va_start(args, format);

      for(; (format[numStrings] != 0) && (numStrings < 16); numStrings++)
      {
         switch(format[numStrings])
         {
            case 's':
               strings[numStrings] = strdup(va_arg(args, char *));
               break;
            case 'd':
               strings[numStrings] = (char *)malloc(16);
               sprintf(strings[numStrings], "%d", va_arg(args, LONG));
               break;
            case 'x':
               strings[numStrings] = (char *)malloc(16);
               sprintf(strings[numStrings], "0x%08X", va_arg(args, DWORD));
               break;
            case 'a':
               strings[numStrings] = (char *)malloc(20);
               IpToStr(va_arg(args, DWORD), strings[numStrings]);
               break;
            case 'e':
               error = va_arg(args, DWORD);
#ifdef _WIN32
               if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | 
                                 FORMAT_MESSAGE_FROM_SYSTEM | 
                                 FORMAT_MESSAGE_IGNORE_INSERTS,
                                 NULL, error,
                                 MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT), // Default language
                                 (LPSTR)&pMsg,0,NULL)>0)
               {
                  pMsg[strcspn(pMsg,"\r\n")] = 0;
                  strings[numStrings]=(char *)malloc(strlen(pMsg) + 1);
                  strcpy(strings[numStrings], pMsg);
                  LocalFree(pMsg);
               }
               else
               {
                  strings[numStrings] = (char *)malloc(64);
                  sprintf(strings[numStrings], "MSG 0x%08X - Unable to find message text", error);
               }
#else   /* _WIN32 */
               strings[numStrings] = strdup(strerror(error));
#endif
               break;
            default:
               strings[numStrings] = (char *)malloc(32);
               sprintf(strings[numStrings], "BAD FORMAT (0x%08X)", va_arg(args, DWORD));
               break;
         }
      }
      va_end(args);
   }

#ifdef _WIN32
   if (m_bUseSystemLog)
   {
      ReportEvent(m_hEventLog, wType, 0, msg, NULL, numStrings, 0, (const char **)strings, NULL);
   }
   else
   {
      LPVOID lpMsgBuf;

      if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | 
                        FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_ARGUMENT_ARRAY,
                        m_hLibraryHandle, msg, 0, (LPTSTR)&lpMsgBuf, 0, strings)>0)
      {
         char *pCR;

         // Replace trailing CR/LF pair with LF
         pCR = strchr((char *)lpMsgBuf, '\r');
         if (pCR != NULL)
         {
            *pCR = '\n';
            pCR++;
            *pCR = 0;
         }
         WriteLogToFile((char *)lpMsgBuf);
         LocalFree(lpMsgBuf);
      }
      else
      {
         char message[64];

         sprintf(message,"MSG 0x%08X - Unable to find message text\n",msg);
         WriteLogToFile(message);
      }
   }
#else  /* _WIN32 */
   pMsg = FormatMessageUX(msg, strings);
   if (m_bUseSystemLog)
   {
      int level;

      switch(wType)
      {
         case EVENTLOG_ERROR_TYPE:
            level = LOG_ERR;
            break;
         case EVENTLOG_WARNING_TYPE:
            level = LOG_WARNING;
            break;
         case EVENTLOG_INFORMATION_TYPE:
            level = LOG_NOTICE;
            break;
         case EVENTLOG_DEBUG_TYPE:
            level = LOG_DEBUG;
            break;
         default:
            level = LOG_INFO;
            break;
      }
      syslog(level, "%s", pMsg);
   }
   else
   {
      WriteLogToFile(pMsg);
   }
   free(pMsg);
#endif /* _WIN32 */

   while(--numStrings >= 0)
      free(strings[numStrings]);
}


//
// Debug printf - write debug record to log if level is less or equal current debug level
//

void LIBNXSRV_EXPORTABLE DbgPrintf(int level, const TCHAR *format, ...)
{
   va_list args;
   TCHAR buffer[4096];

   if (level > g_nDebugLevel)
      return;     // Required application flag(s) not set

   va_start(args, format);
   _vsntprintf(buffer, 4096, format, args);
   va_end(args);
   WriteLog(MSG_DEBUG, EVENTLOG_INFORMATION_TYPE, "s", buffer);
}
