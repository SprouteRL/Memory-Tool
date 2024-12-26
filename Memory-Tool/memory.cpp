#include "memory.h"

DWORD Memory::GetIdByName(const char* procName)
{
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnap == INVALID_HANDLE_VALUE) return false;

	PROCESSENTRY32 pe = { sizeof(PROCESSENTRY32) };

	if (Process32First(hSnap, &pe))
	{
		do
		{
#ifdef UNICODE
			wchar_t wProcName[MAX_PATH];
			size_t convertedChars = 0;
			mbstowcs_s(&convertedChars, wProcName, MAX_PATH, procName, _TRUNCATE); // Safe conversion
			if (wcscmp(pe.szExeFile, wProcName) == 0)
#else
			if (strcmp(pe.szExeFile, procName) == 0)
#endif
			{
				return pe.th32ProcessID;
			}
		} while (Process32Next(hSnap, &pe));
	}
	CloseHandle(hSnap);
	return 0;
}

uintptr_t Memory::GetBaseAddress(const char* moduleName)
{
	uintptr_t dwModuleBaseAddress = 0;
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, id);
	MODULEENTRY32 ModuleEntry32 = { 0 };
	ModuleEntry32.dwSize = sizeof(MODULEENTRY32);

	if (Module32First(hSnapshot, &ModuleEntry32))
	{
		do {
#ifdef UNICODE
			wchar_t wModuleName[MAX_PATH];
			size_t convertedChars = 0;
			mbstowcs_s(&convertedChars, wModuleName, MAX_PATH, moduleName, _TRUNCATE);

			//std::wcout << ModuleEntry32.szExePath << ": 0x" << std::hex << ModuleEntry32.modBaseAddr << std::dec << "\n";

			if (wcscmp(ModuleEntry32.szModule, wModuleName) == 0)
#else
			if (strcmp(ModuleEntry32.szModule, moduleName) == 0)
#endif
			{

				dwModuleBaseAddress = (uintptr_t)ModuleEntry32.modBaseAddr;
				break;
			}
		} while (Module32Next(hSnapshot, &ModuleEntry32));
	}
	CloseHandle(hSnapshot);
	return dwModuleBaseAddress;
}

// if anyone wants organize this
int Memory::Attach(const char* procName, bool waitForProcess)
{
	if (strlen(procName) == 0)
	{
		attached = false;
		return ReturnFlags::FAILED;
	}

	id = GetIdByName(procName);

	if (id == 0)
	{
		attached = false;
		return ReturnFlags::FAILED_FIND_PROGRAM;
	}

	if (waitForProcess)
	{
		while (true)
		{
			id = GetIdByName(procName);

			if (id == 0)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(100)); // this is not ai made i promise bruh 😭
				continue;
			}

			handle = OpenProcess(PROCESS_ALL_ACCESS, 0, id);
			if (handle != nullptr)
			{
				attached = true;
				return true;
			}

		}
	}
	else
	{
		handle = OpenProcess(PROCESS_ALL_ACCESS, 0, id);
		attached = handle != nullptr;
		return attached;
	}
}

void Memory::Detach()
{
	if (handle)
	{
		CloseHandle(handle);
	}

	if (!allocatedMemory.empty())
	{
		for (auto& allocatedMem : allocatedMemory)
		{
			FreeMemory((uintptr_t)allocatedMem);
		}
	}
	if (!heldMutex.empty())
	{
		for (auto& mutex : heldMutex)
		{
			ReleaseMutex(mutex);
			CloseHandle(mutex);
		}
	}

	attached = false;
}

void Memory::WaitForAttach()
{
	do
	{
		std::this_thread::sleep_for(std::chrono::seconds(0));
	} while (!attached);
}

bool Memory::ChangeMemoryPage(const uintptr_t& address, const DWORD& newProtect, SIZE_T& size, DWORD& oldProtect)
{
	if (!handle) return false;

	PVOID baseAddress = reinterpret_cast<PVOID>(address);
	SIZE_T regionSize = size;

	BOOL status = VirtualProtectEx(handle, &baseAddress, size, newProtect, &oldProtect);
	return status == 0;
}
bool Memory::IsMemoryOk(const uintptr_t& address)
{
	MEMORY_BASIC_INFORMATION mem_info;
	if (VirtualQueryEx(handle, reinterpret_cast<const void*>(address), &mem_info, sizeof(mem_info)) == sizeof(mem_info))
		return mem_info.State == MEM_COMMIT && (mem_info.Type == MEM_PRIVATE || mem_info.Type == MEM_MAPPED);

	return false;
}

LPVOID Memory::AllocateMemory(size_t size)
{
	LPVOID allocated = VirtualAllocEx(handle, nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (allocated == 0)
	{
		return 0;
	}

	allocatedMemory.push_back(allocated);
	return allocated;
}

bool Memory::FreeMemory(const uintptr_t& address)
{
	if (VirtualFreeEx(handle, (LPVOID)address, 0, MEM_RELEASE))
	{
		auto it = std::find(allocatedMemory.begin(), allocatedMemory.end(), (LPVOID)address);
		if (it != allocatedMemory.end())
		{
			allocatedMemory.erase(it);
		}

		return true;
	}
	else
	{
		return false;
	}
}

bool Memory::m_CreateMutex(const std::string& mutexName, const LPSECURITY_ATTRIBUTES& attributes, const bool& initialOwner)
{
	HANDLE mutex = CreateMutexA(attributes, initialOwner, mutexName.c_str());
	if (mutex != nullptr)
	{
		heldMutex.push_back(mutex);
		return true;
	}
	return false;
}

bool Memory::KillProcess(const char* processName)
{
	if (processName == "this")
	{
		ExitProcess(0);
	}

	std::unique_ptr<Memory> mem = std::make_unique<Memory>(processName);
	return TerminateProcess(mem->handle, 0);
}

bool Memory::StandardInject(const std::string& path)
{
	/*if (handle == nullptr)
	{
		std::cout << "Handle no.\n";
		return false;
	}

	std::fstream x(path);
	if (!x.good())
	{
		std::cerr << "Failed to locate DLL. Error: " << utilsFuncs::GetLastErrorStr() << "\n";
		return false;
	}
	x.close();

	void* allocatedMem = this->AllocateMemory(path.length());
	if (allocatedMem == nullptr)
	{
		std::cerr << "Failed to allocate memory. Last thread error: " << utilsFuncs::GetLastErrorStr() << "\n";
		system("pause");
		return 1;
	}
	if (!memory->WriteMemory(reinterpret_cast<uintptr_t>(allocatedMem), path.c_str(), path.length() + 1))
	{
		std::cerr << "Failed to write memory. Last thread error: " << utilsFuncs::GetLastErrorStr() << "\n";
		system("pause");
		return 1;
	}

	HANDLE remoteThread = CreateRemoteThread(handle, NULL, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(LoadLibraryA), allocatedMem, 0, NULL);
	if (remoteThread == INVALID_HANDLE_VALUE) {
		std::cerr << "Failed to get create remote thread.\n";
		system("pause");
		return 1;
	}

	WaitForSingleObject(remoteThread, INFINITE);

	DWORD threadExitCode;
	if (!GetExitCodeThread(remoteThread, &threadExitCode))
	{
		std::cerr << "failed to get exit code" << utilsFuncs::GetLastErrorStr() << "\n";
		CloseHandle(remoteThread);
		return false;
	}

	CloseHandle(remoteThread);

	return threadExitCode != 0; */
	return true;
}

Memory::Memory(const char* procName, bool waitForProcess)
{
	attached = false;

	if (strlen(procName) != 0)
	{
		ProcName = procName;
		Attach(procName, waitForProcess);
	}
}

Memory::~Memory()
{
	Detach();
}

bool Memory::TheCheck()
{
	return !this->ProcName.empty() && attached;
}


std::string functions::GetLastErrorStr()
{
	DWORD lastErrorId = GetLastError();
	if (lastErrorId == 0)
	{
		return "No error was found.\n";
	}

	LPSTR messageBuffer = nullptr;

	//Ask Win32 to give us the string version of that message ID.
	//The parameters we pass in, tell Win32 to create the buffer that holds the message for us (because we don't yet know how long the message string will be).
	size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, lastErrorId, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

	//Copy the error message into a std::string.
	std::string message(messageBuffer, size);

	//Free the Win32's string's buffer.
	LocalFree(messageBuffer);

	return message;
}
