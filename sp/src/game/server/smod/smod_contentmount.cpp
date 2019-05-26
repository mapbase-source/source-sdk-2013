#include "cbase.h"

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#pragma warning(disable: 4005)
#include "windows.h"
#pragma warning(default: 4005)
#undef PROTECTED_THINGS_H
#include "tier0/protected_things.h"

#include "filesystem_init.h"
#include "steam/steam_api.h"
#include "tier0/icommandline.h"
#include "scenefilecache/ISceneFileCache.h"
#include "scenefilecache/SceneImageFile.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "datacache/imdlcache.h"
#include "tier1/utlhashtable.h"
#include "tier0/memdbgon.h"

static CDllDemandLoader g_pSoundEmitterSystemDLL("SoundEmitterSystem");

extern ISceneFileCache *scenefilecache;
extern ISoundEmitterSystemBase *soundemitterbase;
extern IServerGameClients *servergameclients;

extern void ClearModelSoundsCache();
extern IGameSystem *SoundEmitterSystem();

static const int g_SupportedAppIDs[] = {
	240, //220, 340, 380, 420,
};

struct MountInfo_t
{
	MountInfo_t();
	~MountInfo_t();

	MountInfo_t(const MountInfo_t &info);
	MountInfo_t(const char *path, const char *pathid="GAME", SearchPathAdd_t type=PATH_ADD_TO_TAIL);

	char m_Path[MAX_PATH];
	char m_PathID[10];
	CCopyableUtlStringList m_Vpks;
	SearchPathAdd_t m_nType;

	bool IsValid() const;

	const MountInfo_t &operator=(const MountInfo_t &rhs);
	bool operator==(const MountInfo_t &rhs) const;
	bool operator!=(const MountInfo_t &rhs) const;
};

MountInfo_t::MountInfo_t()
{
	m_nType = PATH_ADD_TO_TAIL;
	V_strcpy_safe(m_PathID, "GAME");
}

MountInfo_t::MountInfo_t(const MountInfo_t &info)
{
	operator=(info);
}

const MountInfo_t &MountInfo_t::operator=(const MountInfo_t &rhs)
{
	m_nType = rhs.m_nType;
	V_strcpy_safe(m_Path, rhs.m_Path);
	V_strcpy_safe(m_PathID, rhs.m_PathID);
	return *this;
}

MountInfo_t::MountInfo_t(const char *path, const char *pathid, SearchPathAdd_t type)
{
	m_nType = type;
	V_strcpy_safe(m_Path, path);
	V_strcpy_safe(m_PathID, pathid);
}

MountInfo_t::~MountInfo_t()
{
	m_Vpks.PurgeAndDeleteElements();
}

bool MountInfo_t::operator==(const MountInfo_t &rhs) const
{
	if(V_stricmp(m_Path, rhs.m_Path) != 0)
		return false;

	if(V_stricmp(m_PathID, rhs.m_PathID) != 0)
		return false;

	return true;
}

bool MountInfo_t::operator!=(const MountInfo_t &rhs) const
{
	return !operator==(rhs);
}

bool MountInfo_t::IsValid() const
{
	return (m_Path[0] != '\0' && m_PathID[0] != '\0');
}

class CContentMounter : public CBaseGameSystem
{
	public:
		CContentMounter();
		virtual ~CContentMounter();

	public:
		virtual const char *Name();
		virtual bool Init();
		virtual void PostInit();
		virtual void LevelInitPreEntity();
		virtual void LevelShutdownPostEntity();
		int MountGame(int appid);
		int MountGame(const MountInfo_t &info);
		bool UnmountGame(int appid);
		bool UnmountGame(const MountInfo_t &info);
		bool UnmountGameEx(int index);
		int RemountGame(int appid);
		int RemountGameEx(int index);
		int RemountGame(const MountInfo_t &info);
		void MountAll();
		void UnmountAll();
		void PrintMounted();
		void Refresh();
		const char *GetFolderForAppID(int appid);
		int FindFile(const char *file);

	private:
		CUtlVector<MountInfo_t> m_Mounted;
};

static CContentMounter g_ContentMounter;
IGameSystem *ContentMounter() {
	return &g_ContentMounter;
}

CContentMounter::CContentMounter()
{

}

CContentMounter::~CContentMounter()
{

}

const char *CContentMounter::Name()
{
	return "CContentMounter";
}

void CContentMounter::MountAll()
{
	for(int i = ARRAYSIZE(g_SupportedAppIDs)-1; i >= 0; i--)
		MountGame(g_SupportedAppIDs[i]);

	//Refresh();
}

void CContentMounter::UnmountAll()
{
	int c = m_Mounted.Count();
	for(int i = 0; i < c; i++) {
		const MountInfo_t &info = m_Mounted.Element(i);
		UnmountGame(info);
		i--;
		c--;
	}

	//Refresh();
}

class CSoundEmitterSystemBase : public ISoundEmitterSystemBase
{
	public:
		void ParseManifest(const char *path);
		void AddSoundsFromFile(const char *filename, bool unknown1, bool bIsOverride, bool unknown2);
		void *GetAddSoundsFromFilePtr();
};

CSoundEmitterSystemBase *SoundEmitterBase()
{
	return reinterpret_cast<CSoundEmitterSystemBase *>(soundemitterbase);
}

bool CContentMounter::Init()
{
	MountAll();
	return true;
}

void CContentMounter::PostInit()
{
	
}

int CContentMounter::FindFile(const char *file)
{
	if(V_IsAbsolutePath(file))
		return m_Mounted.InvalidIndex();

	char filepath[MAX_PATH];
	bool found = false;
	int i = 0;
	for(; i < m_Mounted.Count(); i++) {
		const MountInfo_t &info = m_Mounted.Element(i);
		V_ComposeFileName(info.m_Path, file, filepath, sizeof(filepath));
		if(filesystem->FileExists(filepath, info.m_PathID)) {
			found = true;
			break;
		} else {
			FOR_EACH_VEC(info.m_Vpks, j) {
				V_ComposeFileName(info.m_Vpks.Element(j), file, filepath, sizeof(filepath));
				if(filesystem->FileExists(filepath, info.m_PathID)) {
					found = true;
					break;
				}
			}
			if(found) {
				break;
			}
		}
	}

	if(!found)
		return m_Mounted.InvalidIndex();

	return i;
}

void ExecCommand(const char *name)
{
	const char *argv[] = {name};
	servergameclients->SetCommandClient(0);
	cvar->FindCommand(argv[0])->Dispatch({ARRAYSIZE(argv), argv});
}

void CSoundEmitterSystemBase::ParseManifest(const char *path)
{
	KeyValues *manifest = new KeyValues(path);
	if(filesystem->LoadKeyValues(*manifest, IFileSystem::TYPE_SOUNDEMITTER, path, "GAME")) {
		FOR_EACH_SUBKEY(manifest, sub) {
			const char *name = sub->GetName();
			const char *value = sub->GetString();
			if(V_stricmp(name, "precache_file") == 0)
				AddSoundsFromFile(value, false, false, false);
			else if(V_stricmp(name, "preload_file") == 0)
				AddSoundsFromFile(value, true, false, false);
		}
	}
	manifest->deleteThis();
}

void CContentMounter::Refresh()
{
	SoundEmitterBase()->Flush();
	SoundEmitterBase()->ParseManifest("scripts/game_sounds_smod_manifest.txt");

	mdlcache->Flush(MDLCACHE_FLUSH_VCOLLIDE);
	ExecCommand("r_flushlod");

	ExecCommand("scene_flush");
}

void CContentMounter::LevelInitPreEntity()
{
	char mapfile[MAX_PATH];
	V_sprintf_safe(mapfile, "maps/%s.bsp", STRING(gpGlobals->mapname));

	int index = FindFile(mapfile);
	if(index != m_Mounted.InvalidIndex()) {
		MountInfo_t info = m_Mounted.Element(index);
		if(info.m_nType != PATH_ADD_TO_HEAD) {
			UnmountGameEx(index);
			info.m_nType = PATH_ADD_TO_HEAD;
			MountGame(info);
			Refresh();
		}
	}
}

void CContentMounter::LevelShutdownPostEntity()
{
	char mapfile[MAX_PATH];
	V_sprintf_safe(mapfile, "maps/%s.bsp", STRING(gpGlobals->mapname));

	int index = FindFile(mapfile);
	if(index != m_Mounted.InvalidIndex()) {
		MountInfo_t info = m_Mounted.Element(index);
		if(info.m_nType != PATH_ADD_TO_TAIL) {
			UnmountGameEx(index);
			info.m_nType = PATH_ADD_TO_TAIL;
			MountGame(info);
			//Refresh();
		}
	}
}

int CContentMounter::RemountGame(int appid)
{
	if(!steamapicontext->SteamApps()->BIsAppInstalled(appid))
		return m_Mounted.InvalidIndex();

	char fullpath[MAX_PATH];
	steamapicontext->SteamApps()->GetAppInstallDir(appid, fullpath, sizeof(fullpath));
	V_ComposeFileName(fullpath, GetFolderForAppID(appid), fullpath, sizeof(fullpath));

	return RemountGame({fullpath, "GAME"});
}

int CContentMounter::RemountGame(const MountInfo_t &info)
{
	if(!info.IsValid())
		return m_Mounted.InvalidIndex();

	int index = m_Mounted.Find(info);
	if(index == m_Mounted.InvalidIndex())
		return m_Mounted.InvalidIndex();

	UnmountGameEx(index);
	return MountGame(info);
}

int CContentMounter::RemountGameEx(int index)
{
	if(index == m_Mounted.InvalidIndex())
		return m_Mounted.InvalidIndex();

	return RemountGame(m_Mounted.Element(index));
}

const char *CContentMounter::GetFolderForAppID(int appid)
{
	switch(appid) {
		case 280: { return "hl1"; }
		case 340: { return "lostcoast"; }
		case 380: { return "episodic"; }
		case 420: { return "ep2"; }
		case 440: { return "tf"; }
		case 300: { return "dod"; }
		case 240: { return "cstrike"; }
		case 730: { return "csgo"; }
		case 550: { return "left4dead2"; }
		case 500: { return "left4dead"; }
		case 400: { return "portal"; }
		case 620: { return "portal2"; }
		case 4000: { return "garrysmod"; }
		case 630: { return "swarm"; }
		case 220: { return "hl2"; }
		case 360: { return "hl1mp"; }
		case 320: { return "hl2mp"; }
		case 1840: { return "tf"; }
		case 211: { return "sourcetest"; }
		case 215: { return "sourcetest"; }
		case 218: { return "sourcetest"; }
		case 243730: { return "sourcetest"; }
		case 243750: { return "sourcetest"; }
		case 17500: { return "zps"; }
		case 235780: { return "metastasis"; }
		case 587650: { return "downfall"; }
		case 290930: { return "hl2"; }
		case 365300: { return "te120"; }
		case 270370: { return "lambdawars"; }
		case 17520: { return "synergy"; }
		case 17510: { return "ageofchivalry"; }
		case 17700: { return "insurgency"; }
		case 17580: { return "dystopia"; }
		case 17530: { return "diprip"; }
		case 17730: { return "smashball"; }
		case 17550: { return "esmod"; }
		case 17570: { return "pvkii"; }
	}

	return "invalidappid";
}

bool CContentMounter::UnmountGame(int appid)
{
	if(!steamapicontext->SteamApps()->BIsAppInstalled(appid))
		return false;

	char fullpath[MAX_PATH];
	steamapicontext->SteamApps()->GetAppInstallDir(appid, fullpath, sizeof(fullpath));
	V_ComposeFileName(fullpath, GetFolderForAppID(appid), fullpath, sizeof(fullpath));

	return UnmountGame({fullpath, "GAME"});
}

bool CContentMounter::UnmountGame(const MountInfo_t &info)
{
	if(!info.IsValid())
		return false;

	int index = m_Mounted.Find(info);
	if(index == m_Mounted.InvalidIndex())
		return false;

	return UnmountGameEx(index);
}

int CContentMounter::MountGame(int appid)
{
	if(!steamapicontext->SteamApps()->BIsAppInstalled(appid))
		return m_Mounted.InvalidIndex();

	char fullpath[MAX_PATH];
	steamapicontext->SteamApps()->GetAppInstallDir(appid, fullpath, sizeof(fullpath));
	V_ComposeFileName(fullpath, GetFolderForAppID(appid), fullpath, sizeof(fullpath));

	return MountGame({fullpath, "GAME", PATH_ADD_TO_TAIL});
}

int CContentMounter::MountGame(const MountInfo_t &info)
{
	if(!info.IsValid())
		return m_Mounted.InvalidIndex();

	int index = m_Mounted.Find(info);
	if(index != m_Mounted.InvalidIndex())
		return index;

	if(info.m_nType == PATH_ADD_TO_TAIL)
		index = m_Mounted.AddToTail(info);
	else if(info.m_nType == PATH_ADD_TO_HEAD)
		index = m_Mounted.AddToHead(info);
	MountInfo_t &mount = m_Mounted.Element(index);

	bool ishl2 = !!V_stristr(mount.m_Path, "hl2");

	if(!ishl2)
		filesystem->AddSearchPath(mount.m_Path, mount.m_PathID, mount.m_nType);

	char fullpath[MAX_PATH];
	char folder[MAX_PATH];
	if(!ishl2)
		V_strcpy_safe(folder, mount.m_Path);
	else {
		steamapicontext->SteamApps()->GetAppInstallDir(243750, folder, sizeof(folder));
		V_ComposeFileName(folder, "hl2", folder, sizeof(folder));
	}

	V_ComposeFileName(folder, "*.vpk", fullpath, sizeof(fullpath));

	FileFindHandle_t handle;
	const char *filename = filesystem->FindFirstEx(fullpath, mount.m_PathID, &handle);
	while(filename)
	{
		if(V_stristr(filename, "_dir.vpk")) {
			V_ComposeFileName(folder, filename, fullpath, sizeof(fullpath));
			if(!ishl2)
				FileSystem_LoadVPK(filesystem, mount.m_PathID, fullpath, mount.m_nType);
			V_StrSlice(fullpath, 0, V_strlen(fullpath)-V_strlen("_dir.vpk"), fullpath, sizeof(fullpath));
			V_SetExtension(fullpath, ".vpk", sizeof(fullpath));
			mount.m_Vpks.CopyAndAddToTail(fullpath);
		}

		filename = filesystem->FindNext(handle);
	}

	filesystem->FindClose(handle);

	return index;
}

CON_COMMAND(printmounted, "")
{
	g_ContentMounter.PrintMounted();
}

void CContentMounter::PrintMounted()
{
	FOR_EACH_VEC(m_Mounted, i) {
		const MountInfo_t &info = m_Mounted.Element(i);
		const char *type = (info.m_nType == PATH_ADD_TO_HEAD ? "PATH_ADD_TO_HEAD" : "PATH_ADD_TO_TAIL");
		Msg("(%i - [%s] %s - %s [%i])\n", i, info.m_PathID, info.m_Path, type, info.m_nType);
		FOR_EACH_VEC(info.m_Vpks, j)
			Msg("  (%i - %s)\n", j, info.m_Vpks.Element(j));
		Msg("\n");
	}
}

bool CContentMounter::UnmountGameEx(int index)
{
	if(index == m_Mounted.InvalidIndex())
		return false;

	MountInfo_t &mount = m_Mounted.Element(index);

	bool ishl2 = !!V_stristr(mount.m_Path, "hl2");

	int c = mount.m_Vpks.Count();
	for(int i = 0; i < c; i++) {
		const char *vpk = mount.m_Vpks.Element(i);
		if(!ishl2)
			FileSystem_UnloadVPK(filesystem, mount.m_PathID, vpk);
		mount.m_Vpks.Remove(i);
		i--;
		c--;
	}

	if(!ishl2)
		filesystem->RemoveSearchPath(mount.m_Path, mount.m_PathID);

	m_Mounted.Remove(index);

	return true;
}

void CSoundEmitterSystemBase::AddSoundsFromFile(const char *filename, bool unknown1, bool bIsOverride, bool unknown2)
{
	typedef void(ISoundEmitterSystemBase::*AddSoundsFromFileFunc)(const char *, bool, bool, bool);
	static AddSoundsFromFileFunc m_pFunc = nullptr;
	if(!m_pFunc) {
		union { void *ui; AddSoundsFromFileFunc uo; };
		ui = CSoundEmitterSystemBase::GetAddSoundsFromFilePtr();
		m_pFunc = uo;
	}
	if(m_pFunc)
		return (this->*m_pFunc)(filename, unknown1, bIsOverride, unknown2);
}

void *CSoundEmitterSystemBase::GetAddSoundsFromFilePtr()
{
	static const char *signature = "\x55\x8B\xEC\x83\xEC\x20\x53\x56";
	unsigned char buffer[512] = "";
	size_t written = 0;
	size_t length = V_strlen(signature);
	for(size_t i = 0; i < length; i++) {
		if(written >= sizeof(buffer))
			break;
		buffer[written++] = signature[i];
		if(signature[i] == '\\' && signature[i + 1] == 'x') {
			if(i + 3 >= length)
				continue;
			char s_byte[3];
			int r_byte;
			s_byte[0] = signature[i + 2];
			s_byte[1] = signature[i + 3];
			s_byte[2] = '\0';
			sscanf(s_byte, "%x", &r_byte);
			buffer[written - 1] = r_byte;
			i += 3;
		}
	}
	const char *pattern = (const char *)buffer;
	const void *handle = (const void *)g_pSoundEmitterSystemDLL.GetFactory();
	uintptr_t baseAddr = 0;
	if(!handle)
		return nullptr;
	MEMORY_BASIC_INFORMATION meminfo;
	IMAGE_DOS_HEADER *dos = nullptr;
	IMAGE_NT_HEADERS *pe = nullptr;
	IMAGE_FILE_HEADER *file = nullptr;
	IMAGE_OPTIONAL_HEADER *opt = nullptr;
	if(!VirtualQuery(handle, &meminfo, sizeof(MEMORY_BASIC_INFORMATION)))
		return nullptr;
	baseAddr = reinterpret_cast<uintptr_t>(meminfo.AllocationBase);
	dos = reinterpret_cast<IMAGE_DOS_HEADER *>(baseAddr);
	pe = reinterpret_cast<IMAGE_NT_HEADERS *>(baseAddr + dos->e_lfanew);
	file = &pe->FileHeader;
	opt = &pe->OptionalHeader;
	if(dos->e_magic != IMAGE_DOS_SIGNATURE || pe->Signature != IMAGE_NT_SIGNATURE || opt->Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC)
		return nullptr;
	if(file->Machine != IMAGE_FILE_MACHINE_I386)
		return nullptr;
	if((file->Characteristics & IMAGE_FILE_DLL) == 0)
		return nullptr;
	char *ptr, *end;
	ptr = reinterpret_cast<char *>(baseAddr);
	end = ptr + opt->SizeOfImage - written;
	bool found = false;
	while(ptr < end) {
		found = true;
		for(register size_t i = 0; i < written; i++) {
			if(pattern[i] != '\x2A' && pattern[i] != ptr[i]) {
				found = false;
				break;
			}
		}
		if(found)
			return ptr;
		ptr++;
	}
	return nullptr;
}