#ifndef AA_H
#define AA_H

#include<vector>
#include<algorithm>
#include<map>

#include <mmintrin.h>//MMX
#include <xmmintrin.h>//SSE
#include <emmintrin.h>//SSE2
#include <emmintrin.h>//SSE3
#include <immintrin.h>//AVX and AVX2 // AVX-512

#define THREADS 8
#define STATS_EFF true

#include <parallel/algorithm>
#include <omp.h>

/*
 * Predicate structure
 */
template<class T,class Z>
struct pred{
	pred(){ tid = 0; attr = 0; }
	pred(Z t, T a){ tid = t; attr = a; }
	Z tid;
	T attr;
};

/*
 * Tuple structure
 */
template<class T,class Z>
struct tuple{
	tuple(){ tid = 0; score = 0; }
	tuple(Z t, T s){ tid = t; score = s; }
	Z tid;
	T score;
};

template<class T,class Z>
static bool cmp_score(const tuple<T,Z> &a, const tuple<T,Z> &b){ return a.score > b.score; };

template<class T,class Z>
static bool cmp_max_pred(const pred<T,Z> &a, const pred<T,Z> &b){ return a.attr > b.attr; };

/*
 * Base class for aggregation algorithm
 */
template<class T,class Z>
class AA{
	public:
		AA(uint64_t n, uint64_t d);
		~AA();

		void compare(AA<T,Z> b);
		std::vector<tuple<T,Z>> get_res(){ return this->res; };
		std::string get_algo(){ return this->algo; };

		void benchmark();

		T*& get_cdata(){ return this->cdata; }
		void set_cdata(T *cdata){ this->cdata = cdata; }

		void set_init_exec(bool initp){ this->initp = initp; }
		void set_topk_exec(bool topkp){ this->topkp = topkp; }

	protected:
		std::string algo;
		std::vector<tuple<T,Z>> res;
		T *cdata;
		uint64_t n;
		uint64_t d;

		bool initp;// Parallel Initialize
		bool topkp;// Parallel TopK Calculation

		Time<msecs> t;
		double tt_init;//initialization time
		double tt_processing;//processing time
		uint64_t pred_count;//count predicate evaluations
		uint64_t tuple_count;//count predicate evaluations

		bool stats_eff;
		bool stats_time;
};

template<class T,class Z>
AA<T,Z>::AA(uint64_t n, uint64_t d){
	this->tt_init = 0;
	this->tt_processing = 0;
	this->pred_count = 0;
	this->tuple_count = 0;
	this->n = n;
	this->d = d;
	this->cdata = NULL;
	this->initp = false;
	this->topkp=false;
}

template<class T,class Z>
AA<T,Z>::~AA(){
	//if(this->cdata!=NULL) free(this->cdata);
}

/*
 * cmp results to check correctness
 */
template<class T,class Z>
void AA<T,Z>::compare(AA<T,Z> b){
	std::string cmp = "PASSED";
	std::map<Z,T> tmap;

	/*create map with tuple ids*/
	for(uint64_t i = 0;i < this->res.size();i++){
		tmap[this->res[i].tid] = this->res[i].score;
	}

	/*check if ids in b exist in my result*/
	for(uint64_t i = 0;i < b.res.size();i++){
		if (tmap.find(b.get_res()[i].tid) == tmap.end()){//find if id of b does not exist in map
			std::cout <<"i:(" << i << ") "<<this->res[i].tid << " = " << this->res[i].score << std::endl;
			cmp  = "FAILED";
			std::cout << "(" <<this->algo <<") != (" << b.get_algo() << ") ( "<< cmp <<" )" << std::endl;
			exit(1);
		}
	}
	std::cout << "(" <<this->algo <<") compare result to (" << b.get_algo() << ") ( "<< cmp <<" )" << std::endl;
}

/*
 * List benchmarking information
 */
template<class T,class Z>
void AA<T,Z>::benchmark(){
	std::cout << "< Benchmark for " << this->algo << " algorithm >" << std::endl;
	std::cout << "tt_init: " << this->tt_init << std::endl;
	std::cout << "tt_procesing: " << this->tt_processing << std::endl;

	std::cout << "pred_count: " << this->pred_count << std::endl;
	std::cout << "tuple_count: " << this->tuple_count << std::endl;
}

#endif
