#pragma once
#include <Windows.h>
#include <iostream>
#include <TlHelp32.h>
#include <vector>
#include <thread>
#include <fstream>
#include <string>
#include <Psapi.h>
#include <memory>

namespace functions
{
	std::string GetLastErrorStr(); // From https://stackoverflow.com/questions/1387064/how-to-get-the-error-message-from-the-error-code-returned-by-getlasterror
}

class Memory
{
public:
	HANDLE handle;
	DWORD id;
	uintptr_t base;

	std::string ProcName;

public:
	std::vector<LPVOID> allocatedMemory;
	std::vector<HANDLE> heldMutex;

	bool attached;

public:
	static DWORD GetIdByName(const char* procName);
	static uintptr_t GetBaseAddress(const char* moduleName);

	bool Attach(const char* procName, bool waitForProcess);
	void Detach();

	// i made it a function cuz im bored typing it every time i use this library
	void WaitForAttach();

	bool ChangeMemoryPage(const uintptr_t& address, const DWORD& newProtect, SIZE_T& size, DWORD& oldProtect);
	bool IsMemoryOk(const uintptr_t& address);

	LPVOID AllocateMemory(size_t size);
	bool FreeMemory(const uintptr_t& address);

	bool m_CreateMutex(const std::string& mutexName, const LPSECURITY_ATTRIBUTES& attributes = 0, const bool& initialOwner = 1);

	template <typename Ty>
	Ty ReadMemory(const uintptr_t& address, bool checkOk = false)
	{
		Ty buffer = {};
		if (!this->handle) return buffer;

		if (checkOk)
		{
			if (!IsMemoryOk(address)) return false;
		}

		if (!ReadProcessMemory(this->handle, addy, &buffer, sizeof(buffer), nullptr)) {
			buffer = {};
		}

		return buffer;
	}

	template <typename Ty>
	bool WriteMemory(const uintptr_t& address, const Ty& buffer)
	{
		if (!this->handle) return false;
		if (address == 0) return false;
		if (!IsMemoryOk(address)) return false;

		return WriteProcessMemory(this->handle, reinterpret_cast<LPVOID>(address), &buffer, sizeof(buffer), nullptr) != 0;
	}

	template <typename Ty>
	bool WriteMemory(const uintptr_t& address, const Ty& buffer, size_t size)
	{
		if (!this->handle) return false;
		if (address == 0) return false;
		if (!IsMemoryOk(address)) return false;

		return WriteProcessMemory(this->handle, reinterpret_cast<LPVOID>(address), &buffer, size, nullptr) != 0;
	}


	static bool KillProcess(const char* processName = "this");

	bool StandardInject(const std::string& path);


public:
	Memory(const char* procName = "", bool waitForProcess = false);
	~Memory();
	bool TheCheck();
};
inline std::unique_ptr<Memory> memory;