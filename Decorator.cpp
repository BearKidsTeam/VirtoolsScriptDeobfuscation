#include "precomp.h"
#include "interfaceData.h"
#include "assert.h"

void decorate_start(interface_t &data, CKBehavior *bb)
{
	assert(bb->GetInputCount() == 1);
	data.start.id = bb->GetInput(0)->GetOwner()->GetID();
}
void decorate_bbs(interface_t &data, CKBehavior *bb)
{
	int cnt = bb->GetSubBehaviorCount();
	data.n_bb = 0;
	data.bbs.clear();
	for (int i = 0; i < cnt; ++i)
	{
		CKBehavior *sub_bb=bb->GetSubBehavior(i);
		if (sub_bb->GetType() != 0x0) // bg
			continue;
		bb_t bb = bb_t();
		bb.id = sub_bb->GetID();
		bb.folded = true;
		bb.depth = 1;
		bb.is_bg = false;
		data.bbs.push_back(bb);
		++data.n_bb;
	}
}
// To create missing entries in interface_t
void decorate(interface_t &data, CKBehavior *bb)
{
	decorate_start(data, bb);
	decorate_bbs(data, bb);
}
