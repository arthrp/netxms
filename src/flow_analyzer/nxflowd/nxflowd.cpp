/*
** nxflowd - NetXMS Flow Collector Daemon
** Copyright (c) 2009-2012 Victor Kirhenshtein
*/

#include "nxflowd.h"
#include <nxconfig.h>

#ifdef _WIN32
#include <conio.h>
#else
#include <signal.h>
#endif

NETXMS_EXECUTABLE_HEADER(nxflowd)


//
// Messages generated by mc.pl (for UNIX version only)
//

#ifndef _WIN32
extern unsigned int g_dwNumMessages;
extern const TCHAR *g_szMessages[];
#endif


//
// Global variables
//

int g_debugLevel = 0;
DWORD g_flags = AF_LOG_SQL_ERRORS;
TCHAR g_listenAddress[MAX_PATH] = _T("0.0.0.0");
DWORD g_tcpPort = IPFIX_DEFAULT_PORT;
DWORD g_udpPort = IPFIX_DEFAULT_PORT;
DB_DRIVER g_dbDriverHandle = NULL;
DB_HANDLE g_dbConnection = NULL;
#ifdef _WIN32
TCHAR g_configFile[MAX_PATH] = _T("C:\\nxflowd.conf");
TCHAR g_logFile[MAX_PATH] = _T("C:\\nxflowd.log");
#else
TCHAR g_configFile[MAX_PATH] = _T("{search}");
TCHAR g_logFile[MAX_PATH] = _T("/var/log/nxflowd");
#endif


//
// Static data
//

#if defined(_WIN32) || defined(_NETWARE)
static CONDITION m_hCondShutdown = INVALID_CONDITION_HANDLE;
#else
char s_pidFile[MAX_PATH] = "/var/run/nxflowd.pid";
#endif
static TCHAR s_dbDriver[MAX_PATH] = _T("");
static TCHAR s_dbDrvParams[MAX_PATH] = _T("");
static TCHAR s_dbServer[MAX_PATH] = _T("localhost");
static TCHAR s_dbName[MAX_DB_NAME] = _T("netxms_db");
static TCHAR s_dbSchema[MAX_DB_NAME] = _T("");
static TCHAR s_dbLogin[MAX_DB_LOGIN] = _T("netxms");
static TCHAR s_dbPassword[MAX_PASSWORD] = _T("");
static NX_CFG_TEMPLATE m_cfgTemplate[] =
{
   { _T("DBDriver"), CT_STRING, 0, 0, MAX_PATH, 0, s_dbDriver },
   { _T("DBDrvParams"), CT_STRING, 0, 0, MAX_PATH, 0, s_dbDrvParams },
   { _T("DBLogin"), CT_STRING, 0, 0, MAX_DB_LOGIN, 0, s_dbLogin },
   { _T("DBName"), CT_STRING, 0, 0, MAX_DB_NAME, 0, s_dbName },
   { _T("DBPassword"), CT_STRING, 0, 0, MAX_PASSWORD, 0, s_dbPassword },
   { _T("DBEncryptedPassword"), CT_STRING, 0, 0, MAX_PASSWORD, 0, s_dbPassword },
   { _T("DBSchema"), CT_STRING, 0, 0, MAX_DB_NAME, 0, s_dbSchema },
   { _T("DBServer"), CT_STRING, 0, 0, MAX_PATH, 0, s_dbServer },
   { _T("ListenAddress"), CT_STRING, 0, 0, MAX_PATH, 0, g_listenAddress },
   { _T("ListenPortTCP"), CT_LONG, 0, 0, 0, 0, &g_tcpPort },
   { _T("ListenPortUDP"), CT_LONG, 0, 0, 0, 0, &g_udpPort },
   { _T("LogFile"), CT_STRING, 0, 0, MAX_PATH, 0, g_logFile },
   { _T("LogFailedSQLQueries"), CT_BOOLEAN, 0, 0, AF_LOG_SQL_ERRORS, 0, &g_flags },
   { _T("LogFile"), CT_STRING, 0, 0, MAX_PATH, 0, g_logFile },
   { _T(""), CT_END_OF_LIST, 0, 0, 0, 0, NULL }
};


//
// Load configuration file
//

static bool LoadConfig()
{
	bool success = false;

	Config *config = new Config();
	if (config->loadConfig(g_configFile, _T("nxflowd")) && config->parseTemplate(_T("nxflowd"), m_cfgTemplate))
   {
      if ((!_tcsicmp(g_logFile, _T("{EventLog}"))) ||
          (!_tcsicmp(g_logFile, _T("{syslog}"))))
      {
         g_flags |= AF_USE_SYSLOG;
      }
      else
      {
         g_flags &= ~AF_USE_SYSLOG;
      }
      success = true;
   }
	delete config;

	// Decrypt password
   DecryptPassword(s_dbLogin, s_dbPassword, s_dbPassword, MAX_PASSWORD);

   return success;
}

/**
 * Initialization
 */
bool Initialize()
{
   // Open log file
   nxlog_open((g_flags & AF_USE_SYSLOG) ? NXFLOWD_SYSLOG_NAME : g_logFile,
      ((g_flags & AF_USE_SYSLOG) ? NXLOG_USE_SYSLOG : 0) |
      ((g_flags & AF_DAEMON) ? 0 : NXLOG_PRINT_TO_STDOUT));

#ifdef _WIN32
	WSADATA wsaData;

   if (WSAStartup(2, &wsaData) != 0)
   {
      TCHAR buffer[1024];
      nxlog_write(NXLOG_ERROR, _T("Call to WSAStartup() failed (%s)"), GetSystemErrorText(WSAGetLastError(), buffer, 1024));
      return false;
   }
#endif

	if (ipfix_init() < 0)
	{
      nxlog_write(NXLOG_ERROR, _T("IPFIX library initialization failed"));
		return false;
	}

	// Initialize database driver and connect to database
	if (!DBInit())
		return false;
	g_dbDriverHandle = DBLoadDriver(s_dbDriver, s_dbDrvParams, (g_debugLevel >= 9), NULL, NULL);
	if (g_dbDriverHandle == NULL)
		return false;

	// Connect to database
	TCHAR errorText[DBDRV_MAX_ERROR_TEXT];
	for(int i = 0; ; i++)
	{
		g_dbConnection = DBConnect(g_dbDriverHandle, s_dbServer, s_dbName, s_dbLogin, s_dbPassword, s_dbSchema, errorText);
		if ((g_dbConnection != NULL) || (i == 5))
			break;
		ThreadSleep(5);
	}
	if (g_dbConnection == NULL)
	{
		nxlog_write(NXLOG_ERROR, _T("Cannot establish connection with database (%s)"), errorText);
		return FALSE;
	}
	nxlog_debug(1, _T("Successfully connected to database %s@%s"), s_dbName, s_dbServer);

	if (!StartCollector())
		return false;

	return true;
}

/**
 * Initiate shutdown from service control handler
 */
void Shutdown()
{
   // Set shutdowm flag
   g_flags |= AF_SHUTDOWN;

	WaitForCollectorThread();

	ipfix_cleanup();
   nxlog_close();

   // Notify main thread about shutdown
#ifdef _WIN32
   ConditionSet(m_hCondShutdown);
#endif

   // Remove PID file
#if !defined(_WIN32) && !defined(_NETWARE)
   remove(s_pidFile);
#endif
}

/**
 * Main loop
 */
void Main()
{
   nxlog_report_event(1, NXLOG_INFO, 0, _T("NetXMS NetFlow/IPFIX Collector started"));

   if (g_flags & AF_DAEMON)
   {
#if defined(_WIN32) || defined(_NETWARE)
      ConditionWait(m_hCondShutdown, INFINITE);
#else
      StartMainLoop(SignalHandler, NULL);
#endif
   }
   else
   {
#if defined(_WIN32)
      printf("NXFLOWD running. Press ESC to shutdown.\n");
      while(_getch() != 27);
      printf("NXFLOWD shutting down...\n");
      Shutdown();
#else
      printf("NXFLOWD running. Press Ctrl+C to shutdown.\n");
      StartMainLoop(SignalHandler, NULL);
      printf("\nStopping nxflowd...\n");
#endif
   }
}


//
// Help text
//

static TCHAR s_helpText[] =
   _T("NetXMS NetFlow/IPFIX Collector  Version ") NETXMS_VERSION_STRING _T("\n")
   _T("Copyright (c) 2009-2012 Raden Solutions\n\n")
   _T("Usage:\n")
   _T("   nxflowd [options]\n\n")
   _T("Where valid options are:\n")
   _T("   -c <file>  : Read configuration from given file\n")
   _T("   -d         : Start as daemon (service)\n")
	_T("   -D <level> : Set debug level (0..9)\n")
   _T("   -h         : Show this help\n")
#ifdef _WIN32
   _T("   -I         : Install service\n")
   _T("   -R         : Remove service\n")
   _T("   -s         : Start service\n")
   _T("   -S         : Stop service\n")
#else
   _T("   -p <file>  : Write PID to given file\n")
#endif
   _T("\n");


//
// Command line options
//

#ifdef _WIN32
#define VALID_OPTIONS    "c:dD:hIRsS"
#else
#define VALID_OPTIONS    "c:dD:hp:"
#endif


//
// main
//

int main(int argc, char *argv[])
{
	int ch, action = 0;
#ifdef _WIN32
	TCHAR moduleName[MAX_PATH];
#endif

   InitNetXMSProcess(false);

	// Parse command line
	opterr = 1;
	while((ch = getopt(argc, argv, VALID_OPTIONS)) != -1)
	{
		switch(ch)
		{
			case 'h':   // Display help and exit
				_putts(s_helpText);
				return 0;
			case 'd':   // Run as daemon
				g_flags |= AF_DAEMON;
				break;
			case 'D':   // Debug
				g_debugLevel = strtol(optarg, NULL, 0);
				break;
			case 'c':
#ifdef UNICODE
				MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, optarg, -1, g_configFile, MAX_PATH);
				g_configFile[MAX_PATH - 1] = 0;
#else
				nx_strncpy(g_configFile, optarg, MAX_PATH);
#endif
				break;
#ifdef _WIN32
			case 'I':
				action = 1;
				break;
			case 'R':
				action = 2;
				break;
			case 's':
				action = 3;
				break;
			case 'S':
				action = 4;
				break;
#else
			case 'p':
				nx_strncpy(s_pidFile, optarg, MAX_PATH);
				break;
#endif
			case '?':
				return 1;
			default:
				break;
		}
	}

#if !defined(_WIN32) && !defined(_NETWARE)
	if (!strcmp(g_configFile, "{search}"))
	{
		if (access(PREFIX "/etc/nxflowd.conf", 4) == 0)
		{
			strcpy(g_configFile, PREFIX "/etc/nxflowd.conf");
		}
		else if (access("/usr/etc/nxflowd.conf", 4) == 0)
		{
			strcpy(g_configFile, "/usr/etc/nxflowd.conf");
		}
		else
		{
			strcpy(g_configFile, "/etc/nxflowd.conf");
		}
	}
#endif

	switch(action)
	{
		case 0:  // Start server
			if (LoadConfig())
         {
#ifdef _WIN32
			   if (g_flags & AF_DAEMON)
			   {
				   InitService();
			   }
			   else
			   {
				   if (Initialize())
               {
					   Main();
               }
			   }
#else
			   if (g_flags & AF_DAEMON)
			   {
				   if (daemon(0, 0) == -1)
				   {
					   perror("Unable to setup itself as a daemon");
					   return 2;
				   }
			   }

			   if (Initialize())
			   {
				   FILE *pf;

				   pf = fopen(s_pidFile, "w");
				   if (pf != NULL)
				   {
					   fprintf(pf, "%d", getpid());
					   fclose(pf);
				   }
					Main();
			   }
#endif
         }
         else
         {
            fprintf(stderr, "Error loading configuration file\n");
            return 2;
         }
			break;
#ifdef _WIN32
		case 1:  // Install service
			GetModuleFileName(GetModuleHandle(NULL), moduleName, MAX_PATH);
			InstallFlowCollectorService(moduleName);
			break;
		case 2:
			RemoveFlowCollectorService();
			break;
		case 3:
			StartFlowCollectorService();
			break;
		case 4:
			StopFlowCollectorService();
			break;
#endif
		default:
			break;
	}

	return 0;
}
