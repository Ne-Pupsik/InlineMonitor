#ifndef INLINEHOOK_H
#define INLINEHOOK_H

#include <windows.h>
#include <iostream>
#include <vector>
#include <iomanip>
#include <capstone/capstone.h>

template <typename FuncSignature>
class InlineHook
{
public:
	InlineHook(void* target, FuncSignature detour) :
		targetAddr(target),
		hookFunc(detour),
		trampolineAddr(nullptr),
		hookSize(0),
		isInstalled(false),
		capstoneHandle(0),
		originalProtect(0)
	{
		cs_err err = cs_open(CS_ARCH_X86, CS_MODE_64, &capstoneHandle);
		if (err != CS_ERR_OK) {
			//std::cout << "cs_open failed: " << cs_strerror(err) << std::endl;
			capstoneHandle = 0;  // Чтобы избежать дальнейших ошибок
		}
		else {
			cs_option(capstoneHandle, CS_OPT_DETAIL, CS_OPT_ON);
		}
	}
	~InlineHook() {
		if (isInstalled) Uninstall();
		if (trampolineAddr) VirtualFree(trampolineAddr, 0, MEM_RELEASE);
		cs_close(&capstoneHandle);
	}
	bool Install() {
		if (isInstalled) return false;
		// 1. Дизассемблер
		if (!CalculateHookSize()) {
			//std::cout << "Failed: CalculateHookSize" << std::endl;
			return false;
		}
		// 2. Трамплин
		if (!AllocateTrampoline()) {
			//std::cout << "Failed: AllocateTrampoline" << std::endl;
			return false;
		}
		if (!BuildTrampoline()) {
			//std::cout << "Failed: BuildTrampoline" << std::endl;
			return false;
		}
		// 3. Сохраняем оригинал
		VirtualProtect(targetAddr, hookSize, PAGE_EXECUTE_READWRITE, &originalProtect);
		originalBytes.resize(hookSize);
		memcpy(originalBytes.data(), targetAddr, hookSize);
		// 4. Перезаписать оригинал
		WriteAbsoluteJump(targetAddr, reinterpret_cast<void*>(hookFunc));
		for (size_t i = 13; i < hookSize; ++i) {
			static_cast<BYTE*>(targetAddr)[i] = 0x90;
		}
		// 5. Старая защита
		DWORD tmp;
		VirtualProtect(targetAddr, hookSize, originalProtect, &tmp);
		isInstalled = true;

		return true;
	}
	bool Uninstall() {
		if (!isInstalled) return false;
		VirtualProtect(targetAddr, hookSize, PAGE_EXECUTE_READWRITE, &originalProtect);
		memcpy(targetAddr, originalBytes.data(), hookSize);
		DWORD temp;
		VirtualProtect(targetAddr, hookSize, originalProtect, &temp);
		isInstalled = false;
		return true;
	}
	FuncSignature GetOriginal() const {
		return reinterpret_cast<FuncSignature>(trampolineAddr);
	}
private:
	bool isInstalled;
	void* targetAddr;
	void* trampolineAddr;
	size_t hookSize;
	FuncSignature hookFunc;
	std::vector<BYTE> originalBytes;
	csh capstoneHandle;
	DWORD originalProtect;

	bool AllocateTrampoline() {
		SYSTEM_INFO sysInfo;
		GetSystemInfo(&sysInfo);
		uint64_t pageSize = sysInfo.dwPageSize;
		uint64_t start = ((uint64_t)targetAddr & ~(pageSize - 1)) - 0x7FFFFF00;  // В пределах ±2 ГБ
		uint64_t end = start + 0xFFFFFFFF;
		for (uint64_t addr = start; addr < end; addr += pageSize) {
			void* mem = VirtualAlloc((void*)addr, pageSize, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
			if (mem) {
				trampolineAddr = mem;
				return true;
			}
		}
		return false;
	}
	bool CalculateHookSize() {
		cs_insn* insn;

		DWORD tmpProtect;
		VirtualProtect(targetAddr, 64, PAGE_EXECUTE_READ, &tmpProtect);
		size_t count = cs_disasm(capstoneHandle, (uint8_t*)targetAddr, 32, (uint64_t)targetAddr, 0, &insn);
		VirtualProtect(targetAddr, 64, tmpProtect, &tmpProtect);

		if (count == 0) return false;
		hookSize = 0;
		for (size_t i = 0; i < count; ++i) {
			hookSize += insn[i].size;
			if (hookSize >= 13) break;  // Минимум для absolute jump
		}
		cs_free(insn, count);
		return hookSize >= 13;
	}
	bool IsRipRelative(cs_insn& inst) {
		cs_x86* x86 = &inst.detail->x86;
		for (uint8_t i = 0; i < x86->op_count; ++i) {
			cs_x86_op& op = x86->operands[i];
			if (op.type == X86_OP_MEM && op.mem.base == X86_REG_RIP) return true;
		}
		return false;
	}
	void RelocateRipRelative(cs_insn& inst, void* newAddr) {
		cs_x86* x86 = &inst.detail->x86;
		if (x86->encoding.disp_size != 4) {
			// Не обрабатываем, если размер displacement не 4 байта (типично для RIP-relative в x64)
			return;
		}
		int32_t disp = 0;
		memcpy(&disp, inst.bytes + x86->encoding.disp_offset, 4);  // 4-байтовый displacement
		disp -= ((uint64_t)newAddr - inst.address);
		memcpy(inst.bytes + x86->encoding.disp_offset, &disp, 4);
	}
	bool IsRelativeJump(cs_insn& inst) {
		return (inst.id >= X86_INS_JAE && inst.id <= X86_INS_JS) || inst.id == X86_INS_JMP;
	}
	bool IsRelativeCall(cs_insn& inst) {
		return inst.id == X86_INS_CALL && inst.bytes[0] == 0xE8;
	}
	size_t BuildAitForRelative(cs_insn& inst, uint8_t* aitMem, bool isCall) {
		uint64_t origTarget = inst.address + inst.size + *(int32_t*)(inst.bytes + 1);  // Для E9/E8
		if (isCall) {
			uint8_t callAbs[] = { 0x49, 0xBA, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x41,0xFF,0xD2 };  // mov r10, addr; call r10
			*(uint64_t*)(callAbs + 2) = origTarget;
			memcpy(aitMem, callAbs, 13);
			return 13;
		}
		else {
			WriteAbsoluteJump(aitMem, (void*)origTarget);
			return 13;
		}
	}
	void RewriteToShortJump(cs_insn& inst, uint8_t* instPtr, uint8_t* aitEntry) {
		uint8_t dist = (uint8_t)(aitEntry - (instPtr + inst.size));
		uint8_t shortJmp[2] = { 0xEB, dist };
		memset(inst.bytes, 0x90, inst.size);  // NOP остаток
		memcpy(inst.bytes, shortJmp, 2);
	}
	bool BuildTrampoline() {
		// Разбор stolen bytes
		cs_insn* insn;
		size_t count = cs_disasm(capstoneHandle, (uint8_t*)targetAddr, hookSize * 2, (uint64_t)targetAddr, 0, &insn);
		if (count == 0) return false;

		// Копируем байты в трамплин
		uint8_t* current = (uint8_t*)trampolineAddr;
		memcpy(current, targetAddr, hookSize);

		// Место для AIT после stolen bytes + jump back
		uint8_t* aitStart = (uint8_t*)trampolineAddr + hookSize + 13;  // +13 для jump back
		uint8_t* aitCurrent = aitStart;

		// Обработка каждой инструкции
		for (size_t i = 0; i < count && current < aitStart; ++i) {
			cs_insn& inst = insn[i];
			uint8_t* instPtr = current;
			if (IsRipRelative(inst)) {
				RelocateRipRelative(inst, (void*)((uint8_t*)trampolineAddr + (instPtr - (uint8_t*)trampolineAddr)));
				memcpy(instPtr, inst.bytes, inst.size);  // Перезапись relocated
			}
			else if (IsRelativeJump(inst) || IsRelativeCall(inst)) {
				size_t aitSize = BuildAitForRelative(inst, aitCurrent, IsRelativeCall(inst));
				RewriteToShortJump(inst, instPtr, aitCurrent);
				memcpy(instPtr, inst.bytes, inst.size);  // Перезапись
				aitCurrent += aitSize;
			}
			current += inst.size;
		}
		// Jump back в оригинал после hookSize
		WriteAbsoluteJump((void*)((uint8_t*)trampolineAddr + hookSize), (void*)((uint8_t*)targetAddr + hookSize));
		cs_free(insn, count);
		return true;
	}
	void WriteAbsoluteJump(void* mem, void* target) {
		uint8_t absJump[] = { 0x49, 0xBA, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x41,0xFF,0xE2 };  // mov r10, addr; jmp r10
		*(uint64_t*)(absJump + 2) = (uint64_t)target;
		memcpy(mem, absJump, 13);
	}
};

#endif // INLINEHOOK_H