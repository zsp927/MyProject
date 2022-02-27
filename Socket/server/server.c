//快捷方式
// 工具-错误查找：专门查看类型
// scanf改成scanf_s
//快速注释：ctrl+shift+/
// https://docs.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-socket
//#pragma warning(disable:6031)

#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <Winsock2.h>
#pragma comment(lib, "Ws2_32.lib")//没有64的版本
int main(void)
{
	//1.设定版本+打开网络库
	WORD wdVersion = MAKEWORD(2, 2); //2.2版本
	//int a = *((char*)&wdVersion);//低地址：主版本
	//int b = *((char*)&wdVersion+1);//高地址：副版本

	WSADATA wdScokMsg;//P或LP开头是指针类型
	int nRes = WSAStartup(wdVersion, &wdScokMsg);//打开网络库 
	if (0 != nRes)                   //514就是2.2版本
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
	if (2 != HIBYTE(wdScokMsg.wVersion) || 2 != LOBYTE(wdScokMsg.wVersion))//高版本+低版本
	{//上一行有一个不是2说明版本不对
		WSACleanup();//清理网络库
		printf("网络库版本不对");
		return 0;
	}

	//2.创建服务器socket
	SOCKET socketServer = socket(AF_INET, SOCK_STREAM, 0);//小写是函数，大写是类型//IPPROTO_TCP写成0
	if (INVALID_SOCKET == socketServer)//打开失败
	{
		int a = WSAGetLastError();//通过WSAGetLastError查看错误类型，得到错误码去查看（VS上或者MSDN）
		WSACleanup();//清理网络库
		return 0;
	}
	else printf("socket 创建成功\n");

	//3.bind绑定地址
	struct sockaddr_in si;//c语言需要加struct
	si.sin_family = AF_INET;//一定与socket的第一个参数匹配上
	si.sin_port = htons(12345);//用宏htons把端口号转换成规范的port   netstat -ano查看端口号
	si.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");//用宏inet_addr转换成规范的（这是在本机），要是再局域网下就是：用ipconfig看192.168.xxx，再输入

	if (SOCKET_ERROR == bind(socketServer, (const struct sockaddr*)&si, sizeof(si)))//成功是返回0，失败返回SOCKET_ERROR
	{										//不能修改
		int a = WSAGetLastError();//通过WSAGetLastError查看错误类型
		closesocket(socketServer);//一定是先关闭socket，再清理网络库
		WSACleanup();//清理网络库
		return 0;
	}

	//4.listen--建立链接了
	if (SOCKET_ERROR == listen(socketServer, SOMAXCONN))//出错了   正确返回0
	{
		int a = WSAGetLastError();
		closesocket(socketServer);//释放
		WSACleanup();//清理网络库
		return 0;
	}

	//5.accept--将客户端的信息创建成socket--与bind函数比较
	
	struct sockaddr_in clientMsg;//将客户端的信息进行接收，传入socketServer
	int len = sizeof(clientMsg);
	//法1 
	SOCKET socketClient = accept(socketServer,(struct sockaddr*)&clientMsg,&len);//SOCKET是数据类型
								//服务器+结构体（向外部穿函数。可以修改）也可以是NULL+长度也可以是NULL
	//法2---全部置空
	//17:00调试技巧
	//SOCKET socketClient = accept(socketServer, NULL, NULL);//SOCKET是数据类型
	//getpeername(socketClient, (struct sockaddr*)&socketClient, &len);
	if(INVALID_SOCKET == socketClient)//无效SOCKET
	{
		printf("客户链接失败\n");
		int a = WSAGetLastError();
		closesocket(socketServer);
		WSACleanup();
		return 0;//经验：必须加上，一旦bind失败，下面的都出错，需要return 0
	}
	else printf("客户端链接成功\n");//dos口是空的：阻塞功能：等待客户端的链接

	//6.先提前小发一下
	if (SOCKET_ERROR == send(socketClient, "server：收到了", sizeof("server：收到了"), 0))
	{//出错了
		int a = WSAGetLastError();
		//根据实际情况处理
		printf("%d\n", a);
	}
	while (1)
	{
		//7.recv
		//最后的步骤：收发消息
		//收recv   接收对方的socket
		char buf[1500] = { 0 };
		int res = recv(socketClient, buf, 1024, 0);//3种情况 //第四个参数MSG_WAITALL是0
		if (0 == res)					printf("链接中断、客户端下线\n");
		else if (SOCKET_ERROR == res) { int b = WSAGetLastError(); }//出错了//WSAGetLastError是得到错误码根据实际情况处理
		else							printf("%d %s\n", res, buf);//正确的

		scanf("%s", buf);

		//8.send
		//发给对方的socket
		if (SOCKET_ERROR == send(socketClient, buf, sizeof(buf), 0))//strlen遇到\0结束
		{//出错了
			printf("测试文字");
			int a = WSAGetLastError();
			printf("%d\n", a);
		}
	}

	//9.关闭
	closesocket(socketClient);//一定是先关闭socket，再清理网络库
	closesocket(socketServer);//一定是先关闭socket，再清理网络库
	WSACleanup();//清理网络库
	system("pause");
	return 0;
}