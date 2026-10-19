#include "SystemCalls.hpp"

int sys_map_path(char** paths, char** new_paths, int num)
{
   system_call_4(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_MAP_PATHS, (uint64_t)(uint32_t)paths, (uint64_t)(uint32_t)new_paths, (uint64_t)num);
   return_to_user_prog(int);
}

int cobra_load_vsh_plugin(unsigned int slot, char* path, void* arg, uint32_t arg_size)
{
   system_call_5(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_LOAD_VSH_PLUGIN, (uint64_t)slot, (uint64_t)(uint32_t)path, (uint64_t)arg, arg_size);
   return_to_user_prog(int);
}

int cobra_unload_vsh_plugin(unsigned int slot)
{
   system_call_2(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_UNLOAD_VSH_PLUGIN, slot);
   return_to_user_prog(int);
}

int ps3mapi_get_core_version()
{
   system_call_2(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_PS3MAPI, PS3MAPI_OPCODE_GET_CORE_VERSION);
   return_to_user_prog(int);
}

int ps3mapi_get_core_minversion()
{
   system_call_2(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_PS3MAPI, PS3MAPI_OPCODE_GET_CORE_MINVERSION);
   return_to_user_prog(int);
}

int ps3mapi_get_fw_type(char* fw)
{
   system_call_3(SC_COBRA_SYSCALL8, (uint64_t)SYSCALL8_OPCODE_PS3MAPI, (uint64_t)PS3MAPI_OPCODE_GET_FW_TYPE, (uint64_t)((uint32_t)fw));
   return_to_user_prog(int);
}

int ps3mapi_get_fw_version()
{
   system_call_2(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_PS3MAPI, PS3MAPI_OPCODE_GET_FW_VERSION);
   return_to_user_prog(int);
}

int ps3mapi_get_all_process_modules_prx_id(sys_pid_t pid, sys_prx_id_t* prx_id_list)
{
   system_call_4(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_PS3MAPI, PS3MAPI_OPCODE_GET_ALL_PROC_MODULE_PID, (uint64_t)pid, (uint64_t)prx_id_list);
   return_to_user_prog(int);
}

int ps3mapi_get_process_module_name_by_prx_id(sys_pid_t pid, sys_prx_id_t prx_id, char* name)
{
   system_call_5(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_PS3MAPI, PS3MAPI_OPCODE_GET_PROC_MODULE_NAME, (uint64_t)pid, (uint64_t)prx_id, (uint64_t)name);
   return_to_user_prog(int);
}

int ps3mapi_get_process_module_filename_by_prx_id(sys_pid_t pid, sys_prx_id_t prx_id, char* filename)
{
   system_call_5(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_PS3MAPI, PS3MAPI_OPCODE_GET_PROC_MODULE_FILENAME, (uint64_t)pid, (uint64_t)prx_id, (uint64_t)filename);
   return_to_user_prog(int);
}

int ps3mapi_load_process_modules(sys_pid_t pid, char* path, void* arg, uint32_t arg_size)
{
   system_call_6(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_PS3MAPI, PS3MAPI_OPCODE_LOAD_PROC_MODULE, (uint64_t)pid, (uint64_t)((uint32_t)path), (uint64_t)((uint32_t)arg), (uint64_t)arg_size);
   return_to_user_prog(int);
}

int ps3mapi_unload_process_modules(sys_pid_t pid, sys_prx_id_t prx_id)
{
   system_call_4(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_PS3MAPI, PS3MAPI_OPCODE_UNLOAD_PROC_MODULE, (uint64_t)pid, (uint64_t)prx_id);
   return_to_user_prog(int);
}

int ps3mapi_unload_vsh_plugin(char* name)
{
   system_call_3(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_PS3MAPI, PS3MAPI_OPCODE_UNLOAD_VSH_PLUGIN, (uint64_t)((uint32_t)name));
   return_to_user_prog(int);
}

int ps3mapi_get_vsh_plugin_info(unsigned int slot, char* name, char* filename)
{
   system_call_5(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_PS3MAPI, PS3MAPI_OPCODE_GET_VSH_PLUGIN_INFO, (uint64_t)slot, (uint64_t)((uint32_t)name), (uint64_t)((uint32_t)filename));
   return_to_user_prog(int);
}

int ps3mapi_load_kernel_plugin(uint8_t* payload, int size)
{
   system_call_3(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_RUN_PAYLOAD, (uint64_t)payload, size);
   return_to_user_prog(uint32_t);
}

int ps3mapi_load_dynamic_kernel_plugin(uint8_t* payload, int size, uint64_t* residence)
{
   system_call_4(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_RUN_PAYLOAD_DYNAMIC, (uint64_t)payload, size, (uint64_t)residence);
   return_to_user_prog(uint32_t);
}

int ps3mapi_unload_dynamic_kernel_plugin(uint64_t residence)
{
   system_call_2(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_UNLOAD_PAYLOAD_DYNAMIC, residence);
   return_to_user_prog(uint32_t);
}

int ps3mapi_get_is_hen()
{
   system_call_1(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_IS_HEN);
   return_to_user_prog(int);
}

int ps3mapi_get_hen_rev()
{
   system_call_1(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_HEN_REV);
   return_to_user_prog(int);
}

int ps3mapi_poke_hen(uint64_t address, uint64_t value)
{
   system_call_3(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_POKE_LV2, address, value);
   return_to_user_prog(int);
}

int ps3mapi_process_page_allocate(sys_pid_t pid, uint64_t size, uint64_t page_size, uint64_t flags, uint64_t is_executable, uint64_t* page_table)
{
   system_call_8(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_PS3MAPI, PS3MAPI_OPCODE_PROC_PAGE_ALLOCATE, (uint64_t)pid, (uint64_t)size, (uint64_t)page_size, (uint64_t)flags, (uint64_t)is_executable, (uint64_t)page_table);
   return_to_user_prog(int);
}

int ps3mapi_process_page_free(sys_pid_t pid, uint64_t flags, uint64_t* page_table)
{
    system_call_5(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_PS3MAPI, PS3MAPI_OPCODE_PROC_PAGE_FREE, (uint64_t)pid, (uint64_t)flags, (uint64_t)page_table);
    return_to_user_prog(int);
}

int ps3mapi_get_process_module_segments(sys_pid_t pid, sys_prx_id_t prx_id, sys_prx_module_info_t* info)
{
   system_call_5(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_PS3MAPI, PS3MAPI_OPCODE_GET_PROC_MODULE_SEGMENTS, (uint64_t)pid, (uint64_t)prx_id, (uint64_t)info);
   return_to_user_prog(int);
}

int ps3mapi_create_process_thread(sys_pid_t pid, thread_t* thread, void* entry, uint64_t arg, int prio, size_t stacksize, const char* threadname)
{
   system_call_8(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_PROC_CREATE_THREAD, (uint64_t)pid, (uint64_t)thread, (uint64_t)entry, (uint64_t)arg, (uint64_t)prio, (uint64_t)stacksize, (uint64_t)threadname);
   return_to_user_prog(int);
}

int ps3mapi_get_all_processes_pid(uint32_t* pid_list)
{
   system_call_3(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_PS3MAPI, PS3MAPI_OPCODE_GET_ALL_PROC_PID, (uint64_t)((uint32_t)pid_list));
   return_to_user_prog(int);
}

int ps3mapi_get_process_name_by_pid(uint32_t pid, char* pid_name)
{
   system_call_4(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_PS3MAPI, PS3MAPI_OPCODE_GET_PROC_NAME_BY_PID, (uint64_t)((uint32_t)pid), (uint64_t)((uint32_t)pid_name));
   return_to_user_prog(int);
}

int ps3mapi_get_process_by_pid(uint32_t pid, process_t process)
{
   system_call_4(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_PS3MAPI, PS3MAPI_OPCODE_GET_PROC_BY_PID, (uint64_t)pid, (uint64_t)((uint32_t)process));
   return_to_user_prog(int);
}

int ps3mapi_get_current_process_critical(process_t process)
{
   system_call_3(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_PS3MAPI, PS3MAPI_OPCODE_GET_CURRENT_PROC_CRIT, (uint64_t)((uint32_t)process));
   return_to_user_prog(int);
}

int ps3mapi_get_current_process(process_t process)
{
   system_call_3(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_PS3MAPI, PS3MAPI_OPCODE_GET_CURRENT_PROC, (uint64_t)((uint32_t)process));
   return_to_user_prog(int);
}

int ps3mapi_check_syscall(int num)
{
   system_call_3(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_PS3MAPI, PS3MAPI_OPCODE_CHECK_SYSCALL, (uint64_t)num);
   return_to_user_prog(int);
}

int ps3mapi_disable_syscall(int num)
{
   system_call_3(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_PS3MAPI, PS3MAPI_OPCODE_DISABLE_SYSCALL, (uint64_t)num);
   return_to_user_prog(int);
}

int ps3mapi_pdisable_syscall8(int mode)
{
   system_call_3(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_PS3MAPI, PS3MAPI_OPCODE_PDISABLE_SYSCALL8, (uint64_t)mode);
   return_to_user_prog(int);
}

int ps3mapi_pcheck_syscall8()
{
   system_call_2(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_PS3MAPI, PS3MAPI_OPCODE_PCHECK_SYSCALL8);
   return_to_user_prog(int);
}

int ps3mapi_get_idps(uint64_t* idps)
{
   system_call_3(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_PS3MAPI, PS3MAPI_OPCODE_GET_IDPS, (uint64_t)((uint32_t)idps));
   return_to_user_prog(int);
}

int ps3mapi_set_idps(uint64_t part1, uint64_t part2)
{
   system_call_4(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_PS3MAPI, PS3MAPI_OPCODE_SET_IDPS, (uint64_t)part1, (uint64_t)part2);
   return_to_user_prog(int);
}

int ps3mapi_get_psid(uint64_t* psid)
{
   system_call_3(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_PS3MAPI, PS3MAPI_OPCODE_GET_PSID, (uint64_t)((uint32_t)psid));
   return_to_user_prog(int);
}

int ps3mapi_set_psid(uint64_t part1, uint64_t part2)
{
   system_call_4(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_PS3MAPI, PS3MAPI_OPCODE_SET_PSID, (uint64_t)part1, (uint64_t)part2);
   return_to_user_prog(int);
}

int ps3mapi_remove_hook(void)
{
   system_call_2(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_PS3MAPI, PS3MAPI_OPCODE_REMOVE_HOOK);
   return_to_user_prog(int);
}

int ps3mapi_enable_access_syscall8(uint64_t key)
{
   system_call_3(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_PS3MAPI, PS3MAPI_OPCODE_REQUEST_ACCESS, key);
   return_to_user_prog(int);
}

int ps3mapi_disable_access_syscall8(uint64_t key)
{
   system_call_3(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_PS3MAPI, PS3MAPI_OPCODE_SET_ACCESS_KEY, key);
   return_to_user_prog(int);
}

int sys_get_mamba()
{
   system_call_1(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_GET_MAMBA);
   return_to_user_prog(int);
}

int sys_get_version(uint32_t* version)
{
   system_call_2(8, SYSCALL8_OPCODE_GET_VERSION, (uint64_t)(uint32_t)version);
   return_to_user_prog(int);
}

int sys_get_version2(uint16_t* version)
{
   system_call_2(8, SYSCALL8_OPCODE_GET_VERSION2, (uint64_t)(uint32_t)version);
   return_to_user_prog(int);
}

uint64_t ps3mapi_lv1_peek(uint64_t addr)
{
   system_call_3(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_PS3MAPI, PS3MAPI_OPCODE_LV1_PEEK, (uint64_t)addr);
   return_to_user_prog(uint64_t);
}

uint64_t ps3mapi_lv1_poke(uint64_t addr, uint64_t value)
{
   system_call_4(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_PS3MAPI, PS3MAPI_OPCODE_LV1_POKE, (uint64_t)addr, (uint64_t)value);
   return_to_user_prog(uint64_t);
}

uint64_t ps3mapi_lv2_peek(uint64_t addr)
{
   system_call_3(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_PS3MAPI, PS3MAPI_OPCODE_LV2_PEEK, (uint64_t)addr);
   return_to_user_prog(uint64_t);
}

uint64_t ps3mapi_lv2_poke(uint64_t addr, uint64_t value)
{
   system_call_4(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_PS3MAPI, PS3MAPI_OPCODE_LV2_POKE, (uint64_t)addr, (uint64_t)value);
   return_to_user_prog(uint64_t);
}

int PS3MAPIGetMemory(sys_pid_t pid, void* destination, void* source, size_t size)
{
   system_call_6(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_PS3MAPI, PS3MAPI_OPCODE_GET_PROC_MEM, (uint64_t)pid, (uint64_t)destination, (uint64_t)source, size);
   return_to_user_prog(int);
}

int PS3MAPISetMemory(sys_pid_t pid, void* destination, const void* source, size_t size)
{
   system_call_6(SC_COBRA_SYSCALL8, SYSCALL8_OPCODE_PS3MAPI, PS3MAPI_OPCODE_SET_PROC_MEM, (uint64_t)pid, (uint64_t)destination, (uint64_t)source, (uint64_t)size);
   return_to_user_prog(int);
}

uint64_t PeekLv1(uint64_t addr)
{
   system_call_1(8, (uint64_t)addr);
   return_to_user_prog(uint64_t);
}

void PokeLv1(uint64_t addr, uint64_t value)
{
   system_call_2(9, (uint64_t)addr, (uint64_t)value);
}

uint64_t PeekLv2(uint64_t address)
{
   system_call_1(6, (uint64_t)address);
   return_to_user_prog(uint64_t);
}

void PokeLv2(uint64_t addr, uint64_t val)
{
   system_call_2(7, (uint64_t)addr, (uint64_t)val);
}

bool PeekChunkLV1(uint64_t start, uint64_t* buffer, uint64_t size)
{
    if (PeekLv1(start) == 0xFFFFFFFF80010003)
        return false;

    for (uint64_t offset = 0; offset < size; offset += 8)
        *(buffer++) = PeekLv1(start + offset);

    return true;
}

bool PeekChunkLV2(uint64_t start, uint64_t* buffer, uint64_t size)
{
    if (PeekLv2(start) == 0xFFFFFFFF80010003)
        return false;

    for (uint64_t offset = 0; offset < size; offset += 8)
        *(buffer++) = PeekLv2(start + offset);

    return true;
}

uint64_t lv2_lv1_call(uint64_t syscallnum, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6, uint64_t arg7)
{
   system_call_8(10, (uint64_t)arg1, (uint64_t)arg2, (uint64_t)arg3, (uint64_t)arg4, (uint64_t)arg5, (uint64_t)arg6, (uint64_t)arg7, (uint64_t)syscallnum);
   return_to_user_prog(uint64_t);
}

uint64_t lv2_func_call(uint64_t addr, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6, uint64_t arg7)
{
   system_call_8(15, (uint64_t)arg1, (uint64_t)arg2, (uint64_t)arg3, (uint64_t)arg4, (uint64_t)arg5, (uint64_t)arg6, (uint64_t)arg7, (uint64_t)addr);
   return_to_user_prog(uint64_t);
}

int sys_dbg_read_process_memory(sys_pid_t pid, void* destination, void* source, size_t size)
{
   system_call_4(904, (uint64_t)pid, (uint64_t)destination, size, (uint64_t)source);
   return_to_user_prog(int);
}

int sys_dbg_write_process_memory(sys_pid_t pid, void* destination, const void* source, size_t size)
{
   system_call_4(905, (uint64_t)pid, (uint64_t)destination, size, (uint64_t)source);
   return_to_user_prog(int);
}

int sys_storage_get_cache_of_flash_ext_flag(uint8_t* flag)
{
   system_call_1(874, (uint64_t)flag);
   return_to_user_prog(int);
}

bool is_nor()
{
   uint8_t vf_flag;
   sys_storage_get_cache_of_flash_ext_flag(&vf_flag);
   return !(vf_flag & 0x1);
}

int lv2_get_platform_info(platform_info_t* info)
{
    system_call_1(387, (uint64_t)info);
    return_to_user_prog(int);
}

int get_target_type(uint64_t* type) // 1-CEX, 2-DEX, 3-DECR/RefTool
{
   system_call_3(985, (uint64_t)type, 0, 0);
   return_to_user_prog(int);
}

bool IsConsoleCex()
{
   uint64_t type;
   get_target_type(&type);
   return type == 1;
}

bool IsConsoleDex()
{
   uint64_t type;
   get_target_type(&type);
   return type == 2;
}

bool IsConsoleDeh()
{
   uint64_t type;
   get_target_type(&type);
   return type == 3;
}

bool IsConsoleHen()
{
   return ps3mapi_get_is_hen() == 0x1337;
}

bool IsConsoleMamba()
{
   return sys_get_mamba() == 0x666;
}

bool IsConsoleCobra()
{
   uint32_t version = 0x99999999;
   if (sys_get_version(&version) < 0)
      return false;

   if (version != 0x99999999) // If value changed, it is cobra
      return true;

   return false;
}

uint16_t GetPayloadVersion()
{
   uint16_t payloadVersion = 0;
   if (IsConsoleHen())
      payloadVersion = ps3mapi_get_hen_rev();
   else
      sys_get_version2(&payloadVersion);

   return payloadVersion;
}

bool DoesConsoleHaveCCAPI()
{
    CellFsStat sb;
    return (cellFsStat("/dev_flash/sys/internal/sys_plugin.sprx", &sb) == CELL_FS_SUCCEEDED);
}

bool DoesConsoleHaveWebman()
{
    CellFsStat sb;
    if (cellFsStat("/dev_hdd0/plugins/webftp_server.sprx", &sb) == CELL_FS_SUCCEEDED
        || cellFsStat("/dev_hdd0/plugins/webftp_server_noncobra.sprx", &sb) == CELL_FS_SUCCEEDED
        || cellFsStat("/dev_hdd0/plugins/webftp_server_cobra_ps3mapi.sprx", &sb) == CELL_FS_SUCCEEDED)
        return true;

    return false;
}

int sys_net_eurus_etherAddr(uint8_t* etherAddr)
{
   system_call_3(SYS_NET_EURUS_POST_COMMAND, CMD_GET_MAC_ADDRESS, (uint64_t)(uint32_t)etherAddr, 0x13);
   return_to_user_prog(int);
}

int sys_dbg_get_thread_list(sys_pid_t pid, sys_ppu_thread_t* ids, uint64_t* ids_num, uint64_t* all_ids_num)
{
   system_call_4(909, (uint64_t)pid, (uint64_t)ids, (uint64_t)ids_num, (uint64_t)all_ids_num);
   return_to_user_prog(int);
}

int _sys_prx_unload_module(sys_prx_id_t id, sys_prx_flags_t flags, const sys_prx_unload_module_option_t* pOpt)
{
   system_call_3(483, (uint64_t)id, (uint64_t)flags, (uint64_t)pOpt);
   return_to_user_prog(int);
}

int _sys_prx_stop_module(sys_prx_id_t id, size_t args, void* argp, int* modres, sys_prx_flags_t flags, sys_prx_stop_module_option_t* pOpt)
{
   system_call_6(482, (uint64_t)id, (uint64_t)args, (uint64_t)argp, (uint64_t)modres, (uint64_t)flags, (uint64_t)pOpt);
   return_to_user_prog(int);
}

sys_prx_id_t _sys_prx_get_module_id_by_name(const char* name, sys_prx_flags_t flags, sys_prx_get_module_id_by_name_option_t* pOpt)
{
   system_call_3(496, (uint64_t)name, (uint64_t)flags, (uint64_t)pOpt);
   return_to_user_prog(sys_prx_id_t);
}

sys_prx_id_t _sys_prx_get_module_id_by_address(void* addr)
{
   system_call_1(461, (uint64_t)addr);
   return_to_user_prog(sys_prx_id_t);
}

sys_prx_id_t _sys_prx_get_my_module_id()
{
   return _sys_prx_get_module_id_by_address((void*)_sys_prx_get_my_module_id);
}

void _sys_ppu_thread_exit(uint64_t val)
{
   system_call_1(SC_PPU_THREAD_EXIT, val);
}