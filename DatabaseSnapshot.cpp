#include "DatabaseSnapshot.h"
#include "CppODBC.h"
#include "../../util/util.h"
#include "../../util/log.h"
#include "../../util/string_util.h"
#include "../../main/runtime.h"
#include "../../config.h"
#include <iostream>
#include <algorithm>

using namespace std;

static const char sWarnUserTable[] = "warning_userinfo";
static const char sWarnModuleTable[] = "warning_moduleinfo";
static const char sWarnTemplateTable[] = "warning_templateinfo";


DatabaseSnapshot *DatabaseSnapshot::m_sMainPointer = NULL;
DatabaseSnapshot *DatabaseSnapshot::m_sSubPointer = NULL;

DatabaseSnapshot::DatabaseSnapshot(const char*dsn, 
	const char*name, const char*pwd)
	: m_pCppODBC(new CppODBC())
{
	StrCopy(m_dsn, dsn, SHOT_LINK_LEN);
	StrCopy(m_uname, name, SHOT_LINK_LEN);
	StrCopy(m_upass, pwd, SHOT_LINK_LEN);
	
	InitialRes();
}

DatabaseSnapshot::~DatabaseSnapshot()
{
	if (NULL != m_pCppODBC)
	{
		m_pCppODBC->Close();
		delete m_pCppODBC;
		m_pCppODBC = NULL;
	}
}

// 初始化资源
void
DatabaseSnapshot::InitialRes()
{
	if (NULL != m_pCppODBC)
	{
	    m_pCppODBC->Initialize();
	    if(!m_pCppODBC->Open())
	    {
	    	ERROR("Failure - Open PublicNews EnvirHandle ODBC \n");
	    }
	}
}

bool
DatabaseSnapshot::DBconnect()
{	
	if (NULL != m_pCppODBC)
	{
		if (m_pCppODBC->Connect(m_dsn, m_uname, m_upass))
		{
			m_pCppODBC->SQLQuery("SET names utf8;");
			return true;
		}
	}
	
	ERROR("Connect data base error[%s-%s-%s]", m_dsn, m_uname, m_upass);

    return false;
}

// 断开数据库连接
bool
DatabaseSnapshot::DBdisconnect()
{
	if (NULL != m_pCppODBC)
	{
		return m_pCppODBC->DisConnect();
	}
	
    return false;
}
const int
DatabaseSnapshot::GetEailAndMobileInfo(char *ip,char *module,ModuleToUser *OutBuf,unsigned int size)
{
	const unsigned short MaxBufLen = 2048;
	char sql[MaxBufLen] = {0};
	int iReturn = snprintf(sql, MaxBufLen, 
		"select %s.uname,emails,mobiles,%s.templateID,%s.v_list from %s,%s,%s where \
		modelName=\'%s\' and %s.uname=%s.uname and %s.templateIDs= %s.templateID;",
		sWarnUserTable,sWarnTemplateTable,sWarnTemplateTable,sWarnUserTable,sWarnModuleTable,sWarnTemplateTable,
		module,sWarnUserTable,sWarnModuleTable,sWarnModuleTable,sWarnTemplateTable);
		
	if (iReturn > MaxBufLen)
	{
		DEBUG("MaxBufLen is small");
		return -1;
	}
	
	iReturn = SqlExeQuery(sql);
	if (iReturn < 0)
	{
		DEBUG("GetEailAndMobileInfo faild[%s]", sql);
	}
	else if (iReturn > 0)
	{
		int column,i=0;
		if(m_pCppODBC->GetCount()>size)
		{
			ERROR("Sql count from GetEailAndMobileInfo exceeded the max buffer size");
			return -1;
		}
		while (!m_pCppODBC->Eof())
		{
			column =0;
			StrCopy(OutBuf[i].IpAddr,ip,MAX_BUFFER_LEN);
			StrCopy(OutBuf[i].ModuleName,module,MAX_BUFFER_LEN);
			StrCopy(OutBuf[i].UserInfo,m_pCppODBC->GetStrValue(column++),MAX_BUFFER_LEN);
			StrCopy(OutBuf[i].EailAddr,m_pCppODBC->GetStrValue(column++),MAX_BUFFER_LEN);
			StrCopy(OutBuf[i].Mobile,m_pCppODBC->GetStrValue(column++),MAX_BUFFER_LEN);
			StrCopy(OutBuf[i].TemplateCode,m_pCppODBC->GetStrValue(column++),MAX_BUFFER_LEN);
			StrCopy(OutBuf[i].TemplateVar,m_pCppODBC->GetStrValue(column++),MAX_BUFFER_LEN);

			i++;
			m_pCppODBC->Next();
		}
	}
	return iReturn;
}


// 执行sql语句
const int 
DatabaseSnapshot::SqlExeOnly(const char *sql)
{
	int iRet = 0;
	iRet = m_pCppODBC->SQLExec(sql);
	if( ( iRet <= 0 ) && ( (unsigned int)-1 == m_pCppODBC->GetError() ) )
	{
		DBdisconnect();
		if (!DBconnect())
		{
			return -1;
		}
		
		iRet = m_pCppODBC->SQLExec(sql);
	}
	
	if( ( iRet <= 0 ) && ( (unsigned int)-1 == m_pCppODBC->GetError() ) )
	{
		return -2;
	}
	
	return iRet;
}

// 执行sql查询语句
const int 
DatabaseSnapshot::SqlExeQuery(const char *sql)
{
	int iRet = 0;
	iRet = m_pCppODBC->SQLQuery(sql);
	if( ( iRet <= 0 ) && ( (unsigned int)-1 == m_pCppODBC->GetError() ) )
	{
		DBdisconnect();
		if (!DBconnect())
		{
			return -1;
		}
		
		iRet = m_pCppODBC->SQLQuery(sql);
	}
	
	if( ( iRet <= 0 ) && ( (unsigned int)-1 == m_pCppODBC->GetError() ) )
	{
		return -2;
	}
	
	return iRet;
}


DatabaseSnapshot* 
DatabaseSnapshot::GetMain()
{
	if (NULL == m_sMainPointer)
	{
		const MySqlDbConfig *pDbConfig = GetDbSetting();
		m_sMainPointer = new DatabaseSnapshot(pDbConfig->DbName, pDbConfig->LogUser, pDbConfig->LogPwd);
	}
	
	return m_sMainPointer;
}

DatabaseSnapshot*
DatabaseSnapshot::GetSub()
{
	if (NULL == m_sSubPointer)
	{
		const MySqlDbConfig *pDbConfig = GetDbSetting();
		m_sSubPointer = new DatabaseSnapshot(pDbConfig->DbName, pDbConfig->LogUser, pDbConfig->LogPwd);
	}
	
	return m_sSubPointer;
}

void 
DatabaseSnapshot::Release()
{
	if (NULL != m_sMainPointer)
	{
		delete m_sMainPointer;
		m_sMainPointer = NULL;
	}
	
	if (NULL != m_sSubPointer)
	{
		delete m_sSubPointer;
		m_sSubPointer = NULL;
	}
}

