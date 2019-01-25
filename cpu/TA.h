#ifndef TA_H
#define TA_H

#include "AA.h"
#include <queue>
#include <unordered_set>

#define TA_BLOCK 1024

template<class T,class Z>
class TA : public AA<T,Z>{
	public:
		TA(uint64_t n,uint64_t d) : AA<T,Z>(n,d)
		{
			this->algo = "TA";
			this->alists = NULL;
		}

		~TA()
		{
			if(this->alists!=NULL)
			{
				for(uint64_t i = 0;i < this->d;i++){
					if(this->alists[i] != NULL)
					{
						free(this->alists[i]);
					}
				}
			}
			free(this->alists);
		}

		void init();
		void findTopK(uint64_t k,uint8_t qq, T *weights, uint8_t *attr);

	private:
		pred<T,Z> **alists;
};

template<class T,class Z>
void TA<T,Z>::init(){
	//this->lists.resize(this->d);
	normalize<T,Z>(this->cdata, this->n, this->d);
	this->alists = (pred<T,Z>**)malloc(sizeof(pred<T,Z>*)*this->d);
	for(uint32_t m = 0; m < this->d; m++){ this->alists[m] = (pred<T,Z>*)malloc(sizeof(pred<T,Z>)*this->n); }

	this->t.start();
	for(uint64_t i=0;i<this->n;i++){
		for(uint8_t m =0;m<this->d;m++){
			this->alists[m][i] = pred<T,Z>(i,this->cdata[i*this->d + m]);
		}
	}

	for(uint32_t m =0;m<this->d;m++){
		__gnu_parallel::sort(this->alists[m],this->alists[m]+this->n,cmp_max_pred<T,Z>);
	}
	this->tt_init = this->t.lap();
}

template<class T,class Z>
void TA<T,Z>::findTopK(uint64_t k,uint8_t qq, T *weights, uint8_t *attr){
	std::cout << this->algo << " find top-" << k << " (" << (int)qq << "D) ...";
	std::unordered_set<Z> eset;
	if(this->res.size() > 0) this->res.clear();
	if(STATS_EFF) this->pred_count=0;
	if(STATS_EFF) this->tuple_count = 0;
	if(STATS_EFF) this->pop_count=0;
	if(STATS_EFF) this->candidate_count=0;

	std::priority_queue<T, std::vector<tuple_<T,Z>>, PQComparison<T,Z>> q;
	this->t.start();
	for(uint64_t i = 0; i < this->n;i++){
		T threshold=0;
		for(uint8_t mm = 0; mm < qq; mm++){
			pred<T,Z> p = this->alists[attr[mm]][i];//M(2)
			T weight = weights[attr[mm]];//M(2)
			threshold+=p.attr*weight;//M(1)

			if(STATS_EFF) this->accesses+=6;//2 + 2 + 1 + 2
			if(eset.find(p.tid) == eset.end()){//M(2)
				T score00 = 0;
				for(uint8_t m = 0; m < qq; m++){ score00+=this->cdata[p.tid * this->d + attr[m]] * weights[attr[m]]; }//M(4)

				if(STATS_EFF) this->accesses+=qq*4;
				if(STATS_EFF) this->pred_count+=this->d;
				if(STATS_EFF) this->tuple_count+=1;
				eset.insert(p.tid);//M(1)
				if(STATS_EFF) this->accesses+=1;
				if(q.size() < k){//insert if empty space in queue//M(1)
					q.push(tuple_<T,Z>(p.tid,score00));//M(1)
					if(STATS_EFF) this->accesses+=1;
				}else if(q.top().score<score00){//delete smallest element if current score is bigger
					q.pop();//M(1)
					q.push(tuple_<T,Z>(p.tid,score00));//M(1)
					if(STATS_EFF) this->accesses+=2;
				}
			}
		}
		if(STATS_EFF) this->accesses+=2;
		if(q.size() >= k && ((q.top().score) >= threshold) ){//M(2)
			this->lvl = i;
			break;
		}
	}
	this->tt_processing += this->t.lap();
	if(STATS_EFF) this->candidate_count=k;

	T threshold = q.top().score;
	while(!q.empty()){
		this->res.push_back(q.top());
		q.pop();
	}
	std::cout << std::fixed << std::setprecision(4);
	std::cout << " threshold=[" << threshold <<"] (" << this->res.size() << ")" << std::endl;
	this->threshold = threshold;
}

#endif
