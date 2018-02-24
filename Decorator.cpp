#include "precomp.h"
#include "interfaceData.h"
#include "assert.h"
#include <map>
#include <queue>
#include <set>
#include <utility>

void decorate_start(interface_t &data, CKBehavior *bb)
{
	assert(bb->GetInputCount() == 1);
	data.start.id = bb->GetInput(0)->GetOwner()->GetID();
}
std::map<CK_ID,bb_t*> bmap;
std::set<CK_ID> pins;
std::set<CK_ID> pouts;
struct pio_pos_t{CK_ID id;int idx;CK_ID lnk_within;};
pio_pos_t GetpInPos(CKParameterIn *pin,CKBehavior **owningbeh)
{
	pio_pos_t ret;
	CKObject* ob=pin->GetOwner();
	ret.id=ob->GetID();
	if(ob->GetClassID()==CKCID_BEHAVIOR)
	{
		CKBehavior* obb=(CKBehavior*)ob;
		ret.idx=obb->GetInputParameterPosition(pin);
		*owningbeh=obb->GetParent();
		ret.lnk_within=(*owningbeh)->GetID();
		return ret;
	}
	else if(ob->GetClassID()==CKCID_PARAMETEROPERATION)
	{
		CKParameterOperation* obop=(CKParameterOperation*)ob;
		ret.idx=obop->GetInParameter1()->GetID()==pin->GetID()?0:1;
		*owningbeh=obop->GetOwner();
		ret.lnk_within=(*owningbeh)->GetID();
		return ret;
	}else throw;
}
pio_pos_t GetpOutPos(CKParameterOut *pout,CKBehavior **owningbeh)
{
	pio_pos_t ret;
	CKObject* ob=pout->GetOwner();
	ret.id=ob->GetID();
	if(ob->GetClassID()==CKCID_BEHAVIOR)
	{
		CKBehavior* obb=(CKBehavior*)ob;
		ret.idx=obb->GetOutputParameterPosition(pout);
		*owningbeh=obb->GetParent();
		ret.lnk_within=(*owningbeh)->GetID();
		return ret;
	}
	else if(ob->GetClassID()==CKCID_PARAMETEROPERATION)
	{
		CKParameterOperation* obop=(CKParameterOperation*)ob;
		ret.idx=0;
		*owningbeh=obop->GetOwner();
		ret.lnk_within=(*owningbeh)->GetID();
		return ret;
	}else throw;
}
link_endpoint_t GetParameterEndpoint(CKParameter* p)
{
	if(p->GetClassID()==CKCID_PARAMETERLOCAL)
	return link_endpoint_t{p->GetID(),0,9};
	CKBehavior* dummy;
	pio_pos_t t=GetpOutPos((CKParameterOut*)p,&dummy);
	return link_endpoint_t{t.id,t.idx,8};
}
CKBehavior* GetParameterOwnerBehavior(CKParameter* p)
{
	if(p->GetClassID()==CKCID_PARAMETERLOCAL)
	return (CKBehavior*)p->GetOwner();
	if(p->GetClassID()==CKCID_PARAMETEROUT)
	{
		CKObject* owner=p->GetOwner();
		if(owner->GetClassID()==CKCID_BEHAVIOR)
		return ((CKBehavior*)owner)->GetParent();
		if(owner->GetClassID()==CKCID_PARAMETEROPERATION)
		return ((CKParameterOperation*)owner)->GetOwner();
		throw;
	}
	throw;
}
//try reconstructing reasonable plinks...
//just iterate through all pIns and pOuts of all subbehaviors
//WARNING: this will look very ugly
//**UNTESTED**
void configure_plink(CKBehavior *root)
//Q: Why we have to configure these outside?
//A: Because a pLink may not belong to parent of the ends.
//   And cross-behavior pLinks may exist.
{
	CKContext *ctx=root->GetCKContext();
	//std::set<std::pair<CK_ID,CK_ID>> pset;
	std::map<CK_ID,std::vector<pio_pos_t>> pin_chain;
	std::map<CK_ID,std::vector<pio_pos_t>> pout_chain;
	for(auto&i:pins)
	{
		std::vector<pio_pos_t> &vp=pin_chain[i]=std::vector<pio_pos_t>();
		CKParameterIn* pin=(CKParameterIn*)ctx->GetObjectA(i);
		CKBehavior *cb;
		vp.push_back(GetpInPos(pin,&cb));
		link_endpoint_t last=link_endpoint_t{vp.back().id,vp.back().idx,7};
		for(;cb&&cb->GetInputParameterPosition(pin)!=-1;cb=cb->GetParent())
		{
			vp.push_back(pio_pos_t{cb->GetID(),cb->GetInputParameterPosition(pin),cb->GetParent()->GetID()});
			link_t lnk_exp;lnk_exp.id=0;lnk_exp.type=0x10002;
			lnk_exp.start=link_endpoint_t{vp.back().id,vp.back().idx,7};
			lnk_exp.end=last;
			last=lnk_exp.start;
			bmap[cb->GetID()]->links.push_back(lnk_exp);
		}
	}
	for(auto&i:pouts)
	{
		std::vector<pio_pos_t> &vp=pout_chain[i]=std::vector<pio_pos_t>();
		CKParameterOut* pout=(CKParameterOut*)ctx->GetObjectA(i);
		CKBehavior *cb;
		vp.push_back(GetpOutPos(pout,&cb));
		link_endpoint_t last=link_endpoint_t{vp.back().id,vp.back().idx,8};
		for(;cb&&cb->GetOutputParameterPosition(pout)!=-1;cb=cb->GetParent())
		{
			vp.push_back(pio_pos_t{cb->GetID(),cb->GetOutputParameterPosition(pout),cb->GetParent()->GetID()});
			link_t lnk_exp;lnk_exp.id=0;lnk_exp.type=0x10002;
			lnk_exp.start=link_endpoint_t{vp.back().id,vp.back().idx,7};
			lnk_exp.end=last;
			last=lnk_exp.start;
			bmap[cb->GetID()]->links.push_back(lnk_exp);
		}
	}
	for(auto&i:pins)
	{
		CKParameterIn* pin=(CKParameterIn*)ctx->GetObjectA(i);
		CKBehavior *cb;pio_pos_t pinp=GetpInPos(pin,&cb);
		std::vector<pio_pos_t>& vpin=pin_chain[pin->GetID()];
		if(pin->GetDirectSource())
		{
			CKParameter* dsrc=pin->GetDirectSource();
			std::vector<pio_pos_t>& vpsrc=pin_chain[dsrc->GetID()];
			if(dsrc->GetClassID()==CKCID_PARAMETERLOCAL&&vpsrc.empty())
			vpsrc.push_back(pio_pos_t{dsrc->GetID(),0,((CKParameterLocal*)dsrc)->GetOwner()->GetID()});
			CKBehavior* dsob=GetParameterOwnerBehavior(dsrc);
			bool conn=false;
			for(auto&aa:vpin)
			{
				if(conn)break;
				for(auto&bb:vpsrc)
				if(!conn&&aa.lnk_within==bb.lnk_within)//direct connection within this behavior
				{
					link_t lnk;lnk.id=0;lnk.type=2;
					lnk.start=link_endpoint_t{bb.id,bb.idx,dsrc->GetClassID()==CKCID_PARAMETERLOCAL?9:8};
					lnk.end=link_endpoint_t{aa.id,aa.idx,7};
					bmap[aa.lnk_within]->links.push_back(lnk);
					conn=true;break;
				}
			}
			if(!conn)//still not connected, use a shortcut instead
			{
				link_t lnk;lnk.id=0;lnk.type=2;
				lnk.start=link_endpoint_t{vpsrc.front().id,vpsrc.front().idx,5};
				lnk.end=link_endpoint_t{pinp.id,pinp.idx,7};
				bmap[pinp.lnk_within]->links.push_back(lnk);
			}
		}
		if(pin->GetSharedSource())
		{
			CKParameterIn* shpin=pin->GetSharedSource();
			CK_ID shpin_within=shpin->GetOwner()->GetID();
			int shpin_idx=0;
			assert(shpin->GetOwner()->GetClassID()==CKCID_BEHAVIOR);
			shpin_idx=((CKBehavior*)shpin->GetOwner())->GetInputParameterPosition(shpin);
			//no shortcut here!
			bool conn=false;
			for(auto&aa:vpin)
			if(aa.lnk_within==shpin_within)
			{
				link_t lnk;lnk.id=0;lnk.type=2;
				lnk.start=link_endpoint_t{shpin_within,shpin_idx,7};
				lnk.end=link_endpoint_t{aa.id,aa.idx,7};
				bmap[aa.lnk_within]->links.push_back(lnk);
				conn=true;break;
			}
			if(!conn)throw;
		}
	}
	//up to here we only have pOut->pOut and pOut->pLocal missing
	//so we iterate through all pOuts
	for(auto&i:pouts)
	{
		CKParameterOut* po=(CKParameterOut*)ctx->GetObjectA(i);
		std::vector<pio_pos_t>& vpout=pout_chain[po->GetID()];
		for(int j=0,cd=po->GetDestinationCount();j<cd;++j)
		{
			CKParameter* dest=po->GetDestination(j);
			link_endpoint_t dendp=GetParameterEndpoint(dest);
			CK_ID dest_within=GetParameterOwnerBehavior(dest)->GetID();
			std::vector<pio_pos_t>& vpdest=pout_chain[dest->GetID()];
			if(dest->GetClassID()==CKCID_PARAMETERLOCAL&&vpdest.empty())
			vpdest.push_back(pio_pos_t{dest->GetID(),0,((CKParameterLocal*)dest)->GetOwner()->GetID()});
			//no shortcut here!
			bool conn=false;
			for(auto&aa:vpout)
			if(aa.lnk_within==dest_within)
			{
				link_t lnk;lnk.id=0;lnk.type=2;
				lnk.start=link_endpoint_t{aa.id,aa.idx,8};
				lnk.end=dendp;
				bmap[aa.lnk_within]->links.push_back(lnk);
				conn=true;break;
			}
			if(!conn)throw;
		}
	}
}
void decorate_bb(bb_t &bb,CKBehavior *beh,int dpt)
{
	bmap[beh->GetID()]=&bb;
	bb.id = beh->GetID();
	bb.folded = true;
	bb.depth = dpt;
	bb.is_bg = (beh->GetType()!=CKBEHAVIORTYPE_BASE);
	for(int i=0,c=beh->GetInputParameterCount();i<c;++i)
	pins.insert(beh->GetInputParameter(i)->GetID());
	for(int i=0,c=beh->GetOutputParameterCount();i<c;++i)
	pouts.insert(beh->GetOutputParameter(i)->GetID());
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
	bmap.clear();
	pins.clear();pouts.clear();
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
	configure_plink(bb);
}
// To create missing entries in interface_t
void decorate(interface_t &data, CKBehavior *bb)
{
	decorate_start(data, bb);
	decorate_bbs(data, bb);
}
