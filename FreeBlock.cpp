#include "precomp.h"


CKObjectDeclaration	*FillBehaviorFreeBlockDecl();
CKERROR CreateFreeBlockProto(CKBehaviorPrototype **);
int FreeBlock(const CKBehaviorContext& behcontext);

CKObjectDeclaration	*FillBehaviorFreeBlockDecl()
{
	CKObjectDeclaration *od = CreateCKObjectDeclaration("FreeBlock");
	od->SetDescription("A block that you can manipulate anything easily.");
	/* rem:
	<SPAN CLASS=in>In K: </SPAN>any of the inputs will trigger the process.<BR>
	<SPAN CLASS=out>Out K: </SPAN>if the building block is activated, all the outputs are then activated.<BR>
	<BR>
	This convenient building block acts just like an interface object in the schematique.<BR>
	You can add as many inputs and outputs as needed.<BR>
	<BR>
	*/
	od->SetType(CKDLL_BEHAVIORPROTOTYPE);
	od->SetGuid(CKGUID(0x51f76780, 0x6c896f5b));
	od->SetAuthorGuid(VIRTOOLS_GUID);
	od->SetAuthorName("Custom");
	od->SetVersion(0x00010000);
	od->SetCreationFunction(CreateFreeBlockProto);
	od->SetCompatibleClassId(CKCID_BEOBJECT);
	od->SetCategory("Custom/Misc");
	return od;
}


CKERROR CreateFreeBlockProto(CKBehaviorPrototype **pproto)
{
	CKBehaviorPrototype *proto = CreateCKBehaviorPrototype("FreeBlock");
	if (!proto) return CKERR_OUTOFMEMORY;

	// proto->DeclareInput("In 0");
	// proto->DeclareOutput("Out 0");

	proto->SetBehaviorFlags((CK_BEHAVIOR_FLAGS)(CKBEHAVIOR_VARIABLEINPUTS | CKBEHAVIOR_VARIABLEOUTPUTS | CKBEHAVIOR_VARIABLEPARAMETERINPUTS | CKBEHAVIOR_VARIABLEPARAMETEROUTPUTS));
	proto->SetFlags(CK_BEHAVIORPROTOTYPE_NORMAL);
	proto->SetFunction(FreeBlock);

	*pproto = proto;
	return CK_OK;

}


int FreeBlock(const CKBehaviorContext& behcontext)
{
	CKBehavior* beh = behcontext.Behavior;

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