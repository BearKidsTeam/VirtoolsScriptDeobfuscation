#include "precomp.h"
#include "interfaceData.h"
#include <ctime>

CKObjectDeclaration	*FillBBDecoderDecl();
CKERROR CreateBBDecoderProto(CKBehaviorPrototype **);
int BBDecoder(const CKBehaviorContext& behcontext);

CKObjectDeclaration	*FillBBDecoderDecl()
{
	CKObjectDeclaration *od = CreateCKObjectDeclaration("BBDecoder");
	od->SetDescription("A block to decode BB.");
	/* rem:
	<SPAN CLASS=in>In K: </SPAN>any of the inputs will trigger the process.<BR>
	<SPAN CLASS=out>Out K: </SPAN>if the building block is activated, all the outputs are then activated.<BR>
	<BR>
	This convenient building block acts just like an interface object in the schematique.<BR>
	You can add as many inputs and outputs as needed.<BR>
	<BR>
	*/
	od->SetType(CKDLL_BEHAVIORPROTOTYPE);
	od->SetGuid(CKGUID(0x37543829, 0xf0c3b39));
	od->SetAuthorGuid(VIRTOOLS_GUID);
	od->SetAuthorName("Custom");
	od->SetVersion(0x00010000);
	od->SetCreationFunction(CreateBBDecoderProto);
	od->SetCompatibleClassId(CKCID_BEOBJECT);
	od->SetCategory("Custom/Misc");
	return od;
}


CKERROR CreateBBDecoderProto(CKBehaviorPrototype **pproto)
{
	CKBehaviorPrototype *proto = CreateCKBehaviorPrototype("BBDecoder");
	if (!proto) return CKERR_OUTOFMEMORY;

	proto->DeclareInput("In 0");
	proto->DeclareOutput("Out 0");
	
	proto->DeclareInParameter("File Name", CKPGUID_STRING);

	proto->SetFlags(CK_BEHAVIORPROTOTYPE_NORMAL);
	proto->SetFunction(BBDecoder);

	*pproto = proto;
	return CK_OK;

}


int BBDecoder(const CKBehaviorContext& behcontext)
{
	interface_t parse_bb_test(CKBehavior *bb, CKFile *file);
	void decorate(interface_t &data, CKBehavior *bb);
	void generate_bb_test(interface_t &interface_data, CKBehavior *bb, CKFile *file);
	CKBehavior* beh = behcontext.Behavior;
	CKContext* ctx = behcontext.Context;
	CKVariableManager* vMan = ctx->GetVariableManager();

	// Dynamic ?
	BOOL dynamic = TRUE;
	CKBOOL keepSAS = FALSE;


	CK_LOAD_FLAGS loadoptions = CK_LOAD_FLAGS(CK_LOAD_DEFAULT | CK_LOAD_AUTOMATICMODE);
	if (dynamic) loadoptions = (CK_LOAD_FLAGS)(loadoptions | CK_LOAD_AS_DYNAMIC_OBJECT);

	CKObjectArray* array = CreateCKObjectArray();

	ctx->SetAutomaticLoadMode(CKLOAD_OK, CKLOAD_OK, CKLOAD_OK, CKLOAD_OK);
	CKSTRING fname = (CKSTRING)beh->GetInputParameterReadDataPtr(0);
	XString filename(fname);
	ctx->GetPathManager()->ResolveFileName(filename, DATA_PATH_IDX, -1);
	if (ctx->Load(filename.Str(), array, loadoptions) != CK_OK) {
		DeleteCKObjectArray(array);
		throw "fuck";
	}
	clock_t c=clock();
	for (array->Reset(); !array->EndOfList(); array->Next()) {
		CKObject* o = array->GetData(ctx);
		if (CKIsChildClassOf(o, CKCID_BEHAVIOR)) {
			CKBehavior* bo = (CKBehavior*)o;
			CKFile *file = bo->GetCKContext()->CreateCKFile();
			interface_t data = parse_bb_test(bo, file);
			decorate(data, bo);
			generate_bb_test(data, bo, file);
		}
	}
	printfdbg("decorate+generate time: %f\n",1.*(clock()-c)/CLOCKS_PER_SEC);
	CKLevel* level = ctx->GetCurrentLevel();
	for (array->Reset(); !array->EndOfList(); array->Next()) {
		CKObject* o = array->GetData(ctx);
		if (!CKIsChildClassOf(o, CKCID_BEHAVIOR)) {
			if (CKIsChildClassOf(o, CKCID_SCENE)) {
				level->AddScene((CKScene*)o);
			}
			else {
				level->AddObject(o);
			}
		}
	}
	level->BeginAddSequence(FALSE);
	DeleteCKObjectArray(array);
	int i, count = beh->GetInputCount();
	for (i = 0; i<count; ++i) {
		beh->ActivateInput(i, FALSE);
	}
	count = beh->GetOutputCount();
	for (i = 0; i<count; ++i) {
		beh->ActivateOutput(i);
	}

	return CKBR_OK;
}