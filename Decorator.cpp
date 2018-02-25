#include "precomp.h"
#include "interfaceData.h"
#include "assert.h"
#include <map>
#include <queue>
#include <set>
#include <utility>
extern "C" {
	extern void __stdcall OutputDebugStringA(_In_opt_ const char* lpOutputString);
};
void printfdbg(const char* fmt, ...)//print a format string to VS debug output
{
	char s[1024];
	va_list args;
	va_start(args, fmt);
	vsnprintf(s, 1024, fmt, args);
	OutputDebugStringA(s);
	va_end(args);
}

class Decorator
{
private:
	interface_t &data;
public:
	Decorator(interface_t &target_data) :data(target_data) { }
	void decorate_start(CKBehavior *bb)
	{
		assert(bb->GetInputCount() == 1);
		data.start.id = bb->GetInput(0)->GetOwner()->GetID();
	}
	std::map<CK_ID, int> bmap;
	std::set<CK_ID> pins;
	std::set<CK_ID> pouts;
	struct pio_pos_t { CK_ID id; int idx; CK_ID lnk_within; };
	pio_pos_t GetpInPos(CKParameterIn *pin, CKBehavior **owningbeh)
	{
		pio_pos_t ret;
		CKObject* ob = pin->GetOwner();
		ret.id = ob->GetID();
		if (ob->GetClassID() == CKCID_BEHAVIOR)
		{
			CKBehavior* obb = (CKBehavior*)ob;
			ret.idx = obb->GetInputParameterPosition(pin);
			if (obb->IsUsingTarget() && obb->GetTargetParameter()->GetID() == pin->GetID())
				ret.idx = -2;
			*owningbeh = obb->GetParent();
			ret.lnk_within = (*owningbeh)->GetID();
			return ret;
		}
		else if (ob->GetClassID() == CKCID_PARAMETEROPERATION)
		{
			CKParameterOperation* obop = (CKParameterOperation*)ob;
			ret.idx = obop->GetInParameter1()->GetID() == pin->GetID() ? 0 : 1;
			*owningbeh = obop->GetOwner();
			ret.lnk_within = (*owningbeh)->GetID();
			return ret;
		}
		else throw;
	}
	pio_pos_t GetpOutPos(CKParameterOut *pout, CKBehavior **owningbeh)
	{
		pio_pos_t ret;
		CKObject* ob = pout->GetOwner();
		ret.id = ob->GetID();
		if (ob->GetClassID() == CKCID_BEHAVIOR)
		{
			CKBehavior* obb = (CKBehavior*)ob;
			ret.idx = obb->GetOutputParameterPosition(pout);
			*owningbeh = obb->GetParent();
			ret.lnk_within = (*owningbeh)->GetID();
			return ret;
		}
		else if (ob->GetClassID() == CKCID_PARAMETEROPERATION)
		{
			CKParameterOperation* obop = (CKParameterOperation*)ob;
			ret.idx = 0;
			*owningbeh = obop->GetOwner();
			ret.lnk_within = (*owningbeh)->GetID();
			return ret;
		}
		else throw;
	}
	pio_pos_t GetpLocalPos(CKParameterLocal *plocal)
	{
		pio_pos_t ret;
		CKObject* ob = plocal->GetOwner();
		assert(ob->GetClassID() == CKCID_BEHAVIOR);
		CKBehavior* obb = (CKBehavior*)ob;
		ret.id = obb->GetID();
		ret.idx = obb->GetLocalParameterPosition(plocal);
		ret.lnk_within = ret.id;
		return ret;
	}
	link_endpoint_t GetParameterEndpoint(CKParameter* p)
	{
		if (p->GetClassID() == CKCID_PARAMETERLOCAL)
		{
			pio_pos_t t = GetpLocalPos((CKParameterLocal*)p);
			return link_endpoint_t{ t.id,t.idx,9 };
		}
		CKBehavior* dummy;
		pio_pos_t t = GetpOutPos((CKParameterOut*)p, &dummy);
		return link_endpoint_t{ t.id,t.idx,8 };
	}
	CKBehavior* GetParameterOwnerBehavior(CKParameter* p)
	{
		if (p->GetClassID() == CKCID_PARAMETERLOCAL)
			return (CKBehavior*)p->GetOwner();
		if (p->GetClassID() == CKCID_PARAMETEROUT)
		{
			CKObject* owner = p->GetOwner();
			if (owner->GetClassID() == CKCID_BEHAVIOR)
				return ((CKBehavior*)owner)->GetParent();
			if (owner->GetClassID() == CKCID_PARAMETEROPERATION)
				return ((CKParameterOperation*)owner)->GetOwner();
			throw;
		}
		throw;
	}
#define mappedb(id) (~bmap[id]?data.bbs[bmap[id]]:data.script_root)
	pio_pos_t GetShortcutParamPos(CK_ID within, CK_ID source)
		//Get a shortcut for `source` within the behavior `within`.
		//If such shortcut doesn't exist, create it.
	{
		for (int i = 0, c = mappedb(within).n_shared_param; i<c; ++i)
			if (mappedb(within).shared_params[i].source_id == source)
			{
				return pio_pos_t{ within,i,within };
			}
		param_t p; p.source_id = source; p.h_pos = p.v_pos = 0;
		p.id = source; p.style = param_style_closed;
		mappedb(within).shared_params.push_back(p);
		++mappedb(within).n_shared_param;
		return pio_pos_t{ within,mappedb(within).n_shared_param - 1,within };
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
		CKContext *ctx = root->GetCKContext();
		//std::set<std::pair<CK_ID,CK_ID>> pset;
		std::map<CK_ID, std::vector<pio_pos_t>> pin_chain;
		std::map<CK_ID, std::vector<pio_pos_t>> pout_chain;
		for (auto&i : pins)
		{
			std::vector<pio_pos_t> &vp = pin_chain[i] = std::vector<pio_pos_t>();
			CKParameterIn* pin = (CKParameterIn*)ctx->GetObjectA(i);
			CKBehavior *cb;
			vp.push_back(GetpInPos(pin, &cb));
			link_endpoint_t last = link_endpoint_t{ vp.back().id,vp.back().idx,vp.back().idx == -2 ? 10 : 7 };
			for (; cb&&cb->GetInputParameterPosition(pin) != -1; cb = cb->GetParent())
			{
				vp.push_back(pio_pos_t{ cb->GetID(),cb->GetInputParameterPosition(pin),cb->GetParent()->GetID() });
				link_t lnk_exp; lnk_exp.id = 0; lnk_exp.type = 0x10002; lnk_exp.point_count = 0;
				lnk_exp.start = link_endpoint_t{ vp.back().id,vp.back().idx,7 };
				lnk_exp.end = last;
				last = lnk_exp.start;
				mappedb(cb->GetID()).links.push_back(lnk_exp);
				++mappedb(cb->GetID()).n_links;
			}
		}
		for (auto&i : pouts)
		{
			std::vector<pio_pos_t> &vp = pout_chain[i] = std::vector<pio_pos_t>();
			CKParameterOut* pout = (CKParameterOut*)ctx->GetObjectA(i);
			CKBehavior *cb;
			vp.push_back(GetpOutPos(pout, &cb));
			link_endpoint_t last = link_endpoint_t{ vp.back().id,vp.back().idx,8 };
			for (; cb&&cb->GetOutputParameterPosition(pout) != -1; cb = cb->GetParent())
			{
				vp.push_back(pio_pos_t{ cb->GetID(),cb->GetOutputParameterPosition(pout),cb->GetParent()->GetID() });
				link_t lnk_exp; lnk_exp.id = 0; lnk_exp.type = 0x10002; lnk_exp.point_count = 0;
				lnk_exp.end = link_endpoint_t{ vp.back().id,vp.back().idx,8 };
				lnk_exp.start = last;
				last = lnk_exp.end;
				mappedb(cb->GetID()).links.push_back(lnk_exp);
				++mappedb(cb->GetID()).n_links;
			}
		}
		for (auto&i : pins)
		{
			CKParameterIn* pin = (CKParameterIn*)ctx->GetObjectA(i);
			CKBehavior *cb; pio_pos_t pinp = GetpInPos(pin, &cb);
			std::vector<pio_pos_t>& vpin = pin_chain[pin->GetID()];
			if (pin->GetDirectSource())
			{
				CKParameter* dsrc = pin->GetDirectSource();
				std::vector<pio_pos_t>& vpsrc = pout_chain[dsrc->GetID()];
				if (dsrc->GetClassID() == CKCID_PARAMETERLOCAL&&vpsrc.empty())
					vpsrc.push_back(GetpLocalPos((CKParameterLocal*)dsrc));
				CKBehavior* dsob = GetParameterOwnerBehavior(dsrc);
				bool conn = false;
				for (auto&aa : vpin)
				{
					for (auto&bb : vpsrc)
						if (aa.lnk_within == bb.lnk_within)//direct connection within this behavior
						{
							link_t lnk; lnk.id = 0; lnk.type = 2; lnk.point_count = 0;
							lnk.start = link_endpoint_t{ bb.id,bb.idx,dsrc->GetClassID() == CKCID_PARAMETERLOCAL ? 9 : 8 };
							lnk.end = link_endpoint_t{ aa.id,aa.idx,pinp.idx == -2 ? 10 : 7 };
							mappedb(aa.lnk_within).links.push_back(lnk);
							++mappedb(aa.lnk_within).n_links;
							conn = true; break;
						}
					if (conn)break;
				}
				if (!conn)//still not connected, use a shortcut instead
				{
					link_t lnk; lnk.id = 0; lnk.type = 2; lnk.point_count = 0;
					pio_pos_t sshp = GetShortcutParamPos(pinp.lnk_within, dsrc->GetID());
					lnk.start = link_endpoint_t{ pinp.lnk_within,sshp.idx,5 };
					lnk.end = link_endpoint_t{ pinp.id,pinp.idx,pinp.idx == -2 ? 10 : 7 };
					mappedb(pinp.lnk_within).links.push_back(lnk);
					++mappedb(pinp.lnk_within).n_links;
				}
			}
			else if (pin->GetSharedSource())
			{
				CKParameterIn* shpin = pin->GetSharedSource();
				assert(shpin->GetOwner()->GetClassID() == CKCID_BEHAVIOR);
				std::vector<pio_pos_t>& vshpin = pin_chain[shpin->GetID()];
				//no shortcut here!
				bool conn = false;
				for (auto&aa : vpin)
				{
					for (auto&bb : vshpin)
						if (aa.lnk_within == bb.id)
						{
							link_t lnk; lnk.id = 0; lnk.type = 2; lnk.point_count = 0;
							lnk.start = link_endpoint_t{ bb.id,bb.idx,7 };
							lnk.end = link_endpoint_t{ aa.id,aa.idx,aa.idx == -2 ? 10 : 7 };
							mappedb(aa.lnk_within).links.push_back(lnk);
							++mappedb(aa.lnk_within).n_links;
							conn = true; break;
						}
					if (conn)break;
				}
				if (!conn)
					ctx->OutputToConsoleEx("pin: can't connect %d <-> %d, source type is %d", pin->GetID(), shpin->GetID(), shpin->GetClassID());
			}
		}
		//up to here we only have pOut->pOut and pOut->pLocal missing
		//so we iterate through all pOuts
		for (auto&i : pouts)
		{
			CKParameterOut* po = (CKParameterOut*)ctx->GetObjectA(i);
			std::vector<pio_pos_t>& vpout = pout_chain[po->GetID()];
			for (int j = 0, cd = po->GetDestinationCount(); j<cd; ++j)
			{
				CKParameter* dest = po->GetDestination(j);
				link_endpoint_t dendp = GetParameterEndpoint(dest);
				CK_ID dest_within = dest->GetOwner()->GetID();
				bool conn = false;
				for (auto&aa : vpout)
					if (aa.lnk_within == dest_within)
					{
						link_t lnk; lnk.id = 0; lnk.type = 2; lnk.point_count = 0;
						lnk.start = link_endpoint_t{ aa.id,aa.idx,8 };
						lnk.end = dendp;
						mappedb(aa.lnk_within).links.push_back(lnk);
						++mappedb(aa.lnk_within).n_links;
						conn = true; break;
					}
				if (!conn)
					if (dest->GetClassID() == CKCID_PARAMETERLOCAL)//when the pOut connects to a shortcut
					{
						link_t lnk; lnk.id = 0; lnk.type = 2; lnk.point_count = 0;
						pio_pos_t ssp = vpout.front();
						pio_pos_t sshp = GetShortcutParamPos(ssp.lnk_within, dest->GetID());
						lnk.start = link_endpoint_t{ ssp.id,ssp.idx,8 };
						lnk.end = link_endpoint_t{ sshp.id,sshp.idx,5 };
						mappedb(ssp.lnk_within).links.push_back(lnk);
						++mappedb(ssp.lnk_within).n_links;
					}
					else ctx->OutputToConsoleEx("pout: can't connect %d <-> %d, dest type is %d", po->GetID(), dest->GetID(), dest->GetClassID());
			}
		}
	}
	void move_to_bb_input(param_t &p, bb_t &bb, int input_pos)
	{
		int bb_h_pos = (int)round((bb.size.h_pos) / 20.0);
		int bb_v_pos = (int)round((bb.size.v_pos) / 20.0);
		p.h_pos = bb_h_pos + input_pos;
		p.v_pos = bb_v_pos - 1;
	}
	void move_to_bb_output(param_t &p, bb_t &bb, int input_pos)
	{
		int bb_h_pos = (int)round((bb.size.h_pos) / 20.0);
		int bb_v_pos = (int)round((bb.size.v_pos) / 20.0);
		p.h_pos = bb_h_pos + input_pos;
		p.v_pos = bb_v_pos + (int)round((bb.size.v_size) / 20.0) + 1;
	}
	void calc_param_local_positions(bb_t &bg, CKBehavior *beh)
	{
		CKContext *ctx = beh->GetCKContext();
		for (auto &plink : bg.links)
		{
			if (plink.type == 2) // plink
			{
				param_t *start = NULL;
				if (plink.start.type == 9)
					start = &bg.local_params[plink.start.index];
				else if(plink.start.type==5)
					start = &bg.shared_params[plink.start.index];
				if (start)
				{
					if (plink.end.type == 7)
						move_to_bb_input(*start, mappedb(plink.end.id), plink.end.index);
					else if(plink.end.type == 10)
						move_to_bb_input(*start, mappedb(plink.end.id), -1);
				}
				param_t *end = NULL;
				if (plink.end.type == 9)
					end = &bg.local_params[plink.end.index];
				if (end)
				{
					if (plink.start.type == 8)
						move_to_bb_output(*end, mappedb(plink.start.id), plink.start.index);
				}

			}
		}
	}
	void calc_bb_size(bb_t &bb, CKBehavior *beh)
	{
		if (bb.depth > 0)
		{
			int height = max(beh->GetOutputCount(), beh->GetInputCount());
			height = max(height, 1);
			int width = max(beh->GetOutputParameterCount(), beh->GetInputParameterCount());
			width = max(width, int((strlen(beh->GetName()) - 1) / 2.5) + 1);
			width = max(width, 2);
			bb.size.h_size = width*20.0;
			bb.size.v_size = height*20.0;
			if (bb.is_bg)
			{
				bb.h_expand_size = bb.size.h_size * 10;
				bb.v_expand_size = bb.size.v_size * 10;
			}
		}
	}
	void recalc_absolute_bb_pos(bb_t &bb, CKBehavior *beh, float start_h, float start_v)
	{
		if (bb.depth == 0)
		{
			bb.size.h_pos = 0;
			bb.size.v_pos = 0;
		}
		bb.size.h_pos += start_h;
		bb.size.v_pos += start_v;
		if (bb.is_bg)
		{
			int cnt = beh->GetSubBehaviorCount();
			for (int i = 0; i < cnt; ++i)
			{
				CKBehavior *sub_beh = beh->GetSubBehavior(i);
				recalc_absolute_bb_pos(mappedb(sub_beh->GetID()), sub_beh, bb.size.h_pos, bb.size.v_pos);
			}
		}
	}
	void decorate_bb(bb_t &bb, CKBehavior *beh, int dpt)
	{
		bb.id = beh->GetID();
		bb.folded = true;
		bb.depth = dpt;
		bb.is_bg = (beh->GetType() != CKBEHAVIORTYPE_BASE);
		calc_bb_size(bb, beh);
		for (int i = 0, c = beh->GetInputParameterCount(); i<c; ++i)
			pins.insert(beh->GetInputParameter(i)->GetID());
		for (int i = 0, c = beh->GetOutputParameterCount(); i<c; ++i)
			pouts.insert(beh->GetOutputParameter(i)->GetID());
		if (beh->IsUsingTarget())
			pins.insert(beh->GetTargetParameter()->GetID());
		for (int i = 0, c = beh->GetParameterOperationCount(); i<c; ++i)
		{
			pins.insert(beh->GetParameterOperation(i)->GetInParameter1()->GetID());
			pins.insert(beh->GetParameterOperation(i)->GetInParameter2()->GetID());
			pouts.insert(beh->GetParameterOperation(i)->GetOutParameter()->GetID());
		}
		if (bb.is_bg)
		{
			for (int i = 0, c = beh->GetSubBehaviorLinkCount(); i<c; ++i)
			{
				link_t lnk; lnk.type = 1;
				CKBehaviorLink *blink = beh->GetSubBehaviorLink(i);
				lnk.id = blink->GetID(); lnk.point_count = 0;
				lnk.start = lnk.end = link_endpoint_t();

				lnk.start.id = blink->GetInBehaviorIO()->GetOwner()->GetID();
				lnk.start.type = 13;
				lnk.start.index = blink->GetInBehaviorIO()->GetOwner()->GetOutputPosition(blink->GetInBehaviorIO());
				if (!~lnk.start.index)
				{
					lnk.start.index = blink->GetInBehaviorIO()->GetOwner()->GetInputPosition(blink->GetInBehaviorIO());
					lnk.start.type = 12;
					if (blink->GetInBehaviorIO()->GetOwner()->GetType() == CKBEHAVIORTYPE_SCRIPT)
						lnk.start.type = 26;
				}

				lnk.end.id = blink->GetOutBehaviorIO()->GetOwner()->GetID();
				lnk.end.type = 12;
				lnk.end.index = blink->GetOutBehaviorIO()->GetOwner()->GetInputPosition(blink->GetOutBehaviorIO());
				if (!~lnk.end.index)
				{
					lnk.end.index = blink->GetOutBehaviorIO()->GetOwner()->GetOutputPosition(blink->GetOutBehaviorIO());
					lnk.end.type = 13;
				}
				bb.links.push_back(lnk);
			}
			for (int i = 0, c = beh->GetParameterOperationCount(); i<c; ++i)
			{
				op_t op;
				CKParameterOperation* pop = beh->GetParameterOperation(i);
				op.id = pop->GetID();
				bb.ops.push_back(op);
			}
			bb.n_ops = bb.ops.size();
			for (int i = 0, c = beh->GetLocalParameterCount(); i<c; ++i)
			{
				param_t p; p.h_pos = p.v_pos = 0;
				CKParameterLocal* pl = beh->GetLocalParameter(i);
				p.id = pl->GetID(); p.style = param_style_closed;
				bb.local_params.push_back(p);
			}
			bb.n_local_param = bb.local_params.size();
		}
	}
	void decorate_bbs(CKBehavior *bb)
	{
		data.n_bb = 0;
		data.bbs.clear();
		bmap.clear();
		pins.clear(); pouts.clear();
		std::queue<std::pair<CKBehavior*, int>> bq;
		bq.push(std::make_pair(bb, 0));
		while (!bq.empty())
		{
			CKBehavior* cur = bq.front().first;
			int cdpt = bq.front().second; bq.pop();
			if (cdpt)
			{
				data.bbs.push_back(bb_t());
				++data.n_bb;
			}
			bmap[cur->GetID()] = cdpt ? data.bbs.size() - 1 : -1;
			decorate_bb(cdpt ? data.bbs.back() : data.script_root, cur, cdpt);
			int cnt = cur->GetSubBehaviorCount();
			for (int i = 0; i < cnt; ++i)
			{
				CKBehavior *sub_bb = cur->GetSubBehavior(i);
				bq.push(std::make_pair(sub_bb, cdpt + 1));
			}
		}
		configure_plink(bb);
		CKContext *ctx = bb->GetCKContext();
		for (auto &kv : bmap)
		{
			bb_t &sub_bb = mappedb(kv.first);
			calc_param_local_positions(mappedb(kv.first), (CKBehavior *)ctx->GetObjectA(sub_bb.id));
		}
	}
	// To create missing entries in interface_t
	void decorate(CKBehavior *bb)
	{
		decorate_start(bb);
		decorate_bbs(bb);
		recalc_absolute_bb_pos(data.script_root, bb, 0.0, 0.0);
	}

};
void decorate(interface_t &data, CKBehavior *bb)
{
	Decorator decorator = Decorator(data);
	decorator.decorate(bb);
}