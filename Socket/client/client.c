#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <WinSock2.h>
#pragma comment(lib,"Ws2_32.lib")
int main()
{
	//打开网络库
	WORD wdVersion = MAKEWORD(2,2);
	WSADATA wdScokMsg;
	int nRes = WSAStartup(wdVersion, &wdScokMsg);//打开网络库
	if (0 != nRes)
	{
		switch (nRes)
		{
		case WSASYSNOTREADY:
			printf("重启下电脑试试，或者检查网络库");
			break;
		case WSAVERNOTSUPPORTED:
			printf("请更新网络库");
			break;
		case WSAEINPROGRESS:
			printf("请重新启动");
			break;
		case WSAEPROCLIM:
			printf("请尝试关掉不必要的软件，以为当前网络运行提供充足资源");
			break;
		}
		return 0;
	}
	else printf("网络库打开成功\n");

	//校验版本
	if (2 != HIBYTE(wdScokMsg.wHighVersion) || 2 != LOBYTE(wdScokMsg.wVersion))
	{
		printf("Version fail!");
		WSACleanup();//关闭库
		return -1;
	}
	//创建一个SOCKET 监听
	//这个参数一定要和  服务器的类型匹配上
	SOCKET socketServer = socket(AF_INET,SOCK_STREAM, 0);//还是创建服务器的socket，因为创建自己的socket没有用（客户是主动，没有人链客户）服务器会有很多链它，它很忙，IPPROTO_TCP
	//这个就是i产生错误了
	if (INVALID_SOCKET == socketServer)
	{
		printf("socket fail!");
		int a = WSAGetLastError();//通过WSAGetLastError查看错误类型
		WSACleanup();
		return -1;
	}
	else  printf("socket 创建成功\n");

//以上一样

	//主动链接服务器

	//初始化服务端的地址， 那么这个 是配置的服务器端的，一定要跟服务器一样，不然就练补上了
	struct sockaddr_in serverMsg;//服务器信息
	serverMsg.sin_family = AF_INET;//还是TCP：必须与服务器类型匹配
	serverMsg.sin_port = htons(12345);//必须是服务器的端口号！！！都是1234把端口号转换成网络字节序   
	serverMsg.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");//不能随便写ip地址,必须与服务器匹配
	//客户端主动连接服务端  这个函数就是链接函数
	if (SOCKET_ERROR == connect(socketServer,(struct sockaddr*)&serverMsg,sizeof(serverMsg)))//connect是把参数1和参数2绑定在一起，再去链接服务器
	{
		printf("connect fail!" );
		closesocket(socketServer);//关闭客户Socket
		WSACleanup();//关闭库
		return -1;
	}

//以下一样

	//发送消息给服务器
	while(1)
	{
		char buf[1500] = { 0 };
		int res = recv(socketServer, buf, 1024, 0);//3种情况 //第四个参数是0，MSG_WAITALL是等待字节数满了才发送
		if (0 == res)					printf("链接中断、客户端下线\n");
		else if (SOCKET_ERROR == res) { int b = WSAGetLastError(); }//出错了//WSAGetLastError是得到错误码根据实际情况处理
		else							printf("%d   %s\n", res, buf);//正确的

		scanf("%s", buf);

		if (SOCKET_ERROR == send(socketServer, buf, sizeof(buf), 0))//向服务器发消息
		{
			int a = WSAGetLastError();//出错了
			printf("%d\n", a);//根据实际情况处理
		}
	}

	closesocket(socketServer);
	WSACleanup();
	system("pause");
	return 0;
}