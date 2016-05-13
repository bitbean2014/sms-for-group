#ifndef _HQ_LINKE_NODE_H
#define _HQ_LINKE_NODE_H

#include "../template/NodeInterface.h"
#include "../template/ThreadMutex.h"
#include "../template/NodeThreadMutex.h"
#include "../../data_type.h"
#include "../../thread/ThreadsManage.h"
#include "../../util/util.h"
#include "../../util/log.h"
#include "../../util/common_types.h"
#include "../../util/string_util.h"
#include "../../util/BuffServer.h"
#include "../../main/runtime.h"
#include <string>
#include <queue>

class LargeMemoryCache;
class LinkManage;

enum LinkState {
	LK_NORMAL = 0,								// ״̬����
	LK_ERROR,										// ���ִ���
	LK_TIMEOUT,									// ��ʱδ�
	LK_CLOSE,										// ��Ҫ�ر�����
	LK_EMPTY,										// ���״̬
};

// ���ӹ���
class LinkNode
{
private:
	char  m_strKey[MAX_LINK_KEY_LEN];						// ��ʶKey
	int fd;													// socket��ʶ
	char IP[MAX_IP_LEN];									// Ip��ַ
	int Port;                                         						// �˿�
	LinkState state;										// ����״̬
	time_t LastestAliveTime;								// ���һ�λʱ��
	LargeMemoryCache *m_pReceiveBuf;						// �������ݻ���
	LargeMemoryCache *m_pSendBuf;							// �������ݻ���
	unsigned int m_iMarkIndex;								// �ڵ�������ʶ
	int m_iLatestSendDataPos;								// ����ķ�������λ��
	

private:
	// �����Ѿ��յ�������
	int ProcessRecData(const void*, const int);
	bool SendEmailThroughHttp(const char *);
	bool SendMobileThroughHttp(const char* ,const char *,bool);
	void CreateXmlData(char *,int ,ModuleToUser *,unsigned int ,char*);
	void CreateXmlDataforGroupSms(char *,int ,ModuleToUser *,unsigned int ,char*);
	
public:
	LinkNode(const char *, const int);
	LinkNode();
	~LinkNode();

// �̳нӿ�
public:
	// ���¹���
	void ResetNode(const LinkNode &, const int);
	// �������
	void Clear();
	// ��ȡ��ʶID
	const std::string GetMarkID()const;
	// �Ƿ��Ѿ���Ч
	bool IsDead();
	// ���ͻ����Ƿ������
	bool IsFull()const;
	// ��ȡ�ڵ�����
	unsigned int GetNodeIndex();
	// ���ýڵ�����
	void SetNodeIndex(const unsigned int index);
	// ���ñ�ʶ�ַ���
	void SetMarkID(std::string &key);


// ���к���
public:
	// ��������socket�ؼ���
	const int GetFd() const;
	// ����socket�ؼ���
	void UpdateSetFd(const int);
	// �����ϼ�������IP��ַ
	const char * GetIp()const;
	//�����ϼ������������Ӷ˿�
	const int GetPort()const;
	
	// ���ؽڵ�״̬
	const LinkState GetLinkState()const;
	// ��������״̬
	void SetState(const enum LinkState);
	// ���½ڵ����»ʱ��
	void UpdateLastestAliveTime();
	// ��ȡ��Ծʱ����
	const int GetAliveTimeInterval(const time_t);
	// �ڵ�������ݴ���
	int NodeReceiveProcess();
	// ׼����������
	bool PushSendData(const void*, const int);
	// ���ͻ�������
	int SendLinkCacheData();
	
	// �Ƿ���Խ���
	bool CanReceive() const;
	// �Ƿ���Է�������
	bool CanSend()const;
	//����http����
	bool HttpParse(char *,CBuffWriter *);
	
};

#endif  /* _HQ_LINKE_NODE_H */

