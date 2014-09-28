#include "stdafx.h"
#include "WinsockHooker.h"

#include <iostream>
#include <sstream>
#include <memory>
#include <cassert>
#include "detours.h"
#include "process.h"

#include "ScopeResourceHandle.h"

WinsockHooker::socket_FuncType WinsockHooker::Real_socket = ::socket;
WinsockHooker::connect_FuncType WinsockHooker::Real_connect = ::connect;
WinsockHooker::closesocket_FuncType WinsockHooker::Real_closesocket = ::closesocket;
WinsockHooker::WSASocket_FuncType WinsockHooker::Real_WSASocket = ::WSASocket;

XMLib::CriticalSection WinsockHooker::cs;
std::set<SOCKET> WinsockHooker::TcpSocketSet;
bool WinsockHooker::IsHooked = false;

#define UUU_MSG_LOG(MSG)

SOCKET WSAAPI WinsockHooker::Hooked_socket(int af, int type, int protocol)
{
	SOCKET s = Real_socket(af, type, protocol);
	int lastError = ::WSAGetLastError();
	if(s != INVALID_SOCKET) {
		// IPv4 Stream TCP Firefox使用IPPROTO_HOPOPTS
		if(af == AF_INET &&
			type == SOCK_STREAM &&
			(protocol == IPPROTO_TCP || protocol == IPPROTO_HOPOPTS)) {
				// 满足这些条件的加入表中
				XMLib::CriticalSectionLockGuard lck(cs);
				TcpSocketSet.insert(s);
		}
	}
	::WSASetLastError(lastError);
	return s;
}

SOCKET WSAAPI WinsockHooker::Hooked_WSASocket(int af, int type, int protocol, LPWSAPROTOCOL_INFO lpProtocolInfo, GROUP g, DWORD dwFlags)
{
	SOCKET s = Real_WSASocket(af, type, protocol, lpProtocolInfo, g, dwFlags);
	int lastError = ::WSAGetLastError();
	if(s != INVALID_SOCKET) {
		// IPv4 Stream TCP Firefox使用IPPROTO_HOPOPTS
		if(af == AF_INET &&
			type == SOCK_STREAM &&
			(protocol == IPPROTO_TCP || protocol == IPPROTO_HOPOPTS)) {
				// 满足这些条件的加入表中
				XMLib::CriticalSectionLockGuard lck(cs);
				TcpSocketSet.insert(s);
		}
	}
	::WSASetLastError(lastError);
	return s;
}

static unsigned int proxy_port = 15868;
struct LocalPortToRemoveAddress {
	USHORT local_port;
	USHORT remote_port;
	ULONG remote_ip;
	HANDLE hWaitForQueryEvent;
};

unsigned __stdcall WaitForQueryRemoteAddressThreadProc(void *arg)
{
	std::auto_ptr<LocalPortToRemoveAddress> sp_arg(reinterpret_cast<LocalPortToRemoveAddress*>(arg));
	DWORD dwWaitResult = ::WaitForSingleObject(sp_arg->hWaitForQueryEvent, 5000);
	::CloseHandle(sp_arg->hWaitForQueryEvent);
	sp_arg->hWaitForQueryEvent = NULL;
	if(dwWaitResult != WAIT_OBJECT_0) {
		return 0;
	}
	std::wstring fileMappingName = L"Local\\GreenSheildPDIPCSharedMemory_";
	std::wstring local_port_str;
	{
		std::wstringstream wss;
		wss << sp_arg->local_port;
		wss >> local_port_str;
	}
	fileMappingName += local_port_str;
	{
		HANDLE hFileMapping = ::OpenFileMapping(FILE_MAP_WRITE, FALSE, fileMappingName.c_str());
		if(hFileMapping == NULL) {
			return 0;
		}
		// 自动关闭内存文件映射
		ScopeResourceHandle<HANDLE, BOOL (WINAPI*)(HANDLE)> autoCloseFileMapping(hFileMapping, ::CloseHandle);

		char* sharedMemeryBuffer = reinterpret_cast<char*>(::MapViewOfFile(hFileMapping, FILE_MAP_WRITE, 0, 0, 256));
		if(sharedMemeryBuffer == NULL) {
			return 0;
		}

		// 自动关闭内存文件视图
		ScopeResourceHandle<LPCVOID, BOOL (WINAPI*)(LPCVOID)> autoUnmapViewOfFile(sharedMemeryBuffer, ::UnmapViewOfFile);
		char ip_and_port[6];

		assert(sizeof(unsigned long) == 4 * sizeof(char) && sizeof(unsigned short) == 2 * sizeof(char));

		*reinterpret_cast<unsigned long*>(&ip_and_port) = sp_arg->remote_ip;
		*reinterpret_cast<unsigned short*>(&ip_and_port[4]) = sp_arg->remote_port;		

		std::memcpy(sharedMemeryBuffer, ip_and_port, sizeof(ip_and_port));
	}

	std::wstring ackEventName = L"Local\\GreenSheildPDIPCSyncAckEvent_";
	ackEventName += local_port_str;
	HANDLE hAckEvent = ::OpenEvent(EVENT_MODIFY_STATE, FALSE, ackEventName.c_str());
	if(hAckEvent == NULL) {
		return 0;
	}
	::SetEvent(hAckEvent);
	::CloseHandle(hAckEvent);
	return 0;
}

int WSAAPI WinsockHooker::Hooked_connect(SOCKET s, const struct sockaddr *name, int namelen)
{
	USHORT remote_port = 0;
	ULONG remote_ip = 0;
	bool is_ipv4_tcp = false;
	{
		XMLib::CriticalSectionLockGuard lck(cs);
		is_ipv4_tcp = TcpSocketSet.find(s) != TcpSocketSet.end();
	}

	if(is_ipv4_tcp && namelen == sizeof(sockaddr_in) && reinterpret_cast<const sockaddr_in*>(name)->sin_family == AF_INET) {
		remote_port = ntohs(reinterpret_cast<const sockaddr_in*>(name)->sin_port);
		remote_ip = reinterpret_cast<const sockaddr_in*>(name)->sin_addr.s_addr;
		is_ipv4_tcp = reinterpret_cast<const sockaddr_in*>(name)->sin_family == AF_INET;
	}

	if(is_ipv4_tcp && (remote_port >= 1024 || remote_port == 80) && IsEnable()) {
		unsigned short local_port = 0;
		sockaddr_in local_addr;
		std::memset(&local_addr, 0, sizeof(local_addr));
		int local_addr_len = sizeof(local_addr);
		if(getsockname(s, reinterpret_cast<sockaddr*>(&local_addr), &local_addr_len) == 0 && local_addr_len == sizeof(local_addr)) {
			local_port = ntohs(reinterpret_cast<const sockaddr_in*>(&local_addr)->sin_port);
		}
		if(local_port == 0) {
			local_addr.sin_family = AF_INET;
			local_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
			local_addr.sin_port = 0;
			if(bind(s, reinterpret_cast<const sockaddr*>(&local_addr), sizeof(local_addr)) == 0) {
				std::memset(&local_addr, 0, sizeof(local_addr));
				local_addr_len = sizeof(local_addr);
				if(getsockname(s, reinterpret_cast<sockaddr*>(&local_addr), &local_addr_len) == 0 && local_addr_len == sizeof(local_addr)) {
					local_port = ntohs(reinterpret_cast<const sockaddr_in*>(&local_addr)->sin_port);
				}
			}
		}
		if(local_port != 0) {
			std::wstring eventName = L"Local\\GreenSheildPDIPCSyncEvent_";
			std::wstring port_str;
			{
				std::wstringstream wss;
				wss << local_port;
				wss >> port_str;
				eventName += port_str;
			}
			HANDLE hEvent = ::CreateEvent(NULL, TRUE, FALSE, eventName.c_str());
			if(hEvent != NULL) {
				sockaddr_in proxy_addr;
				proxy_addr.sin_family = AF_INET;
				proxy_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
				proxy_addr.sin_port = htons(proxy_port);
				int connect_result = Real_connect(s, reinterpret_cast<const sockaddr*>(&proxy_addr), sizeof(proxy_addr));
				DWORD dwLastError = ::WSAGetLastError();
				if(connect_result != 0 &&  dwLastError == 0) {
					dwLastError = WSAEWOULDBLOCK;
				}
				LocalPortToRemoveAddress* args = new LocalPortToRemoveAddress();
				args->hWaitForQueryEvent = hEvent;
				args->local_port = local_port;
				args->remote_ip = remote_ip;
				args->remote_port = remote_port;
				HANDLE hThread = reinterpret_cast<HANDLE>(_beginthreadex(NULL, 0, WaitForQueryRemoteAddressThreadProc, reinterpret_cast<void*>(args), 0, NULL));
				if(hThread == NULL) {
					::CloseHandle(args->hWaitForQueryEvent);
					delete hThread;
				}
				else {
					::CloseHandle(hThread);
				}
				::WSASetLastError(dwLastError);
				return connect_result;
			}
		}
	}
	return Real_connect(s, name, namelen);
}

int WSAAPI WinsockHooker::Hooked_closesocket(SOCKET s)
{
	{
		XMLib::CriticalSectionLockGuard lck(cs);
		std::set<SOCKET>::const_iterator iter = TcpSocketSet.find(s);
		if(iter != TcpSocketSet.end()) {
			TcpSocketSet.erase(iter);
		}
	}
	return Real_closesocket(s);
}

bool WinsockHooker::AttachHook()
{
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(reinterpret_cast<LPVOID*>(&Real_socket), Hooked_socket);
	DetourAttach(reinterpret_cast<LPVOID*>(&Real_WSASocket), Hooked_WSASocket);
	DetourAttach(reinterpret_cast<LPVOID*>(&Real_connect), Hooked_connect);
	DetourAttach(reinterpret_cast<LPVOID*>(&Real_closesocket), Hooked_closesocket);
	DetourTransactionCommit();
	IsHooked = true;
	return true;
}

void WinsockHooker::DetachHook()
{
	if(IsHooked) {
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourDetach(reinterpret_cast<LPVOID*>(&Real_socket), Hooked_socket);
		DetourDetach(reinterpret_cast<LPVOID*>(&Real_WSASocket), Hooked_WSASocket);
		DetourDetach(reinterpret_cast<LPVOID*>(&Real_connect), Hooked_connect);
		DetourDetach(reinterpret_cast<LPVOID*>(&Real_closesocket), Hooked_closesocket);
		DetourTransactionCommit();
		IsHooked = false;
	}
}

bool WinsockHooker::IsEnable()
{
	HANDLE hFileMapping = ::OpenFileMapping(FILE_MAP_READ, FALSE, L"Local\\{1469EA0A-0606-4C68-B120-062DC9CAD0C7}GSFilterEnable");
	if(hFileMapping == NULL) {
		return false;
	}
	// 自动关闭内存文件映射
	ScopeResourceHandle<HANDLE, BOOL (WINAPI*)(HANDLE)> autoCloseFileMapping(hFileMapping, ::CloseHandle);

	char* sharedMemeryBuffer = reinterpret_cast<char*>(::MapViewOfFile(hFileMapping, FILE_MAP_READ, 0, 0, 256));
	if(sharedMemeryBuffer == NULL) {
		return false;
	}

	// 自动关闭内存文件视图
	ScopeResourceHandle<LPCVOID, BOOL (WINAPI*)(LPCVOID)> autoUnmapViewOfFile(sharedMemeryBuffer, ::UnmapViewOfFile);
	return sharedMemeryBuffer[0] == 'G' && sharedMemeryBuffer[1] == 'S' && sharedMemeryBuffer[2] == '\x01';
}
