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
	LK_NORMAL = 0,								// 状态正常
	LK_ERROR,										// 出现错误
	LK_TIMEOUT,									// 超时未活动
	LK_CLOSE,										// 需要关闭连接
	LK_EMPTY,										// 清空状态
};

// 连接管理
class LinkNode
{
private:
	char  m_strKey[MAX_LINK_KEY_LEN];						// 标识Key
	int fd;													// socket标识
	char IP[MAX_IP_LEN];									// Ip地址
	int Port;                                         						// 端口
	LinkState state;										// 链接状态
	time_t LastestAliveTime;								// 最近一次活动时间
	LargeMemoryCache *m_pReceiveBuf;						// 接收数据缓存
	LargeMemoryCache *m_pSendBuf;							// 发送数据缓存
	unsigned int m_iMarkIndex;								// 节点索引标识
	int m_iLatestSendDataPos;								// 最近的发送数据位置
	

private:
	// 处理已经收到的数据
	int ProcessRecData(const void*, const int);
	bool SendEmailThroughHttp(const char *);
	bool SendMobileThroughHttp(const char* ,const char *,bool);
	void CreateXmlData(char *,int ,ModuleToUser *,unsigned int ,char*);
	void CreateXmlDataforGroupSms(char *,int ,ModuleToUser *,unsigned int ,char*);
	
public:
	LinkNode(const char *, const int);
	LinkNode();
	~LinkNode();

// 继承接口
public:
	// 重新构造
	void ResetNode(const LinkNode &, const int);
	// 清除处理
	void Clear();
	// 获取标识ID
	const std::string GetMarkID()const;
	// 是否已经无效
	bool IsDead();
	// 发送缓存是否快满了
	bool IsFull()const;
	// 获取节点索引
	unsigned int GetNodeIndex();
	// 设置节点索引
	void SetNodeIndex(const unsigned int index);
	// 设置标识字符串
	void SetMarkID(std::string &key);


// 公有函数
public:
	// 返回链接socket关键字
	const int GetFd() const;
	// 重置socket关键字
	void UpdateSetFd(const int);
	// 返回上级服务器IP地址
	const char * GetIp()const;
	//返回上级服务器的连接端口
	const int GetPort()const;
	
	// 返回节点状态
	const LinkState GetLinkState()const;
	// 设置链接状态
	void SetState(const enum LinkState);
	// 更新节点最新活动时间
	void UpdateLastestAliveTime();
	// 获取活跃时间间隔
	const int GetAliveTimeInterval(const time_t);
	// 节点接收数据处理
	int NodeReceiveProcess();
	// 准备发送数据
	bool PushSendData(const void*, const int);
	// 发送缓存数据
	int SendLinkCacheData();
	
	// 是否可以接收
	bool CanReceive() const;
	// 是否可以发送数据
	bool CanSend()const;
	//解析http请求
	bool HttpParse(char *,CBuffWriter *);
	
};

#endif  /* _HQ_LINKE_NODE_H */

