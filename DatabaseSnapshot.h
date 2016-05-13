#ifndef DATABASESNAPSHOT_H
#define DATABASESNAPSHOT_H

#include "time.h"
#include <string>
#include <vector>


class CppODBC;
struct ModuleToUser;
#define SHOT_LINK_LEN 32
#define MAX_BUFFER_LEN 128
class DatabaseSnapshot
{
private:
	// 用于调用odbc函数
	CppODBC * m_pCppODBC;
	// ODBC数据源
	char m_dsn[SHOT_LINK_LEN];
	// 用户名
	char m_uname[SHOT_LINK_LEN];
	// 密码
	char m_upass[SHOT_LINK_LEN];
	// 主
	static DatabaseSnapshot *m_sMainPointer;
	// 副
	static DatabaseSnapshot *m_sSubPointer;

private:
    DatabaseSnapshot(const char*, const char*, const char*);
    ~DatabaseSnapshot();
    // 初始化资源
	void InitialRes();
    // 建立数据库连接
	bool DBconnect();
	// 断开数据库连接
	bool DBdisconnect();
	// 执行sql语句
	const int SqlExeOnly(const char*);
	// 执行sql查询语句
	const int SqlExeQuery(const char*);
	
public:
	// 从远端myql中获取用户信息
	const int GetEailAndMobileInfo(char *,char *,ModuleToUser *,unsigned int );
	static DatabaseSnapshot* GetMain();
	static DatabaseSnapshot* GetSub();
	static void Release();
};

#endif // DATABASESNAPSHOT_H
