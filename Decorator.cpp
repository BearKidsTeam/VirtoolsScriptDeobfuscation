#include "precomp.h"
#include "interfaceData.h"
#include "assert.h"
#include <queue>
#include <set>
#include <utility>

void decorate_start(interface_t &data, CKBehavior *bb)
{
	assert(bb->GetInputCount() == 1);
	data.start.id = bb->GetInput(0)->GetOwner()->GetID();
}
void decorate_bb(bb_t &bb,CKBehavior *beh,int dpt)
{
	bb.id = beh->GetID();
	bb.folded = true;
	bb.depth = dpt;
	bb.is_bg = (beh->GetType()!=CKBEHAVIORTYPE_BASE);
	if(bb.is_bg)
	{
		for(int i=0,c=beh->GetSubBehaviorLinkCount();i<c;++i)
		{
			link_t lnk;lnk.type=1;
			CKBehaviorLink *blink=beh->GetSubBehaviorLink(i);
			lnk.id=blink->GetID();lnk.point_count=0;
			lnk.start=lnk.end=link_endpoint_t();

			lnk.start.id=blink->GetInBehaviorIO()->GetOwner()->GetID();
			lnk.start.type=13;
			lnk.start.index=blink->GetInBehaviorIO()->GetOwner()->GetOutputPosition(blink->GetInBehaviorIO());
			if(!~lnk.start.index)
			{
				lnk.start.index=blink->GetInBehaviorIO()->GetOwner()->GetInputPosition(blink->GetInBehaviorIO());
				lnk.start.type=12;
				if(blink->GetInBehaviorIO()->GetOwner()->GetType()==CKBEHAVIORTYPE_SCRIPT)
					lnk.start.type=26;
			}
			
			lnk.end.id=blink->GetOutBehaviorIO()->GetOwner()->GetID();
			lnk.end.type=12;
			lnk.end.index=blink->GetOutBehaviorIO()->GetOwner()->GetInputPosition(blink->GetOutBehaviorIO());
			if(!~lnk.end.index)
			{
				lnk.end.index=blink->GetOutBehaviorIO()->GetOwner()->GetOutputPosition(blink->GetOutBehaviorIO());
				lnk.end.type=13;
			}
			bb.links.push_back(lnk);
		}
		//try reconstructing reasonable plinks...
		//WARNING: untested and unfinished. Comment out two for-loops below to disable.
		std::set<std::pair<CK_ID,CK_ID>> pset;
		//just iterate through all pIns and pOuts of all subbehaviors
		//WARNING: this will look very ugly
		for(int i=0,cb=beh->GetSubBehaviorCount();i<cb;++i)
		{
			CKBehavior *bh=beh->GetSubBehavior(i);
			for(int j=0,cp=bh->GetInputParameterCount();j<cp;++j)
			{
				if(bh->GetInputParameter(j)->GetDirectSource())
				{
					link_t lnk;lnk.id=0;lnk.type=2;
					CKObject *srcowner=bh->GetInputParameter(j)->GetDirectSource()->GetOwner();
					CKBehavior *owningbehavior;
					if(srcowner->GetClassID()==CKCID_BEHAVIOR)
					{
						CKBehavior *srcownerreal=(CKBehavior*)srcowner;
						owningbehavior=srcownerreal->GetParent();
						lnk.start.id=srcownerreal->GetID();lnk.start.type=8;
						lnk.start.index=srcownerreal->GetOutputParameterPosition(
							(CKParameterOut*)bh->GetInputParameter(j)->GetDirectSource()
						);
					}
					if(srcowner->GetClassID()==CKCID_PARAMETEROPERATION)
					{
						CKParameterOperation *srcownerreal=(CKParameterOperation*)srcowner;
						owningbehavior=srcownerreal->GetOwner();
						lnk.start.id=srcownerreal->GetID();
						lnk.start.index=0;lnk.start.type=8;
					}
					if(bh->GetInputParameter(j)->GetDirectSource()->GetParameterClassID()==CKCID_PARAMETERLOCAL)
					{
						owningbehavior=(CKBehavior*)srcowner;
						lnk.start.id=bh->GetInputParameter(j)->GetDirectSource()->GetID();
						lnk.start.index=0;lnk.start.type=9;
					}
					lnk.end.type=7;lnk.end.id=bh->GetID();lnk.end.index=j;
					if(owningbehavior->GetParent()->GetID()==bh->GetParent()->GetID())//direct connction within a BG
					{
						//nothing to do
					}
					else//one of the ends may have an exported plink (but not both), otherwise use a shortcut
					{
						if(bh->GetParent()->GetInputParameterPosition(
							bh->GetInputParameter(j)
						)!=-1)//input has exported plink
						{
							CKBehavior *cd=bh;
							while(cd->GetParent()->GetInputParameterPosition(bh->GetInputParameter(j))!=-1
							&&owningbehavior->GetParent()->GetID()!=cd->GetParent()->GetID())
							{
								link_t exp_lnk;exp_lnk.id=0;exp_lnk.type=0x10002;
								exp_lnk.start.id=cd->GetParent()->GetID();
								exp_lnk.end.id=cd->GetID();
								exp_lnk.start.type=exp_lnk.end.type=7;
								exp_lnk.start.index=cd->GetParent()->GetInputParameterPosition(bh->GetInputParameter(j));
								exp_lnk.end.index=cd->GetInputParameterPosition(bh->GetInputParameter(j));
								bb.links.push_back(exp_lnk);
								cd=cd->GetParent();
							}
							//set final section index
							lnk.end.id=cd->GetID();
							lnk.end.index=cd->GetInputParameterPosition(bh->GetInputParameter(j));
							//if they are still at different depths, use a shortcut
							if(owningbehavior->GetParent()->GetID()!=cd->GetParent()->GetID())
							lnk.start.type=5;
						}
						else
						if(bh->GetInputParameter(j)->GetDirectSource()->GetClassID()==CKCID_PARAMETEROUT
							&&owningbehavior->GetOutputParameterPosition(
								(CKParameterOut*)bh->GetInputParameter(j)->GetDirectSource()
							)!=-1)//output has exported plink
						{
							CKBehavior *cd=owningbehavior;
							CKParameterOut *outref=(CKParameterOut*)bh->GetInputParameter(j)->GetDirectSource();
							link_t exp_lnk;exp_lnk.id=0;exp_lnk.type=0x10002;
							exp_lnk.start.id=bh->GetInputParameter(j)->GetDirectSource()->GetID();
							exp_lnk.end.id=cd->GetID();
							exp_lnk.start.type=exp_lnk.end.type=8;
							if(srcowner->GetClassID()==CKCID_BEHAVIOR)
							exp_lnk.start.index=((CKBehavior*)srcowner)->GetOutputParameterPosition(outref);
							else exp_lnk.start.index=0;
							exp_lnk.end.index=cd->GetOutputParameterPosition(outref);
							bb.links.push_back(exp_lnk);
							while(cd->GetParent()->GetOutputParameterPosition(outref)!=-1
							&&cd->GetParent()->GetID()!=bh->GetParent()->GetID())
							{
								link_t exp_lnk;exp_lnk.id=0;exp_lnk.type=0x10002;
								exp_lnk.start.id=cd->GetID();
								exp_lnk.end.id=cd->GetParent()->GetID();
								exp_lnk.start.type=exp_lnk.end.type=8;
								exp_lnk.start.index=cd->GetOutputParameterPosition(outref);
								exp_lnk.end.index=cd->GetParent()->GetOutputParameterPosition(outref);
								bb.links.push_back(exp_lnk);
								cd=cd->GetParent();
							}
							//set final section index
							lnk.start.id=cd->GetID();
							lnk.start.index=cd->GetOutputParameterPosition(outref);
							//if they are still at different depths, use a shortcut
							if(owningbehavior->GetParent()->GetID()!=cd->GetParent()->GetID())
							lnk.start.type=5;
						}
						else //otherwise simply use a shortcut
						lnk.start.type=5;
					}
					pset.insert(std::make_pair(lnk.start.id,lnk.end.id));
					bb.links.push_back(lnk);
				}
				if(bh->GetInputParameter(j)->GetSharedSource())//shared source connects to pIn only, so this one should be easier
				{
					link_t lnk;lnk.id=0;lnk.type=2;
					CKParameterIn* inref=bh->GetInputParameter(j);
					CKParameterIn* src=bh->GetInputParameter(j)->GetSharedSource();
					if(src->GetOwner()->GetClassID()!=CKCID_BEHAVIOR)
					continue;//wtf happened? it can also be a CKParameterOperation but what does that mean?
					CKBehavior *sb=(CKBehavior*)bh->GetInputParameter(j)->GetSharedSource()->GetOwner();
					CKBehavior *cb=bh;
					while(cb->GetParent()->GetInputParameterPosition(inref)!=-1&&cb->GetParent()->GetID()!=sb->GetID())
					{
						link_t lnk_exp;lnk_exp.id=0;lnk.type=0x10002;
						lnk_exp.end.id=cb->GetID();
						lnk_exp.end.index=cb->GetInputParameterPosition(inref);
						lnk_exp.end.type=lnk_exp.start.type=7;
						lnk_exp.start.id=cb->GetParent()->GetID();
						lnk_exp.start.index=cb->GetParent()->GetInputParameterPosition(inref);
						bb.links.push_back(lnk_exp);
						cb=cb->GetParent();
					}
					if(cb->GetParent()->GetID()!=sb->GetID())throw "shit";
					lnk.start.id=sb->GetID();lnk.start.type=7;lnk.start.index=sb->GetInputParameterPosition(src);
					lnk.end.id=cb->GetID();lnk.end.type=7;lnk.end.index=cb->GetInputParameterPosition(inref);
					bb.links.push_back(lnk);
				}
			}
		}
		for(int i=0,cb=beh->GetSubBehaviorCount();i<cb;++i)
		{
			CKBehavior *bh=beh->GetSubBehavior(i);
			for(int j=0,cp=bh->GetOutputParameterCount();j<cp;++j)
			{
				CKParameterOut *po=bh->GetOutputParameter(j);
				for(int k=0,cdest=po->GetDestinationCount();k<cdest;++k)
				{
				}
			}
		}
		bb.n_links=bb.links.size();
		for(int i=0,c=beh->GetParameterOperationCount();i<c;++i)
		{
			op_t op;
			CKParameterOperation* pop=beh->GetParameterOperation(i);
			op.id=pop->GetID();
			bb.ops.push_back(op);
		}
		bb.n_ops=bb.ops.size();
		for(int i=0,c=beh->GetLocalParameterCount();i<c;++i)
		{
			param_t p;
			CKParameterLocal* pl=beh->GetLocalParameter(i);
			p.id=pl->GetID();p.style=param_style_closed;
			bb.local_params.push_back(p);
		}
		bb.n_local_param=bb.local_params.size();
	}
}
void decorate_bbs(interface_t &data, CKBehavior *bb)
{
	data.n_bb = 0;
	data.bbs.clear();
	std::queue<std::pair<CKBehavior*,int>> bq;
	bq.push(std::make_pair(bb,0));
	while(!bq.empty())
	{
		CKBehavior* cur=bq.front().first;
		int cdpt=bq.front().second;bq.pop();
		if(cdpt)
		{
			data.bbs.push_back(bb_t());
			++data.n_bb;
		}
		decorate_bb(cdpt?data.bbs.back():data.script_root,cur,cdpt);
		int cnt = cur->GetSubBehaviorCount();
		for (int i = 0; i < cnt; ++i)
		{
			CKBehavior *sub_bb=cur->GetSubBehavior(i);
			bq.push(std::make_pair(sub_bb,cdpt+1));
		}
	}
}
// To create missing entries in interface_t
void decorate(interface_t &data, CKBehavior *bb)
{
	decorate_start(data, bb);
	decorate_bbs(data, bb);
}
