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
	// ���ڵ���odbc����
	CppODBC * m_pCppODBC;
	// ODBC����Դ
	char m_dsn[SHOT_LINK_LEN];
	// �û���
	char m_uname[SHOT_LINK_LEN];
	// ����
	char m_upass[SHOT_LINK_LEN];
	// ��
	static DatabaseSnapshot *m_sMainPointer;
	// ��
	static DatabaseSnapshot *m_sSubPointer;

private:
    DatabaseSnapshot(const char*, const char*, const char*);
    ~DatabaseSnapshot();
    // ��ʼ����Դ
	void InitialRes();
    // �������ݿ�����
	bool DBconnect();
	// �Ͽ����ݿ�����
	bool DBdisconnect();
	// ִ��sql���
	const int SqlExeOnly(const char*);
	// ִ��sql��ѯ���
	const int SqlExeQuery(const char*);
	
public:
	// ��Զ��myql�л�ȡ�û���Ϣ
	const int GetEailAndMobileInfo(char *,char *,ModuleToUser *,unsigned int );
	static DatabaseSnapshot* GetMain();
	static DatabaseSnapshot* GetSub();
	static void Release();
};

#endif // DATABASESNAPSHOT_H
