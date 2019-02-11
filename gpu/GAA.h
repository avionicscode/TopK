#ifndef GAA_H
#define GAA_H

/*
 * GPU Aggregation Abstract Header
 */
#include "../tools/CudaHelper.h"
#include "../tools/tools.h"
#include <inttypes.h>
#include <vector>
#include <queue>
#include <algorithm>
#include <cub/cub.cuh>

#define SHARED_MEM_BYTES 49152
#define THREADS_PER_BLOCK 256
#define U32_BYTES_PER_TUPLE 8
#define U64_BYTES_PER_TUPLE 12

#include <unistd.h>

#define VALIDATE false//ENABLE RESULT VALIDATION//
#define USE_DEVICE_MEM true

//PTA DEVICE MEMORY USAGE//
#define USE_POLAR_DEV_MEM true // USE DEVICE MEMORY TO COMPUTE POLAR COORDINATES
#define USE_PART_REORDER_DEV_MEM true
#define USE_BUILD_PART_DEV_MEM true

__constant__ float gpu_weights[NUM_DIMS];
__constant__ uint32_t gpu_query[NUM_DIMS];

template<class T>
__device__ T swap(T a, uint32_t stride, int dir){
	T b = __shfl_xor_sync(0xFFFFFFFF,a,stride);
	return ((a < b) == dir) ? b : a;
}
template<class T>
__device__ T rswap(T a, uint32_t stride, int dir){
	T b = __shfl_xor_sync(0xFFFFFFFF,a,stride);
	return ((a > b) == dir) ? b : a;
}

__device__ uint32_t bfe(uint32_t a, uint32_t i){
	return (a >> i) & 0x1;
}

/*
 * initialize global rearrange vector
 */
template<class Z>
__global__ void init_rvlocal(Z *dkeys_in, uint64_t n)
{
	uint64_t i = blockIdx.x * blockDim.x + threadIdx.x;
	if( i < n ){ dkeys_in[i] = i; }
}

/*
 * initialize first seen position
 */

template<class Z>
__global__ void init_first_seen_position(Z *rvector, uint64_t n)
{
	Z i = blockIdx.x * blockDim.x + threadIdx.x;
	if( i < n ){ rvector[i] = n; }
}

/*
 * find max rearrange value
 */
template<class Z>
__global__ void max_rvglobal(Z *rvector, Z *dkeys_in, uint64_t n)
{
	Z i = blockIdx.x * blockDim.x + threadIdx.x;
	if( i < n ){
		Z obj_id = dkeys_in[i];
		rvector[obj_id] = min(rvector[obj_id],i);
	}

}

void wait(){
	 do { std::cout << '\n' << "Press a key to continue..."; } while (std::cin.get() != '\n');
}

/*
 * Tuple structure
 */
template<class T,class Z>
struct ranked_tuple{
	ranked_tuple(){ tid = 0; score = 0; }
	ranked_tuple(Z t, T s){ tid = t; score = s; }
	Z tid;
	T score;
};

template<class T,class Z>
class MaxFirst{
	public:
		MaxFirst(){};

		bool operator() (const ranked_tuple<T,Z>& lhs, const ranked_tuple<T,Z>& rhs) const{
			return (lhs.score>rhs.score);
		}
};

template<class T, class Z>
static T compute_threshold(T* cdata, uint64_t n, uint64_t d, T *weights, uint32_t *query, uint64_t k){
	std::priority_queue<T, std::vector<ranked_tuple<T,Z>>, MaxFirst<T,Z>> q;
	T *csvector =(T*)malloc(sizeof(T)*n);
	for(uint64_t i = 0; i < n; i++){
		T score = 0;
		for(uint64_t m = 0; m < d; m++){
			uint32_t a = query[m];
			score+=cdata[a*n + i] * weights[a];
		}
		csvector[i] = score;
		if(q.size() < k){
			q.push(ranked_tuple<T,Z>(i,score));
		}else if ( q.top().score < score ){
			q.pop();
			q.push(ranked_tuple<T,Z>(i,score));
		}
	}
	std::cout << std::fixed << std::setprecision(4);
	//std::cout << "threshold: " << q.top().score << std::endl;
	return q.top().score;
}

template<class T, class Z>
static T find_threshold(T *cscores, uint64_t n, uint64_t k){
	std::priority_queue<T, std::vector<ranked_tuple<T,Z>>, MaxFirst<T,Z>> q;
	for(uint64_t i = 0; i < n; i++){
		if(q.size() < k){
			q.push(ranked_tuple<T,Z>(i,cscores[i]));
		}else if ( q.top().score < cscores[i] ){
			q.pop();
			q.push(ranked_tuple<T,Z>(i,cscores[i]));
		}
	}
	std::cout << std::fixed << std::setprecision(4);
	//std::cout << "threshold: " << q.top().score << std::endl;
	return q.top().score;
}

template<class T, class Z>
static T find_partial_threshold(T *cscores, uint64_t n, uint64_t k, bool type, uint32_t remainder){
	std::priority_queue<T, std::vector<ranked_tuple<T,Z>>, MaxFirst<T,Z>> q;

	if(type){
		for(uint64_t j = 0; j < n; j+=4096){
			for(uint64_t i = 0; i < k;i++){
				if(q.size() < k){
					q.push(ranked_tuple<T,Z>(i+j,cscores[i+j]));
				}else if ( q.top().score < cscores[i+j] ){
					q.pop();
					q.push(ranked_tuple<T,Z>(i+j,cscores[i+j]));
				}
			}
		}
	}else{
		for(uint64_t j = 0; j < remainder; j++){
			if(q.size() < k){
				q.push(ranked_tuple<T,Z>(j,cscores[j]));
			}else if ( q.top().score < cscores[j] ){
				q.pop();
				q.push(ranked_tuple<T,Z>(j,cscores[j]));
			}
		}
	}
	std::cout << std::fixed << std::setprecision(4);
	//std::cout << "partial threshold: " << q.top().score << std::endl;
	return q.top().score;
}

template<class T, class Z>
static T find_remain_threshold(T *cscores, uint64_t n, uint64_t k, uint32_t remainder){
	std::priority_queue<T, std::vector<ranked_tuple<T,Z>>, MaxFirst<T,Z>> q;

	for(uint64_t j = 0; j < n; j+=4096){
		for(uint64_t i = 0; i < remainder;i++){
			if(q.size() < k){
				q.push(ranked_tuple<T,Z>(i+j,cscores[i+j]));
			}else if ( q.top().score < cscores[i+j] ){
				q.pop();
				q.push(ranked_tuple<T,Z>(i+j,cscores[i+j]));
			}
		}
	}
	std::cout << std::fixed << std::setprecision(4);
	//std::cout << "remain threshold: " << q.top().score << std::endl;
	return q.top().score;
}

template<class T, class Z>
class GAA{
	public:
		GAA<T,Z>(uint64_t n, uint64_t d){
			this->n = n;
			this->d = d;
			this->cdata = NULL;
			this->gdata = NULL;
			this->cids = NULL;
			this->gids = NULL;
			this->cscores = NULL;
			this->gscores = NULL;
			this->cpu_threshold = 0;
			this->gpu_threshold = 0;

			this->iter = 1;
			this->parts = 0;
			this->tt_init = 0;
			this->tt_processing = 0;
			this->pred_count = 0;
			this->tuple_count = 0;
			this->queries_per_second = 0;
			std::cout << std::fixed << std::setprecision(4);
		};

		~GAA<T,Z>(){
			if(this->cdata != NULL){ cudaFreeHost(this->cdata); this->cdata = NULL; }
			if(this->gdata != NULL){ cudaFree(this->gdata); this->gdata = NULL; }
			if(this->cids != NULL){ cudaFreeHost(this->cids); this->cids = NULL; }
			if(this->gids != NULL){ cudaFree(this->gids); this->gids = NULL; }
			if(this->cscores != NULL){ cudaFreeHost(this->cscores); this->cscores = NULL; }
			if(this->gscores != NULL){ cudaFree(this->gscores); this->gscores = NULL; }
		};

		void findTopKtpac(uint64_t k,uint8_t qq, T *weights, uint32_t *attr);

		std::string get_algo(){ return this->algo; }
		T get_thres(){return this->threshold;}
		T*& get_cdata(){ return this->cdata; }
		void set_cdata(T *cdata){ this->cdata = cdata; }
		T*& get_gdata(){ return this->gdata; }
		void set_gdata(T *gdata){ this->gdata = gdata; }

		void set_iter(uint64_t iter){ this->iter = iter; }
		void set_partitions(uint64_t parts){ this->parts = parts; }

		void benchmark(){
			std::cout << std::fixed << std::setprecision(4);
 			std::cout << "< Benchmark for " << this->algo <<"("<< this->parts <<") algorithm >" << std::endl;
 			std::cout << "Accessing data from {" << (USE_DEVICE_MEM ? "Device Memory" : "Host Memory")  << "}"<< std::endl;
			std::cout << "tt_init: " << this->tt_init << std::endl;
			std::cout << "tt_procesing: " << this->tt_processing/this->iter << std::endl;
			//std::cout << "tuples_per_second: " << (this->tt_processing == 0 ? 0 : WORKLOAD/(this->tt_processing/1000)) << std::endl;
			std::cout << "tuple_count: " << this->tuple_count << std::endl;
			std::cout << "cpu_threshold: " << this->cpu_threshold << std::endl;
			std::cout << "gpu_threshold: " << this->gpu_threshold << std::endl;
			std::cout << "_______________________________________________________________" << std::endl;
			this->reset_stats();
		}

		void reset_stats(){
			//this->tt_init = 0;
			this->tt_processing = 0;

			this->pred_count = 0;
			this->tuple_count = 0;
			this->queries_per_second = 0;
		}

		void copy_query(T *weights, uint32_t *query){
			this->weights = weights;
			this->query = query;
			cutil::cudaCheckErr(cudaMemcpyToSymbol(gpu_weights, weights, sizeof(T)*NUM_DIMS),"copy weights");//Initialize preference vector
			cutil::cudaCheckErr(cudaMemcpyToSymbol(gpu_query, query, sizeof(uint32_t)*NUM_DIMS),"copy query");//Initialize query vector
		}

	protected:
		std::string algo = "default";
		uint64_t n,d;// cardinality,dimensinality
		T *cdata;
		T *gdata;
		Z *cids;
		Z *gids;
		T *cscores;
		T *gscores;
		T cpu_threshold;
		T gpu_threshold;

		T *weights;
		uint32_t *query;

		uint64_t iter;//experiment count
		uint64_t parts;
		double tt_init;
		double tt_processing;
		uint64_t pred_count;//count predicate evaluations
		uint64_t tuple_count;//count predicate evaluations
		uint64_t queries_per_second;

		Time<msecs> t;
};

#endif
