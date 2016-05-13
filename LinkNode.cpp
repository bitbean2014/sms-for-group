#include "LinkNode.h"
#include "LinkManage.h"
#include <sys/select.h> 
#include <sys/time.h>
#include <sys/types.h>
#include <curl/curl.h>
#include <curl/easy.h>
#include <unistd.h>
#include "../../util/util.h"
#include "../../util/aes.h"
#include "../../util/MyAes.h"
#include "../../util/time_util.h"
#include "../../util/sha1.h"
#include "../../util/socket_util.h"
#include "../../data_type.h"
#include "../../config.h"
#include "../../main/runtime.h"
#include "../../thread/NetThread.h"
#include "../db/DatabaseSnapshot.h"
#include "../../util/tinyxml.h"
#include "../../thread/ThreadsManage.h"

using namespace std;

const char *ErrorString[] =
{
	"miss ip address",
	"miss module name",
	"miss description",
	"miss title",
	"url error"
};

LinkNode::LinkNode(const char *ip, const int fd)
	: fd(fd),
	Port(-1),
	state(LK_NORMAL),
	m_pReceiveBuf(NULL),
	m_pSendBuf(NULL),
	m_iLatestSendDataPos(0)
	
{
	StrCopy(IP, ip, MAX_IP_LEN);
	bzero(m_strKey, MAX_LINK_KEY_LEN);
	UpdateLastestAliveTime();
}

LinkNode::LinkNode()
	: fd(-1),
	Port(-1),
	state(LK_NORMAL),
	m_pReceiveBuf(NULL),
	m_pSendBuf(NULL),
	m_iMarkIndex(0),
	m_iLatestSendDataPos(0)
{
	bzero(IP, MAX_IP_LEN);
}

LinkNode::~LinkNode()
{
	if (NULL != m_pReceiveBuf)
	{
		delete m_pReceiveBuf;
		m_pReceiveBuf = NULL;
	}

	if (NULL != m_pSendBuf)
	{
		delete m_pSendBuf;
		m_pSendBuf = NULL;
	}

}

void
LinkNode::UpdateLastestAliveTime()
{
	LastestAliveTime = GetNowTime();
}

// 获取活跃时间间隔
const int
LinkNode::GetAliveTimeInterval(const time_t NowTime)
{
	return (int)(NowTime - LastestAliveTime);
}

// 设置链接状态，CalOperatingThread线程调用
void
LinkNode::SetState(const enum LinkState NewState)
{
	state = NewState;
}

// 重新构造
void
LinkNode::ResetNode(const LinkNode &item, const int PosFlag)
{
	fd = item.fd;
	StrCopy(IP, item.IP, MAX_IP_LEN);
	Port = item.Port;
	state = item.state;
	m_iMarkIndex = item.m_iMarkIndex;
	m_iLatestSendDataPos = item.m_iLatestSendDataPos;


	if (NULL == m_pReceiveBuf)
	{
		m_pReceiveBuf = new LargeMemoryCache(MAX_SINGLE_REQUEST);
	}
	m_pReceiveBuf->ClearAll();

	if (NULL == m_pSendBuf)
	{
		m_pSendBuf = new LargeMemoryCache(MAX_SINGLE_RESPONSE);
	}
	m_pSendBuf->ClearAll();

		
	bzero(m_strKey, MAX_LINK_KEY_LEN);
	StrCopy(m_strKey, item.m_strKey, MAX_LINK_KEY_LEN);

	// 更新状态
	UpdateLastestAliveTime();
	SetState(LK_NORMAL);
}

// 清除处理
void
LinkNode::Clear()
{
	if (fd >= 0)
	{
		DEBUG("Close socket.[fd = %d, IP=%s]", fd, IP);
		int iSockFd = (int)fd;
		Close(iSockFd);
		fd = -1;
		SetState(LK_EMPTY);
	}
	
}

// 获取标识ID
const std::string
LinkNode::GetMarkID()const
{
	return m_strKey;
}

// 是否已经无效
bool
LinkNode::IsDead()
{
	if (LK_NORMAL != state)
	{
		return true;
	}

	time_t NowTime = GetNowTime();
	if (DiffTimeSecond(NowTime, LastestAliveTime) >= LINK_ALIVE_TIME)
	{
		DEBUG("LinkNode dead[%s]", IP);
		SetState(LK_TIMEOUT);
		return true;
	}

	return false;
}

// 节点接收数据处理
int
LinkNode::NodeReceiveProcess()
{
	int iRes = -1;
	
	do
	{
		// 申请内存
		int iBufLen = 0;
		char *pReceiveBuf = m_pReceiveBuf->GetRemainMem(iBufLen);
		if (NULL == pReceiveBuf || iBufLen <= 0)
		{
			DEBUG("Not enough memory.");
			break;
		}
		if (0 != m_pReceiveBuf->GetLatestWarning())
		{
			WARN("m_pReceiveBuf[%d  %s] maybe smaller.", GetFd(), IP);
		}
		
		iRes = recv(GetFd(), pReceiveBuf, iBufLen, 0);
		
		if (iRes <= 0)
		{
			if(!iRes ||(EWOULDBLOCK != errno && EINTR != errno))
			{
				ERROR("recv error [ip=%s errno=%s]", IP, strerror(errno));
				SetState(LK_CLOSE);
				return -2;
			}
			break;
		}		
		m_pReceiveBuf->SetRemainMemStart(iRes);
	
	} while(iRes > 0);
	
	if (m_pReceiveBuf->GetUsedMemoryLen() <= 0)
	{
		return -3;
	}
	else {
		// 更新存活时间
		//UpdateLastestAliveTime();
		if (0 != ProcessRecData(m_pReceiveBuf->GetRawMemPointer(0), m_pReceiveBuf->GetUsedMemoryLen())
			&& LK_CLOSE == GetLinkState())
		{
			return -4;
		}
	}
	return iRes;
}

// 处理已经收到的数据
int
LinkNode::ProcessRecData(const void *data, const int length)
{
	if (NULL == data || length <= 0)
	{
		m_pReceiveBuf->ClearUsedMem();
		return -1;
	}

	CBuffReader reader((void*)data, length, 0);
	
	
	//bool bReserveUnCompleteData = false;
	
	TRACE("ProcessRecData length=%d from ip = %s", length,GetIp());
	TRACE("%s",(char*)data);
	
	int iBufLen = 0;
	char *pBuf = m_pSendBuf->GetRemainMem(iBufLen);
	CBuffWriter writer(pBuf, iBufLen, 0);
	
	if(!HttpParse((char*)data,&writer))
	{
		ERROR("Parse http request failed,maybe missing something");
	}

	m_pSendBuf->SetRemainMemStart(writer.GetNewdataLen());

	return 0;
}

bool
LinkNode::HttpParse(char *data,CBuffWriter *writer)
{
	char ip[32] = {0};
	char module[32] = {0};
	char description[1024] = {0};
	char Title[128] = {0};
	char sendinfo[32] = {0};
	char reqbody[1024] = {0};
	char *src,*tmp;
	const char *ResponceOK="HTTP/1.1 200 OK\r\n\r\nOK\r\n";
	const char *ResponceFailStyle="HTTP/1.1 400 Failed\r\n\r\n\"%s\"\r\n";
	char *beg,*end,*parser;
	int ErrorCode;
	bool warnbysms = false;
	bool warnbygroupsms = false;
	int ret,i;
	char EmailUrl[1024]={0};
	char MobilePostFields[10000]={0};
	char xmldata[4096]={0};
	char xmldataAes[10000]={0};
	char senderId[128]={0};
	char RealTime[32]={0};
	char EmailTitle[128]={0};
	char *key = GetServerConfig()->AesKey;
	
	if(!(data&&writer))
	{
		ERROR("param null");
		return false;
	}
	//获取请求体
	src = data;
	beg = strstr(data,"/?");
	end = strstr(data,"HTTP");
	if(!(beg&&end))
	{
		ErrorCode = 4;
		DEBUG("url error");
		goto FORMATERROR;
	}
	if(*(end-1) == ' ')
		end -= 1;
	strncpy(reqbody,beg,end-beg);

	//解析ip字段
	parser = strstr(reqbody,"a=");
	if(!parser)
	{
		ErrorCode = 0;
		DEBUG("miss ip address");
		goto FORMATERROR;
	}
	tmp = strchr(parser,'&');
	if(tmp)
		strncpy(ip,parser+2,tmp-parser-2);
	else
		strcpy(ip,parser+2);

	//解析模块名称字段
	parser = strstr(reqbody,"n=");
	if(!parser)
	{
		ErrorCode = 1;
		DEBUG("miss module name");
		goto FORMATERROR;
	}
	tmp = strchr(parser,'&');
	if(tmp)
		strncpy(module,parser+2,tmp-parser-2);
	else
		strcpy(module,parser+2);

	//解析标题字段，非必选项
	parser = strstr(reqbody,"t=");
	if(parser)
	{
		tmp = strchr(parser,'&');
		if(tmp)
			strncpy(Title,parser+2,tmp-parser-2);
		else
			strcpy(Title,parser+2);
	}
	//解析描述字段
	parser = strstr(reqbody,"i=");
	if(!parser)
	{
		ErrorCode = 2;
		DEBUG("miss description ");
		goto FORMATERROR;
	}
	tmp = strchr(parser,'&');
	if(tmp)
		strncpy(description,parser+2,tmp-parser-2);
	else
		strcpy(description,parser+2);
	//解析sms字段，非必选项
	parser = strstr(reqbody,"s=");
	if(parser)
	{
		tmp = strchr(parser,'&');
		if(tmp)
			strncpy(sendinfo,parser+2,tmp-parser-2);
		else
			strcpy(sendinfo,parser+2);

		if(strcasecmp(sendinfo,"sms") == 0)
			warnbygroupsms = true;
		else
		if(strcasecmp(sendinfo,"sms2") == 0)
			warnbysms = true;
			
	}

	writer->Push_back(ResponceOK,strlen(ResponceOK));

	
	//通过模块名称从数据库中找出email和mobile信息
	ModuleToUser UserInfo[MAX_USERINFO_TO_SEND];
	bzero(UserInfo,sizeof(ModuleToUser)*MAX_USERINFO_TO_SEND);
	
	// 组织email url,通过http get 发送到远端send mail服务
	ret = DatabaseSnapshot::GetMain()->GetEailAndMobileInfo(ip,module,UserInfo,MAX_USERINFO_TO_SEND);

	if(ret <= 0)
	{
		ERROR("no data found in mysql,ret = [%d]",ret);
		return true;
	}
	
	sprintf(EmailTitle,"告警:该模块产生异常[%s:%s %s]",ip,module,Title);
	for(i=0;i<ret;i++)
	{
		CURL *curl = curl_easy_init();
		char *easy_url = curl_easy_escape(curl,description,strlen(description));
		char *easy_title = curl_easy_escape(curl,EmailTitle,strlen(EmailTitle));
		bzero(EmailUrl,1024);
		sprintf(EmailUrl,"%s/?ip=%s&title=%s&email=%s&description=%s",
			GetServerConfig()->EmailServer,ip,easy_title,UserInfo[i].EailAddr,easy_url);
		curl_free(easy_url);
		curl_free(easy_title);
		SendEmailThroughHttp(EmailUrl);	
	}
	// 组织mobile url ，发送到短信通道

	if(warnbysms)
	{
		CreateXmlData(xmldata,sizeof(xmldata),UserInfo,ret,description);

		//AES 加密，使用PKCS5Padding方式
		if(strlen(xmldata)>0)
		{
			AES aes((unsigned char*)key);
			
			aes.Bm53Cipher(GetServerConfig()->senderId,senderId);
			
			aes.Bm53Cipher(xmldata,xmldataAes);

			aes.Bm53Cipher((char*)"NRT",RealTime);

			sprintf(MobilePostFields,"senderId=%s&smsContent=%s&smsRealtime=%s",senderId,xmldataAes,RealTime);
			
			SendMobileThroughHttp(GetServerConfig()->MobileServer,MobilePostFields,false);
		}
		return true;
	}

	if(warnbygroupsms)
	{
		CreateXmlDataforGroupSms(xmldata,sizeof(xmldata),UserInfo,ret,description);

		if(strlen(xmldata)>0)
		{
			char *base64_str=base64_encode(xmldata,strlen(xmldata));
			SendMobileThroughHttp(GetServerConfig()->MobileServer2,base64_str,true);
		}
		return true;
	}

	return true;
	FORMATERROR:
		char response[1028] = {0};
		sprintf(response,ResponceFailStyle,ErrorString[ErrorCode]);
		writer->Push_back(response,strlen(response));
		return false;

	return true;
	

	
}

void
LinkNode::CreateXmlDataforGroupSms(char *OutData,int OutLen,ModuleToUser *Users,unsigned int UserNum,char *description)
{
	TiXmlDocument doc; 
	TiXmlPrinter smsprinter,fileprinter;
	TiXmlElement * tmp = NULL;
	TiXmlDeclaration * decl = new TiXmlDeclaration("1.0", "UTF-8", "");
	TiXmlElement * root = new TiXmlElement("bisdata"); 
	tmp = new TiXmlElement("reqId");
	tmp->LinkEndChild(new TiXmlText("P1000863"));
	root->LinkEndChild(tmp);

	tmp = new TiXmlElement("serviceId");
	tmp->LinkEndChild(new TiXmlText("1000220000000101"));
	root->LinkEndChild(tmp);

	//生成bizdata
	TiXmlElement *bizdata = new TiXmlElement("bizdata");
	root->LinkEndChild(bizdata);
	
	TiXmlElement *data = new TiXmlElement("data");

	//判断是否有电话号码，需要发送短信
	int checkmobile = 0;
	smsprinter.SetIndent(0);
	smsprinter.SetLineBreak("");

	char currenttime[32]={0};
	GetTimestamp(currenttime);
	
	for(unsigned int i=0;i<UserNum;i++)
	{
		if(strlen(Users[i].Mobile)==0)
			continue;
		else
			checkmobile++;
		
		TiXmlElement *sms,*phoneNo,*taskCode,*params;
		sms = new TiXmlElement("sms");
		phoneNo = new TiXmlElement("phoneNo");
		phoneNo->LinkEndChild(new TiXmlText(Users[i].Mobile));
		sms->LinkEndChild(phoneNo);

		taskCode= new TiXmlElement("taskCode");
		taskCode->LinkEndChild(new TiXmlText(Users[i].TemplateCode));
		sms->LinkEndChild(taskCode);

		char parameter[1024] = {0};
		char WarnMessage[1024] = {0};
		
		sprintf(WarnMessage,"[%s] [%s] [%s] %s.",currenttime,Users[i].IpAddr,Users[i].ModuleName,description);
		sprintf(parameter,"{\"%s\":\"%s\"}",Users[i].TemplateVar,WarnMessage);

		params = new TiXmlElement("params");
		params->LinkEndChild(new TiXmlText(parameter));

		sms->LinkEndChild(params);

		sms->Accept(&smsprinter);
		
		data->LinkEndChild(sms);
		
	}

	bizdata->LinkEndChild(data);
	
	//生成标签sign
	TiXmlElement *sign = new TiXmlElement("sign");
	TiXmlElement *idx = new TiXmlElement("idx");
	idx->LinkEndChild(new TiXmlText("01"));

	sign->LinkEndChild(idx);
	//使用sha1 encode   PzY3w8ES5Iprp02TMYtcRg==
	const char * key= "NsHaYZXaeE5xhCX8";

	char SHA1data[1024] = {0};
	char SHA1dataout[1024] = {0};
	//把data节点里的内容打印出来
	
	sprintf(SHA1data,"%s%s",key,smsprinter.CStr());

	SHA1 sha1;
	sha1.SHA_GO(SHA1data,SHA1dataout);
	//aes1加密
	//AES aes((unsigned char*)GetServerConfig()->AesKey);
	//std::string sha1value = aes.sha1(SHA1data);

	TiXmlElement *value = new TiXmlElement("value");

	value->LinkEndChild(new TiXmlText(SHA1dataout));
	sign->LinkEndChild(value);

	bizdata->LinkEndChild(sign);

	//获取整个xml内容，打印出来
	doc.LinkEndChild(decl);  
  	doc.LinkEndChild(root);

	fileprinter.SetIndent(0);
	fileprinter.SetLineBreak("");
	doc.Accept(&fileprinter);

	if(strlen(fileprinter.CStr())>=(unsigned int)OutLen)
	{
		DEBUG("OutLen too small ,buffer len = %d, outlen = %d",strlen(fileprinter.CStr()),OutLen);
		return ;
	}
	if(checkmobile>0)
		strcpy(OutData,fileprinter.CStr());

	return ;
}

void 
LinkNode::CreateXmlData(char *OutData,int OutLen,ModuleToUser *Users,unsigned int UserNum,char *description)
{
	TiXmlDocument doc; 
	TiXmlDeclaration * decl = new TiXmlDeclaration("1.0", "UTF-8", "");
	TiXmlElement * root = new TiXmlElement("LPMS_Common"); 
	TiXmlElement * request = new TiXmlElement("Request");
	root->LinkEndChild(request);
	TiXmlElement * list = new TiXmlElement("list");
	request->LinkEndChild(list);
	

	int i,requestid,checkmobile=0;
	srand(time(NULL));
	char tmpdata[1024]={0};

	// 获取当前时间，格式为hhmmss 整数
	char currenttime[32]={0};
	GetTimestamp(currenttime);
	
	for(i=0;i<(int)UserNum;i++)
	{
		if(strlen(Users[i].Mobile)==0)
			continue;
		else
			checkmobile++;
		
		TiXmlElement * sms = new TiXmlElement("sms");
		list->LinkEndChild(sms);
		
		requestid = rand()%RANDMAX;
		sprintf(tmpdata,"%d",requestid);
		TiXmlElement *ReqId= new TiXmlElement("RequestID");
		ReqId->LinkEndChild(new TiXmlText(tmpdata));
		sms->LinkEndChild(ReqId);
		
		TiXmlElement *TmplateCode = new TiXmlElement("TemplateCode");
		TmplateCode->LinkEndChild(new TiXmlText(Users[i].TemplateCode));
		sms->LinkEndChild(TmplateCode);

		TiXmlElement *MobileNo = new TiXmlElement("MobileNo");
		MobileNo->LinkEndChild(new TiXmlText(Users[i].Mobile));
		sms->LinkEndChild(MobileNo);
		
		TiXmlElement *ExpireDate = new TiXmlElement("ExpireDate");
		sms->LinkEndChild(ExpireDate);

		TiXmlElement *ArrangeDate = new TiXmlElement("ArrangeDate");
		sms->LinkEndChild(ArrangeDate);

		char ContentBefore[1024]={0};
		char *ContentAfter = NULL;
		char WarnMessage[512]={0};
		
		sprintf(WarnMessage,"[%s] [%s] [%s] %s",currenttime,Users[i].IpAddr,Users[i].ModuleName,description);
		sprintf(ContentBefore,"{\"%s\":\"%s\"}",Users[i].TemplateVar,WarnMessage);

		ContentAfter = base64_encode(ContentBefore,strlen(ContentBefore));
		TiXmlElement * SmsTemplateVar= new TiXmlElement("SmsTemplateVar");
		SmsTemplateVar->LinkEndChild(new TiXmlText(ContentAfter));
		free(ContentAfter);

		sms->LinkEndChild(SmsTemplateVar);

		TiXmlElement * ActivityCode = new TiXmlElement("ActivityCode");
		sms->LinkEndChild(ActivityCode);
		TiXmlElement * Level = new TiXmlElement("Level");
		Level->LinkEndChild(new TiXmlText("L3"));
		sms->LinkEndChild(Level);
	}

	
	TiXmlPrinter printer;
	//printer.SetIndent(0);
	doc.LinkEndChild(decl);  
    doc.LinkEndChild(root);

	doc.Accept(&printer);
	//doc.SaveFile("xmldata.xml");
	
	if(strlen(printer.CStr())>=(unsigned int)OutLen)
	{
		DEBUG("OutLen too small ,buffer len = %d, outlen = %d",strlen(printer.CStr()),OutLen);
		return ;
	}
	if(checkmobile>0)
		strcpy(OutData,printer.CStr());

	return ;
	
}

// 通过http get方式发送到email
bool 
LinkNode::SendEmailThroughHttp(const char *EmailUrl)
{
	CURL * curl;
	CURLcode code;
	curl_global_init(CURL_GLOBAL_ALL);

	curl = curl_easy_init();

	curl_easy_setopt(curl, CURLOPT_URL,EmailUrl);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
	
	code = curl_easy_perform(curl);

	curl_easy_cleanup(curl);
	

	if (code != CURLE_OK)
	{
		ThreadsManage::GetNetThread()->AddToCurlResendList(EmailUrl);
		DEBUG("curl_easy_perform failed ,result [%d] for [%s]",code,EmailUrl);
		return false;
	}
	else
		return true;
}


bool
LinkNode::SendMobileThroughHttp(const char* MobileUrl,const char *PostFields,bool https)
{
	if(!MobileUrl)
	{
		ERROR("MobileUrl NULL");
		return false;
	}
	CURL * curl;
	CURLcode code;
	curl_global_init(CURL_GLOBAL_ALL);

	curl = curl_easy_init();
	FILE *fptr;
	const char * filename = "result.txt";
	if ((fptr = fopen(filename, "ab+")) == NULL) {  
        fprintf(stderr, "fopen file error: %s\n", filename);  
       return false;  
    }  
	curl_easy_setopt(curl, CURLOPT_URL, MobileUrl);   
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, PostFields);   
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);   
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fptr);   
    curl_easy_setopt(curl, CURLOPT_POST, 1);   
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1); 
	if(https)
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER,FALSE);
	//
    curl_easy_setopt(curl, CURLOPT_HEADER, 1);   
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);  
	
	code = curl_easy_perform(curl);  
    curl_easy_cleanup(curl); 
	fclose(fptr);

	if (code != CURLE_OK)
	{
		DEBUG("curl_easy_perform failed ,result [%d] for [%s]",code,MobileUrl);
		return false;
	}
	else
		return true;
}

// 准备发送数据
bool
LinkNode::PushSendData(const void *data, const int length)
{
	int iBufLen = 0;
	
	char *pReceiveBuf = m_pSendBuf->GetRemainMem(iBufLen);
	if (NULL == pReceiveBuf || iBufLen <= 0 || iBufLen < length)
	{
		DEBUG("Not enough memory.");
		
		return false;
	}
	CBuffWriter writer(pReceiveBuf, iBufLen, 0);
	if (!writer.Push_back((void*)data, length))
	{
		DEBUG("Push_back failed.");
		
		return false;
	}
	m_pSendBuf->SetRemainMemStart(writer.GetNewdataLen());
		
	return true;
}

// 发送缓存数据
int
LinkNode::SendLinkCacheData()
{
	if (NULL == m_pSendBuf)
	{
		return -1;
	}
	if (m_pSendBuf->GetUsedMemoryLen() <= 0)
	{
		return -2;
	}

	
	int iSendLen = 0;
	int iDataLen = 0;
	char *pData = m_pSendBuf->GetPosData(m_iLatestSendDataPos, iDataLen);
	if (NULL == pData || 0 >= iDataLen)
	{
		
		return -3;
	}

	if ((iSendLen = send(GetFd(), pData, iDataLen, 0)) <= 0)
	{
		if (EWOULDBLOCK != errno && EINTR != errno)
		{
			//连接需要关闭
			DEBUG("send error found [ip=%s errno=%s]", IP, strerror(errno));
			SetState(LK_CLOSE);
			m_iLatestSendDataPos = 0;
			m_pSendBuf->ClearUsedMem();
			
			return 0;
		}
		else
		{
			
			return -4;
		}
	}

	// 判断发送结果
	if (iSendLen < iDataLen)
	{
		DEBUG("Send Part[m_iLatestSendDataPos=%d iSendLen=%d iDataLen=%d]", 
			m_iLatestSendDataPos, iSendLen, iDataLen);
		m_iLatestSendDataPos += iSendLen;
	}
	else
	{
		m_iLatestSendDataPos = 0;
		m_pSendBuf->ClearUsedMem();
	}
		
	return iSendLen;
	
}

// 是否可以接收
bool
LinkNode::CanReceive() const
{
	if (LK_NORMAL != state)
	{
		return false;
	}

	if (m_iLatestSendDataPos > 0 && 0 != m_pSendBuf->GetLatestWarning())
	{
		return false;
	}

	if (0 != m_pReceiveBuf->GetLatestWarning())
	{
		return false;
	}

	return true;
}
//是否数据快满了
bool
LinkNode::IsFull()const
{
	int CurrentSize = m_pSendBuf->GetUsedMemoryLen();
	int BufferSize = m_pSendBuf->GetRawMemLength();

	return (BufferSize-CurrentSize<1024);
}

// 是否可以发送数据
bool
LinkNode::CanSend()const
{
	if (LK_NORMAL != state)
	{
		return false;
	}

	if (m_pSendBuf->GetUsedMemoryLen() <= 0)
	{
		return false;
	}

	return true;
}


// 获取节点索引											
unsigned int 
LinkNode::GetNodeIndex()								
{
	return m_iMarkIndex;
}

// 设置节点索引
void
LinkNode::SetNodeIndex(const unsigned int index)				
{
	m_iMarkIndex = index;
}

// 设置标识字符串
void 
LinkNode::SetMarkID(std::string &key)						
{
	StrCopy(m_strKey, key.c_str(), MAX_LINK_KEY_LEN);
}

// 返回链接socket关键字
const int 
LinkNode::GetFd() const						
{
	return fd;
}

// 重置socket关键字
void
LinkNode::UpdateSetFd(const int Sock)
{
	fd = Sock;
}

// 返回上级服务器IP地址
const char * 
LinkNode::GetIp()const							
{
	return IP;
}

//返回上级服务器的连接端口
const int
LinkNode::GetPort()const                        
{
	return Port;
}



// 返回节点状态
const LinkState
LinkNode::GetLinkState()const					
{
	return state;
}


