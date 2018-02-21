//
// Custom.cpp : Defines the initialization routines for the DLL.
//
#include "precomp.h"

			

#ifdef CK_LIB
	#define RegisterBehaviorDeclarations	Register_Custom_BehaviorDeclarations
	#define InitInstance					_Custom_InitInstance
	#define ExitInstance					_Custom_ExitInstance
	#define CKGetPluginInfoCount			CKGet_Custom_PluginInfoCount
	#define CKGetPluginInfo					CKGet_Custom_PluginInfo
	#define g_PluginInfo					g_Custom_PluginInfo
#else
	#define RegisterBehaviorDeclarations	RegisterBehaviorDeclarations
	#define InitInstance					InitInstance
	#define ExitInstance					ExitInstance
	#define CKGetPluginInfoCount			CKGetPluginInfoCount
	#define CKGetPluginInfo					CKGetPluginInfo
	#define g_PluginInfo					g_PluginInfo
#endif









CKERROR InitInstance(CKContext* context);
CKERROR ExitInstance(CKContext* context);
PLUGIN_EXPORT void RegisterBehaviorDeclarations(XObjectDeclarationArray *reg);

#define CUSTOM_BEHAVIOR	CKGUID(0x70fb4259,0x4fce2169)

CKPluginInfo g_PluginInfo;

PLUGIN_EXPORT int CKGetPluginInfoCount() { return 1; }

PLUGIN_EXPORT CKPluginInfo* CKGetPluginInfo(int Index)
{

//	InitVrt();

	g_PluginInfo.m_Author			= "Custom";
	g_PluginInfo.m_Description		= "Custom building blocks";
	g_PluginInfo.m_Extension		= "";
	g_PluginInfo.m_Type				= CKPLUGIN_BEHAVIOR_DLL;
	g_PluginInfo.m_Version			= 0x000001;
	g_PluginInfo.m_InitInstanceFct	= InitInstance;
	g_PluginInfo.m_ExitInstanceFct	= ExitInstance;
	g_PluginInfo.m_GUID				= CUSTOM_BEHAVIOR;
	g_PluginInfo.m_Summary			= "Custom";
	return &g_PluginInfo;
}

#define CKPGUID_INTERSECTIONPRECISIONTYPE		CKDEFINEGUID(0x6cf55733,0x5af72dae)
#define CKPGUID_RECTBOXMODE						CKDEFINEGUID(0x5a6a3bd9,0x7e2797d)
#define CKPGUID_PROXIMITY						CKDEFINEGUID(0x7fff5699,0x7571336d)


/**********************************************************************************/
/**********************************************************************************/
CKERROR InitInstance(CKContext* context)
{
	return CK_OK;
}

 
CKERROR ExitInstance(CKContext* context)
{

	return CK_OK;
}

void RegisterBehaviorDeclarations(XObjectDeclarationArray *reg)
{
	RegisterBehavior(reg, FillBehaviorFreeBlockDecl);
	RegisterBehavior(reg, FillBBDecoderDecl);
}

